#!/usr/bin/env python3
"""Create a factory-style merged ESP-IDF flash image from build/flash_args.

The resulting image contains the bootloader, partition table, application, and
other entries that ESP-IDF would normally flash separately.  Run this after
``idf.py build`` from an activated ESP-IDF environment.
"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = ROOT / "build"
DEFAULT_OUTPUT_NAME = "m5sticks3_bluetooth_mic-factory.bin"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Merge ESP-IDF flash artifacts into a single factory-style image."
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help="ESP-IDF build directory containing flash_args (default: build)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output image path (default: <build-dir>/m5sticks3_bluetooth_mic-factory.bin)",
    )
    parser.add_argument(
        "--chip",
        default="esp32s3",
        help="esptool chip name to pass to merge_bin (default: esp32s3)",
    )
    parser.add_argument(
        "--esptool",
        default=None,
        help="Optional esptool executable. Defaults to 'python -m esptool'.",
    )
    return parser.parse_args()


def flash_args_file(build_dir: Path) -> Path:
    flash_args = build_dir / "flash_args"
    if not flash_args.exists():
        raise FileNotFoundError(
            f"{flash_args} does not exist; run 'idf.py build' before creating the factory image"
        )
    return flash_args


def validate_flash_entries(build_dir: Path, flash_args: Path) -> None:
    tokens = shlex.split(flash_args.read_text(encoding="utf-8"), comments=True)
    missing: list[Path] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token.startswith("0x"):
            if index + 1 >= len(tokens):
                raise ValueError(f"flash_args offset {token} is missing a binary path")
            candidate = build_dir / tokens[index + 1]
            if not candidate.exists():
                missing.append(candidate)
            index += 2
        else:
            index += 1
    if missing:
        formatted = "\n".join(f"- {path}" for path in missing)
        raise FileNotFoundError(f"flash_args references missing binary artifacts:\n{formatted}")


def esptool_command(args: argparse.Namespace, flash_args: Path, output: Path) -> list[str]:
    if args.esptool:
        command = [args.esptool]
    else:
        command = [sys.executable, "-m", "esptool"]
    command.extend(
        [
            "--chip",
            args.chip,
            "merge_bin",
            "-o",
            str(output),
            f"@{flash_args.name}",
        ]
    )
    return command


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    output = (args.output or (build_dir / DEFAULT_OUTPUT_NAME)).resolve()

    try:
        flash_args = flash_args_file(build_dir)
        validate_flash_entries(build_dir, flash_args)
        output.parent.mkdir(parents=True, exist_ok=True)
        command = esptool_command(args, flash_args, output)
        subprocess.run(command, cwd=build_dir, check=True)
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"error: esptool merge_bin failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    print(f"Factory flash image written to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
