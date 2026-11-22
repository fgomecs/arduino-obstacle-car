// NodeMCU ESP8266 WiFi bridge for the obstacle car
// Board: "NodeMCU 1.0 (ESP-12E Module)"  (Boards Manager: esp8266 by ESP8266 Community)
// Library: "WiFiManager" by tzapu (Library Manager). Everything else ships with the esp8266 core.
//
// What it does
//  - First boot (or new router): opens a hotspot "TiagoCar-Setup". Join it from your phone,
//    a page pops up, pick your home WiFi + password. Saved in flash, no reflashing.
//  - Then it joins your WiFi and serves the remote at  http://tiagocar.local  (or its IP).
//  - Talks to the Uno over SoftwareSerial (D5/D6, 9600 baud) using one-letter commands.
//  - Change WiFi later: hold the FLASH button 3 s, or use "Change WiFi" on the page.
//  - Over-the-air updates: Arduino IDE > Tools > Port > "tiagocar at ..." (password below).
//
// Wiring (see README), no soldering - the motor shield's free SER1 servo header:
//   NodeMCU D6 -> SER1 S    NodeMCU Vin -> SER1 +    NodeMCU GND -> SER1 -
// Optional return wire for distance on the phone: Uno A3 -> 1k -> D5, 2k from D5 to GND.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <SoftwareSerial.h>

#define HOSTNAME     "tiagocar"
#define SETUP_AP     "TiagoCar-Setup"
#define OTA_PASSWORD "tiagocar"     // change if you like
#define UNO_RX       D5             // optional: receives from Uno A3 (through the divider)
#define UNO_TX       D6             // sends to the Uno via the SER1 header signal pin
#define FLASH_BTN    0              // on-board FLASH button (GPIO0), active LOW
#define LED_PIN      LED_BUILTIN    // blue LED on the ESP module, active LOW

SoftwareSerial uno(UNO_RX, UNO_TX);
ESP8266WebServer server(80);

char carMode = '?', carMotion = '?';
int carDist = -1;
unsigned long lastStatusMs = 0;
String line;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Tiago's Car</title>
<style>
:root{--bg:#0f1220;--card:#171b2e;--line:#262b45;--txt:#e8eaf6;--mut:#8b90b3;--acc:#5b8cff;--ok:#2ecc71;--bad:#ff5c6c;--warn:#f5c542}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;font:16px system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--txt);
 user-select:none;-webkit-user-select:none;touch-action:none;display:flex;flex-direction:column;align-items:center;min-height:100vh}
