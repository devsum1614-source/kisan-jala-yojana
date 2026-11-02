# 🌾 Kisan Jala Yojana (किसान जल योजना)
### *IoT-Based Smart Borewell Motor Remote Controller & Water Management System*

[![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20FreeRTOS-blue.svg)](https://www.espressif.com/)
[![Modem](https://img.shields.io/badge/Modem-SIMCom%20A7670C--LASS%20(4G%20Cat--1)-orange.svg)](https://www.simcom.com/)
[![Protocol](https://img.shields.io/badge/Protocol-DTMF%20%7C%20MQTT%20%7C%20SMS-brightgreen.svg)](https://mqtt.org/)
[![Affiliation](https://img.shields.io/badge/Academic-VTU%20Belagavi%20%7C%20KIT%20Mangalore-red.svg)](https://vtu.ac.in/)

---

## 📌 Executive Summary

**Kisan Jala Yojana** is a low-cost, resilient, 4G-connected agricultural IoT device designed to provide remote operational control of borewell pumps from anywhere. It bridges high-technology automation with rural realities by introducing a **dual-channel hybrid control paradigm**:

1. **Offline Dual-Tone Multi-Frequency (DTMF) Control:** Operates via standard cellular telephone calls without requiring internet data or smartphone ownership. Farmers dial the device's SIM number from any basic feature phone and use keypad digits to switch pumps ON/OFF, set timed runs, or check status.
2. **Online Cloud MQTT Telemetry:** Enables smartphone users to monitor real-time pump running state, view power telemetry, receive dry-run alerts, and schedule irrigation through an interactive mobile app.

---

## 🎯 Problems Solved

* **Midnight Travel Hazards:** Agricultural three-phase electricity grids in rural India are often energized during irregular nocturnal hours (11 PM – 4 AM). Farmers are forced to commute through pitch-dark, unpaved terrain with risks of venomous snakebites and electrical accidents.
* **Catastrophic Motor Burnout:** When water tables plunge below the pump intake, submersible motors run dry. Operating without water lubrication causes rapid mechanical friction and stator coil burnout, costing farmers ₹8,000 to ₹15,000 per repair.
* **Groundwater & Electricity Depletion:** Forgetting to shut off pumps on time squanders thousands of liters of precious groundwater and results in high electricity bills.
* **2G Network Sunset:** Legacy commercial GSM starters rely on obsolete 2G networks that are being retired by telecom providers. Kisan Jala Yojana is built on 4G LTE Cat-1 technology with VoLTE.

---

## 🏗️ System Architecture & Block Diagram

```
[ Farmer's Basic Phone ] (Voice Call / DTMF)      [ Smartphone App ] (MQTT over 4G Data)
             │                                                    │
             ▼                                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│             4G LTE Cellular Network  &  HiveMQ Cloud Broker            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ 4G RF Link (SMA Antenna)
┌───────────────────────────────────▼────────────────────────────────────┐
│       Valetron SIMCom A7670C-LASS (4G Cat-1 Modem with Audio Codec)     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ UART (AT Commands: DDET / CMQTT)
┌───────────────────────────────────▼────────────────────────────────────┐
│            ESP32 DevKit v1 Microcontroller (Main Control Unit)         │
│          ◄── [ SCT-013 CT Sensor ] & [ Ultrasonic Level Sensor ]       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ GPIO 25 Output
┌───────────────────────────────────▼────────────────────────────────────┐
│          Optocoupler-Isolated Relay Module (5V Coil, 10A/250VAC)       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Safe Mechanical Switching Contacts
┌───────────────────────────────────▼────────────────────────────────────┐
│          Agricultural Motor Starter / Contactor (230V / 415V)          │
│          ⟶ Submersible Borewell Water Pump                            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## ⚡ Hardware Pinout & Wiring Specifications

### 1. ESP32 ⟷ Valetron SIMCom A7670C-LASS (UART Interface)
| ESP32 Pin | A7670C Modem Pin | Signal Direction | Function / Description |
| :---: | :---: | :---: | :--- |
| **GPIO17** | **RXD** | ESP32 $\rightarrow$ Modem | Transmits AT commands from MCU |
| **GPIO16** | **TXD** | Modem $\rightarrow$ ESP32 | Receives URCs, DTMF tones, and MQTT data |
| **GPIO04** | **RESET** | ESP32 $\rightarrow$ Modem | Hardware pulse reset for watchdog recovery |
| **GPIO05** | **PWRKEY** | ESP32 $\rightarrow$ Modem | Software power on / off toggle |
| **GND** | **GND** | Common Ground | Common digital reference ground |

### 2. ESP32 ⟷ Optocoupler Relay & Starter
| ESP32 Pin | Relay Module Pin | Description |
| :---: | :---: | :--- |
| **GPIO25** | **IN1** | Active HIGH trigger to energize relay coil |
| **5V (VIN)** | **VCC** | Relay optocoupler coil power supply |
| **GND** | **GND** | Digital ground |
| *Relay COM/NO* | *Contactor Coil* | In series with the motor starter push-to-start loop |

> **⚠️ Critical Safety Note:** Never connect an ESP32 GPIO directly to the motor starter coil. An optocoupler-isolated relay module must always be used to decouple 3.3V logic from 230V/415V inductive switching spikes.

### 3. Power Supply Architecture
* **Input:** 12V/2A DC Adapter (or 12V Solar Battery bank).
* **Modem Power:** Dedicated 12V $\rightarrow$ 5V Buck Converter (LM2596) capable of supplying up to **500 mA peak transmission bursts** without sagging.
* **Microcontroller:** Step-down 3.3V LDO for the ESP32.

---

## 📱 Operational Modes

### Mode A: Phone Call (DTMF Keypad Commands)
1. The farmer calls the SIM card number installed in the field unit.
2. The modem issues `RING` and `+CLIP: "+91XXXXXXXXXX"` over UART.
3. The ESP32 compares the incoming number against its **NVS Flash Whitelist**.
   * *If unauthorized:* Rejected immediately (`ATH`).
   * *If authorized:* Answered automatically (`AT+ATA`).
4. The farmer presses keypad digits:
   * **`1`**: Turn Motor **ON**
   * **`0`**: Turn Motor **OFF**
   * **`2`**: Run for **30 Minutes** (Auto timed shut-off)
   * **`3`**: Run for **60 Minutes** (Auto timed shut-off)
   * **`9`**: Query status (Unit sends SMS report back)
   * **`#`**: Hang up call
5. The ESP32 sends a delivery-confirmed SMS notification:  
   `"Kisan Jala Yojana: Motor turned ON successfully."`

### Mode B: Mobile Application (Cloud MQTT)
* **Broker:** `broker.hivemq.com` (Port 1883)
* **Command Topic:** `borewell/cmd`
  * Payload: `{"cmd": "motor_on"}` or `{"cmd": "motor_off"}`
* **Telemetry Topic:** `borewell/status`
  * Payload: `{"state": "running", "uptime_min": 45, "current_amps": 12.4, "dry_run": false}`

---

## 🛡️ Safety & Protection Features

* **Dry-Run Detection:** Monitors motor current using a non-invasive current transformer (`SCT-013`). If current drops below the normal threshold while the pump is ON (indicating loss of water lubrication), the controller trips the motor within 5 seconds and alerts the farmer via SMS.
* **Caller ID Whitelisting:** Prevents malicious or accidental switching by unauthorized phone numbers.
* **Failsafe Hardware Timers:** Non-blocking hardware timers automatically turn the motor off if communication is lost during a scheduled run.

---

## 📦 Bill of Materials (BOM)

| Component | Specification | Purpose | Approx. Cost (INR) |
| :--- | :--- | :--- | :---: |
| **SIMCom A7670C-LASS** | 4G Cat-1 with Audio Codec | DTMF voice decoding, SMS, MQTT | ₹1,199 |
| **ESP32 DevKit v1** | 240 MHz Dual-Core, 4MB Flash | Main controller & timer logic | ₹350 – ₹500 |
| **Relay Module** | 5V Optocoupler, 10A/250VAC | Contactor coil switching isolation | ₹80 – ₹150 |
| **Current Sensor** | SCT-013 Non-Invasive CT (30A) | Dry-run motor protection | ₹150 – ₹250 |
| **4G SMA Antenna** | 3.5 dBi High Gain Rubber Duck | Strong RF signal in remote fields | ₹100 – ₹150 |
| **Power Supply** | 12V/2A DC Adapter + Buck Converter | Dual power regulation | ₹150 – ₹300 |
| **Enclosure** | IP65 Weatherproof Junction Box | Dust, insect & moisture barrier | ₹200 – ₹400 |
| **Total Build Cost** | | | **₹2,200 – ₹2,800** |

---

## 📂 Project Repository Structure

```
├── docs/
│   ├── Kisan_Jala_Yojana_Synopsis_Without_Front_Page.pdf   # VTU-compliant 5-page PDF synopsis
│   ├── kisan_jala_yojana_body_synopsis.html                # Print-ready HTML source
│   ├── kisan_jala_yojana_vtu_synopsis.doc                  # Editable Word document
│   └── VTU_Project_Synopsis_Kisan_Jala_Yojana.md           # Markdown synopsis source
├── firmware/
│   └── esp32_kisan_jala_yojana/
│       └── esp32_kisan_jala_yojana.ino                     # ESP32 C++ firmware
├── schematics/
│   └── project_sceme.txt                                   # Hardware wiring schematic
└── README.md                                               # Project documentation
```

---

## 🎓 Academic Information

* **Institution:** Karavali Institute of Technology (KIT), Mangalore
* **Affiliation:** Visvesvaraya Technological University (VTU), Belagavi
* **Department:** Department of Computer Science and Engineering (CSE)
* **Academic Year:** 2025–26
* **Degree:** Bachelor of Engineering (B.E.)

---

## 📜 License
This project is open-source under the [MIT License](LICENSE).
