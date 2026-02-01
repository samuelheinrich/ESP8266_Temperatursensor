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





## 🔌 Wiring diagram (breadboard style)

Top view (simplified)

             3V3 rail  =========================================
             GND rail  =========================================


        +--------------------------------------------------+
        |                   ESP8266 (NodeMCU)              |
        |                                                  |
        |   [D1] GPIO5  -------------------- SCL ----------+---- DS3231
        |   [D2] GPIO4  -------------------- SDA ----------+---- DS3231
        |   [D4] GPIO2  ----+------------- DATA -----------+---- DS18B20
        |                   |                              
        |                  4.7kΩ                            
        |                   |                              
        |                  3V3                              
        |                                                  |
        |   3V3 -------------------------------------------+---- VCC (RTC + Sensor)
        |   GND -------------------------------------------+---- GND (RTC + Sensor)
        +--------------------------------------------------+


DS18B20 pinout (flat side facing you)

   +----------------+
   |  GND DATA VCC  |
   +----------------+

DATA line needs 4.7kΩ pull-up to 3V3



## 🔌 Full connection list

ESP8266  →  DS18B20
-----------------------
GND      →  GND
D4       →  DATA
3V3      →  VCC
4.7kΩ    →  between DATA and 3V3

ESP8266  →  DS3231
-----------------------
3V3      →  VCC
GND      →  GND
D2       →  SDA
D1       →  SCL