header{width:100%;display:flex;align-items:center;gap:10px;padding:14px 18px;border-bottom:1px solid var(--line)}
h1{font-size:19px;margin:0}.pill{font-size:12px;padding:3px 10px;border-radius:99px;background:#222842;color:var(--mut)}
.pill.on{background:rgba(46,204,113,.15);color:var(--ok)}.pill.off{background:rgba(255,92,108,.15);color:var(--bad)}
main{width:100%;max-width:420px;padding:16px;display:flex;flex-direction:column;gap:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px}
.dist{display:flex;align-items:baseline;justify-content:space-between}
.dist b{font-size:44px}.dist span{color:var(--mut)}
.bar{height:12px;border-radius:6px;background:#222842;overflow:hidden;margin-top:10px}
.bar i{display:block;height:100%;width:0;background:var(--ok);transition:width .2s,background .2s}
.modes{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.modes button{padding:14px;font-size:17px}
button{font:inherit;border:1px solid var(--line);background:#20253d;color:var(--txt);border-radius:14px;cursor:pointer}
button.sel{background:var(--acc);border-color:var(--acc);color:#fff}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.pad button{aspect-ratio:1;font-size:34px;border-radius:22px}
.pad button.held{background:var(--acc);border-color:var(--acc)}
.pad .stop{background:var(--bad);border-color:var(--bad);font-size:20px;font-weight:700}
.hint{color:var(--mut);font-size:13px;text-align:center}
a{color:var(--mut);font-size:13px}
</style></head><body>
<header><h1>🚗 Tiago's Car</h1><span id="conn" class="pill">…</span></header>
<main>
 <div class="card">
  <div class="dist"><span>Distance ahead</span><span id="mot"></span></div>
  <div class="dist"><b id="d">--</b><span>cm</span></div>
  <div class="bar"><i id="bar"></i></div>
 </div>
 <div class="modes">
  <button id="mA" onclick="send('a')">🤖 Auto</button>
  <button id="mM" onclick="send('m')">🎮 Manual</button>
 </div>
 <div class="card">
  <div class="pad">
   <span></span><button data-c="f">▲</button><span></span>
   <button data-c="l">◀</button><button class="stop" id="stop">STOP</button><button data-c="r">▶</button>
   <span></span><button data-c="b">▼</button><span></span>
  </div>
  <p class="hint">Hold an arrow to drive. Let go and it stops.<br>It won't drive forward into a wall.<br>
  Keyboard: arrows / WASD drive · Space or OK stops · 1 auto · 2 manual</p>
 </div>
 <p class="hint"><a href="#" onclick="forget();return false">Change WiFi</a></p>
</main>
<script>
const $=id=>document.getElementById(id);
let mode='?',timer=null,okAt=0;
function send(c){return fetch('/cmd?c='+c).then(()=>okAt=Date.now()).catch(()=>{});}
function release(){if(timer){clearInterval(timer);timer=null;}
 document.querySelectorAll('.pad .held').forEach(b=>b.classList.remove('held'));send('s');}
document.querySelectorAll('.pad button[data-c]').forEach(b=>{
 b.addEventListener('pointerdown',e=>{e.preventDefault();b.setPointerCapture(e.pointerId);
  if(mode!=='M')send('m');b.classList.add('held');const c=b.dataset.c;send(c);
  clearInterval(timer);timer=setInterval(()=>send(c),150);});
 ['pointerup','pointercancel','lostpointercapture'].forEach(ev=>b.addEventListener(ev,release));
});
$('stop').addEventListener('pointerdown',e=>{e.preventDefault();release();});
document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
// Keyboard (PC, TV or the mini wireless keyboard): arrows/WASD hold to drive, space/OK stops,
// 1 = auto, 2 = manual
const KEYS={ArrowUp:'f',ArrowDown:'b',ArrowLeft:'l',ArrowRight:'r',w:'f',s:'b',a:'l',d:'r'};
let keyDown=null;
document.addEventListener('keydown',e=>{
 const k=e.key.length===1?e.key.toLowerCase():e.key;
 if(k===' '||k==='Enter'){e.preventDefault();keyDown=null;release();return;}
 if(k==='1'){send('a');return;} if(k==='2'){send('m');return;}
 const c=KEYS[k]; if(!c)return; e.preventDefault(); if(keyDown===k)return;   // ignore auto-repeat
 keyDown=k; if(mode!=='M')send('m'); send(c);
 document.querySelectorAll('.pad .held').forEach(b=>b.classList.remove('held'));
 const btn=document.querySelector('.pad button[data-c="'+c+'"]'); if(btn)btn.classList.add('held');
 clearInterval(timer); timer=setInterval(()=>send(c),150);});
document.addEventListener('keyup',e=>{const k=e.key.length===1?e.key.toLowerCase():e.key;
 if(k===keyDown){keyDown=null;release();}});
function forget(){if(confirm('Forget this WiFi? The car will open the "TiagoCar-Setup" hotspot so you can pick a new network.'))fetch('/forget',{method:'POST'});}
async function poll(){
 try{const s=await (await fetch('/status')).json();okAt=Date.now();mode=s.mode;
  $('d').textContent=s.dist<0?'--':(s.dist>=300?'300+':s.dist);
  const p=Math.max(0,Math.min(100,s.dist/3));$('bar').style.width=p+'%';
  $('bar').style.background=s.dist<30?'var(--bad)':s.dist<60?'var(--warn)':'var(--ok)';
  $('mot').textContent={F:'driving',B:'reversing',T:'turning',X:'stopped'}[s.motion]||'';
  $('mA').classList.toggle('sel',mode==='A');$('mM').classList.toggle('sel',mode==='M');
  const fresh=s.age<2000;$('conn').textContent=fresh?'connected':'connected · no feedback wire';
  $('conn').className='pill '+(fresh?'on':'');
  if(!fresh){$('d').textContent='--';$('bar').style.width='0';$('mot').textContent='';}
 }catch(e){$('conn').textContent='offline';$('conn').className='pill off';}
 setTimeout(poll,300);}
poll();
</script></body></html>)HTML";

// ---------------------------------------------------------------- Uno link
void parseLine(const String &s) {          // "S,A,F,123"
  if (!s.startsWith("S,") || s.length() < 7) return;
  carMode = s.charAt(2);
  carMotion = s.charAt(4);
  carDist = s.substring(6).toInt();
  lastStatusMs = millis();
}

void readUno() {
  while (uno.available()) {
    char ch = uno.read();
    if (ch == '\n') { parseLine(line); line = ""; }
    else if (ch != '\r' && line.length() < 32) line += ch;
  }
}

// ---------------------------------------------------------------- web
void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleCmd() {
  String c = server.arg("c");
  if (c.length() == 1 && strchr("amfblrs", c.charAt(0))) {
    uno.write(c.charAt(0));
    server.send(200, "text/plain", "ok");
  } else {
    server.send(400, "text/plain", "bad command");
  }
}

void handleStatus() {
  char buf[120];
  long age = lastStatusMs ? (long)(millis() - lastStatusMs) : 999999;
  snprintf(buf, sizeof(buf), "{\"mode\":\"%c\",\"motion\":\"%c\",\"dist\":%d,\"age\":%ld,\"rssi\":%d}",
           carMode, carMotion, carDist, age, WiFi.RSSI());
  server.send(200, "application/json", buf);
}

void forgetWifi() {
  uno.write('s');                         // stop the car first
  WiFiManager wm;
  wm.resetSettings();
  delay(300);
  ESP.restart();
}

void handleForget() {
  server.send(200, "text/plain", "Forgetting WiFi, restarting into setup hotspot...");
  delay(500);
  forgetWifi();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  uno.begin(9600);
  pinMode(FLASH_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);             // LED on while connecting

  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(10);                // lower TX power: avoids brown-out resets on cheap boards/USB
  WiFi.hostname(HOSTNAME);
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);         // 5 min in setup mode, then retry
  wm.setTitle("Tiago's Car");
  Serial.println(F("\nConnecting (or opening the " SETUP_AP " hotspot)..."));
  if (!wm.autoConnect(SETUP_AP)) {
    Serial.println(F("No WiFi configured, restarting"));
    ESP.restart();
  }
  WiFi.setAutoReconnect(true);
  digitalWrite(LED_PIN, HIGH);            // LED off = connected
  Serial.print(F("Connected. Open http://" HOSTNAME ".local or http://"));
  Serial.println(WiFi.localIP());

  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { uno.write('s'); });   // stop the car before updating
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/status", handleStatus);
  server.on("/forget", HTTP_POST, handleForget);
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  readUno();

  // Hold FLASH for 3 s -> forget WiFi (LED blinks while held)
  static unsigned long pressedAt = 0;
  if (digitalRead(FLASH_BTN) == LOW) {
    if (!pressedAt) pressedAt = millis();
    digitalWrite(LED_PIN, (millis() / 150) % 2);
    if (millis() - pressedAt > 3000) forgetWifi();
  } else if (pressedAt) {
    pressedAt = 0;
    digitalWrite(LED_PIN, HIGH);
  }
}
