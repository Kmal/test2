#!/usr/bin/env python3
from __future__ import annotations

import json
import struct
import tempfile
from argparse import Namespace
from pathlib import Path

import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import make_factory_image as factory  # noqa: E402


def esp_image(segments: list[tuple[int, bytes]]) -> bytes:
    data = bytearray(24)
    data[0] = factory.ESP_IMAGE_MAGIC
    data[1] = len(segments)
    for load_addr, payload in segments:
        data.extend(struct.pack("<II", load_addr, len(payload)))
        data.extend(payload)
        while len(data) % 4:
            data.append(0)
    return bytes(data)


def write_plan(build_dir: Path, flash_files: dict[str, str], **roles: dict[str, str]) -> None:
    payload = {
        "flash_settings": {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "8MB"},
        "flash_files": flash_files,
    }
    payload.update(roles)
    (build_dir / "flasher_args.json").write_text(json.dumps(payload), encoding="utf-8")


def expect_value_error(fn, needle: str) -> None:
    try:
        fn()
    except ValueError as exc:
        assert needle in str(exc), str(exc)
    else:
        raise AssertionError(f"expected ValueError containing {needle!r}")


def test_json_plan_rejects_app_at_zero() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build = Path(tmp)
        (build / "app.bin").write_bytes(esp_image([(0x3FCE2820, b"app")]))
        write_plan(
            build,
            {"0x0": "app.bin"},
            app={"offset": "0x0", "file": "app.bin"},
        )
        plan = factory.load_flash_plan(build)
        expect_value_error(
            lambda: factory.validate_flash_plan(plan, "esp32s3", build / "factory.bin"),
            "application image at 0x0",
        )


def test_legacy_flash_args_rejects_unidentified_zero_image() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build = Path(tmp)
        (build / "m5sticks3_bluetooth_mic_app.bin").write_bytes(esp_image([(0x3FCE2820, b"app")]))
        (build / "flash_args").write_text("0x0 m5sticks3_bluetooth_mic_app.bin\n", encoding="utf-8")
        plan = factory.load_flash_plan(build)
        expect_value_error(
            lambda: factory.validate_flash_plan(plan, "esp32s3", build / "factory.bin"),
            "application image at 0x0",
        )


def test_corrupt_boot_segment_table_is_rejected() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build = Path(tmp)
        corrupt = bytearray(esp_image([(0x3FCE2820, b"boot")]))
        corrupt[1] = 2
        corrupt.extend(struct.pack("<II", 0x6C252820, 0x25202975))
        (build / "bootloader.bin").write_bytes(corrupt)
        write_plan(
            build,
            {"0x0": "bootloader.bin"},
            bootloader={"offset": "0x0", "file": "bootloader.bin"},
        )
        plan = factory.load_flash_plan(build)
        expect_value_error(
            lambda: factory.validate_flash_plan(plan, "esp32s3", build / "factory.bin"),
            "implausible length 0x25202975",
        )


def test_esptool_command_uses_json_settings_and_sorted_entries() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build = Path(tmp)
        (build / "bootloader.bin").write_bytes(esp_image([(0x3FCE2820, b"boot")]))
        (build / "app.bin").write_bytes(esp_image([(0x3C000020, b"app")]))
        (build / "partition.bin").write_bytes(b"partition-data")
        write_plan(
            build,
            {"0x10000": "app.bin", "0x0": "bootloader.bin", "0x8000": "partition.bin"},
            bootloader={"offset": "0x0", "file": "bootloader.bin"},
            app={"offset": "0x10000", "file": "app.bin"},
            **{"partition-table": {"offset": "0x8000", "file": "partition.bin"}},
        )
        plan = factory.load_flash_plan(build)
        factory.validate_flash_plan(plan, "esp32s3", build / "factory.bin")
        cmd = factory.esptool_command(
            Namespace(esptool="esptool.py", chip="esp32s3"), plan, build / "factory.bin"
        )
        assert cmd[:6] == ["esptool.py", "--chip", "esp32s3", "merge-bin", "-o", str(build / "factory.bin")]
        assert "--flash-mode" in cmd and "dio" in cmd
        assert cmd[-6:] == ["0x0", "bootloader.bin", "0x8000", "partition.bin", "0x10000", "app.bin"]


if __name__ == "__main__":
    test_json_plan_rejects_app_at_zero()
    test_legacy_flash_args_rejects_unidentified_zero_image()
    test_corrupt_boot_segment_table_is_rejected()
    test_esptool_command_uses_json_settings_and_sorted_entries()
    print("make_factory_image tests passed")
