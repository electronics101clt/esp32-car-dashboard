# Changelog

All notable changes to ESP32 Car Dashboard will be documented in this file.

## [2.0.0] - 2026-06-06

### Major Overhaul - Realistic Analog Gauges

#### Added
- **HTML5 Canvas Gauges**: Complete rewrite using Canvas API for authentic analog gauges
- **Tick Marks**: Major and minor tick marks like real automotive instrument clusters
- **Numbered Dials**: Numbers displayed at major tick intervals around gauge face
- **Realistic Needle**: Rotating needle with center hub and smooth animation
- **Colored Progress Arc**: Arc showing current value range with gradient colors
- **BLE ELM327 Support**: Automatic scanning and connection to Bluetooth OBD-II adapters
- **Persistent Serial Number**: 8-digit random serial stored in flash memory (survives reboots)
- **Dynamic SSID**: WiFi network named `ZS-XXXXXXXX` based on persistent serial

#### Changed
- **Gauge Rendering**: Migrated from SVG to HTML5 Canvas for better performance and realism
- **Layout**: 2 large gauges (280x280) for Speed/RPM, 6 small gauges (140x140) for other metrics
- **SSID Format**: Changed from static "ESP32-Control" to dynamic "ZS-XXXXXXXX"
- **Password**: Remains "12345678" for compatibility
- **Styling**: Dark radial gradient backgrounds matching real car instrument clusters
- **Warning Lights**: Simplified to 3 core warnings (Check Engine, Oil, Battery) with glow effects

#### Technical Details
- **Canvas Drawing**: Custom `drawGauge()` function renders all gauge elements
- **Tick Calculation**: Dynamic tick mark positioning using trigonometry
- **Number Placement**: Text positioned around arc at major tick intervals
- **Needle Rotation**: Smooth rotation based on value percentage (270° arc)
- **Preferences API**: Uses ESP32 Preferences library to store serial number
- **BLE Integration**: Full BLE characteristic discovery and notification support

#### Memory Usage
- **Program Storage**: 1,694,311 bytes (80% of 2,097,152 bytes with no_ota partition)
- **Dynamic Memory**: 62,504 bytes (19% of 327,680 bytes)
- **Partition Scheme**: Requires `no_ota` partition for larger HTML/Canvas code

#### Browser Requirements
- HTML5 Canvas support (all modern browsers)
- JavaScript ES6 support
- No external libraries or dependencies

#### Breaking Changes
- WiFi SSID now dynamic - users must look for `ZS-XXXXXXXX` networks
- SVG-based gauge code removed - not compatible with v1.0.0
- Warning lights reduced from 6 to 3

## [1.0.0] - 2026-06-03

### Initial Release

#### Features

**8 Real-Time Animated Gauges:**
- Speedometer (0-200 km/h) with green gradient and smooth needle animation
- Tachometer/RPM (0-8000 RPM) with red-zone styling and orange gradient
- Coolant Temperature (50-130°C) with blue→red color gradient
- Fuel Level (0-100%) with yellow gradient and low-fuel warning colors
- Engine Load (0-100%) with purple→red gradient
- Battery Voltage (10-16V) with health indicator (red→green gradient)
- Intake Air Temperature (0-100°C) with blue→orange gradient
- Throttle Position (0-100%) with green→yellow gradient

**Dashboard Elements:**
- 6 Warning Lights: Check Engine, Oil Pressure, Battery, Brake, ABS, Airbag
- Pulsing animation for active warning lights
- Info Panel: Odometer (125,478 km), Trip Distance (542.3 km), Average Speed (62 km/h), Fuel Consumption (7.2 L/100km)
- Modern dark theme with cyberpunk neon styling
- Gradient backgrounds and glowing text effects

**Technical Implementation:**
- Real-time data updates every 500ms via JSON API
- WiFi Access Point mode (no internet required)
- HTTP Web Server on port 80 at 192.168.4.1
- RESTful JSON API endpoint: `/api/data`
- Simulated realistic sensor data with smooth transitions
- SVG-based gauges with animated needles and arcs
- CSS3 animations and gradients
- Vanilla JavaScript (no external dependencies)

**Responsive Design:**
- Phone portrait (320px-480px): Single column layout
- Tablet landscape (768px+): 2-4 column grid layout
- Desktop (1024px+): 4 column layout
- Dynamic font sizing with CSS clamp()
- Touch-optimized spacing

