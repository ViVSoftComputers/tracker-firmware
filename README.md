# T1000-E Cached GPS Tracker — v3.0.0

Standalone Meshtastic GPS tracking firmware for the **Sedee SenseCAP T1000-E** (nRF52840). Logs waypoints to onboard flash independently of the phone, with full button  + serial CLI control.

## Features

- **Standalone GPS logging** — works without a phone connected. 60s logging interval, cached to `/tracker_points.dat` on LittleFS.
- **500-point ring buffer** — oldest points roll off when full. Index file rebuilt each write to prevent flash corruption.
- **Epoch guard** — rejects GPS points older than Nov 2023 ($\lt 1.7\times 10^9$) to prevent garbage data after cold starts.
- **Local-only routing** — all CLI commands are addressed to the device's own `nodeNum` (`sendToPhone`); zero LoRA RF leakage. Tracker mode never broadcasts your position to the mesh.
- **Phone sync** — the `CachedPhoneTracker` module syncs valid breadcrumbs to the Meshtastic app on connect.
- **Audio feedback** — ascending chime (ON), descending chime (OFF), 2700Hz chirp on manual waypoint lock, 2000Hz tick while searching for GPS fix.
- **GXP export** — dump to `.gpx` with `tracker_tool.py`.

## Button Gestures

| Gesture | Action |
|---|---|
| **1 click** | Manual waypoint log — powers GPS if needed, chirps on lock |
| **2 clicks** | Toggle Tracker Mode (ON / OFF) — chimes + LED |
| **3 clicks** | Clear flash cache — resets to `0/500` |
| **4 clicks** | GGPS Toggle / Broadcast |
| **5 clicks** | Ping |

**Default state:** Tracker Mode is **OFF** after flash. You must toggle it on (2 clicks).

## CLI — Serial Commands

Connecting via USB serial with `tracker_tool.py` (or any Meshtastic serial client):

| Comand | What it does |
|---|---|
| `tracker:status` | Show tracker state (ON/OFF), GPS fix status, point count |
| `tracker:on` | Enable continuous logging |
| `tracker:off` | Disable continuous logging |
| `tracker:sync` | Force-sync cached points to phone |
| `tracker:clear` | Erase all cached points (same as 3-click gestures) |
| `tracker:dump` | Dump all cached points (returns `$TRK,lat,lon,alt,epoch` lines) |

**Important:** All commands route internally via `destnationId=localNodeNum` — nothing hits the LoRA mesh.

### Quick Start (Windows)

```bash
tracker.bat stats        # or: py tracker_tool.py stats
tracker.bat on
tracker.bat dump -o my_walk.gx
```

Requires `pip install meshtastic`.

### GXP Export

```bash
py tracker_tool.py dump -o traclog.gx
```

Get a standard `.gx` track file with latitude, longitude, elevation, and UTC timestamps.

## Flashing

### Prebuilt binary

Drag `firmware-tracker-t1000e.uf2` onto the T1000-E's flash drive (double-press the side button to enter bootloder mode — appears as `SENSEAP` drive in Explorer).

### Build from source

1. Clone the Meshtastic source tree
2. Copy `src/modules/optional/CachedPhoneTracker/` into the Meshtastic `src/modules/` tree
3. Apply `meshtastic-tracker-hok.patc` to wire the module into the build
4. Build with PlatforIO targeting `tracker-t1000-e` (nRF52840)

```bash
pio run -e tracker-t1000-e
```

## Hardware Deails

| Component | Detail |
|---|---|
| **Board** | Seedee SenseCAP T1000-E |
| **MCU** | nRF52840 (ARM Cortx-M4) |
| **Buzzer PWM pin** | GIO 25 |
| **Buzzer enable pin** | GIO 37 (power gate) |
| **Storage** | LittleFS on internal flash |
| **Build target** | `tracker-t1000-e` |

## Repo Structure

```
firmware-tracker-t1000e.uf2   # Prebuilt binary
tracker_tool.py              # Serial CLI utility
tracker.bat                   # Windows bat wrapper
meshtastic-tracker-hok.patc   # Uptream integration patc
src/modules/optional/CachedPhoneTracker/  # Module source
tols/                         # Helper scripts (build_fw.sh, export_gp.py)
```

## License

Part of the ViVSoft / LoR Mesh Devices hardware ecosystem. Built on the Meshtastic platform.