#!/usr/bin/env python3
"""Create a merged ESP-IDF flash image from machine-readable build artifacts."""

from __future__ import annotations

import argparse
import json
import shlex
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = ROOT / "build"
DEFAULT_OUTPUT_NAME = "m5sticks3_bluetooth_mic.bin"
ESP_IMAGE_MAGIC = 0xE9
MAX_SEGMENTS = 16
MAX_SEGMENT_LENGTH = 16 * 1024 * 1024


@dataclass(frozen=True)
class FlashEntry:
    offset: int
    path: Path
    role: str | None = None


@dataclass(frozen=True)
class FlashPlan:
    source: Path
    settings: dict[str, str]
    entries: list[FlashEntry]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Merge ESP-IDF flash artifacts into a single factory-style image."
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help="ESP-IDF build directory containing flasher_args.json or flash_args (default: build)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output image path (default: <build-dir>/m5sticks3_bluetooth_mic.bin)",
    )
    parser.add_argument(
        "--chip",
        default="esp32s3",
        help="esptool chip name to pass to merge-bin (default: esp32s3)",
    )
    parser.add_argument(
        "--esptool",
        default=None,
        help="Optional esptool executable. Defaults to 'python -m esptool'.",
    )
    return parser.parse_args()


def normalize_offset(raw: str | int) -> int:
    if isinstance(raw, int):
        return raw
    return int(raw, 0)


def plan_file(build_dir: Path) -> Path:
    flasher_args = build_dir / "flasher_args.json"
    if flasher_args.exists():
        return flasher_args
    flash_args = build_dir / "flash_args"
    if flash_args.exists():
        return flash_args
    raise FileNotFoundError(
        f"{build_dir} does not contain flasher_args.json or flash_args; run 'idf.py build' first"
    )


def infer_role_from_path(path: str) -> str | None:
    parts = Path(path).parts
    name = Path(path).name.lower()
    if "bootloader" in parts or name.startswith("bootloader"):
        return "bootloader"
    if "partition_table" in parts or "partition-table" in name:
        return "partition-table"
    if name.endswith("_app.bin"):
        return "app"
    return None


def entry_from_json_role(data: dict[str, object], role: str) -> tuple[int, str] | None:
    raw = data.get(role)
    if not isinstance(raw, dict):
        return None
    offset = raw.get("offset")
    file_name = raw.get("file")
    if offset is None or not isinstance(file_name, str):
        return None
    return normalize_offset(offset), file_name


def load_json_plan(build_dir: Path, path: Path) -> FlashPlan:
    data = json.loads(path.read_text(encoding="utf-8"))
    raw_files = data.get("flash_files")
    entries_by_offset: dict[int, FlashEntry] = {}
    roles_by_file: dict[str, str] = {}
    roles_by_offset: dict[int, str] = {}

    for role in ("bootloader", "partition-table", "partition_table", "app"):
        role_entry = entry_from_json_role(data, role)
        if role_entry is not None:
            offset, file_name = role_entry
            roles_by_file[file_name] = role
            roles_by_offset[offset] = role

    if isinstance(raw_files, dict):
        iterable = raw_files.items()
    elif isinstance(raw_files, list):
        iterable = ((item.get("offset"), item.get("file")) for item in raw_files if isinstance(item, dict))
    else:
        raise ValueError(f"{path} does not contain a usable flash_files object")

    for raw_offset, raw_file in iterable:
        if raw_offset is None or not isinstance(raw_file, str):
            raise ValueError(f"{path} contains a flash_files entry without offset and file")
        offset = normalize_offset(raw_offset)
        role = roles_by_offset.get(offset) or roles_by_file.get(raw_file) or infer_role_from_path(raw_file)
        if offset in entries_by_offset:
            raise ValueError(f"duplicate flash offset 0x{offset:x} in {path}")
        entries_by_offset[offset] = FlashEntry(offset, build_dir / raw_file, role)

    settings: dict[str, str] = {}
    raw_settings = data.get("flash_settings")
    if isinstance(raw_settings, dict):
        for key, value in raw_settings.items():
            if isinstance(value, str):
                settings[key] = value

    return FlashPlan(path, settings, sorted(entries_by_offset.values(), key=lambda entry: entry.offset))


def load_flash_args_plan(build_dir: Path, path: Path) -> FlashPlan:
    tokens = shlex.split(path.read_text(encoding="utf-8"), comments=True)
    entries: list[FlashEntry] = []
    settings: dict[str, str] = {}
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token in {"--flash_mode", "--flash_freq", "--flash_size"}:
            if index + 1 >= len(tokens):
                raise ValueError(f"{path} option {token} is missing a value")
            settings[token.removeprefix("--")] = tokens[index + 1]
            index += 2
        elif token.startswith("0x"):
            if index + 1 >= len(tokens):
                raise ValueError(f"{path} offset {token} is missing a binary path")
            entries.append(
                FlashEntry(
                    normalize_offset(token),
                    build_dir / tokens[index + 1],
                    infer_role_from_path(tokens[index + 1]),
                )
            )
            index += 2
        else:
            index += 1

    offsets = [entry.offset for entry in entries]
    if len(offsets) != len(set(offsets)):
        raise ValueError(f"duplicate flash offset in {path}")
    return FlashPlan(path, settings, sorted(entries, key=lambda entry: entry.offset))


