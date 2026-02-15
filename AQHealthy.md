# AQHealthy
This project is a real-time air quality monitoring system which has been built using an **Arduino Nano**, a **PMS5003** particulate matter sensor and an **MQ135** gas sensor

### This Projects measures:
- PM1.0 dust readings
- PM2.5 dust readings
- Gas levels (Via the MQ135)
- Calculated Air Quality Index (Calculated AQI)
- Air Quality trend (Rising/Falling/Stable)
- Sustained unhealthy exposure duration
- Health-based alert states
This device operates in two operation modes:
- Plot Mode - Numeric output for Arduino Serial Plotter
- Message Mode - Human-readable reports with alerts

### Objectives:
- Convert raw particulate data into genuine AQI readings
- Detect unhealthy air conditions
- Classify dust type (Desert,Indoor,Mixed)
- Monitor sustained exposure duration
- Provide clear and repeated warning alerts
- Enable live data visualization

### System Architecture
#### Sensing layer
- Reads PMS5003 data packets (32 bytes)
- Extracts PM1.0, PM2.5, PM10
- Reads MQ135 analog gas signal

#### Analysis layer
- Converts PM values into AQI using EPA breakpoints
- Classifies AQI into health categories
- Maintains rolling of PM2.5 history for trend detection
- Tracks sustained unhealthy exposure time
- Determines alert states

#### Reporting layer
- Plot mode -> Outputs numerical values for graphing
- Messaging mode -> Prints readable summaries and warnings

### Hardware Components
- Arduino Nano -> Microcontroller
- PMS5003 -> Laser particulate sensor
- MQ135 -> Gas sensor
- Jumper Wires -> Connections
- Power Supply -> 5V USB

### Wiring
- PMS5003 TX -> D8
- PMS5003 RX -> D9
- MQ135 AOUT -> A3

### AQI Calculation
The AQI is calculated using official **EPA** breakpoints for:
- PM2.5
- PM10

#### Health Categories
- Good
- Moderate
- Unhealthy
- Very unhealthy
- Hazardous


### Usage Instructions
#### Upload Code
- Connect Arduino via USB
- Connect Arduino IDE
- Select correct board and port
- Upload sketch

#### Serial Monitor Mode (Texts + Plots)
- Open serial monitor (9600 bauds)
- Type:
- - m -> Message mode
  - p -> Plot mode

#### Serial Plotter Mode (Live graph)
- Type p in the serial monitor
- Close Serial monitor
- Open Serial plotter
- View live graphs of
- - PM2.5
  - PM10
  - AQI
  - Gas Level
 
### Future Improvements:
- SD Card logging
- Wireless data transmission (ESP32)
- Web dashboard visualization
- Calibrated gas concentration modeling
- Integration of temp and humidity sensors
