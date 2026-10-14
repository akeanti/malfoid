#!/usr/bin/env python3
"""Stage the GitHub Pages site: copy docs/, merge each firmware build into a
single image at offset 0 and write the ESP Web Tools manifests.

    pio run -d firmware -e xiao -e core
    python tools/stage_site.py --out _site [--sha abc1234]

Requires esptool (pip install esptool).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARDS = {"xiao": "XIAO ESP32S3", "core": "Malfoid Core rev A"}


def find_boot_app0() -> Path:
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    hits = sorted(core.glob("packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin"))
    if not hits:
        sys.exit(f"boot_app0.bin not found under {core}/packages - build the firmware first")
    return hits[0]


def firmware_version() -> str:
    ini = (ROOT / "firmware" / "platformio.ini").read_text(encoding="utf-8")
    m = re.search(r'MALFOID_FW_VERSION=\\"([^\\"]+)\\"', ini)
    return m.group(1) if m else "dev"


def merge(env: str, out: Path, boot_app0: Path) -> bool:
    build = ROOT / "firmware" / ".pio" / "build" / env
    parts = [build / "bootloader.bin", build / "partitions.bin", build / "firmware.bin"]
    if not all(p.exists() for p in parts):
        print(f"skip {env}: no build in {build}")
        return False
    cmd = [
        sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin", "-o", str(out),
        "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep",
        "0x0", str(parts[0]), "0x8000", str(parts[1]), "0xe000", str(boot_app0), "0x10000", str(parts[2]),
    ]
    subprocess.run(cmd, check=True)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", default="_site", type=Path)
    parser.add_argument("--sha", default="", help="short commit hash appended to the version")
    args = parser.parse_args()

    out: Path = args.out if args.out.is_absolute() else ROOT / args.out
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(ROOT / "docs", out)

    version = firmware_version() + (f"+{args.sha}" if args.sha else "")
    boot_app0 = find_boot_app0()
    for env, name in BOARDS.items():
        image = f"malfoid-{env}.bin"
        if not merge(env, out / "firmware" / image, boot_app0):
            continue
        manifest = {
            "name": f"Malfoid ({name})",
            "version": version,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32-S3", "parts": [{"path": image, "offset": 0}]}],
        }
        (out / "firmware" / f"manifest-{env}.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"staged {image} ({version})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
