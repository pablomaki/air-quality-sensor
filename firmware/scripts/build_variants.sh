#!/usr/bin/env bash
#
# Build the firmware variants defined in variants/ and collect the resulting UF2
# images into bin/.
#
# Each variant is <name>.overlay plus an optional <name>.conf, layered on top of
# prj.conf and the board overlay. Variant aqs_001 builds in build_001 and lands
# in bin/aqs_001.uf2.
#
# Builds run one at a time and pinned to a few cores on purpose: the Matter GN
# step spawns a nested ninja that does not inherit west's -j, and with LTO the
# link is large. Unpinned this exhausts RAM on a 16 core machine.

set -euo pipefail

FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$FIRMWARE_DIR"

BOARD="${BOARD:-xiao_ble/nrf52840/sense}"
NCS_VERSION="${NCS_VERSION:-v3.2.4}"
NCS_ROOT="${NCS_ROOT:-$HOME/ncs}"
CPUS="${CPUS:-0-3}"
PRISTINE=""
SETUP_ENV=1

usage() {
    cat <<EOF
Usage: ${0##*/} [options] [variant ...]

Builds every variant in variants/ unless specific ones are named.

Options:
  -b BOARD   board target (default: $BOARD)
  -p         pristine build, discarding the existing build directory
  -E         do not set up the toolchain environment, use the current shell's
  -h         this help

Environment overrides: BOARD, NCS_VERSION, NCS_ROOT, CPUS

Examples:
  ${0##*/}                 build all variants
  ${0##*/} aqs_001         build one
  ${0##*/} -p aqs_002 aqs_003
EOF
}

while getopts ":b:pEh" opt; do
    case "$opt" in
        b) BOARD="$OPTARG" ;;
        p) PRISTINE="--pristine=always" ;;
        E) SETUP_ENV=0 ;;
        h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

die() { echo "error: $*" >&2; exit 1; }

# Resolve the toolchain bundle for NCS_VERSION and export what a plain shell
# needs. Deliberately does not add the toolchain's usr/local/bin to PATH: the
# ninja there is a python wrapper that re-invokes ninja by name and fork bombs if
# it finds itself first. Only opt/bin is needed, for gn.
setup_environment() {
    local manifest="$NCS_ROOT/toolchains/toolchains.json"
    [ -f "$manifest" ] || die "no toolchain manifest at $manifest"

    local bundle
    bundle="$(python3 - "$manifest" "$NCS_VERSION" <<'PY'
import json, sys
manifest, want = sys.argv[1], sys.argv[2]
for group in json.load(open(manifest)):
    for tc in group.get("toolchains", []):
        if want in tc.get("ncs_versions", []):
            print(tc["identifier"]["bundle_id"])
            sys.exit(0)
sys.exit(1)
PY
)" || die "no toolchain registered for $NCS_VERSION"

    local tc="$NCS_ROOT/toolchains/$bundle"
    local ncs="$NCS_ROOT/$NCS_VERSION"
    [ -d "$tc" ] || die "toolchain $bundle not installed"
    [ -d "$ncs" ] || die "$NCS_VERSION not installed at $ncs"

    export NCS_DIR="$ncs"
    export ZEPHYR_BASE="$ncs/zephyr"
    export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
    export ZEPHYR_SDK_INSTALL_DIR="$tc/opt/zephyr-sdk"
    export PATH="$tc/opt/bin:$PATH"

    # The Matter code generator imports python_path, which the matter module does
    # not ship, and needs jinja2 and lark from the toolchain's site-packages.
    local shim="$FIRMWARE_DIR/build_shim"
    mkdir -p "$shim"
    cat > "$shim/python_path.py" <<'PY'
import os, sys


class PythonPath:
    def __init__(self, path, relative_to=None):
        if relative_to:
            path = os.path.join(os.path.dirname(os.path.abspath(relative_to)), path)
        self.path = path

    def __enter__(self):
        sys.path.insert(0, self.path)
        return self

    def __exit__(self, *exc):
        try:
            sys.path.remove(self.path)
        except ValueError:
            pass
PY
    export PYTHONPATH="$shim:$tc/usr/local/lib/python3.12/site-packages:$ncs/modules/lib/matter/scripts/py_matter_idl${PYTHONPATH:+:$PYTHONPATH}"

    echo "toolchain $bundle for $NCS_VERSION"
}

# The BSEC sources live in a west manifest group that is disabled by default, and
# their absence otherwise surfaces as a bare "cannot find source file" from CMake.
check_bsec() {
    local conf="$1"
    [ -f "$conf" ] || return 0
    grep -q "^CONFIG_BME68X_IAQ=y" "$conf" || return 0

    local ncs="${NCS_DIR:-${ZEPHYR_BASE%/zephyr}}"
    [ -f "$ncs/modules/lib/bme68x/src/bme68x/bme68x.c" ] && return 0

    cat >&2 <<EOF
error: this variant needs the Bosch BSEC libraries, which are not fetched.
       They belong to the 'bsec' west manifest group, disabled by default:

           cd $ncs
           west config manifest.group-filter -- +bsec
           west update bme68x bsec

       Note that BSEC is licensed software from Bosch.
EOF
    exit 1
}

variants=("$@")
if [ ${#variants[@]} -eq 0 ]; then
    for overlay in variants/*.overlay; do
        [ -e "$overlay" ] || die "no variants found in variants/"
        variants+=("$(basename "$overlay" .overlay)")
    done
fi

[ "$SETUP_ENV" -eq 1 ] && setup_environment
command -v west >/dev/null || die "west not found on PATH"

mkdir -p bin build_logs
declare -a summary=()

for variant in "${variants[@]}"; do
    overlay="variants/$variant.overlay"
    [ -f "$overlay" ] || die "$overlay does not exist"

    build_dir="build_${variant#aqs_}"
    check_bsec "variants/$variant.conf"

    args=(-DEXTRA_DTC_OVERLAY_FILE="$overlay")
    [ -f "variants/$variant.conf" ] && args+=(-DEXTRA_CONF_FILE="variants/$variant.conf")

    echo
    echo "=== $variant -> $build_dir ($BOARD)"

    log="build_logs/$variant.log"
    pristine_args=()
    [ -n "$PRISTINE" ] && pristine_args=("$PRISTINE")

    set +e
    taskset -c "$CPUS" west build -b "$BOARD" -d "$build_dir" --no-sysbuild \
        "${pristine_args[@]}" -- "${args[@]}" 2>&1 \
        | tee "$log" | grep -E "error:|Error|warning:|FLASH:|RAM:|IDT_LIST:"
    status=${PIPESTATUS[0]}
    set -e
    [ "$status" -eq 0 ] || die "$variant failed to build, full log in $log"

    uf2="$build_dir/zephyr/zephyr.uf2"
    [ -f "$uf2" ] || die "$variant did not produce $uf2, see $log"

    cp "$uf2" "bin/$variant.uf2"
    echo "wrote bin/$variant.uf2"
    summary+=("$variant: $(grep -m1 'FLASH:' "$log" | tr -s ' ' || echo 'size unknown')")
done

echo
echo "=== built ${#variants[@]} variant(s)"
for line in "${summary[@]}"; do echo "  $line"; done
