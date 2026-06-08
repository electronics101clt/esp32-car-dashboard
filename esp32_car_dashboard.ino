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

// Professional car dashboard with animated needle gauges
const char* html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Car Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;color:#fff;font-family:Arial,sans-serif;overflow-x:hidden}
.dash{display:flex;flex-wrap:wrap;gap:15px;padding:20px;justify-content:center;align-items:center}
.gauge{position:relative}
.gauge canvas{display:block}
.gauge-label{position:absolute;top:10px;left:50%;transform:translateX(-50%);font-size:11px;font-weight:bold;letter-spacing:1px;color:#aaa;text-shadow:0 0 5px rgba(0,0,0,0.8)}
.gauge-value{position:absolute;bottom:25%;left:50%;transform:translateX(-50%);font-size:24px;font-weight:bold;color:#fff;text-shadow:0 0 10px rgba(255,255,255,0.5)}
.gauge-unit{position:absolute;bottom:20%;left:50%;transform:translateX(-50%);font-size:10px;color:#888}
.warnings{display:flex;gap:15px;justify-content:center;padding:20px;position:fixed;bottom:0;width:100%;background:rgba(17,17,17,0.95);backdrop-filter:blur(10px);border-top:1px solid #333}
.warn{padding:8px 16px;border-radius:6px;font-size:11px;font-weight:bold;background:#222;color:#666;border:2px solid #333;transition:all 0.3s;cursor:pointer}
.warn.on{background:#d00;color:#fff;border-color:#f44;box-shadow:0 0 20px rgba(255,68,68,0.6);animation:pulse 1s infinite}
.modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.9);z-index:1000;justify-content:center;align-items:center}
.modal-content{background:#222;padding:30px;border-radius:10px;max-width:500px;width:90%;border:2px solid #f44;box-shadow:0 0 30px rgba(255,68,68,0.5)}
.modal-title{font-size:20px;font-weight:bold;color:#f44;margin-bottom:20px;text-align:center}
.modal-close{position:absolute;top:15px;right:20px;font-size:30px;color:#888;cursor:pointer;transition:color 0.3s}
.modal-close:hover{color:#fff}
.dtc-list{list-style:none;max-height:300px;overflow-y:auto}
.dtc-item{padding:10px;margin:8px 0;background:#333;border-left:4px solid #f44;border-radius:4px;font-family:monospace;font-size:14px}
.dtc-empty{text-align:center;color:#888;padding:20px;font-size:14px}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.7}}
@media(max-width:768px){.gauge canvas{width:120px!important;height:120px!important}.gauge-value{font-size:18px}}
</style></head><body>
<div class="dash">
<div class="gauge"><canvas id="g1" width="280" height="280"></canvas><div class="gauge-label">SPEED</div><div class="gauge-value" id="v1">0</div><div class="gauge-unit">MPH</div></div>
<div class="gauge"><canvas id="g2" width="280" height="280"></canvas><div class="gauge-label">RPM</div><div class="gauge-value" id="v2">0</div></div>
<div class="gauge"><canvas id="g3" width="150" height="150"></canvas><div class="gauge-label">COOLANT</div><div class="gauge-value" id="v3">0</div><div class="gauge-unit">°F</div></div>
<div class="gauge"><canvas id="g4" width="150" height="150"></canvas><div class="gauge-label">FUEL</div><div class="gauge-value" id="v4">0</div><div class="gauge-unit">%</div></div>
<div class="gauge"><canvas id="g5" width="150" height="150"></canvas><div class="gauge-label">LOAD</div><div class="gauge-value" id="v5">0</div><div class="gauge-unit">%</div></div>
<div class="gauge"><canvas id="g6" width="150" height="150"></canvas><div class="gauge-label">VOLTAGE</div><div class="gauge-value" id="v6">0</div><div class="gauge-unit">V</div></div>
<div class="gauge"><canvas id="g7" width="150" height="150"></canvas><div class="gauge-label">INTAKE</div><div class="gauge-value" id="v7">0</div><div class="gauge-unit">°F</div></div>
<div class="gauge"><canvas id="g8" width="150" height="150"></canvas><div class="gauge-label">THROTTLE</div><div class="gauge-value" id="v8">0</div><div class="gauge-unit">%</div></div>
</div>
<div class="warnings">
<span class="warn" id="ce" onclick="showDTC()">CHECK ENGINE</span>
<span class="warn" id="oil">OIL PRESS</span>
<span class="warn" id="bat">BATTERY</span>
<span class="warn" id="st">OBD: --</span>
</div>
<div class="modal" id="dtcModal" onclick="if(event.target==this)closeDTC()">
<div class="modal-content">
<span class="modal-close" onclick="closeDTC()">&times;</span>
<div class="modal-title">DIAGNOSTIC TROUBLE CODES</div>
<ul class="dtc-list" id="dtcList"><li class="dtc-empty">Loading...</li></ul>
</div>
</div>
<script>
function drawGauge(ctx,w,h,val,min,max,zones){
ctx.clearRect(0,0,w,h);const cx=w/2,cy=h/2,r=Math.min(w,h)/2-15;
const sa=-0.75*Math.PI,ea=0.75*Math.PI,range=ea-sa;
ctx.save();ctx.translate(cx,cy);
const grad=ctx.createRadialGradient(0,0,r*0.5,0,0,r);
grad.addColorStop(0,'#1a1a1a');grad.addColorStop(1,'#0a0a0a');
ctx.fillStyle=grad;ctx.beginPath();ctx.arc(0,0,r,0,2*Math.PI);ctx.fill();
ctx.strokeStyle='#333';ctx.lineWidth=2;ctx.stroke();
const ticks=20;for(let i=0;i<=ticks;i++){
const a=sa+range*i/ticks;const isMajor=i%5===0;
ctx.save();ctx.rotate(a);ctx.strokeStyle='#555';ctx.lineWidth=isMajor?2:1;
ctx.beginPath();ctx.moveTo(r-10,0);ctx.lineTo(r-(isMajor?20:12),0);ctx.stroke();
if(isMajor){ctx.fillStyle='#888';ctx.font='10px Arial';ctx.textAlign='center';ctx.textBaseline='middle';
const num=Math.round(min+(max-min)*i/ticks);ctx.fillText(num,r-30,0);}
ctx.restore();}
const p=(val-min)/(max-min);const aa=sa+range*Math.max(0,Math.min(1,p));
zones.forEach(z=>{if(p>=z.start&&p<z.end){
ctx.strokeStyle=z.color;ctx.lineWidth=8;ctx.beginPath();
ctx.arc(0,0,r-5,sa+range*z.start,sa+range*z.end);ctx.stroke();}});
ctx.save();ctx.rotate(aa);
const grd=ctx.createLinearGradient(0,0,r-25,0);
grd.addColorStop(0,'#444');grd.addColorStop(1,'#fff');
ctx.fillStyle=grd;ctx.beginPath();ctx.moveTo(-8,0);ctx.lineTo(r-25,0);ctx.lineTo(r-20,3);ctx.lineTo(-5,3);ctx.closePath();ctx.fill();
ctx.strokeStyle='#fff';ctx.lineWidth=1;ctx.stroke();
ctx.restore();
ctx.fillStyle='#333';ctx.beginPath();ctx.arc(0,0,12,0,2*Math.PI);ctx.fill();
ctx.fillStyle='#666';ctx.beginPath();ctx.arc(0,0,8,0,2*Math.PI);ctx.fill();
ctx.restore();}
const gauges=[
{id:'g1',v:'v1',min:0,max:140,zones:[{start:0,end:0.7,color:'#0f6'},{start:0.7,end:0.9,color:'#ff0'},{start:0.9,end:1,color:'#f55'}]},
{id:'g2',v:'v2',min:0,max:8000,zones:[{start:0,end:0.7,color:'#0f6'},{start:0.7,end:0.85,color:'#ff0'},{start:0.85,end:1,color:'#f55'}]},
{id:'g3',v:'v3',min:120,max:270,zones:[{start:0,end:0.6,color:'#4cf'},{start:0.6,end:0.85,color:'#0f6'},{start:0.85,end:1,color:'#f55'}]},
{id:'g4',v:'v4',min:0,max:100,zones:[{start:0,end:0.25,color:'#f55'},{start:0.25,end:0.5,color:'#ff0'},{start:0.5,end:1,color:'#0f6'}]},
{id:'g5',v:'v5',min:0,max:100,zones:[{start:0,end:0.7,color:'#0f6'},{start:0.7,end:0.9,color:'#ff0'},{start:0.9,end:1,color:'#f55'}]},
{id:'g6',v:'v6',min:10,max:16,zones:[{start:0,end:0.3,color:'#f55'},{start:0.3,end:0.6,color:'#ff0'},{start:0.6,end:1,color:'#0f6'}]},
{id:'g7',v:'v7',min:30,max:210,zones:[{start:0,end:0.5,color:'#48f'},{start:0.5,end:0.8,color:'#0f6'},{start:0.8,end:1,color:'#f55'}]},
{id:'g8',v:'v8',min:0,max:100,zones:[{start:0,end:0.7,color:'#0f6'},{start:0.7,end:0.9,color:'#ff0'},{start:0.9,end:1,color:'#f55'}]}
].map(g=>({...g,canvas:document.getElementById(g.id),ctx:document.getElementById(g.id).getContext('2d'),val:0}));
async function update(){try{const r=await fetch('/api/data');const d=await r.json();
const vals=[d.speed*0.621371,d.rpm,d.coolant*9/5+32,d.fuel,d.load,d.voltage,d.intake*9/5+32,d.throttle];
gauges.forEach((g,i)=>{g.val=g.val*0.8+vals[i]*0.2;
drawGauge(g.ctx,g.canvas.width,g.canvas.height,g.val,g.min,g.max,g.zones);
document.getElementById(g.v).textContent=Math.round(g.val);});
document.getElementById('ce').className=d.checkEngine?'warn on':'warn';
document.getElementById('oil').className=d.oilPressure?'warn on':'warn';
document.getElementById('bat').className=d.voltage<12?'warn on':'warn';
document.getElementById('st').textContent='OBD: '+(d.obdConnected?'CONN':'OFF');
document.getElementById('st').className=d.obdConnected?'warn':'warn on';
}catch(e){}}
gauges.forEach(g=>drawGauge(g.ctx,g.canvas.width,g.canvas.height,0,g.min,g.max,g.zones));
update();setInterval(update,100);
async function showDTC(){
const modal=document.getElementById('dtcModal');
const list=document.getElementById('dtcList');
modal.style.display='flex';
list.innerHTML='<li class="dtc-empty">Reading codes...</li>';
try{
const r=await fetch('/api/dtc');
const d=await r.json();
if(!d.connected){
list.innerHTML='<li class="dtc-empty">OBD not connected</li>';
}else if(d.codes.length===0){
list.innerHTML='<li class="dtc-empty">No trouble codes found</li>';
}else{
list.innerHTML=d.codes.map(c=>'<li class="dtc-item">'+c+'</li>').join('');
}
}catch(e){
list.innerHTML='<li class="dtc-empty">Error reading codes</li>';
}}
function closeDTC(){document.getElementById('dtcModal').style.display='none';}
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

String getDTCType(char c) {
    switch(c) {
        case '0': return "P0";
        case '1': return "P1";
        case '2': return "P2";
        case '3': return "P3";
        case '4': return "C0";
        case '5': return "C1";
        case '6': return "C2";
        case '7': return "C3";
        case '8': return "B0";
        case '9': return "B1";
        case 'A': return "B2";
        case 'B': return "B3";
        case 'C': return "U0";
        case 'D': return "U1";
        case 'E': return "U2";
        case 'F': return "U3";
        default: return "??";
    }
}

void handleDTC() {
    Serial.println("DTC request");
    String json = "{\"codes\":[";

    if (obdConnected) {
        String resp = sendELM("03");
        Serial.print("DTC Response: ");
        Serial.println(resp);

        // Parse response: "43 01 33 00 00 00 00" = 1 code (P0133)
        // Format: 43 [count] [code1_hi] [code1_lo] [code2_hi] [code2_lo] ...
        if (resp.indexOf("43") >= 0) {
            int idx = resp.indexOf("43");
            // Remove all spaces for easier parsing
            resp.replace(" ", "");
            resp = resp.substring(idx + 2);  // Skip "43"

            if (resp.length() >= 2) {
                int count = parseHex(resp, 0, 2);
                Serial.print("DTC count: ");
                Serial.println(count);

                bool first = true;
                for (int i = 0; i < count && (i * 4 + 2) < resp.length(); i++) {
                    int codeIdx = 2 + (i * 4);
                    if (codeIdx + 4 <= resp.length()) {
                        String codeHex = resp.substring(codeIdx, codeIdx + 4);
                        if (codeHex != "0000") {
                            // Parse DTC format
                            char firstChar = codeHex.charAt(0);
                            String dtcType = getDTCType(firstChar);
                            String dtcNum = codeHex.substring(1);

                            if (!first) json += ",";
                            json += "\"" + dtcType + dtcNum + "\"";
                            first = false;
                        }
                    }
                }
            }
        }
    }

    json += "],\"connected\":" + String(obdConnected ? "true" : "false") + "}";

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
    server.on("/api/dtc", handleDTC);
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
