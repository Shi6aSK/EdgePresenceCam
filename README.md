# EdgePresenceCam — firmware & developer README

This repository contains the firmware and tools for EdgePresenceCam: a Seeed XIAO ESP32 S3 device that combines a 24 GHz presence radar (LD2410-like), an ESP camera (MJPEG), and a small audio capture flow persisted to LittleFS. This README focuses on practical setup, build, run, and diagnostics for developers and graders.

Quick summary
- Firmware entry: `CPRE4580.ino`
- Key modules: `radar.*`, `web.*`, `camera.*`, `modules/audio.*`, `log.*`
- Tools: `tools/esp_proxy.py` — laptop proxy to forward audio to OpenAI and save uploads at `tools/uploads/`

Requirements
- Hardware: Seeed XIAO ESP32 S3 (or compatible), LD2410-like mmWave sensor, compatible camera module, USB cable, common ground between sensor and XIAO.
- Software: Arduino IDE or `arduino-cli` with esp32 board package; Python 3 for the proxy (Flask + requests). Optional: HLKRadarTool for changing radar baud.

Wiring (recommended)
- Sensor TX -> XIAO RX (D2)
- Sensor RX <- XIAO TX (D3)
- Common GND
- Camera pins: see [camera_pins.h](camera_pins.h)

Baud notes
- The sensor ships at 256000 baud. Use hardware `Serial1` on the XIAO for 256000. For software-serial examples, set sensor to 9600 with HLKRadarTool.

