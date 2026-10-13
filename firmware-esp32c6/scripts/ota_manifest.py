"""Write the python-matter-server OTA manifest for one built variant.

The Matter server's OTA provider picks up every *.json in its --ota-provider-dir
and serves the image its otaUrl names. A file:// URL is resolved relative to
that directory, so the manifest and the image are copied there side by side.
"""

import argparse
import base64
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path


def read_sdkconfig(path):
    """Return the sdkconfig of a build as a dict of option name to raw value."""
    options = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^(CONFIG_\w+)=(.*)$", line)
        if match:
            options[match.group(1)] = match.group(2).strip('"')
    return options


def read_version(cmakelists):
    """Return (version number, version string) from the project CMakeLists.txt."""
    text = cmakelists.read_text()
    number = re.search(r'set\(PROJECT_VER_NUMBER\s+(\d+)\)', text)
    string = re.search(r'set\(PROJECT_VER\s+"([^"]+)"\)', text)
    if not number or not string:
        raise ValueError(f"no PROJECT_VER/PROJECT_VER_NUMBER in {cmakelists}")
    return int(number.group(1)), string.group(1)


def main():
    """Copy the variant's OTA image to a versioned name and write its manifest."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("variant", help="Variant name, e.g. aqs_001")
    parser.add_argument("build_dir", type=Path, help="Build directory of the variant")
    parser.add_argument("out_dir", type=Path, help="Output directory of the variant")
    args = parser.parse_args()

    firmware_dir = Path(__file__).resolve().parent.parent
    options = read_sdkconfig(args.build_dir / "sdkconfig")
    version, version_string = read_version(firmware_dir / "CMakeLists.txt")

    image_name = f"{args.variant}-{version_string}.ota"
    image = args.out_dir / image_name
    shutil.copyfile(args.build_dir / "air_quality_sensor-ota.bin", image)
    data = image.read_bytes()

    manifest = {
        "modelVersion": {
            "vid": int(options["CONFIG_DEVICE_VENDOR_ID"], 16),
            "pid": int(options["CONFIG_DEVICE_PRODUCT_ID"], 16),
            "softwareVersion": version,
            "softwareVersionString": version_string,
            "cdVersionNumber": 1,
            "softwareVersionValid": True,
            "otaUrl": f"file:///{image_name}",
            "otaFileSize": len(data),
            "otaChecksum": base64.b64encode(hashlib.sha256(data).digest()).decode("ascii"),
            "otaChecksumType": 1,
            "minApplicableSoftwareVersion": 0,
            "maxApplicableSoftwareVersion": version - 1,
        }
    }
    (args.out_dir / f"{args.variant}-{version_string}.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
