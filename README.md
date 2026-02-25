# 🏎️ GoPro Motion Player – LITE (C++ Edition)

A lightweight, native Windows application that plays back GoPro telemetry data and streams it as **SimTools-compatible UDP packets** — enabling dynamic motion rigs to replay real-world driving sessions from GoPro video recordings.

Supports video synchronization with **VLC** (local files) and **DeoVR** (local or web VR video), making it ideal for VR motion simulation setups.

---

## ✨ Features

| Feature | Description |
|---|---|
| **GoPro Telemetry Playback** | Reads JSON telemetry files extracted from GoPro footage (surge, sway, heave, roll, pitch, yaw, gyro, speed, RPM, gear) |
| **SimTools UDP Output** | Sends 311-byte motion packets to `127.0.0.1:5300` at 60 Hz — compatible with SimTools / Forza Motorsport 7 protocol |
| **VLC Video Sync** | Launches VLC media player in sync with telemetry playback |
| **DeoVR Local Mode** | Controls DeoVR via TCP (port 23554) to load and play local video files in sync |
| **DeoVR Web Mode** | Synchronizes telemetry playback with a video already playing in DeoVR's built-in browser |
| **Live Sync Offset** | Adjustable real-time offset (±5 s) to fine-tune video/motion synchronization during playback |
| **Smart Filtering** | Adaptive smoothing, deadzone filtering, rate limiting, and gravity compensation |
| **Synthetic Roll/Pitch** | Generates roll and pitch from lateral/longitudinal G-forces when no orientation data is available |
| **Tire Load (TL) Simulation** | Yaw-based lateral velocity simulation with configurable gain and threshold |
| **UP Command** | Sends a 2-second heave sine wave to verify rig connectivity |
| **Always on Top** | Optional window pinning for use alongside VR headsets |

---

## 📸 Screenshot

The application provides a clean, dark-themed Win32 GUI with:
- Telemetry and video file selectors
- One-click player mode buttons (**VLC**, **DeoVR Local**, **DeoVR Web**)
- Play / Pause / Stop controls
- Live Sync Offset slider
- Context-sensitive instructions for each mode

---

## 🔧 Build Instructions

### Requirements

- **Windows 10/11**
- **Visual Studio 2019+** (Community edition is fine)
- **C++17** or later

### How to Build

1. Open Visual Studio and create a new **Windows Desktop Application** project (or empty C++ project).
2. Add these two files to the project:
   - `player_core.h` — engine, JSON parser, VLC/DeoVR integration, motion processing
   - `player_v66_lite_gui.cpp` — Win32 GUI
3. Set the following project properties:
   - **Subsystem**: `Windows (/SUBSYSTEM:WINDOWS)`
   - **Entry Point**: `WinMainCRTStartup`
   - **Character Set**: `Unicode`
4. Build the solution (`Ctrl+Shift+B`).

> [!NOTE]
> All dependencies (Winsock2, Shell32, ComDlg32) are linked via `#pragma comment(lib, ...)` directives in the source — no manual library configuration needed.

### VLC Integration (Optional)

The application dynamically loads VLC's `libvlc.dll` at runtime. If VLC is installed, it will be auto-detected from:
- `C:\Program Files\VideoLAN\VLC`
- `C:\Program Files (x86)\VideoLAN\VLC`

No VLC SDK or headers are required.

---

## 🚀 Usage

### 1. Prepare Telemetry

Extract GoPro telemetry to JSON format using tools like [gopro-telemetry](https://github.com/JuanIrache/gopro-telemetry). The JSON should contain a `samples` array and optionally a `settings` object.

### 2. Configure SimTools / Motion Platform

Set your motion platform software to receive **Forza Motorsport 7** telemetry on **UDP port 5300** (`127.0.0.1`).

### 3. Choose Player Mode

| Mode | Setup |
|---|---|
| **VLC Player** | Select both a telemetry file and a video file. VLC will open automatically. |
| **DeoVR Local** | Open DeoVR and play **any** video first to establish the TCP connection. Then press Play in this app — it will load and sync your selected video. |
| **DeoVR Web** | Open DeoVR, navigate to a web video and start it. Then press Play — the app will rewind and sync playback automatically. |

### 4. Play

Press **PLAY** to start motion output. Use the **Live Sync Offset** slider to adjust timing in real-time if needed.

---

## ⚙️ Configuration (JSON Settings)

Settings are embedded in the telemetry JSON file under a `"settings"` key:

```json
{
  "settings": {
    "video_offset": 0.0,
    "gain_surge": 1.0,
    "gain_sway": 1.0,
    "gain_heave": 0.8,
    "gain_roll": 1.0,
    "gain_pitch": 1.0,
    "gain_tl": 1.5,
    "enable_surge": true,
    "enable_sway": true,
    "enable_heave": true,
    "enable_roll": true,
    "enable_pitch": true,
    "enable_tl": true,
    "invert_surge": false,
    "invert_sway": false,
    "invert_heave": false,
    "invert_roll": false,
    "invert_pitch": false,
    "invert_tl": false,
    "base_smooth": 0.7,
    "deadzone": 0.03,
    "limit_g": 2.0,
    "limit_angle": 10.0,
    "tl_threshold": 4.0,
    "heave_reset_threshold": 0.1,
    "use_synthetic_roll": true,
    "use_synthetic_pitch": true,
    "compensate_gravity": true,
    "output_freq": 60
  },
  "samples": [ ... ]
}
```

---

## 🏗️ Architecture

```
┌──────────────────────────────────┐
│   player_v66_lite_gui.cpp        │  Win32 GUI (buttons, labels, owner-draw)
│   WinMain → WndProc              │
└──────────┬───────────────────────┘
           │ includes
┌──────────▼───────────────────────┐
│   player_core.h                  │  Self-contained engine header
│                                  │
│   ├── JsonParser        (JSON → TelemetrySample[])
│   ├── VlcLoader         (dynamic libvlc.dll loading)
│   ├── DeoVRClient       (TCP control on port 23554)
│   └── MotionPlayer      (playback loop, filtering, UDP output)
│                                  │
│   Output: 311-byte UDP packets   │
│   → 127.0.0.1:5300 @ 60 Hz      │
└──────────────────────────────────┘
```

---

## 📄 Files

| File | Description |
|---|---|
| `player_core.h` | Core engine — JSON parser, VLC loader, DeoVR client, motion processing, packet generation, playback thread |
| `player_v66_lite_gui.cpp` | Win32 native GUI — window creation, button drawing, event handling |

---

## 📝 License

This project is provided as-is for personal use with motion simulation hardware.

---

## 🤝 Credits

- **GoPro** — for embedding rich telemetry in action camera footage
- **VLC** — open-source media player used for local video sync
- **DeoVR** — VR video player with TCP remote control API
- **SimTools** — motion platform software ecosystem
