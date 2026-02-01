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



## 🔌 Wiring diagram

### System overview

```
          +-------------------+
          |     ESP8266       |
          |    (NodeMCU)      |
          |                   |
          | D4  → DS18B20     |
          | D2  → SDA (RTC)   |
          | D1  → SCL (RTC)   |
          | 3V3 → VCC         |
          | GND → GND         |
          +---------+---------+
                    |
          +---------+---------+
          |                   |
     +----+----+         +----+----+
     | DS18B20 |         | DS3231  |
     | Temp    |         | RTC     |
     +---------+         +---------+
```

---

### Breadboard-style wiring

```
TOP VIEW
=================================================================

3V3  ==============================================
GND  ==============================================


                 ESP8266 (NodeMCU)
        +-----------------------------------+
        |                                   |
        |  D4 (GPIO2)  -------- DATA -------+---- DS18B20
        |                    |              
        |                   4.7kΩ           
        |                    |              
        |                   3V3             
        |                                   |
        |  D2 (GPIO4)  -------- SDA --------+---- DS3231
        |  D1 (GPIO5)  -------- SCL --------+---- DS3231
        |                                   |
        |  3V3 -----------------------------+---- VCC (all)
        |  GND -----------------------------+---- GND (all)
        +-----------------------------------+
```

---

### DS18B20 pinout

```
Flat side facing you:

   +-------------+
   |  GND DATA VCC |
   +-------------+
```

---

### Connection table

DS18B20:
- GND → GND
- DATA → D4 (GPIO2)
- VCC → 3V3
- 4.7kΩ resistor between DATA and 3V3

DS3231 (RTC):
- VCC → 3V3
- GND → GND
- SDA → D2 (GPIO4)
- SCL → D1 (GPIO5)
- SQW → not used
