#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RTClib.h>

#include "FS.h"
#include <LittleFS.h>

// ===== Pins =====
#define ONE_WIRE_BUS 2           // D4 = GPIO2
#define I2C_SDA 4                // D2 = GPIO4
#define I2C_SCL 5                // D1 = GPIO5

// ===== SoftAP =====
const char* AP_SSID = "ESPCONNECT";
const char* AP_PASS = "pass1234";
IPAddress apIP(10, 1, 1, 1);
IPAddress apGW(10, 1, 1, 1);
IPAddress apSN(255, 255, 255, 0);

// ===== Sensor / RTC =====
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
RTC_DS3231 rtc;
bool rtc_ok = false;

// ===== Web =====
ESP8266WebServer server(80);

// ===== Timing =====
unsigned long last_measure_ms = 0;
const unsigned long MEASURE_INTERVAL_MS = 5000; // 5s (später 60s oder 300s)
unsigned long last_heartbeat_ms = 0;
const unsigned long HEARTBEAT_MS = 10000;

// ===== Live Cache =====
String last_ts = "0000-00-00 00:00:00";
float  last_temp = NAN;
bool   last_temp_ok = false;

// ===== History Ringbuffer =====
static const int HIST_MAX = 200;
String hist[HIST_MAX];
int hist_head = 0;
int hist_count = 0;

// ===== Logging (LittleFS) =====
const char* DEFAULT_FILE = "/default.csv";
String active_file = "";     // wenn gesetzt, wird zusätzlich in dieses File geloggt

// ---------- Helpers ----------
String tsToString(const DateTime& dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

String safeName(String s) {
  s.trim();
  if (s.length() == 0) return "messung";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '-' || c == '_';
    if (!ok) s.setCharAt(i, '_');
  }
  return s;
}

String startFilename(String prefix, const DateTime& dt) {
  // /m_<prefix>_YYMMDD_HHMMSS.csv  (kurz & safe)
  char buf[64];
  snprintf(buf, sizeof(buf),
           "/m_%s_%02d%02d%02d_%02d%02d%02d.csv",
           prefix.c_str(),
           dt.year() % 100, dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}


void pushHistoryLine(const String& line) {
  hist[hist_head] = line;
  hist_head = (hist_head + 1) % HIST_MAX;
  if (hist_count < HIST_MAX) hist_count++;
}

bool ensureHeader(const String& path) {
  File f = LittleFS.open(path, "r");
  bool needHeader = true;
  if (f) {
    needHeader = (f.size() == 0);
    f.close();
  }
  if (needHeader) {
    File w = LittleFS.open(path, "a");
    if (!w) return false;
    w.print("timestamp,temp_c\n");
    w.close();
  }
  return true;
}


bool appendCSVLine(const String& path, const String& line) {
  File f = LittleFS.open(path, "a");
  if (!f) return false;
  f.print(line);
  f.close();
  return true;
}

// ---------- Measurement + Logging ----------
void updateMeasurementsAndLog() {
  // Zeit
  DateTime now;
  if (rtc_ok) now = rtc.now();

  if (rtc_ok) last_ts = tsToString(now);
  else {
    unsigned long s = millis() / 1000;
    unsigned long hh = (s / 3600) % 24;
    unsigned long mm = (s / 60) % 60;
    unsigned long ss = s % 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "uptime %02lu:%02lu:%02lu", hh, mm, ss);
    last_ts = String(buf);
  }

  // Temperatur
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C) {
    last_temp_ok = false;
    last_temp = NAN;
  } else {
    last_temp_ok = true;
    last_temp = t;
  }

  // CSV line (ohne Header)
  String line = last_ts;
  line += ",";
  line += last_temp_ok ? String(last_temp, 2) : "";
  line += "\n";

  // History (ohne newline) – fuer /history Seite
  pushHistoryLine(last_ts + "," + (last_temp_ok ? String(last_temp, 2) : ""));

  // ===== Logging =====

  // Default log immer
  ensureHeader(DEFAULT_FILE);
  if (!appendCSVLine(DEFAULT_FILE, line)) {
    Serial.println("ERR append default: " + String(DEFAULT_FILE));
  }

  // Active log optional zusätzlich
  if (active_file.length() > 0) {
    if (!LittleFS.exists(active_file)) {
      Serial.println("ERR active_file missing: " + active_file);
    } else {
      if (!appendCSVLine(active_file, line)) {
        Serial.println("ERR append active: " + active_file);
      }
    }
  }
}


