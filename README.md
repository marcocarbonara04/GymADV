# 🏋️‍♂️ GymTracker Cardputer: Advanced VBT & Cardio Telemetry Tracker

Welcome to **GymTracker Cardputer**, a state-of-the-art custom firmware that transforms your **M5Stack Cardputer** into a professional workout computer, featuring biomechanical force analysis, real-time heart rate monitoring, and a premium interactive dashboard.

This system integrates real-time velocity analysis (**VBT - Velocity Based Training**), wireless **BLE (Bluetooth Low Energy)** heart rate scanning, a **Zero-RAM Web Dashboard**, and a highly resilient **Dual-Filesystem (SD + Flash)** storage architecture.

---

## 🚀 Key Features

### 1. 📊 Real-Time Biomechanics & VBT (Velocity Based Training)
* **100Hz IMU Precision Sampling:** Tracks repetitions, distance, and acceleration using the onboard accelerometer (specifically tuned for short-travel weight stacks of only a few centimeters).
* **Rep-by-Rep Peak Velocity Profile:** Records and stores the peak concentric velocity ($m/s$) of *every single repetition* in a set, creating a precise fatigue-management profile.
* **Adaptive Acoustic Feedback (Rep-Beep):** Instantly guides your form using high-fidelity tone beeps:
  * 🟢 **High Pitch:** Perfect form (perfect concentric-to-eccentric control ratio).
  * 🟡 **Medium Pitch:** Good form (stable repetition pacing).
  * 🔴 **Low Pitch:** Poor form (repetition completed too quickly or dropped too fast).

### 2. ⚡ Intelligent Power Saver (Backlight Dimmer)
* **Smart Backlight Dimmer:** During rest timers (`STATE_SUMMARY`), the screen auto-dims to 25% brightness (`40`) after 10 seconds of inactivity, extending battery life by up to **40%**. It instantly wakes to full brightness (`160`) at 5 seconds remaining (with an alarm beep) or as soon as any key is pressed.

### 3. 💾 Resilient Dual-Filesystem (SD Card + LittleFS Fallback)
* **25MHz SPI SD Card Protocol:** Uses optimal high-speed SPI on pins `SCK=40, MISO=39, MOSI=14, CS=12`.
* **Automatic Fallback:** Dynamically auto-detects and mounts the MicroSD card (FAT32) on boot. If a MicroSD card is present, heavy workout logs (`/workout_log.json`, `/workout_log_temp.json`) and **custom routines (`/routines.json`)** are read and saved directly from it. If the card is absent, it seamlessly falls back to the **internal SPI Flash (LittleFS)**.
* **Data Integrity:** Configuration and active workout progress files (`prs.json`, `weights.json`, `favs.bin`, `active_session.json`, and `wifi_config.json`) remain securely inside the internal Flash memory, ensuring your parameters are never lost!

### 4. 🫀 Wireless Cardio Telemetry (BLE Heart Rate)
* **MiBand & Polar Auto-Scanner:** Connects wirelessly to any standard BLE heart rate chest strap or smartwatch, providing active live telemetry.
* **Heart Rate Recovery (HRR) Analysis:** Computes average and peak heart rates, and tracks the exact rate of cardiovascular recovery during rest timers.

### 5. 🌐 Responsive Web Dashboard (Zero-RAM Streaming)
* **Flash-Streamed PROGMEM Web Page:** The 45KB interactive dashboard is served directly from flash memory (`send_P`), using virtually zero RAM. This prevents heap fragmentation and completely eliminates out-of-memory (OOM) reboots during Bluetooth/IMU activity.
* **VBT Hover Tooltips:** The workout diary displays average set speed (e.g. `Ø 0.58 m/s`) and shows an interactive **rep-by-rep tooltip** upon mouse hover (e.g., `Rep 1: 0.65 m/s, Rep 2: 0.61 m/s, Rep 3: 0.58 m/s`), allowing you to analyze fatigue curves at a glance.
* **Interactive Cardiac Graphs:** Visualizes your live heart rate curves during sets and recovery periods.
* **Instant CSV Export:** Download your entire workout history database as an Excel-compatible `.csv` file in one click.

