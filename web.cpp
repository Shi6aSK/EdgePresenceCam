#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include "stats.h"
#include "log.h"
#include "presence.h"
#include "camera.h"
#include "radar.h"
#include "esp_eap_client.h"
#include "esp_wifi.h"
#include "esp_camera.h"
#include "modules/audio.h"
#include <LittleFS.h>

static WebServer server(80);
static TaskHandle_t s_webTask = NULL;

// WPA2-Enterprise credentials (use only for testing; consider secure storage)
static const char *sta_ssid = "eduroam";
static const char *sta_username = "shiba@iastate.edu";
static const char *sta_password = "ShobhSK@CE24";

static bool configureWiFiEnterprise() {
  Serial.printf("web: attempting WPA2-Enterprise connect to %s\n", sta_ssid);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setSleep(false);

  // Set identity/username/password for EAP
  esp_eap_client_set_identity((uint8_t*)sta_username, strlen(sta_username));
  esp_eap_client_set_username((uint8_t*)sta_username, strlen(sta_username));
  esp_eap_client_set_password((uint8_t*)sta_password, strlen(sta_password));
  esp_wifi_sta_enterprise_enable();

  WiFi.begin(sta_ssid);

  const uint32_t connectTimeoutMs = 30000;
  uint32_t startAttempt = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - startAttempt < connectTimeoutMs)) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("web: Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("web: Wi-Fi connection timed out.");
  return false;
}

static void handle_status() {
  String body = "{";
  body += "\"presence_percent\":" + String(stats_get_last_presence_percent());
  body += ",\"capture_count\":" + String(stats_get_capture_count());
  EventGroupHandle_t eg = presence_get_event_group();
  int present = 0;
  if (eg) {
    present = (xEventGroupGetBits(eg) & PRESENCE_BIT) ? 1 : 0;
  }
  body += ",\"presence_bit\":" + String(present);
  body += ",\"camera_connected\":" + String(camera_is_connected() ? 1 : 0);
  body += ",\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? 1 : 0);
  if (WiFi.status() == WL_CONNECTED) {
    body += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  }
  body += ",\"uptime_ms\":" + String(millis());
  body += "}";
  server.send(200, "application/json", body);
}

static void handle_logs() {
  String j = log_get_json();
  server.send(200, "application/json", j);
}

static void handle_trigger() {
  // manual trigger endpoint
  camera_trigger_manual();
  server.send(200, "text/plain", "OK");
}