// ---------- HTTP Handlers ----------
void handleData() {
  // CSV Snapshot: Header + letzte Zeile
  String out = "timestamp,temp_c\n";
  out += last_ts;
  out += ",";
  out += last_temp_ok ? String(last_temp, 2) : "";
  out += "\n";
  server.send(200, "text/csv; charset=utf-8", out);
}

void handleData1() {
  // nur eine Zeile, ohne Header: timestamp,temp_c
  String out = last_ts;
  out += ",";
  out += last_temp_ok ? String(last_temp, 2) : "";
  out += "\n";
  server.send(200, "text/plain; charset=utf-8", out);
}

void handleRaw() {
  String out = last_ts + " " + (last_temp_ok ? String(last_temp, 2) : "NA") + "\n";
  server.send(200, "text/plain; charset=utf-8", out);
}

void handleStart() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain; charset=utf-8", "missing name\n");
    return;
  }
  if (!rtc_ok) {
    server.send(500, "text/plain; charset=utf-8", "RTC not available (cannot timestamp filename)\n");
    return;
  }

  String name = safeName(server.arg("name"));
  DateTime now = rtc.now();

  String fn = startFilename(name, now);     // z.B. /m_test_2026-...csv
  Serial.println("START requested file: " + fn);

  // Datei sofort wirklich anlegen + Header schreiben
  // (damit wir sicher wissen: existiert / nicht existiert)
  File f = LittleFS.open(fn, "a");
  if (!f) {
    Serial.println("ERR: LittleFS.open() failed for " + fn);
    server.send(500, "text/plain; charset=utf-8", "ERR create failed: " + fn + "\n");
    return;
  }
  if (f.size() == 0) {
    f.print("timestamp,temp_c\n");
  }
  f.close();

  if (!LittleFS.exists(fn)) {
    Serial.println("ERR: exists() still false for " + fn);
    server.send(500, "text/plain; charset=utf-8", "ERR exists false after create: " + fn + "\n");
    return;
  }

  active_file = fn;
  server.send(200, "text/plain; charset=utf-8", "OK started: " + active_file + "\n");
}


void handleStop() {
  active_file = "";
  server.send(200, "text/plain; charset=utf-8", "OK stopped (logging only default)\n");
}

String urlDecode(String s) {
  s.replace("+", " ");
  String out; out.reserve(s.length());
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '%' && i + 2 < s.length()) {
      auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
      };
      int a = hex(s[i+1]), b = hex(s[i+2]);
      if (a >= 0 && b >= 0) {
        out += char((a << 4) | b);
        i += 2;
        continue;
      }
    }
    out += c;
  }
  return out;
}


String normalizePath(String p) {
  p = urlDecode(p);
  p.trim();
  if (!p.startsWith("/")) p = "/" + p;
  return p;
}

bool tryStreamFile(const String& path) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;

  // nicer filename in browser (optional)
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + String(path.substring(path.lastIndexOf('/') + 1)) + "\"");
  server.streamFile(f, "text/csv");
  f.close();
  return true;
}

void handleDelete() {
  if (!server.hasArg("f")) {
    server.send(400, "text/plain; charset=utf-8", "missing f\n");
    return;
  }

  String path = normalizePath(server.arg("f"));

  if (!path.endsWith(".csv")) {
    server.send(400, "text/plain; charset=utf-8", "only .csv allowed\n");
    return;
  }

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain; charset=utf-8", "file not found: " + path + "\n");
    return;
  }

  if (active_file == path) active_file = "";

  bool ok = LittleFS.remove(path);
  server.send(200, "text/plain; charset=utf-8", ok ? "OK deleted\n" : "delete failed\n");
}



void handleDownload() {
  if (!server.hasArg("f")) {
    server.send(400, "text/plain; charset=utf-8", "missing f\n");
    return;
  }

  String path = normalizePath(server.arg("f"));

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain; charset=utf-8", "file not found: " + path + "\n");
    return;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(500, "text/plain; charset=utf-8", "open failed: " + path + "\n");
    return;
  }

  String fn = path.substring(path.lastIndexOf('/') + 1);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fn + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}

