# BlueGhost

# BlueGhost & EIB Mitigation Testbed
This repository contains the proof-of-concept implementation and evaluation framework for BlueGhost, a BLE privacy tracking attack, and EIB, a practical defense mechanism. 

🛠 Prerequisites

* **Hardware:** ESP32 development board 
* **Software:** Arduino IDE `1.8.19`
* **Environment:** A Bash terminal is required to run the automated evaluation scripts. 

## 📁 Repository Structure

* `EIB.ino` — The core firmware for the ESP32. It handles the malicious peripheral impersonation and the dynamic toggling of the EIB defense mechanism.
* `/time/` — Automated performance evaluation scripts and datasets.
  * `time.sh`: A shell script to automate the BLE connection, pairing, and disconnection cycle for latency measurement.
  * `original.csv`: Original pairing execution times.
  * `EIB.csv`: EIB defense pairing execution times.
* `/trace/` — Tracking and RPA resolution components.
  * `trace.ino`: Sniffer program to capture and decode target BLE advertisements.
  * `irk.h`: Header file for storing and managing the extracted Identity Resolving Keys (IRK).

## 🚀 Usage Guide

### 1. Attack & Defense Demonstration (`EIB.ino`)
1. Open `EIB.ino` in Arduino IDE 1.8.19 and flash it to your ESP32 board.
2. Open the **Serial Monitor**.
3. You can dynamically control the defense state by sending the following commands via the serial interface:
   * `eib_disable` (Default): Turns OFF the EIB defense. The device will distribute the persistent Master IRK during JW pairing, demonstrating a successful BlueGhost attack and enabling long-term tracking.
   * `eib_enable`: Turns ON the EIB defense. The device will dynamically derive and distribute a short-lived `eph_IRK`, effectively thwarting persistent tracking.

### 2. Automated Latency Evaluation (`/time`)
```bash
cd time
./time.sh


##  Acknowledgments
The RPA decoding and tracking logic located in `/trace/trace.ino` and `/trace/irk.h` builds upon open-source implementations. Special thanks to **[fryefryefrye](https://github.com/fryefryefrye/Decoding-Random-Bluetooth-Address)** for providing the foundational decoding source code.
