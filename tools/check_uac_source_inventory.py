#!/usr/bin/env python3
"""Validate source-backed USB Audio Class implementation inventory."""
from __future__ import annotations
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    "src/usb_audio/uac_config.c": ["UAC_AUDIO_MODE_MIC_ONLY", "UAC_AUDIO_MODE_SPEAKER_ONLY", "UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER"],
    "src/usb_audio/uac_audio_buffer.c": ["underruns", "overruns", "uac_audio_buffer_read_or_silence"],
    "src/usb_audio/uac_device_adapter.c": ["uac_device_adapter_input_cb", "uac_device_adapter_output_cb", "uac_audio_clamp_volume_percent"],
    "src/usb_audio/uac_esp_device.c": ["uac_device_config_t", "output_cb", "input_cb", "skip_tinyusb_init", "uac_device_init"],
    "src/usb_audio/uac_mic_source.c": ["BOARD_AUDIO_PROFILE_CAPTURE_ONLY", "board_i2s_read_mono_i16"],
    "src/usb_audio/uac_speaker_sink.c": ["BOARD_AUDIO_PROFILE_PLAYBACK_ONLY", "require_audio_power_enable", "uac_audio_clamp_volume_percent"],
    "src/usb_audio/uac_service.c": ["uac_service_start_from_kconfig", "xTaskCreate", "uac_esp_device_start", "CONFIG_APP_USB_UAC_MIC", "BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER", "combined UAC descriptors are enabled without audio bridge tasks", "uac_service_release_buffers"],
    "src/app/main.c": ["uac_service_start_from_kconfig"],
    "src/audio/board_audio.h": ["BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER"],
    "src/idf_component.yml": ["espressif/usb_device_uac", "1.2.3", "$CONFIG{APP_USB_UAC_DEVICE} == True"],
}

def main() -> int:
    errors: list[str] = []
    for rel, terms in REQUIRED.items():
        path = ROOT / rel
        if not path.exists():
            errors.append(f"missing {rel}")
            continue
        text = path.read_text(encoding="utf-8")
        for term in terms:
            if term not in text:
                errors.append(f"{rel} missing {term!r}")
    defaults = (ROOT / "config/sdkconfig.defaults").read_text(encoding="utf-8")
    if "CONFIG_APP_USB_UAC_DEVICE=n" not in defaults:
        errors.append("default config missing CONFIG_APP_USB_UAC_DEVICE=n")
    kconfig = (ROOT / "src/Kconfig.projbuild").read_text(encoding="utf-8")
    for symbol in ["config APP_USB_UAC_MIC", "config APP_USB_UAC_SPEAKER"]:
        if symbol not in kconfig:
            errors.append(f"Kconfig missing {symbol}")
    if errors:
        print("UAC source inventory validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("UAC source inventory validation passed")
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