void handleFS() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain; charset=utf-8", "");

  server.sendContent("LittleFS listing /\n\n");

  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String n = dir.fileName();
    size_t s = dir.fileSize();
    server.sendContent(n + "  " + String(s) + " bytes\n");
    yield();
  }

  server.sendContent("\nActive file: " + (active_file.length() ? active_file : String("(none)")) + "\n");
  server.sendContent("Exists(active): " + String(active_file.length() ? (LittleFS.exists(active_file) ? "yes" : "no") : "-") + "\n");

  server.sendContent("");
}


String htmlEscape(String s) {
  s.replace("&","&amp;"); s.replace("<","&lt;"); s.replace(">","&gt;");
  s.replace("\"","&quot;"); s.replace("'","&#39;");
  return s;
}

String urlEncode(String s) {
  const char *hex = "0123456789ABCDEF";
  String out; out.reserve(s.length() * 3);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    // '/' lassen wir absichtlich drin, damit /default.csv sauber bleibt
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '/') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

String fmtBytes(size_t b) {
  if (b < 1024) return String(b) + " B";
  if (b < 1024 * 1024) return String((float)b / 1024.0, 1) + " KB";
  return String((float)b / (1024.0 * 1024.0), 2) + " MB";
}

void handleFiles() {
  String html;
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Files</title>";
  html += "<style>body{font-family:system-ui,-apple-system,sans-serif;margin:16px;}"
          "table{border-collapse:collapse;width:100%;}"
          "th,td{border:1px solid #ddd;padding:8px;}"
          "th{background:#f5f5f5;}"
          "code{font-family:ui-monospace,Menlo,monospace;}</style>";
  html += "</head><body>";
  html += "<h2>Files</h2>";
  html += "<p><a href='/'>Live</a> <a href='/history'>History</a></p>";
  html += "<table><tr><th>Pfad</th><th>Groesse</th><th>Aktion</th></tr>";

  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName();     // kann ohne leading '/' kommen -> normalisieren
    String path = normalizePath(name);

    if (!path.endsWith(".csv")) continue;

    size_t sz = dir.fileSize();
    String pEsc = htmlEscape(path);
    String pEnc = urlEncode(path);

    html += "<tr>";
    html += "<td><code>" + pEsc + "</code></td>";
    html += "<td>" + fmtBytes(sz) + "</td>";
    html += "<td>"
            "<a href='/download?f=" + pEnc + "'>Download</a> "
            "<a href='/delete?f=" + pEnc + "' onclick='return confirm(\"Datei loeschen? " + pEsc + "\");'>Delete</a>"
            "</td>";
    html += "</tr>";
    yield();
  }

  html += "</table></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}



