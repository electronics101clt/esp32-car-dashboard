#include <WiFi.h>
#include <WebServer.h>

// WiFi AP credentials
const char* ssid = "ESP32-Control";
const char* password = "12345678";

// Web server on port 80
WebServer server(80);

// Simulated sensor data
float speed = 0;
float rpm = 0;
float coolantTemp = 90;
float fuelLevel = 75;
float engineLoad = 45;
float voltage = 14.2;
float intakeTemp = 25;
float throttle = 0;

// HTML page with comprehensive car dashboard
const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Car Dashboard</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: 'Segment7', Arial, sans-serif;
            background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);
            color: #fff;
            overflow-x: hidden;
        }
        .dashboard {
            max-width: 1400px;
            margin: 20px auto;
            padding: 20px;
        }
        .title {
            text-align: center;
            font-size: clamp(24px, 4vw, 42px);
            margin-bottom: 30px;
            color: #00ff88;
            text-shadow: 0 0 20px rgba(0, 255, 136, 0.5);
        }
        .gauge-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 25px;
            margin-bottom: 30px;
        }
        .gauge-container {
            background: linear-gradient(145deg, #16213e, #0f1922);
            border-radius: 20px;
            padding: 20px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
            position: relative;
        }
        .gauge-title {
            text-align: center;
            font-size: clamp(14px, 2vw, 18px);
            margin-bottom: 15px;
            color: #00d9ff;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        .gauge {
            position: relative;
            width: 100%;
            padding-bottom: 100%;
        }
        .gauge-svg {
            position: absolute;
            width: 100%;
            height: 100%;
        }
        .gauge-value {
            position: absolute;
            top: 65%;
            left: 50%;
            transform: translate(-50%, -50%);
            font-size: clamp(28px, 4vw, 48px);
            font-weight: bold;
            color: #00ff88;
            text-shadow: 0 0 15px rgba(0, 255, 136, 0.8);
        }
        .gauge-unit {
            position: absolute;
            top: 78%;
            left: 50%;
            transform: translateX(-50%);
            font-size: clamp(12px, 1.5vw, 16px);
            color: #aaa;
        }
        .warning-lights {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(80px, 1fr));
            gap: 15px;
            margin-top: 30px;
        }
        .warning-light {
            background: #1a1a2e;
            border-radius: 10px;
            padding: 15px;
            text-align: center;
            border: 2px solid #333;
            transition: all 0.3s;
        }
        .warning-light.active {
            border-color: #ff3838;
            box-shadow: 0 0 20px rgba(255, 56, 56, 0.5);
            animation: pulse 1.5s infinite;
        }
        .warning-icon {
            font-size: 24px;
            margin-bottom: 5px;
        }
        .warning-text {
            font-size: 10px;
            color: #aaa;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.6; }
        }
        .info-panel {
            background: linear-gradient(145deg, #16213e, #0f1922);
            border-radius: 20px;
            padding: 20px;
            margin-top: 20px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .info-item {
            display: flex;
            justify-content: space-between;
            padding: 10px;
            border-bottom: 1px solid #333;
        }
        .info-label {
            color: #00d9ff;
        }
        .info-value {
            color: #00ff88;
            font-weight: bold;
        }
        @media (min-width: 768px) {
            .gauge-grid {
                grid-template-columns: repeat(2, 1fr);
            }
        }
        @media (min-width: 1024px) {
            .gauge-grid {
                grid-template-columns: repeat(4, 1fr);
            }
        }
    </style>
</head>
<body>
    <div class="dashboard">
        <h1 class="title">⚡ CAR DASHBOARD ⚡</h1>

        <div class="gauge-grid">
            <!-- Speedometer -->
            <div class="gauge-container">
                <div class="gauge-title">Speed</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="speed-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad1)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#00ff88"/>
                        <line id="speed-needle" x1="100" y1="100" x2="100" y2="40" stroke="#00ff88" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad1">
                                <stop offset="0%" stop-color="#00ff88"/>
                                <stop offset="100%" stop-color="#00d9ff"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="speed-value">0</div>
                    <div class="gauge-unit">km/h</div>
                </div>
            </div>

            <!-- RPM -->
            <div class="gauge-container">
                <div class="gauge-title">RPM</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="rpm-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad2)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#ff3838"/>
                        <line id="rpm-needle" x1="100" y1="100" x2="100" y2="40" stroke="#ff3838" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad2">
                                <stop offset="0%" stop-color="#ff3838"/>
                                <stop offset="100%" stop-color="#ff9500"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="rpm-value" style="color: #ff9500;">0</div>
                    <div class="gauge-unit">x1000</div>
                </div>
            </div>

            <!-- Coolant Temperature -->
            <div class="gauge-container">
                <div class="gauge-title">Coolant Temp</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="coolant-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad3)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#00d9ff"/>
                        <line id="coolant-needle" x1="100" y1="100" x2="100" y2="40" stroke="#00d9ff" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad3">
                                <stop offset="0%" stop-color="#00d9ff"/>
                                <stop offset="100%" stop-color="#ff3838"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="coolant-value" style="color: #00d9ff;">90</div>
                    <div class="gauge-unit">°C</div>
                </div>
            </div>

            <!-- Fuel Level -->
            <div class="gauge-container">
                <div class="gauge-title">Fuel Level</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="fuel-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad4)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#ffeb3b"/>
                        <line id="fuel-needle" x1="100" y1="100" x2="100" y2="40" stroke="#ffeb3b" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad4">
                                <stop offset="0%" stop-color="#ff3838"/>
                                <stop offset="100%" stop-color="#ffeb3b"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="fuel-value" style="color: #ffeb3b;">75</div>
                    <div class="gauge-unit">%</div>
                </div>
            </div>

            <!-- Engine Load -->
            <div class="gauge-container">
                <div class="gauge-title">Engine Load</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="load-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad5)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#9c27b0"/>
                        <line id="load-needle" x1="100" y1="100" x2="100" y2="40" stroke="#9c27b0" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad5">
                                <stop offset="0%" stop-color="#9c27b0"/>
                                <stop offset="100%" stop-color="#ff3838"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="load-value" style="color: #9c27b0;">45</div>
                    <div class="gauge-unit">%</div>
                </div>
            </div>

            <!-- Battery Voltage -->
            <div class="gauge-container">
                <div class="gauge-title">Battery</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="voltage-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad6)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#00ff88"/>
                        <line id="voltage-needle" x1="100" y1="100" x2="100" y2="40" stroke="#00ff88" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad6">
                                <stop offset="0%" stop-color="#ff3838"/>
                                <stop offset="100%" stop-color="#00ff88"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="voltage-value">14.2</div>
                    <div class="gauge-unit">V</div>
                </div>
            </div>

            <!-- Intake Temperature -->
            <div class="gauge-container">
                <div class="gauge-title">Intake Temp</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="intake-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad7)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#03a9f4"/>
                        <line id="intake-needle" x1="100" y1="100" x2="100" y2="40" stroke="#03a9f4" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad7">
                                <stop offset="0%" stop-color="#03a9f4"/>
                                <stop offset="100%" stop-color="#ff9500"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="intake-value" style="color: #03a9f4;">25</div>
                    <div class="gauge-unit">°C</div>
                </div>
            </div>

            <!-- Throttle Position -->
            <div class="gauge-container">
                <div class="gauge-title">Throttle</div>
                <div class="gauge">
                    <svg class="gauge-svg" viewBox="0 0 200 200">
                        <path d="M 30 170 A 85 85 0 1 1 170 170" fill="none" stroke="#222" stroke-width="20" stroke-linecap="round"/>
                        <path id="throttle-arc" d="M 30 170 A 85 85 0 0 1 100 15" fill="none" stroke="url(#grad8)" stroke-width="20" stroke-linecap="round"/>
                        <circle cx="100" cy="100" r="8" fill="#4caf50"/>
                        <line id="throttle-needle" x1="100" y1="100" x2="100" y2="40" stroke="#4caf50" stroke-width="3" stroke-linecap="round"/>
                        <defs>
                            <linearGradient id="grad8">
                                <stop offset="0%" stop-color="#4caf50"/>
                                <stop offset="100%" stop-color="#ffeb3b"/>
                            </linearGradient>
                        </defs>
                    </svg>
                    <div class="gauge-value" id="throttle-value" style="color: #4caf50;">0</div>
                    <div class="gauge-unit">%</div>
                </div>
            </div>
        </div>

        <!-- Warning Lights -->
        <div class="warning-lights">
            <div class="warning-light" id="check-engine">
                <div class="warning-icon">⚠️</div>
                <div class="warning-text">CHECK ENGINE</div>
            </div>
            <div class="warning-light" id="oil-pressure">
                <div class="warning-icon">🛢️</div>
                <div class="warning-text">OIL</div>
            </div>
            <div class="warning-light" id="battery-warn">
                <div class="warning-icon">🔋</div>
                <div class="warning-text">BATTERY</div>
            </div>
            <div class="warning-light" id="brake-warn">
                <div class="warning-icon">🅿️</div>
                <div class="warning-text">BRAKE</div>
            </div>
            <div class="warning-light" id="abs-warn">
                <div class="warning-icon">ABS</div>
                <div class="warning-text">ABS</div>
            </div>
            <div class="warning-light" id="airbag-warn">
                <div class="warning-icon">🎈</div>
                <div class="warning-text">AIRBAG</div>
            </div>
        </div>

        <!-- Info Panel -->
        <div class="info-panel">
            <div class="info-item">
                <span class="info-label">Odometer:</span>
                <span class="info-value">125,478 km</span>
            </div>
            <div class="info-item">
                <span class="info-label">Trip:</span>
                <span class="info-value">542.3 km</span>
            </div>
            <div class="info-item">
                <span class="info-label">Avg Speed:</span>
                <span class="info-value">62 km/h</span>
            </div>
            <div class="info-item">
                <span class="info-label">Fuel Consumption:</span>
                <span class="info-value">7.2 L/100km</span>
            </div>
        </div>
    </div>

    <script>
        // Update gauge needle rotation
        function updateGauge(needleId, arcId, value, min, max) {
            const percentage = Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100));
            const angle = -135 + (percentage * 2.7); // -135° to 135° range

            const needle = document.getElementById(needleId);
            if (needle) {
                needle.style.transform = `rotate(${angle}deg)`;
                needle.style.transformOrigin = '100px 100px';
            }

            // Update arc
            const arc = document.getElementById(arcId);
            if (arc) {
                const endAngle = -135 + (percentage * 2.7);
                const radians = (endAngle * Math.PI) / 180;
                const x = 100 + 85 * Math.cos(radians);
                const y = 100 + 85 * Math.sin(radians);
                const largeArc = percentage > 50 ? 1 : 0;
                arc.setAttribute('d', `M 30 170 A 85 85 0 ${largeArc} 1 ${x} ${y}`);
            }
        }

        // Fetch data from ESP32
        async function updateData() {
            try {
                const response = await fetch('/api/data');
                const data = await response.json();

                // Update gauges
                updateGauge('speed-needle', 'speed-arc', data.speed, 0, 200);
                updateGauge('rpm-needle', 'rpm-arc', data.rpm, 0, 8000);
                updateGauge('coolant-needle', 'coolant-arc', data.coolant, 50, 130);
                updateGauge('fuel-needle', 'fuel-arc', data.fuel, 0, 100);
                updateGauge('load-needle', 'load-arc', data.load, 0, 100);
                updateGauge('voltage-needle', 'voltage-arc', data.voltage, 10, 16);
                updateGauge('intake-needle', 'intake-arc', data.intake, 0, 100);
                updateGauge('throttle-needle', 'throttle-arc', data.throttle, 0, 100);

                // Update values
                document.getElementById('speed-value').textContent = Math.round(data.speed);
                document.getElementById('rpm-value').textContent = (data.rpm / 1000).toFixed(1);
                document.getElementById('coolant-value').textContent = Math.round(data.coolant);
                document.getElementById('fuel-value').textContent = Math.round(data.fuel);
                document.getElementById('load-value').textContent = Math.round(data.load);
                document.getElementById('voltage-value').textContent = data.voltage.toFixed(1);
                document.getElementById('intake-value').textContent = Math.round(data.intake);
                document.getElementById('throttle-value').textContent = Math.round(data.throttle);

                // Update warning lights
                document.getElementById('check-engine').classList.toggle('active', data.checkEngine);
                document.getElementById('oil-pressure').classList.toggle('active', data.oilPressure);
                document.getElementById('battery-warn').classList.toggle('active', data.voltage < 12);
            } catch (error) {
                console.error('Error fetching data:', error);
            }
        }

        // Initial update and set interval
        updateData();
        setInterval(updateData, 500); // Update every 500ms
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", html);
}

