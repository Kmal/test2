#!/usr/bin/env python3
"""Validate that StickS3 defaults select the BLE rule-event transport only."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDKCONFIG_DEFAULTS = ROOT / "config" / "sdkconfig.defaults"
KCONFIG = ROOT / "src" / "Kconfig.projbuild"

FORBIDDEN_ENABLED = {
    "CONFIG_APP_TRANSPORT_WIFI_UDP_PCM=y": "the default transport must be Bluetooth, not Wi-Fi",
    "CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y": "ESP32-S3 cannot use BR/EDR-only Bluetooth",
    "CONFIG_BT_" + "HF" + "P_CLIENT_ENABLE=y": "unsupported Bluetooth audio profiles are not StickS3 defaults",
    "CONFIG_BT_SCO_DATA_PATH_HCI=y": "SCO-over-HCI is only relevant to unsupported Bluetooth audio",
}


def main() -> int:
    errors: list[str] = []
    defaults = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    for needle, reason in FORBIDDEN_ENABLED.items():
        if needle in defaults:
            errors.append(f"{needle} must not be enabled in StickS3 defaults: {reason}")

    kconfig = KCONFIG.read_text(encoding="utf-8") if KCONFIG.exists() else ""
    if "APP_TRANSPORT_NONE" not in kconfig:
        errors.append("src/Kconfig.projbuild must define APP_TRANSPORT_NONE for explicit board-only fallback")
    if "APP_TRANSPORT_BLE_GATT_RULE_EVENTS" not in kconfig:
        errors.append("src/Kconfig.projbuild must define APP_TRANSPORT_BLE_GATT_RULE_EVENTS for the default StickS3 transport")
    if "APP_USB_UAC_DEVICE" not in kconfig:
        errors.append("src/Kconfig.projbuild must define APP_USB_UAC_DEVICE for the explicit opt-in USB Audio Class profile")
    for symbol in ["APP_USB_UAC_MIC", "APP_USB_UAC_SPEAKER"]:
        if symbol not in kconfig:
            errors.append(f"src/Kconfig.projbuild must define {symbol} for opt-in USB Audio Class directions")
    if "CONFIG_APP_TRANSPORT_BLE_GATT_RULE_EVENTS=y" not in defaults:
        errors.append("config/sdkconfig.defaults must select BLE GATT rule events as the functional StickS3 transport")

    required_default_facts = {
        "CONFIG_APP_BATTERY_FACTS=y": "battery-percent automation facts must compile into the default firmware",
        "CONFIG_APP_USB_POWER_FACTS=y": "USB/external-power automation facts must compile into the default firmware",
        "CONFIG_APP_BMI270_FACTS=y": "BMI270 motion automation facts must compile into the default firmware",
        "CONFIG_APP_ADC_FACTS=y": "safe ADC voltage automation facts must compile into the default firmware",
        "CONFIG_APP_USB_UAC_DEVICE=n": "USB Audio Class must not be enabled in the default automation firmware",
    }
    for needle, reason in required_default_facts.items():
        if needle not in defaults:
            errors.append(f"config/sdkconfig.defaults must enable {needle}: {reason}")
    forbidden_kconfig = ["APP_TRANSPORT_" + "HF" + "P" + "_LEGACY", "BT_" + "HF" + "P" + "_CLIENT_ENABLE", "BT_SCO_DATA_PATH_HCI"]
    for needle in forbidden_kconfig:
        if needle in kconfig:
            errors.append(f"src/Kconfig.projbuild must not keep removed unsupported Bluetooth audio config {needle}")

    if errors:
        print("Transport-config validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Transport-config validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
