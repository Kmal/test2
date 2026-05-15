# StickS3 speaker hardware notes

The StickS3 includes an ES8311 codec, MEMS microphone, AW8737 speaker amplifier, and M5PM1 speaker-control function (`PYG3_SPK_Pulse`). The implemented audio-output scope is intentionally narrow: the Kconfig-gated `speaker_tone` action uses a bounded 16 kHz square-tone generator on the playback-only ES8311/I2S path and schematic net `G14_I2S_DDAC`, configures M5PM1 GPIO3 as a normal push-pull output, drives it high only while tone PCM is being written, then drives it low and releases playback resources. This is not a general M5Unified speaker playback API; broader sample rates, streaming, gain modes, and arbitrary PCM playback remain out of scope. The optional `board_speaker_amp_pulse()` gain/pulse helper remains fail-closed because no feature currently needs AW8737 pulse-mode gain control.

The official StickS3 microphone example states that the microphone and speaker cannot be used at the same time. Default automation firmware therefore stops demand-driven microphone capture before a speaker action, starts `BOARD_AUDIO_PROFILE_PLAYBACK_ONLY`, and resynchronizes sound-level demand after playback so `BOARD_AUDIO_PROFILE_CAPTURE_ONLY` can restart if rules or Web UI telemetry still need it. Experimental USB Audio Class support adds a separate `BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER` owner for opt-in UAC experiments only; it is not enabled by the default automation image and still requires real StickS3 hardware validation before any simultaneous mic+speaker product claim. The official product page also recommends speaker volume below 75% when running from battery; both rule validation, `action_speaker` validation, and UAC speaker helpers cap speaker volume at `74` so integer values stay below 75%.

## Speaker-action conformance review

| Official source/spec point | Firmware check result |
| --- | --- |
| Product documentation lists ES8311 audio codec, MEMS microphone, AW8737 power amplifier, and an 8Ω/1W speaker. | Firmware keeps ES8311 codec setup in `es8311.c`, M5PM1/PYG3 amplifier control in `board_audio_power.c`, and the bounded 16 kHz square-tone action in `action_speaker.c`; it does not expose generic speaker streaming/playback. |
| StickS3 schematic names GPIO14 as `G14_I2S_DDAC` and GPIO16 as `G16_I2S_DADC`; M5Unified uses GPIO14 for speaker data out and GPIO16 for microphone data in. | `BOARD_I2S_DO_IO=14` is used only by playback profiles and `BOARD_I2S_DI_IO=16` is used only by capture profiles. |
| M5Unified initializes StickS3 PYG3 as a normal GPIO output and toggles M5PM1 GPIO output bit 3 for speaker enable/disable. | `board_speaker_amp_set()` writes GPIO3 function, output mode, push-pull drive, and output high/low with read-modify-write preservation of unrelated PMIC bits. |
| Official microphone example says mic and speaker cannot be used simultaneously. | Default automation speaker actions still stop the sound-level service before playback and call `app_sound_level_sync()` afterwards; the separate UAC simultaneous mic+speaker profile is experimental and hardware-validation-gated. |
| Product speaker-volume notice recommends keeping battery speaker volume below 75%. | Rule and action validation reject volume `0` and values above `74`; default web-import value is `50`. |
| Product IR note says IR reception requires speaker amplifier off. | IR receive remains unimplemented; the speaker action always disables PYG3 after success or failed writes. |

## Sources

- M5Stack StickS3 product page for ES8311, AW8737 speaker amplifier, 8Ω/1W speaker, speaker-volume notice, and IR/speaker interaction note: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 speaker Arduino API page for the official speaker feature: https://docs.m5stack.com/en/arduino/m5sticks3/speaker
- M5Unified source repository for StickS3 audio ownership and speaker behavior references: https://github.com/m5stack/M5Unified
- Firmware speaker action implementation: ../../../src/actions/action_speaker.c
- Firmware speaker amplifier control implementation: ../../../src/audio/board_audio_power.c
- Firmware rule volume cap definition: ../../../src/rules/rule_types.h
- Firmware USB Audio Class speaker helper implementation: ../../../src/usb_audio/uac_speaker_sink.c
