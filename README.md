# M5Stack Stick S3 Bluetooth Microphone Firmware

This project contains firmware for the [M5Stack Stick S3](https://shop.m5stack.com/products/sticks3) that turns the device into a **Bluetooth microphone**.  When flashed onto the Stick S3 the board advertises itself as a Hands‑Free Profile (HFP) headset; after pairing it with a Mac mini (or another audio gateway) the on‑board microphone can be used as an input source.  The firmware was developed using the Espressif ESP‑IDF build system and demonstrates how to combine the I2S and I2C peripherals with the classic Bluetooth stack.

## Hardware overview

According to CNX Software’s review of the Stick S3, the board is based on an **ESP32‑S3‑PICO‑1‑N8R8** module with 8 MB flash and 8 MB PSRAM【412301115246439†L40-L48】 and includes:

| component | details |
|---|---|
| Display | 1.14‑inch IPS screen (240×135 pixels) |
| Audio | ES8311 I2S codec with 24‑bit ADC/DAC; built‑in MEMS microphone (SNR ≈ 65 dB) and 8 Ω/1 W speaker with AW8737 amplifier【412301115246439†L40-L69】 |
| Wireless | Wi‑Fi 4 and **Bluetooth 5 LE + Mesh**【412301115246439†L40-L48】 |
| Battery | 250 mAh Li‑ion |
| Expansion | Grove connector and 16‑pin HAT 2 expansion header |

The ES8311 is a low‑power mono audio codec with a 24‑bit multi‑bit delta–sigma ADC/DAC that supports sampling rates from 8 kHz to 96 kHz【320084825420146†L32-L42】.  Control communication uses I²C while audio data is exchanged over I²S【705914580000106†L107-L117】.

## Implementation notes

* **I²S audio capture** – The firmware configures the ESP32‑S3’s I²S peripheral in full‑duplex mode (16 kHz, 16‑bit, mono).  Audio is read from the ES8311’s ADC and down‑sampled to 8 kHz on‑the‑fly before being handed to the Bluetooth stack via the HFP outgoing data callback.  Incoming audio from the remote device is written to the codec’s DAC so you can monitor the audio.
* **ES8311 driver** – A minimal driver (`es8311.c`) performs a soft reset, sets the sample rate and enables the ADC and DAC.  For advanced features (volume control, mic gain, etc.) you can use the `esp_codec_dev` component; its README explains how the codec is abstracted via `audio_codec_ctrl_if_t` and `audio_codec_data_if_t` interfaces and how to combine them into a `esp_codec_dev_handle_t`【705914580000106†L231-L298】.
* **Bluetooth configuration** – Only classic Bluetooth (BR/EDR) is enabled.  The firmware registers a Hands‑Free client, sets the device name to `M5StickS3‑Mic` and makes the device discoverable/connectable.  Voice‑over‑HCI is selected so that SCO audio flows through the application layer; this allows the microphone data to be provided by the outgoing data callback【60199267661565†L468-L517】.  The service level connection (SLC) is automatically negotiated when the host pairs with the device, and the audio connection is opened once the SLC is established.
* **Status UI** – `main/status_ui.c` and `main/status_ui.h` keep user-facing state indication and button polling separate from Bluetooth/audio logic.  The module currently reports the states `booting`, `discoverable`, `paired`, `HFP connected`, `audio streaming` and `error` through ESP-IDF logging, so the firmware can build without display hardware or display components.  The same API can later be backed by an LCD/OLED implementation without changing the HFP callbacks.

## Buttons and status output

The lightweight status module configures active-low GPIO inputs with internal pull-ups and dispatches button events back to `main.c` through callbacks.  The default assignments are:

| Function | Default GPIO | Firmware action |
| --- | ---: | --- |
| Clear pairing | GPIO37 | Removes all bonded Bluetooth devices, clears the cached peer address and returns the unit to discoverable mode. |
| Toggle monitoring output | GPIO39 | Enables/disables routing incoming HFP audio to the ES8311 DAC.  Microphone capture and Bluetooth streaming are unaffected. |
| Toggle discoverable mode | GPIO35 | Switches between connectable/discoverable and connectable-only scan modes. |

Override the button pins at compile time by defining `STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO`, `STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO` or `STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO` for the `main` component if your board wiring differs.

No additional component is required for the default log-based status output.  If you add an on-device display, use ESP-IDF's LCD/LVGL stack; the recommended managed component source is Espressif's component registry, for example `idf.py add-dependency "espressif/esp_lvgl_port^2"` for LVGL integration, plus the appropriate `esp_lcd` panel driver for the display hardware.  Keep that display-specific code behind the `status_ui` API so Bluetooth and audio code remain independent from optional UI dependencies.

## Building and flashing

1. **Install ESP‑IDF** – Follow the official [ESP‑IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for your platform.  Ensure you can run `idf.py` from a terminal and that the `IDF_PATH` environment variable is set.
2. **Clone this repository** – Copy the `bluetooth_mic` directory to your ESP‑IDF workspace.  The directory tree should look like:

   ```
   bluetooth_mic/
     ├── CMakeLists.txt
     ├── main/
     │   ├── CMakeLists.txt
     │   ├── main.c
     │   ├── status_ui.c
     │   ├── status_ui.h
     │   ├── es8311.c
     │   └── es8311.h
     ├── config/
     │   └── sdkconfig.defaults
     └── README.md
   ```

3. **Configure the project** – In the project root run:

   ```
   idf.py menuconfig
   ```

   * Navigate to **`Component config → Bluetooth → Classic Bluetooth`** and ensure **Hands‑Free Profile client** is enabled.  
   * Under **`Bluetooth controller → BR/EDR Sync (SCO/eSCO) default data path`**, select **HCI** to enable voice‑over‑HCI【60199267661565†L468-L517】.
   * Optionally adjust I²S DMA buffer sizes in **`Driver configurations → I²S configuration`**.

4. **Build and flash** – Connect the Stick S3 via USB‑C and run:

   ```
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```

   Replace `/dev/ttyUSB0` with the serial port of your device.  The monitor will display log messages tagged with `BT_MIC`.

5. **Pair with Mac mini** – On the Mac, open **System Settings → Bluetooth**, find `M5StickS3‑Mic` in the list of devices and click **Connect**.  After pairing, the device should appear in **System Settings → Sound → Input** as an audio input source.  Selecting it will route microphone audio from the Stick S3 into your Mac.  Because the HFP profile is limited to 8 kHz CVSD audio the quality is suitable for voice but not hi‑fidelity recording (Espressif’s HFP client API notes that CVSD is the default codec【750023267507510†L294-L303】).

## Limitations and further work

* **Audio quality** – Hands‑Free Profile is designed for voice calls.  The CVSD codec offers narrow‑band (8 kHz) audio; enabling the optional mSBC codec may improve quality but requires Wide Band Speech support on both sides【750023267507510†L378-L399】.
* **Power considerations** – CNX Software warns that when the Stick S3 is battery‑powered the speaker volume should be kept below 75 % to prevent unexpected reboots due to high current draw【412301115246439†L103-L107】.  This firmware does not currently monitor battery voltage.
* **Pairing & reconnect** – Automatic reconnection logic can be added by storing the last paired device address (captured in the GAP callback) and calling `esp_hf_client_connect()` during startup.

## References

* **M5Stack Stick S3 specification** – summarises the ESP32‑S3 controller, audio codec, microphone and speaker features【412301115246439†L40-L69】.
* **ES8311 component** – details the 24‑bit ADC/DAC capabilities of the codec【320084825420146†L32-L42】.
* **Espressif codec framework** – provides a generic way to integrate codec devices and includes examples for ES8311【705914580000106†L231-L298】.
* **HFP client API documentation** – explains how to register data callbacks and select the audio data path (PCM vs HCI)【60199267661565†L468-L517】.