**Data Simulation:**
- Speed: Smooth acceleration/deceleration (0-180 km/h)
- RPM: Varies with throttle (600-7000 RPM)
- Coolant: Realistic temperature range (85-105°C)
- Fuel: Gradual decrease over time, resets at 10%
- Voltage: Fluctuates around healthy 14.2V
- Engine Load: Calculated from throttle position
- Intake Temp: Increases with engine load
- Throttle: Mapped from RPM values

#### Hardware Requirements
- ESP32 Dev Module (ESP32-D0WD-V3 or compatible)
- USB cable for programming and power
- Minimum 4MB flash memory

#### Network Configuration
- **SSID**: ESP32-Control
- **Password**: 12345678
- **IP Address**: 192.168.4.1 (default AP gateway)
- **Port**: 80 (HTTP)
- **Protocol**: Cleartext HTTP

#### Memory Usage
- **Program Storage**: 956,783 bytes (73% of 1,310,720 bytes)
- **Dynamic Memory**: 47,072 bytes (14% of 327,680 bytes)
- **Flash Required**: Minimum 1MB

#### Browser Compatibility
- ✅ Chrome/Chromium (Android, Desktop)
- ✅ Firefox
- ✅ Safari (iOS, macOS)
- ✅ Edge
- ✅ WebView (Android apps)
- ✅ All modern browsers with SVG and ES6 support

#### API Specification

**GET /** - Returns HTML dashboard interface

**GET /api/data** - Returns JSON with sensor data:
```json
{
  "speed": 78.5,        // km/h (float)
  "rpm": 3200,          // revolutions per minute (integer)
  "coolant": 92.3,      // degrees Celsius (float)
  "fuel": 68.7,         // percentage (float)
  "load": 45.2,         // percentage (float)
  "voltage": 14.1,      // volts (float)
  "intake": 28.5,       // degrees Celsius (float)
  "throttle": 42.0,     // percentage (float)
  "checkEngine": false, // boolean
  "oilPressure": false  // boolean
}
```

#### OBD-II Integration Ready

The dashboard is designed for easy integration with OBD-II readers:

**Supported OBD-II PIDs:**
- `0x0C` - Engine RPM
- `0x0D` - Vehicle Speed
- `0x05` - Coolant Temperature
- `0x2F` - Fuel Tank Level Input
- `0x04` - Calculated Engine Load
- `0x0F` - Intake Air Temperature
- `0x11` - Throttle Position
- `0x42` - Control Module Voltage

**Integration Points:**
- Replace simulated data in `handleData()` function
- Add OBD-II library (ESP32-OBD-II, ELM327, etc.)
- Connect via UART, CAN bus, or Bluetooth
- Map OBD-II PID responses to dashboard variables

#### Use Cases
- Custom car dashboard displays
- OBD-II data visualization
- Vehicle monitoring systems
- Automotive education/training
- DIY instrument clusters
- Race car telemetry displays
- Fleet management dashboards
- Car audio system integration

#### Known Limitations
- Uses simulated data by default (requires OBD-II integration for real data)
- No data logging or history (future feature)
- No configuration UI (edit code to change settings)
- Single gauge style (future: multiple themes)

#### Future Enhancements
- Real OBD-II integration examples
- Data logging to SD card
- Configurable gauge ranges via web UI
- Multiple theme options
- Mobile app companion
- Bluetooth connectivity
- WebSocket support for lower latency

#### Credits & Inspiration
Built with pure JavaScript, HTML5, and CSS3. Inspired by:
- [Speedometer.js](https://github.com/vladyspavlov/html5-canvas-speedometer) - HTML5 Canvas car dashboard
- [EQM_OBDWEB](https://github.com/EQMOD/EQM_OBDWEB) - ESP32 WROVER OBD-II platform
- [ElektorLabs OBD2 Dashboard](https://github.com/ElektorLabs/obd2-dashboard) - ESP32-S3 OBD-II dashboard

#### Related Projects
- [ZScreen Dash](https://github.com/electronics101clt/ZScreenDash) - Android app for automatic WiFi connection
- [ZScreen ESP Client](https://github.com/electronics101clt/ZScreenESPClient) - Simple ESP32 control interface

#### License
MIT License - Free for personal and commercial use

---

## Version Format

This project follows [Semantic Versioning](https://semver.org/):
- **MAJOR** version for incompatible API changes
- **MINOR** version for new functionality in a backwards compatible manner
- **PATCH** version for backwards compatible bug fixes

## Changelog Format

Based on [Keep a Changelog](https://keepachangelog.com/):
- **Added** for new features
- **Changed** for changes in existing functionality
- **Deprecated** for soon-to-be removed features
- **Removed** for now removed features
- **Fixed** for any bug fixes
- **Security** for vulnerability fixes