Install libraries
1. Clone required libraries into your Arduino libraries folder (if you haven't):

```powershell
cd "$env:USERPROFILE\Documents\Arduino\libraries"
git clone https://github.com/limengdu/mmwave_for_XIAO.git mmwave_for_XIAO
```

Build & upload
- Arduino IDE: open `CPRE4580.ino`, select `XIAO_ESP32S3`, compile & upload.
- arduino-cli:

```powershell
cd 'C:\Users\ssk20\Documents\Arduino\CPRE4580'
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3
arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32S3 -p COM3
```

Run quickstart
1. Power the board and watch Serial at 115200 for Wi‑Fi or AP IP.
2. Open `http://<device-ip>/` — dashboard includes MJPEG preview, radar status, logs, and audio controls.

HTTP endpoints (most useful)
- `/` — dashboard UI (camera, radar plot, OpenAI test box, audio controls)
- `/stream` — MJPEG stream (multipart)
- `/camera.jpg` — single JPEG snapshot
- `/status` — JSON device status
- `/radar` — current radar summary JSON
- `/radar/raw` — last raw frame hex + legacy info
- `/radar/history` — JSON circular history (used by dashboard canvas)
- `/uart/sniff?on=1` — enable UART sniffer (prints hex to Serial)
- `/audio/start?dur=3000` — start local audio capture (ms)
- `/audio/latest.wav` — download last WAV
- `/audio/proxy` — GET/POST to set proxy URL stored at `/upload_proxy.txt`
- `/audio/apikey` — POST to store OpenAI key at `/openai_key.txt` (GET returns presence)
- `/audio/upload` — POST triggered upload/transcribe (uses proxy if configured)
- `/openai/test` — POST raw prompt text; device uses stored OpenAI key to call Chat Completions and returns JSON

Dashboard features
- MJPEG preview (via `/stream`) and single-shot image refresh (`/camera.jpg`).
- Radar timeline canvas (uses `/radar/history`) showing recent states.
- OpenAI test box: enter a prompt, click "Send Prompt" — device will call `/openai/test` and display returned JSON.

Using the proxy (recommended for OpenAI uploads)
1. On your laptop, run the proxy in `tools/`:

```powershell
cd 'C:\Users\ssk20\Documents\Arduino\CPRE4580\tools'
python .\esp_proxy.py --key <OPENAI_KEY> --port 5000
```

2. Set device proxy via dashboard or:

```powershell
curl -X POST http://<device-ip>/audio/proxy -d "http://<laptop-ip>:5000/upload"
```

3. Trigger `/audio/upload` on the device; the proxy saves uploaded files to `tools/uploads/` and forwards to OpenAI.

Testing & diagnostics
- UART sniffer: flash `mmwave_test/mmwave_sniffer.ino` and open Serial Monitor at 115200 to capture raw UART bytes.
- Raw frames: GET `/radar/raw` to fetch `last_frame_hex` for parser tuning.
- Logs: GET `/logs` for recent trace entries (the firmware uses `log_append()` in key paths).
- Capture test: click "Start Capture" on dashboard and then "Upload & Transcribe" to exercise the upload flow.

Troubleshooting
- No bytes in sniffer: check sensor TX->XIAO RX wiring, common ground, and sensor power. Try 9600 and 256000 baud.
- Garbled frames: likely baud mismatch or software-serial limit — use hardware `Serial1` and 256000 where possible.
- LittleFS issues: firmware tries to mount and format on first run; check Serial for errors.
- OpenAI errors: check `tools/uploads/` and proxy console for saved uploads and forwarding errors. Verify API key and scopes.

Developer notes
- Relevant files: `CPRE4580.ino`, `radar.cpp/h`, `web.cpp/h`, `camera.cpp/h`, `modules/audio.cpp`, `log.cpp/h`, `tools/esp_proxy.py`.
- To collect radar samples for parser tuning: enable the UART sniffer, capture several frames from Serial, and paste hex output into an issue or to the repo's `docs/` folder.

Contributing & License
- Fork, branch, and PR. Add hardware photos and sniffer logs in `docs/` when improving parser.
- Add a `LICENSE` file at repo root for your chosen license.

Contact
- When reporting issues, include Serial logs and `/radar/raw` output.

# EdgePresenceCam — XIAO ESP32 S3 mmWave + Camera Integration

Project type: Embedded IoT + Real-Time Demonstration

Project tagline: Edge wake-on-presence with camera and audio capture on XIAO ESP32 S3.

---

## Title Page

- **Project Title:** EdgePresenceCam
- **Project Type:** Embedded IoT + Real-Time Demonstration
- **Student name(s):** Shobhit Singh — Course 4580
- **Course / Department / University / Semester:** [Course], [Department], [University], [Semester]
- **Date:** [DATE]
- **One-line tagline:** Edge wake-on-presence + camera streaming

---

## Abstract (≈250 words)

Problem: Integrate an LD2410-like 24GHz mmWave presence sensor with a XIAO ESP32 S3 to produce reliable presence detection that can wake a camera and microphone, stream MJPEG video, persist audio to LittleFS, and provide web-based diagnostics.

Approach: We implemented a hardware-Serial (`Serial1`) parser task for the radar, a FreeRTOS-based pipeline (radarTask → transferTask) that sets EventGroup bits and enqueues RadarEvents, an `esp_camera` MJPEG stream served by an Arduino WebServer, and a simple WAV writer to LittleFS. To handle cloud ASR we implemented a laptop proxy that accepts `/upload` and forwards to OpenAI (avoiding heavy TLS on the MCU).

Evaluation: The evaluation focuses on real-time metrics: detection-to-wake latency (ms), detection-to-dashboard latency, queue/backpressure behavior under bursts, and resource impacts (task CPU/core split and memory usage). We instrumented the pipeline with timestamps and a circular history buffer to measure these metrics.

Results / Contributions: Delivered a working web dashboard, a UART sniffer and raw-frame endpoint (`/radar/raw`), a circular history (`/radar/history`), LittleFS audio persistence and a proxy-based upload flow. The wake-on-presence flow (radar → camera/mic wake → capture → upload attempt) is functional; transcription via OpenAI failed in our environment (see notes) and is a documented stretch-goal shortfall.

---

## Summary of TA Feedback and Response

TA requested a clearer demonstration of what was accomplished and explicit real-time evaluation. We reorganized the deliverable to present:
- A clear list of base goals achieved (working radar parsing, wake-on-presence flow, MJPEG stream, audio save).  
- Stretch goals and their status (cloud STT attempted but failing due to OpenAI upload/transcription errors in our environment).  
- Evaluation metrics focused on real-time behavior and reproducible measurement steps.

---

## Accomplishments (What we have achieved)

- Radar UART parsing: `radarTask` reads `Serial1` and detects frames; legacy `0x53 0x59` frames and higher-level parses available.  
- Diagnostic endpoints: `/radar/raw` (hex), `/radar/history` (circular JSON), `/uart/sniff` (toggleable sniffer).  
- Wake-on-presence: when presence is detected the system wakes camera + microphone paths to perform capture (camera stream is available via `/stream`; audio saved to `/audio/latest.wav`).  
- Audio persistence: WAV writer writes to LittleFS; `/audio/latest.wav` is downloadable.  
- Upload pipeline: `tools/esp_proxy.py` accepts POST uploads from device and forwards to OpenAI; this avoids TLS on the MCU.  

Known limitation (stretch-goal shortfall): OpenAI transcription did not complete successfully in our tests (proxy forwarded uploads but API calls failed due to API-key/permission/format mismatch or runtime errors). The device-side functionality and proxy are present; transcription is marked as stretched and partially complete.

---

## Base Goals vs Stretch Goals (Status)

Base goals (complete):
- Reliable UART reading on `Serial1` and a working parser skeleton that produces `RadarStatus`.
- Web dashboard with MJPEG `/stream`, `/radar/raw`, `/radar/history`, and `/logs` endpoints.
- Wake-on-presence flow: radar detection triggers camera and audio capture; captures are persisted locally on LittleFS.

Stretch goals (partially/attempted):
- Direct on-device TLS upload to OpenAI (not completed — resource/TLS constraints).  
- Accurate, high-confidence transcription results from OpenAI: attempted via `tools/esp_proxy.py`, but transcription calls failed in our environment — evidence and logs included in Appendix C.  

---

## 1. Introduction (condensed)

Background & Motivation: Presence sensing triggers multimedia capture only when required, saving bandwidth and preserving privacy while enabling richer contextual data. The XIAO ESP32 S3 is attractive for edge demos because it supports camera modules and has enough CPU to run concurrent tasks with FreeRTOS.

High-level problem: perform robust high-baud UART parsing and propagate events quickly enough to wake higher-latency subsystems (camera, audio) while serving network clients.

---

## 2. Real-time Characterization (emphasized)

We explicitly instrumented the pipeline for real-time evaluation: timestamps are recorded at these key points:
- T0: frame byte with complete header/tail parsed in `radarTask` (parse timestamp).  
- T1: `radarTask` enqueues event and sets EventGroup bit.  
- T2: `transferTask` dequeues and records history entry.  
- T3: camera start request issued (if configured to wake camera).  
- T4: audio capture start returned / WAV file closed.  

Primary metrics to report (please replace placeholders with measured values):
- Detection-to-wake latency (T1 − T0): typical = __ ms (measure with `log_append()` entries).  
- Detection-to-dashboard reflect (polling interval dependent): observed ≈ __ ms.  
- Queue occupancy and peak depth during bursts: typical/peak = __ / __.  
- CPU/core use: rough split by task (radarTask high, web tasks lower) — use GPIO toggles and an oscilloscope or FreeRTOS runtime stats to measure.

How we measured: each `RadarEvent` is timestamped and appended to the circular `s_history` and `log` buffer; the `/radar/history` endpoint can be polled to produce timelines. For fine-grain timing, toggle a debug GPIO at T0/T3 and observe with a logic analyzer.

---

## 3. Revised Solution Overview (what was changed per TA)

- Consolidated the README/report to make clear base vs stretch scope.  
- Emphasized measured real-time metrics (latency from parse → camera wake) and provided instructions and placeholders for numeric results.  
- Documented the proxy approach and why we chose it (MCU TLS limitations) and recorded the failure mode for transcription.

---

## 4. Failure Mode: OpenAI Transcription (stretch-goal note)

What we attempted: device uploads `/audio/latest.wav` to `tools/esp_proxy.py` running on a laptop; the proxy forwards the file to OpenAI’s transcription endpoint with an API key.

Observed failure: proxy logged an upload and attempted to forward, but OpenAI responses indicated errors (authentication/formatting or rate-limit/permission). Reproducible logs and saved uploads are in `tools/uploads/` and Appendix C. Root causes to investigate: API key validity/scopes, content-type/form-data formatting, TLS handshake issues, or rate limits. The device-side logic and proxy code are present and can be retried.

Recommendation: treat cloud STT as a stretch goal — document the proxy and provide exact curl commands so graders can repeat the flow with a verified OpenAI key.

---

## 5. How to reproduce our key real-time result (short checklist)

1. Flash `mmwave_test/mmwave_sniffer.ino` to confirm sensor bytes and set sensor baud as recommended (use HLKRadarTool to change to 9600 for soft-serial examples or keep 256000 for hardware Serial1).  
2. Flash `CPRE4580.ino` (project firmware) to the XIAO ESP32 S3.  
3. Connect to device IP and open dashboard `/stream`.  
4. Trigger presence; observe `/radar/raw` and `/radar/history` update and check timestamps in `log` for latency metrics.  
5. If you want to test transcription via proxy: start the proxy on your laptop and POST `/upload` (see `tools/esp_proxy.py --help`) and inspect `tools/uploads/` and proxy logs.

---

## 6. Report & Presentation Next Steps (how I can help now)

I can:
1. Replace the `__` placeholders with measured numbers if you run the timing script and paste logs here.  
2. Commit this folder to a new Git repo and push to GitHub (I will produce exact git commands and a recommended `.gitignore`).  
3. Export a ProjectReport.pdf (from this README) and/or generate the 10-slide presentation skeleton with speaker notes adjusted to highlight the real-time evaluation and TA feedback.

Tell me which of the three you want me to do next (1 = fill numbers from logs you paste, 2 = create/push GitHub repo, 3 = export PDF + create PPTX). I will proceed and show the exact commands I will run.

---

## Appendix pointers

- Appendix C: saved uploads and proxy logs in `tools/uploads/` (for troubleshooting OpenAI failures).  
- Appendix D: build & flash commands:  
  - `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3`  
  - `arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32S3 -p COMx`  
# EdgePresenceCam — XIAO ESP32 S3 mmWave + Camera Integration

Project type: Embedded IoT + Real-Time Demonstration

Project tagline: Edge presence detection and live MJPEG camera with audio capture on a resource-constrained XIAO ESP32 S3.

---

## Title Page

- **Project Title:** EdgePresenceCam
- **Project Type:** Embedded IoT + Real-Time Demonstration
- **Student name(s):** [Your Name] — Course #458/558
- **Course / Department / University / Semester:** [Course], [Department], [University], [Semester]
- **Date:** [DATE]
- **One-line tagline:** Edge presence detection + camera streaming on XIAO ESP32 S3

---

## Abstract (≈250 words)

This project integrates a low-cost 24GHz mmWave presence sensor (LD2410-like) with a Seeed XIAO ESP32 S3 running an onboard camera to provide an edge presence-detection system with a live MJPEG web UI and on-device audio capture. The core problem is to reliably parse high-rate UART frames from the mmWave sensor (factory baud 256000) while concurrently streaming camera video, serving an HTTP dashboard, and persisting short audio clips to LittleFS — all on a small MCU with FreeRTOS. The approach uses Serial1 (hardware UART) for the radar, a modular `radar` task that parses frames and pushes events to a FreeRTOS queue/EventGroup, a transfer worker that coalesces state changes, and an Arduino WebServer-based HTTP API that provides `/radar/raw`, `/radar/history`, `/stream` (MJPEG) and audio endpoints. Audio capture writes a WAV file to LittleFS; uploads are proxied through a laptop to avoid heavy TLS on the MCU. Evaluation captures presence detection latency, event throughput, and resource usage (task CPU/core split and queue activity). Results include a working dashboard, a raw-frame capture endpoint for debugging, a circular history buffer, an optional UART sniffer, and reproducible test harness sketches in `mmwave_test` for frame sniffing and library integration. Key contributions: (1) a practical integration pattern for parsing high-baud UART frames on XIAO ESP32 S3, (2) an end-to-end demo with camera, radar, audio persistence, and a simple proxy-based cloud upload flow, and (3) artifacts and guidance for reproducing and evaluating performance.

---

## 1. Introduction

### Background & Motivation
Presence sensing combined with camera imaging enables many applications (occupancy-aware lighting/HVAC, security, human–machine interfaces). mmWave sensors offer privacy-friendly presence detection (no imagery needed) with resilience in low-light conditions. Integrating a mmWave sensor with an on-board camera allows context-rich edge detection (presence + visual confirmation) while keeping sensitive data local.

### Context
This project targets the Seeed XIAO ESP32 S3 (a tiny dual-core MCU with Wi‑Fi and camera support) connected to a 24GHz mmWave sensor (LD2410-like) sending binary frames over UART. The platform uses the Arduino-ESP32 core and FreeRTOS primitives to coordinate concurrent tasks: radar parsing, camera streaming (MJPEG), HTTP web server, and audio writes to LittleFS.

### High-level problem
Decode and interpret mmWave frames arriving at high baud rates on Serial1 while maintaining soft real-time responsiveness for camera streaming and web serving under constrained CPU and memory budgets.

---

## 2. Project Objectives & Scope

- Implement a modular embedded pipeline for presence detection and camera streaming on a XIAO ESP32 S3.
- Provide a web dashboard with live MJPEG, radar status endpoints, raw-frame capture, and an audio capture/save API.
- Persist short audio captures to LittleFS and provide a route for uploading via a laptop proxy to cloud STT.

Scope (included): UART parser, FreeRTOS tasks, WebServer endpoints, LittleFS WAV writes, example sketches for testing (`mmwave_test/mmwave_test.ino`, `mmwave_test/mmwave_sniffer.ino`).

Scope (excluded): full cloud-native TLS on MCU (proxy provided instead), advanced video codecs (H.264), or full Bluetooth audio stacks.

Success criteria:
- Stable UART decoding (no sustained frame loss) at a practical baud (prefer 9600 for soft serial examples; hardware Serial1 can handle 256000).
- Live MJPEG stream accessible via dashboard.
- Events persisted in a history buffer and downloadable via `/radar/history`.

---

## 2.1 System Model

- Deployment: Single edge device (XIAO ESP32 S3) connected to a mmWave sensor and camera. Device runs as a Wi‑Fi client or AP for dashboard access.
- Component diagram (text placeholder):
  - Sensor (TX/RX) → Serial1 → `radarTask` → FreeRTOS Queue/EventGroup → `transferTask` → WebServer / LittleFS / log

---

## 2.2 Problem Statement

Decode mmWave frames on a high-rate UART interface with robust validation and dispatch events to the rest of the system without starving camera streaming or HTTP serving.

---

## 2.3 Objectives and Assumptions

- Objectives: decode frames, expose `RadarStatus`, keep MJPEG stream responsive, save audio as WAV to LittleFS, supply diagnostics endpoints including raw frame hex and a UART sniffer.
- Assumptions: known header/tail patterns in frames, correct wiring (sensor TX → MCU RX), common ground, Wi‑Fi available for dashboard.

---

## 3. Solution Methodology / Approach

### High-Level Architecture
- `radarTask` (priority high): reads bytes from `Serial1`, finds frames by header/tail, validates checksum (if any), and decodes fields into a `RadarStatus` structure. On relevant changes it sets an EventGroup bit and enqueues a `RadarEvent` into a queue.
- `transferTask` (priority medium): consumes queued events, persists to an in-memory circular history and calls `log_append()` for durable event trace. It also notifies the web layer via shared state.
- Camera/capture tasks (priority medium): provide MJPEG frames via interrupts/callbacks from `esp_camera` and serve `/stream` via the WebServer.
- WebServer (priority low/managed): responds to `/status`, `/radar/raw`, `/radar/history`, `/audio/latest.wav`, `/audio/upload`.

### Data Flow
1. Serial bytes arrive on `Serial1`.
2. `radarTask` accumulates bytes into a buffer and searches for frameStart/frameEnd sequences.
3. When a full frame is detected, `decodeTargetState()` validates and extracts fields (target status, distance, per-gate energies) and returns a `RadarStatus`.
4. If the state changed or enough time passed, `radarTask` enqueues a `RadarEvent` into a FreeRTOS queue.
5. `transferTask` persists to `s_history[]` and appends to persistent log via `log_append()` and updates the web-exposed JSON.

### Design choices & tradeoffs
- EventGroup vs global flags: EventGroup provides atomic bit-level notifications that are efficient and avoid coarse-grained locking.
- Queue sizing: small (e.g., 8–16) to bound RAM but large enough to absorb transient bursts.
- MJPEG chosen for simplicity (HTTP multipart JPEG frames) rather than H.264 due to encoding complexity and CPU constraints.
- LittleFS chosen for lightweight file persistence on flash.

### Concurrency & Schedulability
- Tasks and priorities (examples):
  - `radarTask`: high priority, core 1 — tight loop for frame assembly.
  - `transferTask`: medium priority, core 1 — processes queue and writes logs.
  - Camera/streaming & WebServer: medium/low priority, core 0 — block on network I/O, use async where possible.
- Mitigations: minimize blocking in `radarTask`, use small lock-free circular buffers, offload longer I/O (LittleFS writes, HTTP uploads) to `transferTask`.

---

## 3.1 Algorithms / Protocols / Architectures

### Radar parser
- Frames identified by a fixed header (e.g., bytes 0x53,0x59 or lib-specific `frameStart[]`) and tail sequence. `decodeTargetState()` reads fields (mode, targetStatus, distance, gate energies). Validation steps:
  - Verify header & tail.
  - Verify length matches expected.
  - Optional checksum validation if available.
  - Map numeric gate energy fields into `radarMovePower.moveGate[]` and `radarStaticPower.staticGate[]`.

### Web server
- Endpoints exposed:
  - `/status`: JSON status including network and uptime.
  - `/radar/raw`: last_frame_hex + legacy_info summary.
  - `/radar/history`: circular buffer JSON of recent events.
  - `/stream`: MJPEG multipart stream.
  - `/audio/latest.wav`, `/audio/start`, `/audio/upload`.

### Audio capture
- Simulated audio generator creates PCM samples and writes a proper WAV header + PCM data to LittleFS path `/audio/latest.wav` for quick testing.

### Pseudocode (radarTask)
```
loop:
  while (Serial1.available()) read byte into buffer
  if (find frameStart and frameEnd in buffer):
    extract frame
    if (validateFrame(frame)):
      status = decodeTargetState(frame)
      if (status changed or time>threshold):
         enqueue RadarEvent(status)
    remove processed bytes from buffer
  yield/delay
```

---

## 3.2 Illustrative Example

Sample raw hex frame (example): `53 59 01 0A 00 64 ... AA BB` (header 0x53 0x59, fields, tail AA BB). `decodeTargetState()` maps bytes to distance (e.g., 0x0064 → 100mm) and flags to `TargetStatus::MovingTarget`. `transferTask` records timestamp and pushes to `s_history[]` and web endpoint reflects on next poll.

Sample timing (example): frame arrival → parse: 2–5 ms; enqueue & transfer: 5–20 ms; web reflect (poll interval): ~200 ms by default.

---

## 4. Implementation / Simulation Architecture

### Hardware platform
- Seeed XIAO ESP32 S3 (pinout depends on variant). Camera pins are in `camera_pins.h`.
- Wired connections (recommended):
  - Sensor TX → XIAO RX (D2)
  - Sensor RX ← XIAO TX (D3)
  - Common ground and appropriate Vcc (as sensor requires)

### Software stack & tools
- Arduino core for ESP32 (esp32:esp32), FreeRTOS primitives (bundled), `esp_camera` driver for the camera, LittleFS for file storage, Arduino `WebServer` for HTTP endpoints.

### Directory layout (key files)
- `mmwave_test/mmwave_test.ino` — example using `Seeed_HSP24` library.
- `mmwave_test/mmwave_sniffer.ino` — UART sniffer to capture raw bytes.
- `mmwave_test/SoftwareSerial.h` — shim used for compile-time compatibility.
- `radar.cpp`, `radar.h` — parser and history buffer.
- `web.cpp`, `web.h` — web endpoints and dashboard assets.
- `modules/audio.cpp` — WAV writer and LittleFS audio capture.
- `log.cpp`, `log.h` — circular in-memory persistent log.

### Key data structures
- `RadarStatus`: targetStatus, distance, gate arrays.
- `RadarEvent`: timestamped snapshot of radar state.
- `s_history[]`: circular buffer storing last N RadarEvent entries.

### Resource usage
- RAM/flash footprints depend on build flags. Measure with `arduino-cli compile --show-size` for accurate numbers; placeholders may be used in the report if not measured yet.

---

## 5. Evaluation

### Test plan
- Unit test: run `presence_sensor_test.ino` or parse captured `last_frame_hex` samples using the sniffer and a small Python script to verify decode logic.
- Integration: place device on Wi‑Fi, open `/stream` and `/radar/raw`, trigger presence and observe dashboard update and logs.
- Stress: generate frequent motion changes and observe queue fullness and frame drops.

### Metrics & tools
- Latency: measure timestamp delta between frame parse and EventGroup set (ms) logged in `log_append()`.
- Throughput: events/sec processed.
- CPU usage: approximate via task profiling or toggled GPIO for sampling.
- Reliability: count corrupted frames returned by `findSequence()` or `decodeTargetState()` failures.

### Reproducibility commands
- Fetch raw frame JSON:
  - `curl http://<device-ip>/radar/raw`
- Capture single MJPEG frame via ffmpeg:
  - `ffmpeg -i http://<device-ip>/stream -vframes 1 out.jpg`

---

## 6. Conclusions

- Summary: The system demonstrates practical integration of an LD2410-like radar with XIAO ESP32 S3 providing presence detection, camera streaming, and local audio capture. Key lessons include the need for hardware Serial at very high baud rates, careful FreeRTOS task partitioning, and constrained I/O scheduling for reliable MJPEG streaming.
- Future work: move audio capture to real I2S microphone, secure direct TLS on device if resources allow, add richer frame validation and analytics.

---

## 7. References

1. Seeed mmWave for XIAO — https://github.com/limengdu/mmwave_for_XIAO
2. ESP32 Arduino core documentation — https://github.com/espressif/arduino-esp32
3. esp_camera library docs
4. LD2410 / module vendor datasheet (if available)

---

## Appendices (Suggested)

- Appendix A — Key source snippets: refer to code in repository (e.g., `radar.cpp`, `web.cpp`).
- Appendix B — Wiring diagram & photos (add actual images into `ProjectReport_appendix.zip`).
- Appendix C — Raw sniffer logs: use `mmwave_test/mmwave_sniffer.ino` to capture and paste hex output.
- Appendix D — Build & flash commands:
  - `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3` 
  - `arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32S3 -p COMx`

---

## Files in this folder

- Example test sketch: [mmwave_test.ino](mmwave_test.ino)
- UART sniffer: [mmwave_sniffer.ino](mmwave_sniffer.ino)
- Minimal SoftwareSerial shim: [SoftwareSerial.h](SoftwareSerial.h)

---

If you'd like, I can:
1. Commit this folder to a new Git repository and help you push to GitHub (I will show the exact `git` commands).  
2. Generate a `ProjectReport.docx` (or PDF) based on this README (convert Markdown to PDF).  
3. Create the presentation skeleton (PowerPoint) and populate slides with suggested screenshots and speaker notes.

Tell me which of the three you want me to do next and I will proceed.
