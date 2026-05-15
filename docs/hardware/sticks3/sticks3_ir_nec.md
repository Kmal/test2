# StickS3 IR NEC hardware notes

IR TX is GPIO46 and IR RX is GPIO42 in the StickS3 pin map. NEC IR actions use the RMT TX path on GPIO46 after rule validation. GPIO42 is reserved as documented IR RX, but no receive action/source is wired.

The StickS3 documentation notes that IR reception requires the speaker amplifier to be off. This firmware still implements only IR transmit actions, but the speaker action disables PYG3 after every tone and on playback-write failures so future IR RX work starts from an amplifier-off policy.

## Sources

- M5Stack StickS3 product page pin map for IR TX/RX GPIOs and IR reception notes: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 IR NEC Arduino API page for official NEC IR behavior: https://docs.m5stack.com/en/arduino/m5sticks3/ir_nec
- Firmware board constants for `BOARD_IR_TX_GPIO` and `BOARD_IR_RX_GPIO`: ../../../src/board/board_sticks3.h
- Firmware NEC IR transmit action implementation: ../../../src/actions/action_ir.c
- Firmware speaker amplifier disable path relevant to future IR RX policy: ../../../src/audio/board_audio_power.c