void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");

  // ===== HTML HEAD =====
  server.sendContent(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>AP Temp Logger</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,sans-serif;margin:16px;}"
    "a{margin-right:12px;}"
    ".sticky{position:sticky;top:0;background:#fff;padding:10px 0;border-bottom:1px solid #eee;z-index:5;}"
    "canvas{display:block;border:1px solid #ddd;border-radius:8px;}"
    ".row{display:flex;gap:12px;flex-wrap:wrap;align-items:flex-start;}"
    ".card{background:#f7f7f7;padding:10px 12px;border-radius:8px;min-width:240px;}"
    "input,button{font-size:16px;padding:8px;margin:4px;}"
    ".status{margin-top:8px;padding:8px;border-radius:8px;background:#eef6ff;white-space:pre-wrap;}"
    "#log{margin-top:12px;font-family:ui-monospace,Menlo,monospace;}"
    ".entry{padding:6px 8px;border-bottom:1px solid #eee;white-space:pre;}"
    ".entry.new{background:#fff7cc;font-weight:700;}"
    "</style></head><body>"
  );

  // ===== TOP BAR =====
  server.sendContent(
    "<div class='sticky'>"
    "<h2 style='margin:0 0 8px 0;'>AP Temp Logger</h2>"
    "<div><a href='/'>Live</a><a href='/history'>History</a><a href='/files'>Files</a></div>"

    "<div class='row' style='margin-top:10px;'>"
    "<div class='card'><b>Letzter Wert</b><div id='last'>-</div></div>"
    "<div class='card'><b>Aktive Messung</b><div id='active'>(keine)</div></div>"
    "</div>"

    "<div style='margin-top:10px;'>"
    "<input id='mname' placeholder='Messungsname (Prefix)'>"
    "<button id='btnStart' type='button'>Start Messung</button>"
    "<button id='btnStop' type='button'>Messung stoppen</button>"
    "<div id='startStatus' class='status' style='display:none;'></div>"
    "<div id='liveStatus' class='status'>Live: (noch nichts)</div>"
    "</div>"

    "<canvas id='c' width='520' height='200' style='margin-top:10px;'></canvas>"
    "<div style='font-size:12px;color:#666;margin-top:6px;'>Graph: 0–70°C (Messlinie rot)</div>"
    "</div>"

    "<div id='log'></div>"
  );

  // ===== JAVASCRIPT =====
  server.sendContent(
    "<script>"
    "const MAXP=120, MAXL=300;"
    "const kL='liveLines', kT='liveTemps', kA='activeFile';"

    "const logEl=document.getElementById('log');"
    "const lastEl=document.getElementById('last');"
    "const activeEl=document.getElementById('active');"
    "const startStatus=document.getElementById('startStatus');"
    "const liveStatus=document.getElementById('liveStatus');"
    "const btnStart=document.getElementById('btnStart');"
    "const btnStop=document.getElementById('btnStop');"
    "const nameIn=document.getElementById('mname');"

    "const c=document.getElementById('c'); const ctx=c.getContext('2d');"

    "let lastLine='';"
    "let lines=[];"
    "let temps=[];"
    "let activeFile='';"

    // ---- load/save ----
    "function load(){"
    " try{lines=JSON.parse(localStorage.getItem(kL)||'[]')||[];}catch(e){}"
    " try{temps=JSON.parse(localStorage.getItem(kT)||'[]')||[];}catch(e){}"
    " try{activeFile=localStorage.getItem(kA)||'';}catch(e){}"
    " activeEl.textContent=activeFile?activeFile:'(keine)';"
    "}"

    "function save(){"
    " try{localStorage.setItem(kL,JSON.stringify(lines));}catch(e){}"
    " try{localStorage.setItem(kT,JSON.stringify(temps));}catch(e){}"
    " try{localStorage.setItem(kA,activeFile||'');}catch(e){}"
    "}"

    // ---- render log ----
    "function render(){"
    " let h='';"
    " for(let i=0;i<lines.length;i++){"
    "   h+=`<div class='entry ${i===0?'new':''}'>${lines[i]}</div>`;"
    " }"
    " logEl.innerHTML=h;"
    "}"

    // ---- draw graph ----
    "function draw(){"
    " ctx.clearRect(0,0,c.width,c.height);"
    " const pad=24,W=c.width-2*pad,H=c.height-2*pad,min=0,max=70;"

    " ctx.strokeStyle='#ccc'; ctx.fillStyle='#000';"
    " for(let y=0;y<=70;y+=10){"
    "  const py=pad+(1-((y-min)/(max-min)))*H;"
    "  ctx.beginPath(); ctx.moveTo(pad,py); ctx.lineTo(pad+W,py); ctx.stroke();"
    "  ctx.fillText(y,2,py+4);"
    " }"

    " if(temps.length<2) return;"

    " ctx.strokeStyle='#ff0000';"
    " ctx.beginPath();"
    " for(let i=0;i<temps.length;i++){"
    "  const v=temps[i];"
    "  const x=pad+(i/(temps.length-1))*W;"
    "  const y=pad+(1-((v-min)/(max-min)))*H;"
    "  if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);"
    " }"
    " ctx.stroke();"
    "}"

    // ---- poll ----
    "async function poll(){"
    " try{"
    "  const r=await fetch('/data1',{cache:'no-store'});"
    "  const line=(await r.text()).trim();"
    "  if(!line) return;"
    "  liveStatus.textContent='Live: OK';"
    "  if(line!==lastLine){"
    "   lastLine=line;"
    "   lastEl.textContent=line;"
    "   lines.unshift(line);"
    "   if(lines.length>MAXL) lines=lines.slice(0,MAXL);"
    "   const t=parseFloat(line.split(',')[1]);"
    "   if(!isNaN(t)){temps.push(t); if(temps.length>MAXP) temps=temps.slice(-MAXP);}"
    "   render(); draw(); save();"
    "  }"
    " }catch(e){ liveStatus.textContent='Live Fehler'; }"
    "}"

    // ---- START ----
    "btnStart.onclick=async()=>{"
    " const n=nameIn.value.trim();"
    " if(!n) return;"
    " const r=await fetch('/start?name='+encodeURIComponent(n));"
    " const t=(await r.text()).trim();"
    " startStatus.style.display='block'; startStatus.textContent=t;"
    " const m=t.match(/OK started:\\s*(.*)/);"
    " if(m){activeFile=m[1]; activeEl.textContent=activeFile; save();}"
    "};"

    // ---- STOP ----
    "btnStop.onclick=async()=>{"
    " const r=await fetch('/stop');"
    " const t=(await r.text()).trim();"
    " startStatus.style.display='block'; startStatus.textContent=t;"
    " activeFile=''; activeEl.textContent='(keine)'; save();"
    "};"

    "load(); render(); draw(); setInterval(poll,2000); poll();"
    "</script></body></html>"
  );

  server.sendContent("");
}



