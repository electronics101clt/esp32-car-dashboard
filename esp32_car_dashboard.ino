#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

// WiFi AP credentials
const char* ssid = "ESP32-Control";
const char* password = "12345678";

// BLE ELM327 - common UUIDs
static BLEUUID serviceUUID("fff0");
static BLEUUID charTXUUID("fff1");  // Write to ELM
static BLEUUID charRXUUID("fff2");  // Read from ELM (notify)

BLEScan* pBLEScan;
BLEClient* pClient;
BLERemoteCharacteristic* pTXChar;
BLERemoteCharacteristic* pRXChar;
bool obdConnected = false;
unsigned long lastOBDQuery = 0;
int currentPID = 0;
String bleResponse = "";

// Web server on port 80
WebServer server(80);

// OBD-II data
float speed = 0;
float rpm = 0;
float coolantTemp = 0;
float fuelLevel = 0;
float engineLoad = 0;
float voltage = 0;
float intakeTemp = 0;
float throttle = 0;
bool checkEngine = false;
bool oilPressure = false;

// Minimal HTML dashboard
const char* html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dashboard</title><style>
*{margin:0;padding:0;box-sizing:border-box}body{background:#111;color:#fff;font-family:sans-serif}
.g{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;padding:10px}
.c{background:#1a1a1a;border-radius:8px;padding:15px;text-align:center}
.v{font-size:28px;font-weight:bold}.u{font-size:11px;color:#666}.l{font-size:11px;color:#888;margin-top:5px}
.c:nth-child(1) .v{color:#0f6}.c:nth-child(2) .v{color:#f55}.c:nth-child(3) .v{color:#4cf}
.c:nth-child(4) .v{color:#fd0}.c:nth-child(5) .v{color:#e4f}.c:nth-child(6) .v{color:#6fa}
.c:nth-child(7) .v{color:#48f}.c:nth-child(8) .v{color:#fa4}
.w{display:flex;gap:8px;justify-content:center;padding:10px;flex-wrap:wrap}
.w span{background:#222;padding:6px 12px;border-radius:4px;font-size:10px;color:#555}
.w span.on{background:#f22;color:#fff}
.s{text-align:center;padding:8px;font-size:11px;color:#555}
@media(max-width:600px){.g{grid-template-columns:repeat(2,1fr)}}
</style></head><body>
<div class="g">
<div class="c"><div class="v" id="spd">0</div><div class="u">km/h</div><div class="l">SPEED</div></div>
<div class="c"><div class="v" id="rpm">0</div><div class="u">x1000</div><div class="l">RPM</div></div>
<div class="c"><div class="v" id="tmp">0</div><div class="u">C</div><div class="l">COOLANT</div></div>
<div class="c"><div class="v" id="fue">0</div><div class="u">%</div><div class="l">FUEL</div></div>
<div class="c"><div class="v" id="lod">0</div><div class="u">%</div><div class="l">LOAD</div></div>
<div class="c"><div class="v" id="vlt">0</div><div class="u">V</div><div class="l">BATTERY</div></div>
<div class="c"><div class="v" id="int">0</div><div class="u">C</div><div class="l">INTAKE</div></div>
<div class="c"><div class="v" id="thr">0</div><div class="u">%</div><div class="l">THROTTLE</div></div>
</div>
<div class="w"><span id="ce">CHECK</span><span id="oil">OIL</span><span id="bat">BATT</span></div>
<div class="s" id="st">OBD: --</div>
<script>
async function u(){try{const r=await fetch('/api/data'),d=await r.json();
document.getElementById('spd').textContent=Math.round(d.speed);
document.getElementById('rpm').textContent=(d.rpm/1000).toFixed(1);
document.getElementById('tmp').textContent=Math.round(d.coolant);
document.getElementById('fue').textContent=Math.round(d.fuel);
document.getElementById('lod').textContent=Math.round(d.load);
document.getElementById('vlt').textContent=d.voltage.toFixed(1);
document.getElementById('int').textContent=Math.round(d.intake);
document.getElementById('thr').textContent=Math.round(d.throttle);
document.getElementById('ce').className=d.checkEngine?'on':'';
document.getElementById('oil').className=d.oilPressure?'on':'';
document.getElementById('bat').className=d.voltage<12?'on':'';
document.getElementById('st').textContent='OBD: '+(d.obdConnected?'Connected':'Disconnected');
}catch(e){}}u();setInterval(u,500);
</script></body></html>
)rawliteral";

// BLE notification callback
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    for (int i = 0; i < length; i++) {
        char c = (char)pData[i];
        if (c == '>') {
            // Response complete
        } else if (c != '\r' && c != '\n') {
            bleResponse += c;
        }
    }
}

bool connectToELM() {
    Serial.println("Scanning for BLE ELM327...");
    BLEScanResults* results = pBLEScan->start(5, false);

    for (int i = 0; i < results->getCount(); i++) {
        BLEAdvertisedDevice device = results->getDevice(i);
        String name = device.getName().c_str();
        Serial.print("Found: ");
        Serial.println(name);

        // Look for OBDII or similar names
        if (name.indexOf("OBD") >= 0 || name.indexOf("ELM") >= 0 ||
            name.indexOf("V-LINK") >= 0 || name.indexOf("IOS-") >= 0) {
            Serial.println("Connecting to ELM327...");

            pClient = BLEDevice::createClient();
            if (pClient->connect(&device)) {
                Serial.println("Connected!");

                // Try to find service
                BLERemoteService* pService = pClient->getService(serviceUUID);
                if (pService == nullptr) {
                    // Try alternate UUID
                    pService = pClient->getService(BLEUUID("ffe0"));
                }
                if (pService == nullptr) {
                    Serial.println("Service not found");
                    pClient->disconnect();
                    continue;
                }

                // Get characteristics
                pTXChar = pService->getCharacteristic(charTXUUID);
                if (pTXChar == nullptr) {
                    pTXChar = pService->getCharacteristic(BLEUUID("ffe1"));
                }
                pRXChar = pService->getCharacteristic(charRXUUID);
                if (pRXChar == nullptr) {
                    pRXChar = pService->getCharacteristic(BLEUUID("ffe1"));
                }

                if (pTXChar && pRXChar) {
                    if (pRXChar->canNotify()) {
                        pRXChar->registerForNotify(notifyCallback);
                    }
                    pBLEScan->stop();
                    return true;
                }
            }
        }
    }
    pBLEScan->clearResults();
    return false;
}

String sendELM(String cmd) {
    if (!obdConnected || !pTXChar) return "";

    bleResponse = "";
    String cmdWithCR = cmd + "\r";
    pTXChar->writeValue((uint8_t*)cmdWithCR.c_str(), cmdWithCR.length());

    delay(500);  // Wait for response

    String resp = bleResponse;
    resp.trim();
    return resp;
}

void initELM() {
    Serial.println("Initializing ELM327...");
    sendELM("ATZ");    delay(1000);
    sendELM("ATE0");
    sendELM("ATL0");
    sendELM("ATS0");
    sendELM("ATH0");
    sendELM("ATSP0");
    Serial.println("ELM327 initialized");
}

int parseHex(String s, int start, int len) {
    if (start + len > s.length()) return 0;
    String hex = s.substring(start, start + len);
    return (int)strtol(hex.c_str(), NULL, 16);
}

void queryOBD() {
    if (!obdConnected) return;

    String resp;
    switch (currentPID) {
        case 0:
            resp = sendELM("010D");
            if (resp.indexOf("410D") >= 0) {
                int idx = resp.indexOf("410D") + 4;
                speed = parseHex(resp, idx, 2);
            }
            break;
        case 1:
            resp = sendELM("010C");
            if (resp.indexOf("410C") >= 0) {
                int idx = resp.indexOf("410C") + 4;
                int a = parseHex(resp, idx, 2);
                int b = parseHex(resp, idx + 2, 2);
                rpm = ((a * 256) + b) / 4.0;
            }
            break;
        case 2:
            resp = sendELM("0105");
            if (resp.indexOf("4105") >= 0) {
                int idx = resp.indexOf("4105") + 4;
                coolantTemp = parseHex(resp, idx, 2) - 40;
            }
            break;
        case 3:
            resp = sendELM("012F");
            if (resp.indexOf("412F") >= 0) {
                int idx = resp.indexOf("412F") + 4;
                fuelLevel = parseHex(resp, idx, 2) * 100.0 / 255.0;
            }
            break;
        case 4:
            resp = sendELM("0104");
            if (resp.indexOf("4104") >= 0) {
                int idx = resp.indexOf("4104") + 4;
                engineLoad = parseHex(resp, idx, 2) * 100.0 / 255.0;
            }
            break;
        case 5:
            resp = sendELM("0142");
            if (resp.indexOf("4142") >= 0) {
                int idx = resp.indexOf("4142") + 4;
                int a = parseHex(resp, idx, 2);
                int b = parseHex(resp, idx + 2, 2);
                voltage = ((a * 256) + b) / 1000.0;
            }
            break;
        case 6:
            resp = sendELM("010F");
            if (resp.indexOf("410F") >= 0) {
                int idx = resp.indexOf("410F") + 4;
                intakeTemp = parseHex(resp, idx, 2) - 40;
            }
            break;
        case 7:
            resp = sendELM("0111");
            if (resp.indexOf("4111") >= 0) {
                int idx = resp.indexOf("4111") + 4;
                throttle = parseHex(resp, idx, 2) * 100.0 / 255.0;
            }
            break;
    }
    currentPID = (currentPID + 1) % 8;
}

void handleRoot() {
    server.send(200, "text/html", html);
}

void handleData() {
    String json = "{";
    json += "\"speed\":" + String(speed, 1) + ",";
    json += "\"rpm\":" + String(rpm, 0) + ",";
    json += "\"coolant\":" + String(coolantTemp, 1) + ",";
    json += "\"fuel\":" + String(fuelLevel, 1) + ",";
    json += "\"load\":" + String(engineLoad, 1) + ",";
    json += "\"voltage\":" + String(voltage, 1) + ",";
    json += "\"intake\":" + String(intakeTemp, 1) + ",";
    json += "\"throttle\":" + String(throttle, 1) + ",";
    json += "\"checkEngine\":" + String(checkEngine ? "true" : "false") + ",";
    json += "\"oilPressure\":" + String(oilPressure ? "true" : "false") + ",";
    json += "\"obdConnected\":" + String(obdConnected ? "true" : "false");
    json += "}";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(9600);
    delay(1000);

    Serial.println();
    Serial.println("Starting ESP32 Car Dashboard (BLE)...");

    // Init BLE
    BLEDevice::init("ESP32-Dashboard");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Try to connect
    if (connectToELM()) {
        obdConnected = true;
        initELM();
    } else {
        Serial.println("ELM327 not found, will retry...");
    }

    // Set up WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("SSID: ");
    Serial.println(ssid);

    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.begin();
    Serial.println("Dashboard started!");
}

void loop() {
    server.handleClient();

    // Reconnect if needed
    if (!obdConnected && millis() - lastOBDQuery > 15000) {
        Serial.println("Scanning for ELM327...");
        if (connectToELM()) {
            obdConnected = true;
            initELM();
        }
        lastOBDQuery = millis();
    }

    // Query OBD
    if (obdConnected && millis() - lastOBDQuery > 200) {
        queryOBD();
        lastOBDQuery = millis();
    }
}
