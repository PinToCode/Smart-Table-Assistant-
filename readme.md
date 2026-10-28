# Smart Table Assistant 🕒🤖

> An interactive, motion-aware desktop companion and wellbeing workstation assistant powered by the Seeed Studio XIAO BLE (nRF52840 Sense) and a 1.3" SH1106 I2C OLED display.

[![Platform](https://img.shields.io/badge/Platform-Seeed%20XIAO%20nRF52840%20Sense-blue.svg)](https://www.seeedstudio.com/Seeed-XIAO-BLE-Sense-nRF52840-p-5253.html)
[![Display](https://img.shields.io/badge/Display-SH1106%20128x64%20OLED-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 🌟 Overview

The **Smart Table Assistant** is designed as a glanceable desk companion that monitors workstation activity via an onboard 6-axis IMU, provides multiple dynamic dashboards, and reacts to desk nudges and vibrations. It incorporates expressive companion animations (`RoboEyes`), non-blocking haptic cues, full Bluetooth Low Energy (BLE) telemetry/call alerts, scheduled wellbeing reminders, and an on-screen QR code generator for sharing your GitHub repository directly from the device.

---

## 📸 Key Features

- **Motion-Aware Display Modes:**
  - **IMU Wake Mode:** Powers off after 10 seconds of desk stillness; wakes instantly upon any desk tap, keystroke vibration, or nudge.
  - **Always-On Companion Mode:** Stays illuminated; enters animated, natural-blinking companion eyes (`RoboEyes`) after 3 seconds of stillness.
  - **Shake Trigger:** Detects desk bumps or jolts, triggering a confused companion eye animation with haptic feedback.
- **Multiple Watch Faces (1-Second Button Hold to Switch):**
  - **Face 1 (Telemetry Dashboard):** Live battery voltage (moving average), BLE connection state, 12-hour running clock, and battery gauge.
  - **Face 2 (Calendar & Climate):** Large desk clock centered above BLE-synced date banner and room temperature readout.
  - **Face 3 (Minimalist Digital):** Extra-large stacked digits, inverted AM/PM pill, vertical battery bar, and weather glyphs (Sun/Cloud/Rain).
- **Offline 12-Hour Clock Config:** Full on-device time setup (Hours, Minutes, AM/PM) with automatic 5-second step transitions and progress bars—no phone required.
- **On-Screen GitHub QR Code:** Dynamically generates a high-contrast, inverted QR code on the OLED screen with a dedicated quiet zone for phone cameras. The screen stays awake indefinitely until clicked.
- **BLE Telemetry & Alert Takeover:** Real-time push notification preview and continuous vibration incoming call notifications with `"CLR"` dismiss support.
- **Workstation Wellbeing Alerts:** Scheduled two-stage reminders at 07:00, 12:00, 16:00 (hydration check), and 22:00 (sleep prompt).

---

## 🛠️ Hardware Components

| Component | Specification / Notes |
| :--- | :--- |
| **Microcontroller** | Seeed Studio XIAO BLE Sense (Nordic nRF52840 + LSM6DS3 IMU) |
| **Display** | 1.3-inch I2C Monochrome OLED (SH1106 Driver, 128×64, Address `0x3C`) |
| **Vibration Motor** | Coreless disc haptic motor driven by NPN transistor / MOSFET on GPIO 10 |
| **Button** | 1× Tactile push button on GPIO 1 (Active-LOW, internal pull-up) |
| **Power Source** | 3.7V Lithium-Polymer cell connected to XIAO underside battery pads |

---

## 📌 Wiring Diagram

| XIAO BLE Sense Pin | Target Pin / Component | Description |
| :--- | :--- | :--- |
| **P0.04 / D2** | OLED `SDA` | I2C Data line |
| **P0.05 / D3** | OLED `SCL` | I2C Clock line |
| **D1** | Push Button (Terminal 1) | Navigation input (Terminal 2 to GND) |
| **D10** | Transistor Gate / Base | Non-blocking vibration motor drive |
| **D4** | OLED `VCC` | Display power rail (switched HIGH in code) |
| **D5** | OLED Aux / Enable | Supplementary power line |
| **GND** | OLED `GND`, Button `GND`, Motor `GND` | Common system ground |
| **BAT+ / BAT-** | LiPo Battery | Battery charging and onboard monitoring |

---

## 📦 Required Libraries

Install each of these libraries via the **Arduino Library Manager** (`Sketch` ➔ `Include Library` ➔ `Manage Libraries...`):

1. **`Adafruit_GFX`** (by Adafruit)
2. **`Adafruit_SH110X`** (by Adafruit)
3. **`ArduinoBLE`** (by Arduino)
4. **`LSM6DS3`** (Seeed Arduino LSM6DS3)
5. **`FluxGarage_RoboEyes`** (by FluxGarage)
6. **`QRCode`** (by **Richard Moore**) — *Required for the QR screen generator*

---

## 🕹️ Single-Button Control Map

All interactions are executed through a single tactile button on `D1`:

| Gesture | Context | Action / Feedback |
| :--- | :--- | :--- |
| **Short Click (<1.0s)** | Screen Asleep | Instantly wakes display (50 ms haptic pulse) |
| **Short Click (<1.0s)** | Home Screen | Opens the **SETTINGS** menu (100 ms pulse) |
| **Short Click (<1.0s)** | Inside Settings | Cycles cursor through the 6 menu options |
| **Short Click (<1.0s)** | Time Config Mode | Increments active value (Hours: 1–12, Mins: 00–59, AM/PM) |
| **Short Click (<1.0s)** | QR Code Screen | **Exits QR view immediately back to Home** |
| **Short Click (<1.0s)** | Active Alert / Eyes | Dismisses notification or returns eyes to clock |
| **Hold (≥ 1.0s)** | Home Screen | Opens **WATCH FACE** selection menu |
| **Hold (≥ 3.0s)** | Time Config Mode | Saves configured time to internal clock and exits (250 ms pulse) |

> **Auto-Select Bar:** Settings menus feature a 2-second visual countdown bar across the bottom ($Y=60$). Ceasing button presses confirms and selects the current option.

---

## 📡 Bluetooth Low Energy (BLE) Interface

The device advertises under the local name **`XIAO-Watch`**.

### 1. Assistant Service (`0x1805`)
- **`timeChar`** (`0x2A2B`, R/W): Write `"HH:MM:SS"` to set the internal clock.
- **`dateChar`** (`0x2A27`, R/W): Write formatted string (e.g., `"Wed 28 Oct"`) for Face 2.
- **`weatherChar`** (`0x2A24`, R/W): Write temperature/weather (e.g., `"24C"` or `"24, Clear"`).
- **`notifyChar`** (`0x2A46`, R/W): Push text notifications (up to 40 characters). Send `"CLR"` to dismiss.
- **`callChar`** (`0x2A47`, R/W): Send caller ID string for continuous 3s alert vibrations. Send `"CLR"` to hang up.

### 2. Battery Service (`0x180F`)
- **`batteryLevelChar`** (`0x2A19`, Read/Notify): Filtered battery charge percentage (`0–100%`).
- **`batteryVoltageChar`** (`0x2A1A`, Read/Notify): Real-time moving average cell voltage (`3.20V – 4.20V`).

---

## 🚀 Quick Start & Flashing

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (version 2.0+).
2. Add the Seeed nRF52 board manager package:
   - Go to **File ➔ Preferences ➔ Additional Boards Manager URLs**:
     ```text
     https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
     ```
   - Open **Boards Manager**, search for `Seeed nRF52`, and install.
3. Select board: **Seeed XIAO BLE - nRF52840 Sense**.
4. Clone this repository:
   ```bash
   git clone https://github.com/<your-username>/<your-repo-name>.git
   ```
5. Open the project sketch in Arduino IDE.
6. Configure your GitHub repository URL on **line 18**:
   ```cpp
   const char* GITHUB_REPO_URL = "https://github.com/<your-username>/<your-repo-name>";
   ```
7. Put the XIAO into bootloader mode (double-tap the tiny reset button near the USB-C port if needed) and click **Upload**.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE) — free to modify, adapt, and build upon.