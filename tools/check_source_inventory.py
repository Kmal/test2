#!/usr/bin/env python3
"""Validate that every main C implementation file has an explicit build status."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN_DIR = ROOT / "main"
CMAKE = MAIN_DIR / "CMakeLists.txt"
HOST_RUNNER = ROOT / "tests" / "host" / "run_host_tests.sh"
SDKCONFIG_DEFAULTS = ROOT / "config" / "sdkconfig.defaults"
DOCS = ROOT / "docs" / "README.md"
INVENTORY_DOC = ROOT / "docs" / "implementation_inventory.md"

# Source files that intentionally remain outside unconditional APP_SRCS.
# Sound-level files are conditionally linked by CONFIG_APP_SOUND_LEVEL_TRIGGERS,
# which is enabled in the checked-in StickS3 defaults; helper audio sources stay
# out of the default app component.
SOUND_LEVEL_SRCS = {
    "audio_metrics.c",
    "board_audio.c",
    "board_audio_clock.c",
    "board_audio_power.c",
    "board_i2s.c",
    "es8311.c",
    "sound_level_service.c",
}
HELPER_AUDIO_SRCS = {
    "audio_pipeline.c",
    "audio_resample.c",
}
CONDITIONAL_TRANSPORT_SRCS = {
    "transport_ble_gatt.c",
    "transport_hfp_legacy.c",
}
EXPECTED_NON_DEFAULT_SRCS = SOUND_LEVEL_SRCS | HELPER_AUDIO_SRCS | CONDITIONAL_TRANSPORT_SRCS


def quoted_sources(text: str) -> set[str]:
    return {Path(match).name for match in re.findall(r'"([^"\n]+\.c)"', text)}


def cmake_default_sources(text: str) -> set[str]:
    match = re.search(r"set\(APP_SRCS(?P<body>.*?)\)\s*set\(", text, flags=re.S)
    if not match:
        return set()
    return quoted_sources(match.group("body"))


def inventory_rows(text: str) -> dict[str, tuple[str, str]]:
    rows: dict[str, tuple[str, str]] = {}
    for line in text.splitlines():
        if not line.startswith("| `"):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 4:
            continue
        source = cells[0].strip("`")
        rows[source] = (cells[1], cells[2])
    return rows


def main() -> int:
    errors: list[str] = []
    all_main = {path.name for path in MAIN_DIR.glob("*.c")}
    cmake_text = CMAKE.read_text(encoding="utf-8")
    default_sources = cmake_default_sources(cmake_text)
    conditional_sources = quoted_sources(cmake_text) - default_sources
    host_sources = quoted_sources(HOST_RUNNER.read_text(encoding="utf-8")) & all_main
    sdkconfig_defaults = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    sound_enabled_by_default = "CONFIG_APP_SOUND_LEVEL_TRIGGERS=y" in sdkconfig_defaults
    docs_text = DOCS.read_text(encoding="utf-8")
    inventory = inventory_rows(INVENTORY_DOC.read_text(encoding="utf-8"))

    missing_doc_rows = all_main - set(inventory)
    if missing_doc_rows:
        errors.append(f"docs/implementation_inventory.md missing source rows: {sorted(missing_doc_rows)}")
    extra_doc_rows = set(inventory) - all_main
    if extra_doc_rows:
        errors.append(f"docs/implementation_inventory.md references unknown sources: {sorted(extra_doc_rows)}")

    for source in sorted(all_main & set(inventory)):
        if source in default_sources:
            expected_status = "default"
        elif source in SOUND_LEVEL_SRCS and source in conditional_sources and sound_enabled_by_default:
            expected_status = "default via sound config"
        elif source in conditional_sources:
            expected_status = "conditional"
        else:
            expected_status = "helper-only"
        expected_host = "yes" if source in host_sources else "no"
        actual_status, actual_host = inventory[source]
        if actual_status != expected_status:
            errors.append(
                f"docs/implementation_inventory.md status mismatch for {source}: expected {expected_status}, found {actual_status}"
            )
        if actual_host != expected_host:
            errors.append(
                f"docs/implementation_inventory.md host coverage mismatch for {source}: expected {expected_host}, found {actual_host}"
            )

    unknown_default = default_sources - all_main
    if unknown_default:
        errors.append(f"main/CMakeLists.txt references missing sources: {sorted(unknown_default)}")

    missing_from_inventory = all_main - default_sources - conditional_sources - EXPECTED_NON_DEFAULT_SRCS
    if missing_from_inventory:
        errors.append(f"main sources lack explicit build status: {sorted(missing_from_inventory)}")

    unexpected_non_default = (all_main - default_sources - conditional_sources) - EXPECTED_NON_DEFAULT_SRCS
    if unexpected_non_default:
        errors.append(f"unexpected non-default sources: {sorted(unexpected_non_default)}")

    helper_audio_linked_by_default = HELPER_AUDIO_SRCS & default_sources
    if helper_audio_linked_by_default:
        errors.append(f"helper-only audio sources linked by default: {sorted(helper_audio_linked_by_default)}")

    missing_sound_conditionals = SOUND_LEVEL_SRCS - conditional_sources
    if missing_sound_conditionals:
        errors.append(f"sound-level sources not represented in CMake conditionals: {sorted(missing_sound_conditionals)}")

    missing_conditionals = CONDITIONAL_TRANSPORT_SRCS - conditional_sources
    if missing_conditionals:
        errors.append(f"conditional transport sources not represented in CMake conditionals: {sorted(missing_conditionals)}")

    # Host checks are the executable evidence for helper-only C modules that can
    # run off-device. Keep this list explicit so a helper source cannot be added
    # without either host coverage or a conscious inventory update.
    host_expected = (SOUND_LEVEL_SRCS | HELPER_AUDIO_SRCS) - {"board_audio_power.c"}
    missing_host = host_expected - host_sources
    if missing_host:
        errors.append(f"optional helper sources missing host-test compilation: {sorted(missing_host)}")

    inventory_text = INVENTORY_DOC.read_text(encoding="utf-8")
    for phrase in ["Runtime capture starts only while at least one enabled automation rule uses a `sound.*` trigger", "Maintainers can still disable the feature through Kconfig"]:
        if phrase not in docs_text:
            errors.append(f"docs/README.md missing implementation-status phrase: {phrase!r}")
    if "one row per C implementation file" not in inventory_text:
        errors.append("docs/implementation_inventory.md must state that every C implementation file is inventoried")

    if errors:
        print("Source-inventory validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        "Source-inventory validation passed: "
        f"{len(default_sources)} default, {len(conditional_sources)} conditional, "
        f"{len(EXPECTED_NON_DEFAULT_SRCS - conditional_sources)} helper-only sources classified"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
