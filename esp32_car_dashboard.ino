#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <Preferences.h>

// Persistent serial number
Preferences preferences;
uint32_t serialNumber;
char ssid[32];
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

// Realistic analog gauges with canvas rendering
const char* html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dashboard</title><style>
*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#fff;font-family:Arial;overflow:hidden}
.d{display:flex;flex-wrap:wrap;gap:10px;padding:10px;justify-content:center}
canvas{background:radial-gradient(circle at 50% 50%,#1a1a1a,#000);border-radius:10px;border:2px solid #222}
.w{display:flex;gap:10px;justify-content:center;padding:10px;position:fixed;bottom:0;width:100%;background:#111}
.w span{padding:6px 12px;border-radius:5px;font-size:10px;background:#222;color:#555;border:1px solid #333}
.w span.on{background:#f22;color:#fff;box-shadow:0 0 10px #f22}
@media(max-width:600px){canvas{width:140px!important;height:140px!important}}
</style></head><body>
<div class="d">
<canvas id="c1" width="280" height="280"></canvas>
<canvas id="c2" width="280" height="280"></canvas>
<canvas id="c3" width="140" height="140"></canvas>
<canvas id="c4" width="140" height="140"></canvas>
<canvas id="c5" width="140" height="140"></canvas>
<canvas id="c6" width="140" height="140"></canvas>
<canvas id="c7" width="140" height="140"></canvas>
<canvas id="c8" width="140" height="140"></canvas>
</div>
<div class="w"><span id="ce">CHECK</span><span id="oil">OIL</span><span id="bat">BATT</span><span id="st">--</span></div>
<script>
function drawGauge(ctx,w,h,val,min,max,label,color,ticks){ctx.clearRect(0,0,w,h);const cx=w/2,cy=h/2,r=Math.min(w,h)/2-20;ctx.fillStyle='#0a0a0a';ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#1a1a1a';ctx.lineWidth=3;ctx.stroke();const sa=Math.PI*0.75,ea=Math.PI*2.25,range=ea-sa;for(let i=0;i<=ticks;i++){const a=sa+range*i/ticks;const cos=Math.cos(a),sin=Math.sin(a);const isM=i%(ticks/5)==0;ctx.strokeStyle='#444';ctx.lineWidth=isM?3:1;const len=isM?15:8;ctx.beginPath();ctx.moveTo(cx+cos*(r-5),cy+sin*(r-5));ctx.lineTo(cx+cos*(r-5-len),cy+sin*(r-5-len));ctx.stroke();if(isM){ctx.fillStyle='#888';ctx.font=(w>200?14:9)+'px Arial';ctx.textAlign='center';ctx.textBaseline='middle';const num=min+i*(max-min)/ticks;ctx.fillText(num,cx+cos*(r-25),cy+sin*(r-25));}}const p=(val-min)/(max-min);const aa=sa+range*Math.max(0,Math.min(1,p));ctx.strokeStyle=color;ctx.lineWidth=4;ctx.beginPath();ctx.arc(cx,cy,r-10,sa,aa);ctx.stroke();ctx.save();ctx.translate(cx,cy);ctx.rotate(aa);ctx.strokeStyle=color;ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(r-15,0);ctx.stroke();ctx.fillStyle=color;ctx.beginPath();ctx.arc(0,0,6,0,Math.PI*2);ctx.fill();ctx.restore();ctx.fillStyle='#fff';ctx.font=(w>200?12:8)+'px Arial';ctx.textAlign='center';ctx.fillText(label,cx,cy-r+15);ctx.font=(w>200?24:16)+'px Arial';ctx.fillStyle=color;ctx.fillText(Math.round(val),cx,cy+r-20);}
const cfg=[
{id:'c1',min:0,max:140,label:'SPEED MPH',color:'#0f6',ticks:14},
{id:'c2',min:0,max:8,label:'RPM x1000',color:'#f55',ticks:8},
{id:'c3',min:120,max:270,label:'COOLANT F',color:'#4cf',ticks:15},
{id:'c4',min:0,max:100,label:'FUEL',color:'#fd0',ticks:10},
{id:'c5',min:0,max:100,label:'LOAD',color:'#e4f',ticks:10},
{id:'c6',min:10,max:16,label:'VOLTS',color:'#6fa',ticks:6},
{id:'c7',min:30,max:210,label:'INTAKE F',color:'#48f',ticks:18},
{id:'c8',min:0,max:100,label:'THROTTLE',color:'#fa4',ticks:10}
];
const ctxs=cfg.map(c=>{const el=document.getElementById(c.id);return{ctx:el.getContext('2d'),w:el.width,h:el.height,...c}});
async function u(){try{const r=await fetch('/api/data'),d=await r.json();
const mph=d.speed*0.621371;const coolF=d.coolant*9/5+32;const intF=d.intake*9/5+32;
drawGauge(ctxs[0].ctx,ctxs[0].w,ctxs[0].h,mph,0,140,'SPEED MPH','#0f6',14);
drawGauge(ctxs[1].ctx,ctxs[1].w,ctxs[1].h,d.rpm/1000,0,8,'RPM x1000','#f55',8);
drawGauge(ctxs[2].ctx,ctxs[2].w,ctxs[2].h,coolF,120,270,'COOLANT F','#4cf',15);
drawGauge(ctxs[3].ctx,ctxs[3].w,ctxs[3].h,d.fuel,0,100,'FUEL','#fd0',10);
drawGauge(ctxs[4].ctx,ctxs[4].w,ctxs[4].h,d.load,0,100,'LOAD','#e4f',10);
drawGauge(ctxs[5].ctx,ctxs[5].w,ctxs[5].h,d.voltage,10,16,'VOLTS','#6fa',6);
drawGauge(ctxs[6].ctx,ctxs[6].w,ctxs[6].h,intF,30,210,'INTAKE F','#48f',18);
drawGauge(ctxs[7].ctx,ctxs[7].w,ctxs[7].h,d.throttle,0,100,'THROTTLE','#fa4',10);
document.getElementById('ce').className=d.checkEngine?'on':'';
document.getElementById('oil').className=d.oilPressure?'on':'';
document.getElementById('bat').className=d.voltage<12?'on':'';
document.getElementById('st').textContent='OBD:'+(d.obdConnected?'OK':'--');
}catch(e){}}u();setInterval(u,500);
</script></body></html>
)rawliteral";

// BLE notification callback
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("BLE RX (");
    Serial.print(length);
    Serial.print("): ");
    for (int i = 0; i < length; i++) {
        char c = (char)pData[i];
        Serial.print(c);
        if (c == '>') {
            // Response complete
        } else if (c != '\r' && c != '\n') {
            bleResponse += c;
        }
    }
    Serial.println();
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

                // Print all services and characteristics for debugging
                std::map<std::string, BLERemoteService*>* services = pClient->getServices();
                Serial.println("=== Available Services ===");
                for (auto& s : *services) {
                    Serial.print("Service: ");
                    Serial.println(s.second->getUUID().toString().c_str());
                    std::map<std::string, BLERemoteCharacteristic*>* chars = s.second->getCharacteristics();
                    for (auto& c : *chars) {
                        Serial.print("  Char: ");
                        Serial.print(c.second->getUUID().toString().c_str());
                        Serial.print(" [");
                        if (c.second->canRead()) Serial.print("R");
                        if (c.second->canWrite()) Serial.print("W");
                        if (c.second->canNotify()) Serial.print("N");
                        if (c.second->canIndicate()) Serial.print("I");
                        Serial.println("]");
                    }
                }
                Serial.println("=========================");

                // Try to find service
                BLERemoteService* pService = pClient->getService(serviceUUID);
                if (pService == nullptr) {
                    // Try alternate UUID
                    pService = pClient->getService(BLEUUID("ffe0"));
                }
                if (pService == nullptr) {
                    Serial.println("Service not found, trying first available...");
                    // Use first available service if standard ones not found
                    for (auto& s : *services) {
                        pService = s.second;
                        Serial.print("Using service: ");
                        Serial.println(pService->getUUID().toString().c_str());
                        break;
                    }
                }
                if (pService == nullptr) {
                    Serial.println("No services available");
                    pClient->disconnect();
                    continue;
                }

                // Get characteristics - try multiple known UUIDs
                std::map<std::string, BLERemoteCharacteristic*>* chars = pService->getCharacteristics();
                for (auto& c : *chars) {
                    BLERemoteCharacteristic* ch = c.second;
                    if (ch->canWrite() && pTXChar == nullptr) {
                        pTXChar = ch;
                        Serial.print("TX Char: ");
                        Serial.println(ch->getUUID().toString().c_str());
                    }
                    if ((ch->canNotify() || ch->canIndicate() || ch->canRead()) && pRXChar == nullptr) {
                        pRXChar = ch;
                        Serial.print("RX Char: ");
                        Serial.println(ch->getUUID().toString().c_str());
                    }
                }

                if (pTXChar && pRXChar) {
                    Serial.println("TX and RX characteristics found");
                    if (pRXChar->canNotify()) {
                        Serial.println("Registering for notifications...");
                        pRXChar->registerForNotify(notifyCallback);
                        Serial.println("Notifications registered");
                    } else if (pRXChar->canIndicate()) {
                        Serial.println("Using indications instead...");
                        pRXChar->registerForNotify(notifyCallback, false);
                    } else {
                        Serial.println("RX polling mode (no notify/indicate)");
                    }
                    pBLEScan->stop();
                    return true;
                } else {
                    Serial.print("Missing chars - TX:");
                    Serial.print(pTXChar ? "OK" : "NULL");
                    Serial.print(" RX:");
                    Serial.println(pRXChar ? "OK" : "NULL");
                }
            }
        }
    }
    pBLEScan->clearResults();
    return false;
}

String sendELM(String cmd) {
    if (!obdConnected || !pTXChar) {
        Serial.println("Not connected!");
        return "";
    }

    bleResponse = "";
    String cmdWithCR = cmd + "\r";
    Serial.print("TX: ");
    Serial.println(cmd);
    pTXChar->writeValue((uint8_t*)cmdWithCR.c_str(), cmdWithCR.length());

    unsigned long start = millis();
    while (millis() - start < 2000) {
        delay(100);
        // Try reading directly if notifications not working
        if (pRXChar && pRXChar->canRead()) {
            String val = pRXChar->readValue();
            if (val.length() > 0) {
                for (int i = 0; i < val.length(); i++) {
                    char c = val[i];
                    if (c != '\r' && c != '\n') {
                        bleResponse += c;
                    }
                }
            }
        }
        if (bleResponse.length() > 0 && bleResponse.indexOf(">") >= 0) break;
    }

    Serial.print("Response: [");
    Serial.print(bleResponse);
    Serial.println("]");

    String resp = bleResponse;
    resp.trim();
    return resp;
}

void initELM() {
    Serial.println("Initializing ELM327...");
    sendELM("ATZ");    delay(1500);  // Reset - need longer delay
    sendELM("ATE0");   delay(100);   // Echo off
    sendELM("ATL0");   delay(100);   // Linefeeds off
    sendELM("ATS0");   delay(100);   // Spaces off
    sendELM("ATH0");   delay(100);   // Headers off
    sendELM("ATSP0");  delay(100);   // Auto protocol
    sendELM("ATST64"); delay(100);   // Set timeout (100 * 4ms = 400ms)
    // Try to connect to vehicle
    String resp = sendELM("0100");   // Query supported PIDs
    Serial.print("PIDs supported: ");
    Serial.println(resp);
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
            Serial.print("SPD: "); Serial.println(resp);
            if (resp.indexOf("410D") >= 0) {
                int idx = resp.indexOf("410D") + 4;
                speed = parseHex(resp, idx, 2);
            }
            break;
        case 1:
            resp = sendELM("010C");
            Serial.print("RPM: "); Serial.println(resp);
            if (resp.indexOf("410C") >= 0) {
                int idx = resp.indexOf("410C") + 4;
                int a = parseHex(resp, idx, 2);
                int b = parseHex(resp, idx + 2, 2);
                rpm = ((a * 256) + b) / 4.0;
            }
            break;
        case 2:
            resp = sendELM("0105");
            Serial.print("TMP: "); Serial.println(resp);
            if (resp.indexOf("4105") >= 0) {
                int idx = resp.indexOf("4105") + 4;
                coolantTemp = parseHex(resp, idx, 2) - 40;
            }
            break;
        case 3:
            resp = sendELM("012F");
            Serial.print("FUE: "); Serial.println(resp);
            if (resp.indexOf("412F") >= 0) {
                int idx = resp.indexOf("412F") + 4;
                fuelLevel = parseHex(resp, idx, 2) * 100.0 / 255.0;
            }
            break;
        case 4:
            resp = sendELM("0104");
            Serial.print("LOD: "); Serial.println(resp);
            if (resp.indexOf("4104") >= 0) {
                int idx = resp.indexOf("4104") + 4;
                engineLoad = parseHex(resp, idx, 2) * 100.0 / 255.0;
            }
            break;
        case 5:
            resp = sendELM("0142");
            Serial.print("VLT: "); Serial.println(resp);
            if (resp.indexOf("4142") >= 0) {
                int idx = resp.indexOf("4142") + 4;
                int a = parseHex(resp, idx, 2);
                int b = parseHex(resp, idx + 2, 2);
                voltage = ((a * 256) + b) / 1000.0;
            }
            break;
        case 6:
            resp = sendELM("010F");
            Serial.print("INT: "); Serial.println(resp);
            if (resp.indexOf("410F") >= 0) {
                int idx = resp.indexOf("410F") + 4;
                intakeTemp = parseHex(resp, idx, 2) - 40;
            }
            break;
        case 7:
            resp = sendELM("0111");
            Serial.print("THR: "); Serial.println(resp);
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
    Serial.println("API request");
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
    Serial.println("Starting ZS Car Dashboard (BLE)...");

    // Load or generate serial number (persistent across reboots)
    preferences.begin("zs-dash", false);
    serialNumber = preferences.getUInt("serial", 0);
    if (serialNumber == 0) {
        // First boot - generate and save new serial
        serialNumber = esp_random() % 100000000;  // 0-99999999 (8 digits)
        preferences.putUInt("serial", serialNumber);
        Serial.print("Generated new serial: ");
        Serial.println(serialNumber);
    } else {
        Serial.print("Loaded serial: ");
        Serial.println(serialNumber);
    }
    preferences.end();
    snprintf(ssid, sizeof(ssid), "ZS-%08u", serialNumber);

    // Set up WiFi AP FIRST to ensure hotspot is always available
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("SSID: ");
    Serial.println(ssid);

    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.begin();
    Serial.println("Web server started!");

    // Init BLE after WiFi is running
    char bleName[32];
    snprintf(bleName, sizeof(bleName), "ZS-Dash-%04X", serialNumber & 0xFFFF);
    BLEDevice::init(bleName);
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Try to connect to OBD
    Serial.println("Scanning for BLE ELM327...");
    if (connectToELM()) {
        obdConnected = true;
        initELM();
    } else {
        Serial.println("ELM327 not found, will retry...");
    }

    Serial.print("ZS Dashboard started! Serial: ");
    Serial.println(serialNumber, HEX);
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