static void webTask(void* pv) {
  Serial.println("web: task started");
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void web_init() {
  // Try to join STA with WPA2‑Enterprise first. If it fails, fall back to SoftAP.
  bool joined = configureWiFiEnterprise();
  if (!joined) {
    const char* apName = "CPRE4580_AP";
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(apName);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("web: Access Point '%s' started, IP=%s\n", apName, ip.toString().c_str());
  }

  server.on("/status", HTTP_GET, handle_status);
  server.on("/logs", HTTP_GET, handle_logs);
  server.on("/trigger", HTTP_POST, handle_trigger);

  // Camera status endpoint
  server.on("/camera", HTTP_GET, []() {
    String body = "{";
    body += "\"connected\":" + String(camera_is_connected() ? 1 : 0);
    body += "}";
    server.send(200, "application/json", body);
  });

  // Start an audio recording (simulated or real) for duration_ms
  server.on("/audio/start", HTTP_GET, []() {
    // query param ?dur=3000
    uint32_t dur = 3000;
    if (server.hasArg("dur")) dur = (uint32_t)atoi(server.arg("dur").c_str());
    // Temporarily disable UART sniffer to avoid hex spam on Serial while
    // capturing audio. Preserve previous sniffer state.
    bool prevSniff = radar_get_sniffer();
    if (prevSniff) radar_set_sniffer(false);
    audio_start_recording(dur);
    if (prevSniff) radar_set_sniffer(true);
    server.send(200, "application/json", String("{\"started\":1,\"dur\":" + String(dur) + "}"));
  });

  // Set or get the OpenAI API key used for transcription. POST body can be
  // the raw key or form field `key=<key>`. Key is stored in LittleFS at
  // `/openai_key.txt`. GET returns whether a key exists (not the key value).
  server.on("/audio/apikey", HTTP_ANY, []() {
    if (server.method() == HTTP_GET) {
      if (!LittleFS.begin()) LittleFS.begin();
      bool has = LittleFS.exists("/openai_key.txt");
      server.send(200, "application/json", String("{\"has_key\":" + String(has?1:0) + "}"));
      return;
    }
    // POST/PUT - store key
    String body = server.hasArg("plain") ? server.arg("plain") : String();
    // If form-encoded, parse `key=` param
    if (body.length() == 0 && server.hasArg("key")) body = server.arg("key");
    body.trim();
    if (body.length() == 0) {
      server.send(400, "text/plain", "missing key");
      return;
    }
    if (!LittleFS.begin()) LittleFS.begin();
    File f = LittleFS.open("/openai_key.txt", FILE_WRITE);
    if (!f) { server.send(500, "text/plain", "fs open failed"); return; }
    f.print(body);
    f.close();
    server.send(200, "application/json", "{\"saved\":1}");
  });

  // Set or get the proxy URL used by the ESP to upload audio. POST body can
  // be the raw proxy URL like "http://192.168.1.42:5000/upload" or form
  // field `proxy=`. The value is stored in LittleFS at `/upload_proxy.txt`.
  server.on("/audio/proxy", HTTP_ANY, []() {
    const char* proxyPath = "/upload_proxy.txt";
    if (server.method() == HTTP_GET) {
      if (!LittleFS.begin()) LittleFS.begin();
      bool has = LittleFS.exists(proxyPath);
      String body = String("{\"has_proxy\":") + String(has?1:0) + "}";
      server.send(200, "application/json", body);
      return;
    }
    String body = server.hasArg("plain") ? server.arg("plain") : String();
    if (body.length() == 0 && server.hasArg("proxy")) body = server.arg("proxy");
    body.trim();
    if (body.length() == 0) { server.send(400, "text/plain", "missing proxy"); return; }
    if (!LittleFS.begin()) LittleFS.begin();
    File f = LittleFS.open(proxyPath, FILE_WRITE);
    if (!f) { server.send(500, "text/plain", "fs open failed"); return; }
    f.print(body);
    f.close();
    server.send(200, "application/json", "{\"saved\":1}");
  });

  // Download latest WAV
  server.on("/audio/latest.wav", HTTP_GET, []() {
    const char* path = "/audio/latest.wav";
    if (!LittleFS.begin()) LittleFS.begin();
    if (!LittleFS.exists(path)) {
      server.send(404, "text/plain", "no audio");
      return;
    }
    File f = LittleFS.open(path, FILE_READ);
    server.streamFile(f, "audio/wav");
    f.close();
  });

  // Upload latest WAV to OpenAI transcription API (direct) or to a configured
  // intermediary proxy (plain HTTP). If /upload_proxy.txt exists, the ESP
  // will POST to that URL instead of OpenAI. This allows a laptop to relay
  // the file and perform TLS to OpenAI for us.
  server.on("/audio/upload", HTTP_POST, []() {
    const char* keyPath = "/openai_key.txt";
    const char* wavPath = "/audio/latest.wav";
    const char* proxyPath = "/upload_proxy.txt";
    if (!LittleFS.begin()) LittleFS.begin();
    if (!LittleFS.exists(wavPath)) {
      server.send(404, "application/json", "{\"error\":\"no_audio\"}");
      return;
    }
    File wf = LittleFS.open(wavPath, FILE_READ);
    if (!wf) { server.send(500, "application/json", "{\"error\":\"audio_open_failed\"}"); return; }

    // If a proxy is configured, POST the multipart to that proxy using plain HTTP.
    if (LittleFS.exists(proxyPath)) {
      File pf = LittleFS.open(proxyPath, FILE_READ);
      if (!pf) { wf.close(); server.send(500, "application/json", "{\"error\":\"proxy_open_failed\"}"); return; }
      String proxy = pf.readString(); pf.close(); proxy.trim();
      if (proxy.startsWith("http://")) proxy = proxy.substring(7);
      int slash = proxy.indexOf('/');
      String hostport = (slash >= 0) ? proxy.substring(0, slash) : proxy;
      String path = (slash >= 0) ? proxy.substring(slash) : String("/");
      String host = hostport;
      int port = 80;
      int colon = hostport.indexOf(':');
      if (colon >= 0) { host = hostport.substring(0, colon); port = hostport.substring(colon+1).toInt(); }

      String boundary = "----ESP32FormBoundary";
      String preamble = String("--") + boundary + "\r\n";
      preamble += "Content-Disposition: form-data; name=\"file\"; filename=\"latest.wav\"\r\n";
      preamble += "Content-Type: audio/wav\r\n\r\n";
      String epilogue = "\r\n" + String("--") + boundary + "--\r\n";
      String modelPart = String("--") + boundary + "\r\n";
      modelPart += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
      modelPart += "whisper-1\r\n";

      size_t fileSize = wf.size();
      size_t contentLen = preamble.length() + fileSize + epilogue.length() + modelPart.length();

      WiFiClient client;
      if (!client.connect(host.c_str(), port)) { wf.close(); server.send(502, "application/json", "{\"error\":\"proxy_connect_failed\"}"); return; }
      client.printf("POST %s HTTP/1.1\r\n", path.c_str());
      client.printf("Host: %s\r\n", host.c_str());
      client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
      client.printf("Content-Length: %u\r\n", (unsigned int)contentLen);
      client.print("Connection: close\r\n\r\n");
      client.print(modelPart);
      client.print(preamble);
      const size_t bufSz = 1024;
      uint8_t buf[bufSz];
      while (wf.available()) {
        size_t r = wf.read(buf, bufSz);
        if (r == 0) break;
        client.write(buf, r);
      }
      wf.close();
      client.print(epilogue);
      unsigned long start = millis(); String resp;
      while (client.connected() && (millis() - start) < 30000) {
        while (client.available()) { resp += (char)client.read(); start = millis(); }
        delay(5);
      }
      client.stop();
      int idx = resp.indexOf("\r\n\r\n"); String body = resp; if (idx >= 0) body = resp.substring(idx + 4);
      server.send(200, "application/json", body);
      return;
    }

    // No proxy configured: attempt direct OpenAI TLS upload (existing behavior)
    if (!LittleFS.exists(keyPath)) {
      wf.close();
      server.send(400, "application/json", "{\"error\":\"no_api_key\"}");
      return;
    }
    File kf = LittleFS.open(keyPath, FILE_READ);
    if (!kf) { wf.close(); server.send(500, "application/json", "{\"error\":\"key_open_failed\"}"); return; }
    String key = kf.readString(); kf.close(); key.trim();

    const char* host = "api.openai.com";
    const char* path = "/v1/audio/transcriptions";
    String boundary = "----ESP32FormBoundary";
    String preamble = String("--") + boundary + "\r\n";
    preamble += "Content-Disposition: form-data; name=\"file\"; filename=\"latest.wav\"\r\n";
    preamble += "Content-Type: audio/wav\r\n\r\n";
    String epilogue = "\r\n" + String("--") + boundary + "--\r\n";
    String modelPart = String("--") + boundary + "\r\n";
    modelPart += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    modelPart += "whisper-1\r\n";
    size_t fileSize = wf.size();
    size_t contentLen = preamble.length() + fileSize + epilogue.length() + modelPart.length();

    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(host, 443)) { wf.close(); server.send(502, "application/json", "{\"error\":\"tls_connect_failed\"}"); return; }
    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", key.c_str());
    client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    client.printf("Content-Length: %u\r\n", (unsigned int)contentLen);
    client.print("Connection: close\r\n\r\n");
    client.print(modelPart);
    client.print(preamble);
    const size_t bufSz2 = 1024;
    uint8_t buf2[bufSz2];
    while (wf.available()) {
      size_t r = wf.read(buf2, bufSz2);
      if (r == 0) break;
      client.write(buf2, r);
    }
    wf.close();
    client.print(epilogue);
    unsigned long start = millis(); String resp;
    while (client.connected() && (millis() - start) < 15000) {
      while (client.available()) { resp += (char)client.read(); start = millis(); }
      delay(10);
    }
    client.stop();
    int idx = resp.indexOf("\r\n\r\n"); String body = resp; if (idx >= 0) body = resp.substring(idx + 4);
    server.send(200, "application/json", body);
  });

  // Serve a single-frame JPEG (clients can refresh periodically)
  server.on("/camera.jpg", HTTP_GET, []() {
    if (!camera_is_connected()) {
      server.send(503, "text/plain", "camera not available");
      return;
    }
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      log_append("camera capture failed");
      server.send(500, "text/plain", "camera capture failed");
      return;
    }
    // Write raw HTTP response with headers then JPEG bytes so clients receive
    // a proper binary JPEG image.
    WiFiClient client = server.client();
    if (!client) {
      esp_camera_fb_return(fb);
      server.send(500, "text/plain", "no client");
      return;
    }
    // Send status line and headers
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: "); client.print((unsigned int)fb->len); client.print("\r\n");
    client.print("Connection: close\r\n\r\n");
    // Send body
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    // allow server to finish sending
    delay(10);
  });

  // Minimal dashboard page with live-updating image and radar text
  server.on("/", HTTP_GET, []() {
    const char* page = R"HTML(<!doctype html>
  <html>
  <head>
    <meta name=viewport content='width=device-width,initial-scale=1'>
    <title>CPRE4580 Dashboard</title>
    <style>body{font-family:Arial;margin:12px;}img{max-width:100%;border-radius:8px;}pre{background:#111;color:#dcd;padding:8px;}button{padding:6px 10px;margin:6px}</style>
  </head>
  <body>
    <h2>CPRE4580 Dashboard</h2>
    <div>
      <h3>Camera</h3>
      <img id=cam src=/camera.jpg alt='camera' />
      <div><button onclick=reload()>Refresh</button> <small>Auto-refresh every 2s</small></div>
      <pre id=status>loading status...</pre>
    </div>
    <div>
      <h3>Radar</h3>
      <pre id=radar>loading...</pre>
    </div>
    <div>
      <h3>Logs</h3>
      <pre id=logs>loading logs...</pre>
    </div>
    <div>
      <h3>Audio / Transcription</h3>
      <div>
        <label>Proxy URL (laptop relay): </label>
        <input id=proxy type=text style="width:360px" placeholder="http://192.168.1.42:5000/upload" />
        <button onclick=setProxy()>Save Proxy</button>
        <span id=keyStatus style="margin-left:12px;color:#666"></span>
      </div>
      <div style="margin-top:8px">
        <label>Duration ms:</label>
        <input id=aud_dur type=number value=3000 style="width:100px" />
        <button onclick=startCapture()>Start Capture</button>
        <button onclick=uploadTranscribe()>Upload & Transcribe</button>
      </div>
      <div style="margin-top:8px">
        <h4>Transcription</h4>
        <pre id=transcript>no transcription yet</pre>
      </div>
      <div style="margin-top:12px">
        <h4>OpenAI Text Test</h4>
        <textarea id=openai_prompt style="width:100%;height:80px" placeholder="Enter prompt to send to OpenAI..."></textarea>
        <div style="margin-top:6px">
          <button onclick=sendOpenAITest()>Send Prompt</button>
          <span style="margin-left:12px;color:#666">(uses stored API key at /openai_key.txt)</span>
        </div>
        <div style="margin-top:8px">
          <h5>Response</h5>
          <pre id=openai_response>no response yet</pre>
        </div>
      </div>
    </div>
    <script>
      async function fetchStatus(){
        try{
          const r=await fetch('/status'); const j=await r.json();
          document.getElementById('status').textContent = JSON.stringify(j, null, 2);
        }catch(e){document.getElementById('status').textContent='status error'}
      }
      async function fetchRadar(){
        try{const r=await fetch('/radar'); const j=await r.json(); document.getElementById('radar').textContent = 'state:'+j.state+' '+j.name+' | transferring:'+ (j.transferring?1:0) +' queue:'+j.queue_size;}catch(e){document.getElementById('radar').textContent='radar error'}
      }
      async function fetchLogs(){
        try{
          const r=await fetch('/logs'); const j=await r.json(); let out='';
          if (Array.isArray(j)) {
            j.forEach(function(e){
              if (typeof e === 'string') {
                out += e + '\n';
              } else if (typeof e === 'object' && e !== null) {
                out += '[' + (e.ts||'') + '] ' + (e.msg||'') + '\n';
              }
            });
          }
          document.getElementById('logs').textContent = out;
        }catch(e){document.getElementById('logs').textContent='logs error'}
      }

      async function setProxy(){
        try{
          const p = document.getElementById('proxy').value;
          if (!p || p.length < 7) { alert('enter proxy url'); return; }
          const r = await fetch('/audio/proxy', { method:'POST', body:p });
          if (!r.ok) throw new Error('save failed');
          document.getElementById('keyStatus').textContent = 'proxy saved';
        }catch(e){ alert('save proxy failed'); }
      }

      async function startCapture(){
        try{
          const dur = document.getElementById('aud_dur').value || 3000;
          const r = await fetch('/audio/start?dur=' + encodeURIComponent(dur));
          if (r.ok) {
            document.getElementById('transcript').textContent = 'capturing...';
          } else {
            document.getElementById('transcript').textContent = 'capture failed';
          }
        } catch(e) { document.getElementById('transcript').textContent = 'capture error'; }
      }

      async function uploadTranscribe(){
        try{
          document.getElementById('transcript').textContent = 'uploading...';
          const r = await fetch('/audio/upload', { method:'POST' });
          if (!r.ok) {
            document.getElementById('transcript').textContent = 'upload failed: ' + r.status;
            return;
          }
          const j = await r.json();
          document.getElementById('transcript').textContent = JSON.stringify(j, null, 2);
        }catch(e){ document.getElementById('transcript').textContent = 'transcribe error'; }
      }

      async function sendOpenAITest(){
        try{
          const prompt = document.getElementById('openai_prompt').value || '';
          if (!prompt || prompt.length < 2) { alert('enter a prompt'); return; }
          document.getElementById('openai_response').textContent = 'sending...';
          const r = await fetch('/openai/test', { method:'POST', body: prompt });
          if (!r.ok) { document.getElementById('openai_response').textContent = 'request failed: ' + r.status; return; }
          const j = await r.json();
          document.getElementById('openai_response').textContent = JSON.stringify(j, null, 2);
        } catch(e) { document.getElementById('openai_response').textContent = 'error'; }
      }

      async function fetchKeyStatus(){
      try{ const r = await fetch('/audio/proxy'); const j = await r.json(); document.getElementById('keyStatus').textContent = j.has_proxy? 'proxy set':'no proxy'; }catch(e){document.getElementById('keyStatus').textContent='?'}
    }

      function refreshImg(){var i=document.getElementById('cam'); i.src='/stream?ts='+Date.now();}

      // Draw a simple timeline of recent radar states in a canvas
      function drawHistory(hist) {
        const c = document.getElementById('radarCanvas');
        if (!c) return;
        const ctx = c.getContext('2d');
        const w = c.width = 600; const h = c.height = 80;
        ctx.clearRect(0,0,w,h);
        ctx.fillStyle = '#111'; ctx.fillRect(0,0,w,h);
        function yForState(s){
          switch(s){case 0: return h-6; case 1: return h*0.66; case 2: return h*0.33; case 3: return 6; default: return h/2;}
        }
        if (!hist || hist.length==0) return;
        const startTs = hist[0].ts;
        const endTs = hist[hist.length-1].ts;
        const span = Math.max(1, endTs - startTs);
        ctx.strokeStyle='#0f0'; ctx.lineWidth=2; ctx.beginPath();
        for(let i=0;i<hist.length;i++){
          const x = Math.floor((hist[i].ts - startTs)/span * (w-10)) + 5;
          const y = yForState(hist[i].state);
          if (i==0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
        }
        ctx.stroke();
      }

      async function fetchHistory(){
        try{
          const r = await fetch('/radar/history'); const j = await r.json();
          drawHistory(j);
        }catch(e){}
      }

      function reload(){ refreshImg(); fetchStatus(); fetchRadar(); fetchLogs(); fetchKeyStatus(); fetchHistory(); }
      setInterval(reload,2000);
      window.onload = reload;
    </script>
  <div style="margin-top:12px">
    <canvas id="radarCanvas" width=600 height=80 style="border:1px solid #333;background:#111"></canvas>
  </div>
  </body>
  </html>)HTML";
    server.send(200, "text/html", page);
  });

  // Radar status endpoint
  server.on("/radar", HTTP_GET, []() {
    uint8_t s = radar_get_last_state();
    const char* name = radar_get_last_state_name();
    bool transferring = radar_is_transferring();
    int q = radar_get_queue_size();
    String body = "{";
    body += "\"state\":" + String(s);
    body += ",\"name\":\"" + String(name) + "\"";
    body += ",\"transferring\":" + String(transferring ? 1 : 0);
    body += ",\"queue_size\":" + String(q);
    body += "}";
    server.send(200, "application/json", body);
  });

  // Radar history endpoint for plotting
  server.on("/radar/history", HTTP_GET, []() {
    String j = radar_get_history_json();
    server.send(200, "application/json", j);
  });

  // Simple OpenAI test endpoint: POST raw text body -> chat completion using
  // stored API key at /openai_key.txt. Returns JSON from OpenAI or error.
  server.on("/openai/test", HTTP_ANY, []() {
    if (server.method() != HTTP_POST) { server.send(405, "text/plain", "only POST"); return; }
    String prompt = server.hasArg("plain") ? server.arg("plain") : String();
    if (prompt.length() == 0) { server.send(400, "application/json", "{\"error\":\"missing_prompt\"}"); return; }
    const char* keyPath = "/openai_key.txt";
    if (!LittleFS.begin()) LittleFS.begin();
    if (!LittleFS.exists(keyPath)) { server.send(400, "application/json", "{\"error\":\"no_api_key\"}"); return; }
    File kf = LittleFS.open(keyPath, FILE_READ);
    if (!kf) { server.send(500, "application/json", "{\"error\":\"key_open_failed\"}"); return; }
    String key = kf.readString(); kf.close(); key.trim();

    const char* host = "api.openai.com";
    const char* path = "/v1/chat/completions";
    String body = String("{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"user\",\"content\":\"") + prompt + "\"}],\"max_tokens\":150}";

    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(host, 443)) { server.send(502, "application/json", "{\"error\":\"tls_connect_failed\"}"); return; }
    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", key.c_str());
    client.print("Content-Type: application/json\r\n");
    client.printf("Content-Length: %u\r\n", (unsigned int)body.length());
    client.print("Connection: close\r\n\r\n");
    client.print(body);

    unsigned long start = millis(); String resp;
    while (client.connected() && (millis() - start) < 15000) {
      while (client.available()) { resp += (char)client.read(); start = millis(); }
      delay(5);
    }
    client.stop();
    int idx = resp.indexOf("\r\n\r\n"); String respBody = resp;
    if (idx >= 0) respBody = resp.substring(idx + 4);
    if (respBody.length() == 0) { server.send(502, "application/json", "{\"error\":\"no_response\"}"); return; }
    server.send(200, "application/json", respBody);
  });

  // Raw radar frame / debug endpoint
  server.on("/radar/raw", HTTP_GET, []() {
    String body = "{";
    body += "\"last_frame_hex\":\"" + radar_get_last_frame_hex() + "\"";
    body += ",\"serial_available\":" + String(radar_get_serial_available());
    body += ",\"queue_size\":" + String(radar_get_queue_size());
    body += ",\"legacy_info\":\"" + String(radar_get_last_legacy_info()) + "\"";
    body += "}";
    server.send(200, "application/json", body);
  });

  // Simple UART sniffer control endpoint: /uart/sniff?on=1 or on=0
  server.on("/uart/sniff", HTTP_GET, []() {
    bool on = false;
    if (server.hasArg("on")) on = (server.arg("on") == "1");
    radar_set_sniffer(on);
    String body = "{";
    body += "\"sniffing\":" + String(on ? 1 : 0);
    body += ",\"serial_available\":" + String(radar_get_serial_available());
    body += "}";
    server.send(200, "application/json", body);
  });

  // MJPEG stream endpoint
  server.on("/stream", HTTP_GET, []() {
    if (!camera_is_connected()) {
      server.send(503, "text/plain", "camera not available");
      return;
    }
    WiFiClient client = server.client();
    String boundary = "--FRAMEBOUNDARY";
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: multipart/x-mixed-replace; boundary="); client.print(boundary); client.print("\r\n");
    client.print("Connection: close\r\n\r\n");
    while (client.connected()) {
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        delay(50);
        continue;
      }
      client.print(boundary); client.print("\r\n");
      client.print("Content-Type: image/jpeg\r\n");
      client.print("Content-Length: "); client.print((unsigned int)fb->len); client.print("\r\n\r\n");
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      esp_camera_fb_return(fb);
      // small delay to yield
      delay(100);
    }
    // close connection
    client.stop();
  });

  server.begin();

  if (s_webTask == NULL) {
    xTaskCreatePinnedToCore(webTask, "t_web", 4096, NULL, 1, &s_webTask, 1);
  }
}
