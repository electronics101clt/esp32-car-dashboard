# ESP32 Car Dashboard

Realistic automotive dashboard with 8 analog gauges featuring tick marks, numbers, and animated needles. Built with HTML5 Canvas for authentic instrument cluster appearance. Perfect for automotive projects, OBD-II integration, or custom vehicle displays.

![Car Dashboard](https://img.shields.io/badge/ESP32-Car%20Dashboard-green)
![Gauges](https://img.shields.io/badge/Gauges-8-blue)
![License](https://img.shields.io/badge/license-MIT-blue)

## Features

### 8 Realistic Analog Gauges
- **Speedometer** (0-200 km/h) - Large gauge with 20 tick marks and numbers
- **Tachometer/RPM** (0-8000 RPM) - Large gauge with red-zone styling
- **Coolant Temperature** (50-130°C) - Cyan colored gauge with 8 ticks
- **Fuel Level** (0-100%) - Yellow gauge with 10 tick marks
- **Engine Load** (0-100%) - Purple gradient gauge
- **Battery Voltage** (10-16V) - Green voltage indicator with 6 ticks
- **Intake Air Temperature** (0-100°C) - Blue gauge with 10 ticks
- **Throttle Position** (0-100%) - Orange throttle gauge

### Realistic Gauge Features
- **Tick Marks**: Major and minor tick marks like real automotive gauges
- **Numbered Dial**: Numbers displayed at major tick intervals
- **Animated Needle**: Smooth rotating needle with center hub
- **Colored Arc**: Progressive arc showing current value range
- **Dark Background**: Radial gradient background like real instrument clusters
- **Canvas Rendering**: HTML5 Canvas for smooth, realistic graphics

### Dashboard Elements
- **3 Warning Lights**: Check Engine, Oil Pressure, Battery (with glow effect)
- **BLE ELM327 Support**: Connects to Bluetooth OBD-II adapters
- **Persistent Serial**: 8-digit serial number (ZS-XXXXXXXX) stored in flash
- **Responsive Design**: Works on phones, tablets, and desktop
- **Dark Theme**: Automotive-style black background

### Technical Features
- **Real-Time Updates**: 500ms refresh rate via JSON API
- **WiFi Access Point**: Creates standalone network `ZS-XXXXXXXX`
- **HTTP Web Server**: Serves on 192.168.4.1:80
- **JSON API**: RESTful endpoint for sensor data
- **BLE OBD-II**: Automatically scans and connects to ELM327 devices
- **Persistent Config**: Serial number survives reboots, changes only on reflash

## Hardware Requirements

- ESP32 Dev Module (ESP32-D0WD-V3 or similar)
- USB cable for power and programming
- Minimum 4MB flash
- Optional: OBD-II reader/CAN bus module for real vehicle data

## Installation

### Using Arduino CLI
```bash
# Install ESP32 board support
arduino-cli core install esp32:esp32

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_car_dashboard.ino

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 esp32_car_dashboard.ino --upload-field serial.upload_speed=115200
```

### Using Arduino IDE
1. Open `esp32_car_dashboard.ino`
2. Select Board: **ESP32 Dev Module**
3. Select Port: (your ESP32 port)
4. Click **Upload**

## Configuration

### WiFi Settings
The ESP32 creates a unique WiFi access point on first boot:
- **SSID**: `ZS-XXXXXXXX` (where XXXXXXXX is a random 8-digit number)
- **Password**: `12345678`
- **Dashboard URL**: `http://192.168.4.1`

The serial number is stored in flash memory and persists across reboots. It only changes when you reflash the device.

### BLE OBD-II Connection
The dashboard automatically scans for and connects to Bluetooth ELM327 adapters:
- Supports common BLE OBD-II adapters (OBDII, V-LINK, ELM327)
- Auto-reconnects if connection is lost
- Falls back to simulation mode if no adapter found

### Data Simulation
When no OBD-II adapter is connected, the sketch uses built-in data simulation:
- Speed increases/decreases smoothly
- RPM varies with throttle position
- Coolant temp stays in normal range (85-105°C)
- Fuel level decreases over time
- Voltage fluctuates around 14.2V

## API Endpoint

### GET /api/data
Returns JSON with current sensor values:

```json
{
  "speed": 78.5,
  "rpm": 3200,
  "coolant": 92.3,
  "fuel": 68.7,
  "load": 45.2,
  "voltage": 14.1,
  "intake": 28.5,
  "throttle": 42.0,
  "checkEngine": false,
  "oilPressure": false
}
```

## Customization

### Change Gauge Ranges
Edit the `updateGauge()` calls in the HTML:
```javascript
updateGauge('speed-needle', 'speed-arc', data.speed, 0, 200);  // 0-200 km/h
updateGauge('rpm-needle', 'rpm-arc', data.rpm, 0, 8000);       // 0-8000 RPM
```

### Modify Colors
Edit the SVG gradients:
```html
<linearGradient id="grad1">
    <stop offset="0%" stop-color="#00ff88"/>    <!-- Start color -->
    <stop offset="100%" stop-color="#00d9ff"/>  <!-- End color -->
</linearGradient>
```

### Add Real OBD-II Data
Replace the simulated data in `handleData()`:
```cpp
void handleData() {
    // Replace with actual OBD-II readings
    speed = readOBD(0x0D);      // Vehicle speed
    rpm = readOBD(0x0C);        // Engine RPM
    coolantTemp = readOBD(0x05); // Coolant temp
    // ... etc
}
```

## OBD-II Integration

To connect real vehicle data:

1. **Add OBD-II Library**: Use ESP32-OBD-II or similar
2. **Connect Hardware**: ESP32 → OBD-II adapter via UART or CAN
3. **Read PIDs**: Query standard OBD-II PIDs (0x01 mode)
4. **Update Values**: Replace simulated data with real readings

### Common OBD-II PIDs
- `0x0C` - Engine RPM
- `0x0D` - Vehicle Speed
- `0x05` - Coolant Temperature
- `0x2F` - Fuel Level
- `0x04` - Engine Load
- `0x0F` - Intake Air Temperature
- `0x11` - Throttle Position

## Memory Usage

- **Program Storage**: ~957KB / 1310KB (73%)
- **Dynamic Memory**: ~47KB / 327KB (14%)
- **Flash Required**: Minimum 1MB

## Browser Compatibility

- ✅ Chrome/Chromium (Android, Desktop)
- ✅ Firefox
- ✅ Safari (iOS, macOS)
- ✅ Edge
- ✅ WebView (Android apps)

## Use Cases

- Custom car dashboard displays
- OBD-II data visualization
- Vehicle monitoring systems
- Automotive education/training
- DIY instrument clusters
- Race car telemetry displays
- Fleet management dashboards

## Screenshots

### Desktop View
Full 8-gauge layout with warning lights and info panel

### Mobile View
Responsive single-column layout optimized for phones

### Tablet View
Multi-column grid layout perfect for in-car tablets

## Related Projects

- [ZScreen Dash](https://github.com/electronics101clt/ZScreenDash) - Android app for automatic WiFi connection
- [ZScreen ESP Client](https://github.com/electronics101clt/ZScreenESPClient) - Simple ESP32 control interface

## Troubleshooting

### Can't Connect to Dashboard
- Verify ESP32 is powered on
- Connect to WiFi network: **ESP32-Control**
- Navigate to: `http://192.168.4.1`
- Check serial monitor for IP address

### Gauges Not Updating
- Open browser console (F12) and check for errors
- Verify `/api/data` endpoint responds: `http://192.168.4.1/api/data`
- Check JavaScript console for fetch errors

### Upload Failed
- Use slower upload speed: `--upload-field serial.upload_speed=115200`
- Check USB cable (must be data cable, not charge-only)
- Press BOOT button during upload if needed

## License

MIT License - See LICENSE file for details

## Credits

Developed for automotive visualization and OBD-II integration projects.

Built with:
- ESP32 Arduino Core
- HTML5 Canvas & SVG
- CSS3 animations and gradients
- Vanilla JavaScript (no dependencies)

**Inspired by:**
- [Speedometer.js](https://github.com/vladyspavlov/html5-canvas-speedometer)
- [EQM_OBDWEB](https://github.com/EQMOD/EQM_OBDWEB)
- [ElektorLabs OBD2 Dashboard](https://github.com/ElektorLabs/obd2-dashboard)

---

**Note**: This project uses simulated data by default. For real vehicle integration, connect an OBD-II reader or CAN bus module and replace the data simulation code with actual sensor readings.

## Contributing

Pull requests welcome! Areas for improvement:
- Real OBD-II integration examples
- Additional gauge types
- Mobile app companion
- Data logging functionality
- Customizable themes
