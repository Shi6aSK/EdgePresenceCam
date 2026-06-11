# EdgePresenceCam — technical README

This repository contains the firmware and tools for EdgePresenceCam: an edge device built around the Seeed XIAO ESP32 S3 that integrates a 24GHz mmWave presence sensor (LD2410-like), an esp_camera module (MJPEG), and a small audio-capture flow persisted to LittleFS. The project demonstrates a wake-on-presence pipeline: radar frame -> parser -> wake camera/microphone -> capture -> local storage and optional proxy upload.

This README is the technical GitHub README (setup, wiring, build, run, endpoints, and troubleshooting). The formal project report and slides are separate artifacts.

Contents
- Firmware: `CPRE4580.ino` (main), `radar.cpp/h`, `web.cpp/h`, `camera.cpp/h`, `modules/` (audio, cloud, manager).  
- Tools: `tools/esp_proxy.py` — laptop proxy to forward audio to OpenAI.
- Tests: `mmwave_test/` — `mmwave_test.ino` (library example) and `mmwave_sniffer.ino` (UART sniffer).

Requirements
- Hardware
  - Seeed XIAO ESP32 S3 (or compatible XIAO ESP32 board)
  - 24GHz mmWave presence sensor (LD2410-like)
  - Camera module supported by `esp_camera` (check `camera_pins.h` for pin mapping)
  - USB cable, common ground between sensor and XIAO
- Software (development)
  - Arduino IDE (or `arduino-cli`) with ESP32 board support (esp32:esp32)
  - Python 3 (for `tools/esp_proxy.py`), requests & flask as required
  - Optional: HLKRadarTool (mobile app) to change sensor baud (recommended when using SoftwareSerial)

Wiring (recommended)
- Sensor TX -> XIAO RX (D2)
- Sensor RX <- XIAO TX (D3)
- Common GND
- Camera pins: see [camera_pins.h](camera_pins.h) for board-specific mapping

Notes about baud
- Factory sensor baud is 256000. XIAO software serial implementations may not reliably handle 256k; recommended: set sensor to 9600 with HLKRadarTool for the example sketches that use software serial. For production/demo use, prefer hardware `Serial1` (the firmware uses `Serial1`) and 256000 may work depending on board variant.

Install libraries (recommended)
1. Clone / add libraries to Arduino libraries folder:

```powershell
cd "$env:USERPROFILE\Documents\Arduino\libraries"
git clone https://github.com/limengdu/mmwave_for_XIAO.git mmwave_for_XIAO
git clone https://github.com/plerup/espsoftwareserial.git espsoftwareserial  # optional
```

2. Restart the Arduino IDE.

Build & upload (Arduino IDE)
- Open `CPRE4580.ino` in Arduino IDE, select board `XIAO_ESP32S3` (or your XIAO variant), set upload speed and COM port, then Upload.

Build & upload (arduino-cli)

```powershell
cd 'C:\Users\ssk20\Documents\Arduino\CPRE4580'
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3
arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32S3 -p COM3
```

Running the system (quickstart)
1. Power the XIAO and confirm it connects to Wi‑Fi (check Serial at 115200 for IP).  
2. Open the dashboard at `http://<device-ip>/` (contains MJPEG stream and controls).  
3. Use `/radar/raw` to inspect last raw frame hex and `/radar/history` for recent events.  

Key HTTP endpoints
- `/` — dashboard UI (camera, radar status, audio controls)
- `/stream` — MJPEG camera stream
- `/radar` — JSON current radar summary
- `/radar/raw` — raw last frame hex + legacy_info
- `/radar/history` — circular history JSON of recent RadarEvent entries
- `/uart/sniff` — toggle UART sniffer on/off (prints hex to Serial when enabled)
- `/audio/start` — trigger a local audio capture (WAV saved to LittleFS)
- `/audio/latest.wav` — download last saved WAV
- `/audio/upload` — device-initiated upload to configured proxy or direct OpenAI (proxy recommended)

Using the proxy for OpenAI uploads (recommended)
1. On your laptop, run the proxy in `tools/`:

```powershell
cd 'C:\Users\ssk20\Documents\Arduino\CPRE4580\tools'
python .\esp_proxy.py --key <OPENAI_KEY> --port 5000
```

2. Configure the device proxy URL via the dashboard (`/audio/proxy` endpoint) or edit `web.cpp` default.
3. Trigger `/audio/upload` from the device; the proxy will save uploaded files into `tools/uploads/` and forward to OpenAI.

Testing & diagnostics
- UART sniffer: flash `mmwave_test/mmwave_sniffer.ino`, open Serial Monitor at 115200. It prints incoming bytes from `Serial1` in hex and ASCII — use this to confirm wiring, baud, and frame content.
- Library test: `mmwave_test/mmwave_test.ino` uses `Seeed_HSP24` library to exercise the sensor API (requires `mmwave_for_XIAO` library installed).
- Logs: the firmware maintains an in-memory circular `log` and exposes `/logs` for debugging. `log_append()` is used in code paths to record events.

Developer notes (key files)
- `CPRE4580.ino` — main initialization and task startup
- `radar.cpp`, `radar.h` — Serial1 reading, frame parsing, history buffer, and sniffer
- `web.cpp`, `web.h` — HTTP endpoints and dashboard assets
- `camera.cpp`, `camera.h`, `camera_pins.h` — camera init and pin definitions
- `modules/audio.cpp` — WAV writer and LittleFS handling
- `log.cpp`, `log.h` — simple circular logging API
- `tools/esp_proxy.py` — laptop relay for audio uploads to OpenAI

Troubleshooting
- No bytes visible in sniffer: check wiring (sensor TX → XIAO RX), common ground, power to sensor. Try both 9600 and 256000 baud. If RX floats, enable hardware pull-down or add small pulldown resistor.  
- Garbled frames: likely wrong baud or software serial limits; switch to hardware `Serial1` or lower sensor baud using HLKRadarTool.  
- LittleFS errors: ensure LittleFS.begin() is called; if fail, firmware attempts format on first mount (see `modules/audio.cpp`).  
- OpenAI upload failures: check proxy logs in `tools/uploads/`, verify API key and that proxy forwarded request matches OpenAI multipart/form-data expectations. Proxy logs are printed to console.

Security and privacy notes
- Audio and camera data can be sensitive. The default behavior saves audio locally; uploads are performed only if proxy or direct upload is configured. Keep API keys off the device where possible and use the laptop proxy approach.

Contact / Support
- For help, provide serial logs and `/radar/raw` output when opening an issue.