void handleData() {
    // Simulate data changes
    speed += (random(-10, 20) / 10.0);
    speed = constrain(speed, 0, 180);

    rpm += (random(-100, 200));
    rpm = constrain(rpm, 600, 7000);

    throttle = map(rpm, 600, 7000, 0, 100);
    engineLoad = throttle * 0.8;

    coolantTemp += (random(-1, 2) / 10.0);
    coolantTemp = constrain(coolantTemp, 85, 105);

    fuelLevel -= 0.01;
    if (fuelLevel < 10) fuelLevel = 100;

    voltage = 14.2 + (random(-5, 5) / 10.0);

    intakeTemp = 25 + (engineLoad * 0.3);

    // Send JSON response
    String json = "{";
    json += "\"speed\":" + String(speed, 1) + ",";
    json += "\"rpm\":" + String(rpm, 0) + ",";
    json += "\"coolant\":" + String(coolantTemp, 1) + ",";
    json += "\"fuel\":" + String(fuelLevel, 1) + ",";
    json += "\"load\":" + String(engineLoad, 1) + ",";
    json += "\"voltage\":" + String(voltage, 1) + ",";
    json += "\"intake\":" + String(intakeTemp, 1) + ",";
    json += "\"throttle\":" + String(throttle, 1) + ",";
    json += "\"checkEngine\":false,";
    json += "\"oilPressure\":false";
    json += "}";

    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting ESP32 Car Dashboard...");

    // Set up WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);

    // Set up web server routes
    server.on("/", handleRoot);
    server.on("/api/data", handleData);

    server.begin();
    Serial.println("Car Dashboard server started!");
}

void loop() {
    server.handleClient();
}
