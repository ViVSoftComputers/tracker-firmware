# T1000-E Cached GPS Tracker — v3.0.1

Standalone Meshtastic GPS tracking firmware for the **Seeed SenseCAP T1000-E** (nRF52840). Logs waypoints to onboard flash independently of the phone, with full button + serial CLI control.

## Features

- **Standalone GPS logging** — works without a phone connected. 60s logging interval, cached to `/tracker_points.dat` on LittleFS.
- **500-point ring buffer** — oldest points roll off when full. Index file rebuilt each write to prevent flash corruption.
- **Epoch guard** — rejects GPS points older than Nov 2023 (< 1.7×10⁹) to prevent garbage data after cold starts.
- **Local-only routing** — all CLI commands are addressed to the device's own `nodeNum` (`sendToPhone`); zero LoRa RF leakage. Tracker mode never broadcasts your position to the mesh.
- **Phone sync** — the `CachedPhoneTracker` module syncs valid breadcrumbs to the Meshtastic app on connect.
- **Audio feedback** — ascending chime (ON), descending chime (OFF), 2700Hz chirp on manual waypoint lock, 2000Hz tick while searching for GPS fix.
- **GPX export** — dump to `.gpx` with `tracker_tool.py`.

## Button Gestures (v3.0.1)

| Gesture | Action |
|---|---|
| **1 click** | Manual waypoint log — powers GPS if needed, chirps on lock |
| **2 clicks** | Toggle Tracker Mode (ON / OFF) — chimes + LED |
| **3 clicks** | Clear flash cache (resets to 0/500) |
| **4 clicks** | GPS Toggle & Position Broadcast |
| **5 clicks** | Ping — sends "PING OK" to phone |

**Default state:** Tracker Mode is **OFF** after flash. You must toggle it on (2 clicks).

## CLI — Serial Commands

Connect via USB serial with `tracker_tool.py` (or any Meshtastic serial client):

| Command | What it does |
|---|---|
| `tracker:status` | Show tracker state (ON/OFF), GPS fix status, point count |
| `tracker:on` | Enable continuous logging |
| `tracker:off` | Disable continuous logging (cache preserved) |
| `tracker:sync` | Force-sync cached points to phone app |
| `tracker:clear` | Erase all cached points (same as 3-click gesture) |
| `tracker:dump` | Dump all cached points (returns `$TRK,lat,lon,alt,epoch` lines) |
| `tracker:test` | Record a test point with current GPS coords |

**Important:** All commands route internally via `destinationId=localNodeNum` — nothing hits the LoRa mesh.

### Quick Start (Windows)

```bash
tracker.bat status       # or: py tracker_tool.py status
tracker.bat on
tracker.bat dump -o my_walk.gpx
```

Requires `pip install meshtastic`.

### GPX Export

```bash
py tracker_tool.py dump -o tracklog.gpx
```

Get a standard `.gpx` track file with latitude, longitude, elevation, and UTC timestamps.

## Flashing

### Prebuilt binary

Drag `firmware-tracker-t1000e-v3.0.1.uf2` onto the T1000-E's flash drive (double-press the side button to enter bootloader mode — appears as `SENSEAP` drive in Explorer).

### Build from source

```bash
pio run -e tracker-t1000-e
```

Requires Meshtastic firmware source tree with `CachedPhoneTracker` module and `ButtonThread.cpp` patched with `#ifdef TRACKER_T1000_E` gesture routing.

## Hardware Details

| Component | Detail |
|---|---|
| **Board** | Seeed SenseCAP T1000-E |
| **MCU** | nRF52840 (ARM Cortex-M4) |
| **Buzzer PWM pin** | GPIO 25 |
| **Buzzer enable pin** | GPIO 37 (power gate) |
| **LED** | P0.15 |
| **Storage** | LittleFS on internal flash |
| **Build target** | `tracker-t1000-e` |

## Repo Structure

```
firmware-tracker-t1000e-v3.0.1.uf2  # Prebuilt binary
tracker_tool.py                     # Serial CLI utility
tracker.bat                         # Windows batch wrapper
src/input/ButtonThread.cpp           # Gesture routing (#ifdef TRACKER_T1000_E)
src/modules/optional/CachedPhoneTracker/  # Module source (CachedPhoneTracker.cpp/.h)
```

## License

Part of the ViVSoft / LoRaMeshDevices hardware ecosystem. Built on the Meshtastic platform.