void handleHistory() {
  String html;
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>History</title>";
  html += "<style>";
  html += "body{font-family:system-ui,-apple-system,sans-serif;margin:16px;}";
  html += "a{margin-right:12px;}";
  html += ".sticky{position:sticky;top:0;background:white;padding:10px 0;border-bottom:1px solid #eee;z-index:5;}";
  html += "canvas{display:block;border:1px solid #ddd;border-radius:8px;}";
  html += "table{border-collapse:collapse;width:100%;margin-top:12px;}";
  html += "th,td{border:1px solid #ddd;padding:8px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;}";
  html += "th{background:#f5f5f5;}";
  html += "tr:first-child td{background:#fff7cc;font-weight:700;}";
  html += "</style>";
  html += "</head><body>";

  html += "<div class='sticky'>";
  html += "<h2 style='margin:0 0 8px 0;'>History (letzte " + String(hist_count) + " Werte)</h2>";
  html += "<div><a href='/'>Live</a><a href='/files'>Files</a>"
        "<button onclick='location.reload()' style='font-size:16px;padding:8px;margin-left:8px;'>Refresh</button></div>";
  html += "<canvas id='c' width='520' height='200'></canvas>";
  html += "<div style='font-size:12px;color:#666;margin-top:6px;'>Graph: 0–70°C (Messlinie rot)</div>";
  html += "</div>";

  // Tabelle newest first
  html += "<table><tr><th>timestamp</th><th>temp_c</th></tr>";
  for (int i = 0; i < hist_count; i++) {
    int idx = hist_head - 1 - i;
    if (idx < 0) idx += HIST_MAX;
    String line = hist[idx];
    int comma = line.indexOf(',');
    String ts = (comma >= 0) ? line.substring(0, comma) : line;
    String tc = (comma >= 0) ? line.substring(comma + 1) : "";
    html += "<tr><td>" + ts + "</td><td>" + tc + "</td></tr>";
    yield();
  }
  html += "</table>";

  // JS: fixed scale + grid + labels every 5 points, red measurement line
  html += "<script>";
  html += "const canvas=document.getElementById('c'); const ctx=canvas.getContext('2d');";
  html += "const rows=[...document.querySelectorAll('table tr')].slice(1);";
  html += "let temps=[];";
  html += "for(const r of rows){ const t=r.children[1].textContent.trim(); if(t!==''){ const v=parseFloat(t); if(!Number.isNaN(v)) temps.push(v);} }";
  html += "temps=temps.reverse();"; // alt->neu
  html += "function draw(){";
  html += "  ctx.clearRect(0,0,canvas.width,canvas.height);";
  html += "  const pad=24; const W=canvas.width-2*pad; const H=canvas.height-2*pad;";
  html += "  const min=0, max=70;";
  html += "  ctx.font='12px ui-monospace, Menlo, monospace';";

  // grid grey
  html += "  ctx.strokeStyle='#cccccc';";
  html += "  ctx.fillStyle='#000000';";
  html += "  for(let yv=0; yv<=70; yv+=10){";
  html += "    const y=pad + (1-((yv-min)/(max-min)))*H;";
  html += "    ctx.beginPath(); ctx.moveTo(pad,y); ctx.lineTo(pad+W,y); ctx.stroke();";
  html += "    ctx.fillText(String(yv), 2, y+4);";
  html += "  }";
  html += "  for(let i=0;i<temps.length;i+=5){";
  html += "    const x=pad + (i/Math.max(1,temps.length-1))*W;";
  html += "    ctx.beginPath(); ctx.moveTo(x,pad+H); ctx.lineTo(x,pad+H+4); ctx.stroke();";
  html += "    ctx.fillText(String(i), x-6, pad+H+16);";
  html += "  }";

  html += "  if(temps.length<2) return;";

  // measurement red
  html += "  ctx.strokeStyle='#ff0000';";
  html += "  ctx.beginPath();";
  html += "  for(let i=0;i<temps.length;i++){";
  html += "    const v=temps[i];";
  html += "    const x=pad + (i/(temps.length-1))*W;";
  html += "    const y=pad + (1-((v-min)/(max-min)))*H;";
  html += "    if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);";
  html += "  }";
  html += "  ctx.stroke();";

  // last point
  html += "  const lv=temps[temps.length-1];";
  html += "  const lx=pad+W; const ly=pad+(1-((lv-min)/(max-min)))*H;";
  html += "  ctx.fillStyle='#ff0000';";
  html += "  ctx.beginPath(); ctx.arc(lx,ly,3,0,Math.PI*2); ctx.fill();";
  html += "  ctx.fillText(lv.toFixed(2)+'C', lx-70, ly-6);";
  html += "}";
  html += "draw();";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleNotFound() {
  server.send(404, "text/plain; charset=utf-8", "404 Not Found\n");
}

// ---------- Setup / Loop ----------
void setup() {
  delay(1500);
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== ESP8266 AP Temp Logger (Full) ===");

// FS
Serial.println("Mounting LittleFS...");

if (!LittleFS.begin()) {
  Serial.println("LittleFS begin failed -> trying format()");
  if (!LittleFS.format()) {
    Serial.println("LittleFS format failed!");
  } else {
    Serial.println("LittleFS format OK");
  }

  if (!LittleFS.begin()) {
    Serial.println("LittleFS still failed after format. STOP.");
  } else {
    Serial.println("LittleFS mounted after format");
  }
} else {
  Serial.println("LittleFS mounted");
}

// FS Info ausgeben
FSInfo info;
LittleFS.info(info);
Serial.printf("LittleFS total=%u used=%u block=%u page=%u maxOpen=%u maxPath=%u\n",
              info.totalBytes, info.usedBytes, info.blockSize, info.pageSize,
              info.maxOpenFiles, info.maxPathLength);


  // I2C + RTC
  Wire.begin(I2C_SDA, I2C_SCL);
  rtc_ok = rtc.begin();
  if (!rtc_ok) Serial.println("WARN: DS3231 nicht gefunden (I2C).");
  else {
    if (rtc.lostPower()) {
      Serial.println("RTC lostPower() -> setze Zeit auf Compile-Time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // DS18B20
  sensors.begin();
  sensors.setResolution(12);
  Serial.print("DS18B20 Count: ");
  Serial.println(sensors.getDeviceCount());

  // WiFi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apGW, apSN);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  // Routes
  server.on("/", handleRoot);
  server.on("/history", handleHistory);
  server.on("/files", handleFiles);
  server.on("/download", handleDownload);
  server.on("/start", handleStart);

  server.on("/data", handleData);
  server.on("/data1", handleData1);
  server.on("/raw", handleRaw);
  server.on("/delete", handleDelete);
  server.on("/stop", handleStop);
  server.on("/fs", handleFS);



  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP: http://10.1.1.1/");
  Serial.println("Test /data1: http://10.1.1.1/data1");

  // first measurement
  updateMeasurementsAndLog();
  last_measure_ms = millis();
}

void loop() {
  server.handleClient();
  yield();

  unsigned long now = millis();

  if (now - last_measure_ms >= MEASURE_INTERVAL_MS) {
    updateMeasurementsAndLog();
    last_measure_ms = now;
  }

  if (now - last_heartbeat_ms >= HEARTBEAT_MS) {
    Serial.print("HB ");
    Serial.print(last_ts);
    Serial.print(" temp=");
    Serial.print(last_temp_ok ? String(last_temp, 2) : "NA");
    Serial.print(" active=");
    Serial.println(active_file.length() ? active_file : String("-"));
    last_heartbeat_ms = now;
  }
}
