# AgriSense 🌿💧

An open-source, ESP32-powered smart agriculture IoT system designed for real-time environmental monitoring, automated irrigation control, and local data logging. AgriSense provides a standalone web dashboard, local CSV data logging via LittleFS, and modular sensor integration to optimize crop health and water usage.

---

## 🚀 Features

- **Real-Time Sensor Monitoring:** Tracks ambient temperature, relative humidity, soil moisture levels, and ambient light intensity.
- **Automated & Manual Irrigation:** Intelligent relay-driven water pump control that triggers based on soil moisture thresholds, with a manual override toggle via the web dashboard.
- **Local Web Server Dashboard:** Built-in ESPAsyncWebServer (or standard WebServer) providing a responsive mobile-friendly interface to view live metrics and system status.
- **LittleFS CSV Data Logging:** Periodically logs sensor readings with timestamps to an onboard LittleFS CSV file for historical tracking and analysis.
- **Local Data Export:** Download historical CSV logs directly over the local network via the web server interface.
- **Visual Feedback:** Live system status, sensor readouts, and pump states displayed locally on an I2C 1602 LCD module.

---

## 🛠️ Hardware Components

| Component | Function | Pin / Interface |
| :--- | :--- | :--- |
| **ESP32 NodeMCU / DevKit** | Core microcontroller & Wi-Fi AP/STA server | — |
| **DHT11 Sensor** | Ambient temperature and humidity monitoring | Digital Pin (Configurable) |
| **Capacitive Soil Moisture Sensor** | Analog soil moisture level detection | Analog Pin (`ADC`) |
| **LDR (Light Dependent Resistor)** | Ambient light intensity tracking | Analog Pin (`ADC`) |
| **I2C 1602 LCD Display** | Local real-time status & sensor readout | SDA / SCL (`GPIO 21` / `22`) |
| **5V Relay Module** | Controls the 5V/12V mini water pump | Digital Output Pin |
| **Submersible Water Pump** | Delivers irrigation water | Powered via Relay |

---

## 📂 Project Structure

```text
AgriSense/
├── AgriSense.ino          # Main firmware sketch
├── data/                  # LittleFS web assets & logs
│   ├── index.html         # Web dashboard interface
│   └── log.csv            # Historical sensor data log
├── index.h                # HTML/CSS/JS embedded via PROGMEM (Alternative)
└── README.md              # Project documentation
