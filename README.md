# M5Stack Stick S3 Bluetooth Microphone Firmware

This project contains firmware for the [M5Stack Stick S3](https://shop.m5stack.com/products/sticks3) that turns the device into a **Bluetooth microphone**. When flashed onto the Stick S3, the board advertises itself as a Hands-Free Profile (HFP) headset; after pairing it with a Mac mini or another audio gateway, the on-board microphone can be used as an input source. The firmware uses the Espressif ESP-IDF build system and demonstrates how to combine the I2S and I2C peripherals with the classic Bluetooth stack.

## Hardware overview

According to CNX Software's Stick S3 review, the board is based on an **ESP32-S3-PICO-1-N8R8** module with 8 MB flash and 8 MB PSRAM and includes:

| component | details |
|---|---|
| Display | 1.14-inch IPS screen (240 x 135 pixels) |
| Audio | ES8311 I2S codec with 24-bit ADC/DAC; built-in MEMS microphone (SNR about 65 dB) and 8 ohm/1 W speaker with AW8737 amplifier |
| Wireless | Wi-Fi 4 and Bluetooth 5 LE + Mesh |
| Battery | 250 mAh Li-ion |
| Expansion | Grove connector and 16-pin HAT 2 expansion header |

The ES8311 is a low-power mono audio codec with a 24-bit multi-bit delta-sigma ADC/DAC that supports sampling rates from 8 kHz to 96 kHz. Control communication uses I2C while audio data is exchanged over I2S.

## Implementation notes

* **I2S audio capture** - The firmware configures the ESP32-S3 I2S peripheral in full-duplex mode at 16 kHz, 16-bit, mono. Audio is read from the ES8311 ADC and down-sampled to 8 kHz on the fly before being handed to the Bluetooth stack through the HFP outgoing data callback. Incoming audio from the remote device is written to the codec DAC so you can monitor the audio.
* **ES8311 driver** - A minimal driver in [`main/es8311.c`](main/es8311.c) performs a soft reset, sets the sample rate, and enables the ADC and DAC. For advanced features such as volume control, microphone gain, and channel mode, use Espressif's `esp_codec_dev` component.
* **Bluetooth configuration** - Only classic Bluetooth (BR/EDR) is enabled. The firmware registers a Hands-Free client, sets the device name to `M5StickS3-Mic`, and makes the device discoverable/connectable. Voice-over-HCI is selected so SCO audio flows through the application layer and the outgoing data callback can provide microphone data. The service level connection (SLC) is negotiated when the host pairs with the device, and the audio connection is opened once the SLC is established.

## Project tree

```text
.
├── .github/
│   └── workflows/
│       └── build.yml
├── CMakeLists.txt
├── README.md
├── config/
│   └── sdkconfig.defaults
└── main/
    ├── CMakeLists.txt
    ├── es8311.c
    ├── es8311.h
    └── main.c
```

## Building and flashing

1. **Install ESP-IDF** - Follow the official [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) for your platform. Ensure you can run `idf.py` from a terminal and that the `IDF_PATH` environment variable is set.
2. **Clone this repository** - Clone or copy this repository to your ESP-IDF workspace, then change into the repository root.
3. **Configure the project** - In the repository root, run:

   ```sh
   idf.py set-target esp32s3
   idf.py menuconfig
   ```

   * Navigate to **Component config -> Bluetooth -> Classic Bluetooth** and ensure **Hands-Free Profile client** is enabled.
   * Under **Bluetooth controller -> BR/EDR Sync (SCO/eSCO) default data path**, select **HCI** to enable voice-over-HCI.
   * Optionally adjust I2S DMA buffer sizes in **Driver configurations -> I2S configuration**.

   The checked-in [`config/sdkconfig.defaults`](config/sdkconfig.defaults) file supplies the intended default Bluetooth and I2S options for normal builds.

4. **Build and flash** - Connect the Stick S3 via USB-C and run:

   ```sh
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```

   Replace `/dev/ttyUSB0` with the serial port of your device. The monitor will display log messages tagged with `BT_MIC`.

5. **Pair with Mac mini** - On the Mac, open **System Settings -> Bluetooth**, find `M5StickS3-Mic` in the list of devices, and click **Connect**. After pairing, the device should appear in **System Settings -> Sound -> Input** as an audio input source. Selecting it routes microphone audio from the Stick S3 into your Mac. Because the HFP profile is limited to voice-oriented codecs such as CVSD by default, the quality is suitable for speech but not high-fidelity recording.

## Limitations and further work

* **Audio quality** - Hands-Free Profile is designed for voice calls. The CVSD codec offers narrow-band 8 kHz audio; enabling the optional mSBC codec may improve quality but requires Wide Band Speech support on both sides.
* **Power considerations** - CNX Software warns that when the Stick S3 is battery-powered, the speaker volume should be kept below 75% to prevent unexpected reboots due to high current draw. This firmware does not currently monitor battery voltage.
* **Pairing and reconnect** - Automatic reconnection logic can be added by storing the last paired device address captured in the GAP callback and calling `esp_hf_client_connect()` during startup.

## References

* [M5Stack Stick S3 product page](https://shop.m5stack.com/products/sticks3) - official product information for the target device.
* [CNX Software review of the M5Stack Stick S3](https://www.cnx-software.com/2024/11/08/m5stack-stick-s3-esp32-s3-iot-devkit-features-a-color-display-mems-microphone-and-speaker/) - hardware summary used for the module, audio, wireless, and battery notes.
* [ES8311 datasheet hosted by Espressif](https://dl.espressif.com/dl/schematics/Audio_ES8311.pdf) - codec capabilities, including ADC/DAC resolution, sample rates, and audio interface details.
* [Espressif `esp_codec_dev` component](https://components.espressif.com/components/espressif/esp_codec_dev) - reference for the higher-level codec abstraction to use when this minimal driver is not enough.
* [ESP-IDF HFP Client API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_hf_client.html) - callback, codec, and HFP behavior reference.
* [ESP-IDF build system guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html) - project layout and CMake reference for ESP-IDF applications.