def load_flash_plan(build_dir: Path) -> FlashPlan:
    path = plan_file(build_dir)
    if path.name == "flasher_args.json":
        return load_json_plan(build_dir, path)
    return load_flash_args_plan(build_dir, path)


def validate_esp_image(path: Path, label: str) -> None:
    data = path.read_bytes()
    if len(data) < 24:
        raise ValueError(f"{label} image {path} is too small to be an ESP image")
    if data[0] != ESP_IMAGE_MAGIC:
        raise ValueError(f"{label} image {path} does not start with ESP image magic 0x{ESP_IMAGE_MAGIC:02x}")
    segment_count = data[1]
    if segment_count == 0 or segment_count > MAX_SEGMENTS:
        raise ValueError(f"{label} image {path} has invalid segment count {segment_count}")

    cursor = 24
    for segment in range(segment_count):
        if cursor + 8 > len(data):
            raise ValueError(f"{label} image {path} ends before segment {segment} header")
        load_addr, length = struct.unpack_from("<II", data, cursor)
        cursor += 8
        if length > MAX_SEGMENT_LENGTH:
            raise ValueError(
                f"{label} image {path} segment {segment} at 0x{load_addr:08x} has implausible length 0x{length:x}"
            )
        if cursor + length > len(data):
            raise ValueError(
                f"{label} image {path} segment {segment} at 0x{load_addr:08x} overruns file: "
                f"length 0x{length:x}, remaining 0x{len(data) - cursor:x}"
            )
        cursor += length
        cursor = (cursor + 3) & ~3


def validate_flash_plan(plan: FlashPlan, chip: str, output: Path) -> None:
    if not plan.entries:
        raise ValueError(f"{plan.source} does not contain any flash entries")

    missing = [entry.path for entry in plan.entries if not entry.path.exists()]
    if missing:
        formatted = "\n".join(f"- {path}" for path in missing)
        raise FileNotFoundError(f"{plan.source} references missing binary artifacts:\n{formatted}")

    output_resolved = output.resolve()
    for entry in plan.entries:
        if entry.path.resolve() == output_resolved:
            raise ValueError(f"output image {output} would overwrite input flash artifact {entry.path}")

    offset_zero = [entry for entry in plan.entries if entry.offset == 0]
    if chip == "esp32s3" and not offset_zero:
        raise ValueError(f"{plan.source} has no 0x0 boot image entry for ESP32-S3")
    if offset_zero:
        zero = offset_zero[0]
        if zero.role == "app":
            raise ValueError(f"{plan.source} places the application image at 0x0; use the bootloader at 0x0")
        if chip == "esp32s3" and zero.role not in {"bootloader", None}:
            raise ValueError(f"{plan.source} places {zero.role} data at 0x0; use the bootloader at 0x0")
        if chip == "esp32s3" and zero.role is None and "bootloader" not in zero.path.parts:
            raise ValueError(f"{plan.source} 0x0 entry {zero.path} is not identifiable as a bootloader")
        validate_esp_image(zero.path, zero.role or "0x0 boot")

    for entry in plan.entries:
        if entry.role in {"bootloader", "app"}:
            validate_esp_image(entry.path, entry.role)


def esptool_command(args: argparse.Namespace, plan: FlashPlan, output: Path) -> list[str]:
    if args.esptool:
        command = [args.esptool]
    else:
        command = [sys.executable, "-m", "esptool"]
    command.extend(["--chip", args.chip, "merge-bin", "-o", str(output)])

    setting_to_arg = {
        "flash_mode": "--flash-mode",
        "flash_freq": "--flash-freq",
        "flash_size": "--flash-size",
    }
    for setting, option in setting_to_arg.items():
        value = plan.settings.get(setting)
        if value:
            command.extend([option, value])

    for entry in plan.entries:
        command.extend([f"0x{entry.offset:x}", str(entry.path.relative_to(plan.source.parent))])
    return command


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    output = (args.output or (build_dir / DEFAULT_OUTPUT_NAME)).resolve()

    try:
        plan = load_flash_plan(build_dir)
        validate_flash_plan(plan, args.chip, output)
        output.parent.mkdir(parents=True, exist_ok=True)
        command = esptool_command(args, plan, output)
        subprocess.run(command, cwd=build_dir, check=True)
        validate_esp_image(output, "merged factory")
    except (FileNotFoundError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"error: esptool merge-bin failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    print(f"Factory flash image written to {output}")
    print("Flash this merged image at offset 0x0, for example:")
    print(f"  esptool.py --chip {args.chip} --port <PORT> erase_flash")
    print(f"  esptool.py --chip {args.chip} --port <PORT> write_flash 0x0 {output}")
    print("Do not flash the ESP-IDF application-only *_app.bin artifact at 0x0.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
