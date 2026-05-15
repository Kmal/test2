#!/usr/bin/env python3
"""Validate the StickS3 partition table against board flash and ESP-IDF alignment rules."""

from __future__ import annotations

import csv
import json
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions.csv"
MANIFEST = ROOT / "docs" / "hardware" / "sticks3" / "sticks3.board.json"

APP_OFFSET_ALIGNMENT = 0x10000
APP_SIZE_ALIGNMENT = 0x1000
MIN_NVS_SIZE = 0x3000
EXPECTED_FACTORY_OFFSET = 0x10000


@dataclass(frozen=True)
class Partition:
    name: str
    partition_type: str
    subtype: str
    offset: int
    size: int

    @property
    def end(self) -> int:
        return self.offset + self.size


def parse_int(value: str) -> int:
    text = value.strip()
    if not text:
        raise ValueError("empty numeric field")
    if text.lower().endswith("k"):
        return int(text[:-1], 0) * 1024
    if text.lower().endswith("m"):
        return int(text[:-1], 0) * 1024 * 1024
    return int(text, 0)


def load_partitions() -> list[Partition]:
    parsed_partitions: list[Partition] = []
    data_lines = (
        line
        for line in PARTITIONS.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    )
    for fields in csv.reader(data_lines):
        if len(fields) < 5:
            raise ValueError(f"partition row has too few fields: {fields!r}")
        name, partition_type, subtype, offset, size = (field.strip() for field in fields[:5])
        parsed_partitions.append(
            Partition(
                name=name,
                partition_type=partition_type,
                subtype=subtype,
                offset=parse_int(offset),
                size=parse_int(size),
            )
        )
    return parsed_partitions


def main() -> int:
    errors: list[str] = []
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    flash_bytes = int(manifest.get("flash_bytes", 0))
    if flash_bytes != 8 * 1024 * 1024:
        errors.append("docs/hardware/sticks3/sticks3.board.json must record the official 8 MB StickS3 flash size")

    try:
        partitions = load_partitions()
    except ValueError as exc:
        print(f"Partition-table validation failed: {exc}", file=sys.stderr)
        return 1

    by_name = {partition.name: partition for partition in partitions}
    factory = by_name.get("factory")
    nvs = by_name.get("nvs")
    phy = by_name.get("phy_init")

    if factory is None:
        errors.append("partitions.csv must define a factory app partition")
    else:
        if factory.partition_type != "app" or factory.subtype != "factory":
            errors.append("factory partition must have type app and subtype factory")
        if factory.offset != EXPECTED_FACTORY_OFFSET:
            errors.append(f"factory app offset must stay at 0x{EXPECTED_FACTORY_OFFSET:x}")
        if factory.offset % APP_OFFSET_ALIGNMENT != 0:
            errors.append("factory app offset must be aligned to 0x10000 for ESP-IDF app partitions")
        if factory.size % APP_SIZE_ALIGNMENT != 0:
            errors.append("factory app size must be aligned to the 4 KB flash sector size")
        if flash_bytes and factory.end != flash_bytes:
            errors.append(
                f"factory app should end exactly at 8 MB flash boundary: got 0x{factory.end:x}, expected 0x{flash_bytes:x}"
            )

    if nvs is None:
        errors.append("partitions.csv must define an NVS partition")
    elif nvs.size < MIN_NVS_SIZE:
        errors.append("NVS partition must be at least 0x3000 bytes per ESP-IDF guidance")

    if phy is None:
        errors.append("partitions.csv must define a PHY init partition")

    ordered = sorted(partitions, key=lambda partition: partition.offset)
    for previous, current in zip(ordered, ordered[1:]):
        if previous.end > current.offset:
            errors.append(
                f"partitions overlap: {previous.name} ends at 0x{previous.end:x}, {current.name} starts at 0x{current.offset:x}"
            )
    if flash_bytes:
        for partition in partitions:
            if partition.end > flash_bytes:
                errors.append(f"{partition.name} partition ends beyond 8 MB flash: 0x{partition.end:x}")

    if errors:
        print("Partition-table validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Partition-table validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
