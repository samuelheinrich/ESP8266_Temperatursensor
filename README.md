# ESP8266_Temperatursensor
ESP8266-based temperature logger with RTC, web UI and CSV logging. Designed to measure thermal behaviour of Wi-Fi access points inside closed enclosures under real traffic load.

<img width="697" height="872" alt="image" src="https://github.com/user-attachments/assets/80e3f068-5e17-4df6-9e76-047958534403" />


# ESP AP Temperature Logger

A small, fully standalone ESP8266-based temperature logger designed to measure the thermal behaviour of Wi-Fi access points inside closed enclosures.

No cloud.  
No server.  
No infrastructure.  
Just power → connect → log.

---

## 🎯 Goal

New Cisco C9176 (Wi-Fi 6E/7) access points are installed inside existing Thuba EX enclosures.

Because the new hardware:

- additional 6 GHz radio
- higher transmit power
- Multi-Gig uplinks
- up to 60 W PoE

a higher thermal load is expected.

This project measures the internal enclosure temperature over time to evaluate thermal behaviour under real traffic conditions.

---

## ✨ Features

- standalone Wi-Fi Access Point (SoftAP)
- DS18B20 temperature sensor
- DS3231 real-time clock
- CSV logging to flash (LittleFS)
- built-in web interface
- live view + history
- file download & delete
- works completely offline

---

## 🧰 Hardware

| Component | Purpose |
|-----------|-----------|
| ESP8266 (NodeMCU / ESP-12) | controller + Wi-Fi |
| DS18B20 | temperature sensor |
| DS3231 | RTC for timestamps |
| 4.7kΩ resistor | 1-Wire pull-up |
| breadboard + wires | wiring |
| USB or LiPo | power |

<img width="1071" height="759" alt="image" src="https://github.com/user-attachments/assets/07e3f07e-65fc-489a-ad46-881e02780475" />




