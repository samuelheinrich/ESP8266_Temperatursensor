# ESP8266_Temperatursensor
ESP8266-based temperature logger with RTC, web UI and CSV logging. Designed to measure thermal behaviour of Wi-Fi access points inside closed enclosures under real traffic load.

ESP AP Temperature Logger

A lightweight, fully standalone temperature logging system based on an ESP8266.

This project was built to measure the thermal behaviour of Wi-Fi access points installed inside closed outdoor/industrial enclosures.
It records temperature over time and provides live monitoring via a built-in web interface.

No cloud.
No external server.
No dependencies.
Just power → connect → measure.

🎯 Goal

New Cisco C9176 (Wi-Fi 6E/7) access points are installed inside existing Thuba EX enclosures.

Because the new hardware:

• uses additional 6 GHz radios
• supports higher transmit power
• supports Multi-Gig uplinks
• draws up to 60 W (uPoE)

a higher thermal load is expected.

The objective was:

• measure internal enclosure temperature
• compare idle vs traffic load
• compare PoE+ (30 W) vs uPoE (60 W)
• log data over long periods
• keep the setup simple and reproducible

✨ Features

• ESP8266 SoftAP (no router required)
• DS18B20 temperature sensor
• DS3231 RTC timestamps
• CSV logging to flash (LittleFS)
• built-in web UI
• live values + history
• start/stop named measurements
• file download & delete
• works completely offline

🧰 Hardware
Component	Purpose
ESP8266 (NodeMCU / ESP-12)	main controller + Wi-Fi
DS18B20	temperature sensor
DS3231	real-time clock
4.7kΩ resistor	1-Wire pull-up
breadboard + wires	wiring
USB power or LiPo	power supply
🔌 Wiring
DS18B20 (1-Wire)

Pin order (flat side facing you):

[ GND | DATA | VCC ]


Connections:

DS18B20     → ESP8266
-------------------------
GND         → GND
DATA        → D4 (GPIO2)
VCC         → 3V3

4.7kΩ resistor between DATA and 3V3

DS3231 (RTC, I2C)
DS3231      → ESP8266
-------------------------
VCC         → 3V3
GND         → GND
SDA         → D2 (GPIO4)
SCL         → D1 (GPIO5)
SQW         → not used

🧩 Complete schematic (ASCII)
                 +----------------------+
                 |      ESP8266        |
                 |                      |
      3V3 -------+----+-----------------+
                 |    |                 |
                 |   4k7Ω               |
                 |    |                 |
                 |   D4 (GPIO2) --------+---- DS18B20 DATA
                 |                      |
                 |   D2 (GPIO4) --------+---- DS3231 SDA
                 |   D1 (GPIO5) --------+---- DS3231 SCL
                 |                      |
      GND -------+----------------------+---- all GND

🌡 Physical placement

The DS18B20 sensor is placed freely in the air inside the enclosure.

Not glued to metal.
Not attached to the AP.

This measures ambient enclosure temperature, not surface temperature.

🚀 Software

The ESP runs:

• temperature measurement every 5 seconds
• RTC timestamping
• CSV logging to flash
• HTTP server
• live web UI

Wi-Fi

Device creates its own network:

SSID: ESPCONNECT
Password: pass1234
IP: 10.1.1.1


Connect with your laptop/phone and open:

http://10.1.1.1

🖥 Web Interface
Live

• latest temperature
• auto refresh
• small graph
• start/stop recording

History

• full list
• graph
• refresh button

Files

• download CSV
• delete
• file size display

📁 File structure (LittleFS)
/default.csv                  → always recording
/test1_2026-01-27_14-05.csv   → manual recordings
/loadtest_2026-01-27_15-22.csv

📄 CSV format
timestamp,temperature
2026-01-27 14:05:13,29.50
2026-01-27 14:05:18,29.56


Directly usable in:

• Excel
• Numbers
• LibreOffice
• Python / pandas
• Grafana

⏱ Storage capacity

ESP8266 flash: 4 MB
Available for logs: ~2–3 MB

Typical:

• 5s interval → several days
• 1 min interval → weeks
• 5 min interval → months

📊 Test methodology

Traffic generated using:

• iperf3
• Wi-Fi 6E clients
• TCP streams
• WLAN → wired server

Scenarios:

• Idle
• Load
• PoE+ (30 W)
• uPoE (60 W)

🧪 Example results

Observed behaviour:

• fast warm-up after power-on
• stable plateau after few minutes
• load increases temperature
• 60 W higher than 30 W
• max observed: 43.86 °C

No thermal instability detected.

🛠 Build
Arduino IDE

Install:

• ESP8266 board package
• OneWire
• DallasTemperature
• RTClib
• LittleFS (built-in)

Upload sketch.

▶️ Usage

power device

connect to ESPCONNECT

open browser

start measurement

download CSV

Done.

🧠 Design philosophy

Small
Simple
Local
Reliable

Because the best measurement tool is the one that:

• boots instantly
• never crashes
• doesn’t need cloud
• just logs

📜 License

MIT
