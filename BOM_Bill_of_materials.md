

| No. | Component / Part Name | Category | Function & Hardware Safety Role | Qty | Est. Unit Cost (VND) | Total Cost (VND) |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| **1** | **ESP32-C6-DevKitC-1** (or compatible RISC-V board) | Microcontroller SoC | Central computation, Wi-Fi 6 (SNTP sync), FreeRTOS kernel, firmware flashing, and USB CDC/JTAG debugging. | 1 | 150,000 | 150,000 |
| **2** | **SHT30 / SHT31 Module** (or BME280) | I2C Sensor | High-precision ambient temperature/humidity tracking on mutex-protected 400 kHz I2C bus. | 1 | 75,000 | 75,000 |
| **3** | **0.96-inch I2C OLED Display (SSD1306)** | Visual Display | Local visual output (NTP clock, monotonic timers, sensor stream) sharing I2C bus with SHT31. | 1 | 55,000 | 55,000 |
| **4** | **I2C Pull-Up Resistors ($4.7\\text{ k}\\Omega$, 1/4W)** | Passive Component | **Electrical Protection:** External pull-ups on SDA (GPIO 6\) and SCL (GPIO 7\) to guarantee fast signal rise-time and prevent 400 kHz bus lockup. | 2 | 1,000 | 2,000 |
| **5** | **Decoupling Capacitor Set ($100\\mu\\text{F}$ \+ $0.1\\mu\\text{F}$)** | Passive Component | **Power Integrity:** Suppresses transient brownout voltage sags caused by Wi-Fi TX bursts and relay coil activation. | 1 set | 3,000 | 3,000 |
| **6** | **1-Channel Relay Module with Optocoupler (3.3V/5V Trigger)** | Actuator (Isolated) | **Electrical Protection:** Photocoupler isolation \+ flyback diode prevents inductive coil kickback and MCU brownouts during switching. | 1 | 25,000 | 25,000 |
| **7** | **LDR Photoresistor Module** (3-pin Analog) | ADC Sensor | Ambient light tracking; evaluates 12-bit ADC calibrated readings and software oversampling filter. | 1 | 15,000 | 15,000 |
| **8** | **PMS7003 PM2.5 Sensor** (or UART Tool) | UART Peripheral | Continuous byte stream generator to validate UART ISR Ring Buffer and 5-state frame-sync FSM parser. | 1 | 210,000 | 210,000 |
| **9** | **Active Buzzer Module (3.3V \- 5V)** | Acoustic Indicator | Instant acoustic feedback for monotonic timer expirations and critical threshold alarms. | 1 | 5,000 | 5,000 |
| **10** | **MB-102 Solderless Breadboard (830 Points)** | Prototyping | Circuit mounting base for discrete passives, decoupling capacitors, and sensor modules. | 1 | 35,000 | 35,000 |
| **11** | **Breadboard Jumper Wire Set (40 pcs)** | Interconnects | Signal and power interconnects (Male-to-Male and Male-to-Female). | 1 pack | 25,000 | 25,000 |
| **12** | **USB Type-C to Type-A Data Cable** | Interface & Power | 5V DC power supply and native USB CDC/JTAG debugging linked to HP EliteBook 8570w. | 1 | 40,000 | 40,000 |
| **Total** |  |  |  |  |  | **\~640,000 VND** *(\~$25.50 USD)* |

**Hardware Safety & Implementation Notes:**

* **Power Decoupling Placement:** Connect the $100\\mu\\text{F}$ electrolytic capacitor (observing polarity) in parallel with the $0.1\\mu\\text{F}$ (100nF) ceramic capacitor directly across the $5\\text{V}/\\text{VIN}$ and $\\text{GND}$ rails on the breadboard closest to the ESP32-C6 power input pins.  
* **I2C Pull-Up Wiring:** Disable internal pull-ups in firmware (gpio\_pullup\_dis) and wire one $4.7\\text{ k}\\Omega$ resistor from GPIO 6 (SDA) to 3.3V, and the second $4.7\\text{ k}\\Omega$ resistor from GPIO 7 (SCL) to 3.3V.  
* **Low-Cost UART Testing Alternative:** If the PMS7003 (\#8) is omitted initially to save budget, substitute it with a standard USB-to-UART bridge (e.g., CP2102, \~25,000 VND) wired to GPIO 4 (RX) and GPIO 5 (TX), using Python on the laptop to transmit raw 0x42 0x4D byte frames.