### 6. ⌨️ Dynamic Custom Exercise Creator
* **On-the-Fly Database Expansion:** Press the `'N'` key on your Cardputer keyboard while browsing the Exercise Database (View 3) to trigger the typing modal.
* **Premium Overlay Text Entry:** A beautiful custom text entry box slides on-screen. Type any name up to 30 characters using the Cardputer keyboard.
* **Persistent Serialization:** Saved directly to permanent internal SPI Flash storage (`/custom_exercises.json`), surviving SD card removal.
* **Smart Auto-Focus Selector:** On pressing `ENTER`, the system automatically moves you to the new `Custom` muscle category, updates the active list, and focuses your new exercise so you can immediately start training or assign it to a custom routine.

---

## 🎹 Keyboard Quick Reference Guide

GymTracker supports 7 interactive views, navigable using the numeric keys `1` to `7`.

### 🧭 Navigation Views (Press `1` to `7`)
* `1` ➡️ **Workout View:** Active workout tracking, real-time IMU rep counters, peak repetition speed, active heart rate, progress bar, and training duration estimation.
* `2` ➡️ **Statistics:** Interactive bar chart showing cumulative set volume over your last 7 completed workouts.
* `3` ➡️ **Exercise Database:** Complete list of 100+ exercises sorted by muscle group. Press `SPC` to toggle an exercise as favorite.
* `4` ➡️ **History Logs:** Chronological archive of your last 10 completed workouts. View details or delete record using the `DEL` key.
* `5` ➡️ **Personal Records (PR):** Tracks and logs your estimated 1RM and historical personal records for every exercise.
* `6` ➡️ **Custom Routines:** Browse, select, and activate pre-configured custom workout routines.
* `7` ➡️ **Cardio (Live):** High-frequency heart rate scrolling graph with live min/max/average telemetry.

### 🛠️ Hotkey Commands
* `W` ➡️ **Wi-Fi Toggle:** Turns on/off the onboard access point (SSID: `GymTracker`, pass: `12345678`) or connects to saved home Wi-Fi.
* `B` ➡️ **BLE Scanner:** Triggers auto-scanning and connection for BLE heart rate monitors.
* `E` ➡️ **Edit Weight:** Allows keyboard input to instantly edit the active exercise weight.
* `N` ➡️ **New Custom Exercise:** In View 3, type custom exercise names with the keyboard.
* `SPC` (Space) ➡️ **Toggle Favorite:** Marks an exercise as favorite in View 3.
* `ENT` (Enter) ➡️ **Start / OK:** Starts a set, confirms inputs, or saves information.
* `Q` ➡️ **Complete Set / Rest:** Stops the active set, runs VBT fatigue calculations, and launches the rest timer.
* `DEL` (Backspace) ➡️ **Cancel / Back:** Cancels the active operation, exits the current set, or deletes logs in View 4.

---

## 🛠️ Build & Flash Instructions

### 💻 Development Environment
The project is built using **PlatformIO** (VS Code).

#### Configuration `platformio.ini`
```ini
[env:m5stack-cardputer]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = default_8MB.csv
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM=0
```

### 📂 Precompiled Binary Path (`.bin`)
To flash the firmware directly to your Cardputer without compiling, you can find the compiled binary in the PlatformIO build directory:
```text
.pio/build/m5stack-cardputer/firmware.bin
```

---

## 🔌 Hardware Pinout Configuration (MicroSD Slot)
For custom hardware integration or SD card diagnostic purposes, the SPI pins used on the Cardputer are:
* **MISO (Master Input Slave Output):** `GPIO 39`
* **MOSI (Master Output Slave Input):** `GPIO 14`
* **SCK (Serial Clock):** `GPIO 40`
* **CS (Chip Select):** `GPIO 12`
* **SPI Bus Frequency:** `25 MHz`

---

## 📄 License & Credits
Custom GymTracker firmware developed for M5Stack Cardputer. Built to maximize hardware performance and provide lifters with a gorgeous, high-fidelity, and feature-rich biomechanical tracking tool. Lift heavy and train smart! 🏋️‍♂️💪
