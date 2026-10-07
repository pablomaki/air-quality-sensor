#!/usr/bin/env bash
#
# Build the firmware variants defined in variants/ and collect the images into
# bin/<variant>/:
#   factory.bin  merged image for the first USB flash, at offset 0x0
#   app.bin      signed application image
#   app.ota      Matter OTA image, served by the OTA provider
#
# Builds run one at a time and pinned to a few cores on purpose: the Matter GN
# step spawns a nested ninja that does not inherit the outer -j. Unpinned this
# exhausts RAM on a 16 core machine.

set -euo pipefail

FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$FIRMWARE_DIR"

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v6.0.2}"
ESP_MATTER_PATH="${ESP_MATTER_PATH:-$HOME/esp/esp-matter}"
SIGNING_KEY="${AQS_SIGNING_KEY:-$HOME/.config/air-quality-sensor/ota_signing_key.pem}"
CPUS="${CPUS:-0-3}"
PRISTINE=0
SETUP_ENV=1

usage() {
    cat <<USAGE
Usage: ${0##*/} [options] [variant ...]

Builds every variant in variants/ unless specific ones are named.

Options:
  -p   pristine build, discarding the existing build directory
  -E   do not set up the toolchain environment, use the current shell's
  -h   this help

Environment overrides: IDF_PATH, ESP_MATTER_PATH, AQS_SIGNING_KEY, BSEC_DIR, CPUS

Examples:
  ${0##*/}                 build all variants
  ${0##*/} aqs_001         build one
  ${0##*/} -p aqs_002 aqs_003
USAGE
}

while getopts ":pEh" opt; do
    case "$opt" in
        p) PRISTINE=1 ;;
        E) SETUP_ENV=0 ;;
        h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

die() { echo "error: $*" >&2; exit 1; }

if [[ $SETUP_ENV -eq 1 ]]; then
    [[ -f "$IDF_PATH/export.sh" ]] || die "ESP-IDF not found at $IDF_PATH"
    [[ -f "$ESP_MATTER_PATH/export.sh" ]] || die "esp-matter not found at $ESP_MATTER_PATH"
    # shellcheck disable=SC1091
    source "$IDF_PATH/export.sh" >/dev/null
    # shellcheck disable=SC1091
    source "$ESP_MATTER_PATH/export.sh" >/dev/null
fi
export ESP_MATTER_PATH

# Every image must be signed with the same key for the boards to accept updates,
# so it lives outside the repository and outside any build directory.
if [[ ! -f "$SIGNING_KEY" ]]; then
    mkdir -p "$(dirname "$SIGNING_KEY")"
    espsecure.py generate_signing_key --version 2 --scheme ecdsa256 "$SIGNING_KEY"
    chmod 600 "$SIGNING_KEY"
    cat >&2 <<WARN
warning: created a new OTA signing key at
           $SIGNING_KEY
         Back it up. Boards running images signed with it accept updates only
         from the same key; losing it means reflashing every board over USB.
WARN
fi

variants=("$@")
if [[ ${#variants[@]} -eq 0 ]]; then
    for f in variants/*.defaults; do
        variants+=("$(basename "$f" .defaults)")
    done
fi

for variant in "${variants[@]}"; do
    [[ -f "variants/$variant.defaults" ]] || die "no such variant: $variant"

    build_dir="build/$variant"
    out_dir="bin/$variant"
    if [[ $PRISTINE -eq 1 ]]; then
        rm -rf "$build_dir"
    fi
    mkdir -p "$build_dir" "$out_dir"

    printf 'CONFIG_SECURE_BOOT_SIGNING_KEY="%s"\n' "$SIGNING_KEY" > "$build_dir/signing.defaults"

    echo "=== Building $variant ==="
    taskset -c "$CPUS" idf.py -B "$build_dir" \
        -DSDKCONFIG="$build_dir/sdkconfig" \
        -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;variants/$variant.defaults;$build_dir/signing.defaults" \
        build
    idf.py -B "$build_dir" -DSDKCONFIG="$build_dir/sdkconfig" merge-bin -o "$FIRMWARE_DIR/$out_dir/factory.bin"

    cp "$build_dir/air_quality_sensor.bin" "$out_dir/app.bin"
    cp "$build_dir/air_quality_sensor-ota.bin" "$out_dir/app.ota"
    echo "=== $variant: $(stat -c %s "$out_dir/app.bin") bytes, images in $out_dir ==="
done
