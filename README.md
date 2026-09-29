# T1000-E Cached GPS Tracker — v3.0.0

Standalone Meshtastic GPS tracking firmware for the **Sedee SenseCAP T1000-E** (nRF52840). Logs waypoints to LittleFS cache, auto-syncs to the Meshtastic position log on reconnect, and supports button-gesture control.

## Features

- **60-second interval** GPS logging
- **Ring buffer** (2,048 points) to LittleFS (`/tracker_points.dat`)
- **Button gestures:** 1-click (log), 2-click (toggle), 3-click (clear), 4-click (GPS), 5-click (info)
- **Phone sync** via `POSITION_APP` packets when BLE or serial reconnects
- **SPILock concurrency** guarding all flash I/O
- **GPX export** via `tracker_tool.py`

Uses:
- `src/modules/optional/CachedPhoneTracker/` — T1000-E offline tracking module
- `src/modules/optional/LocalGpsTrackLogger/` — Heltec T096 tracking module
- `src/input/ButtonThread.cpp` — multi-click gesture dispatch
- `tools/tracker_tool.py` — CLI for control & GPX export

## Flashing

1. **Export config** and **back up CURRENT.UF2** before flashing.
2. Enter bootloader: hold the button while plugging in USB.
3. Drag `firmware-*.uf2` onto the `NRF52BOOT` drive.

## CLI Tool

```bash
pip install meshtastic
python tools/tracker_tool.py --port COM3 status
python tools/tracker_tool.py --port COM3 gpx-export -o track.gpx
python tools/tracker_tool.py --port COM3 clear
python tools/tracker_tool.py --port COM3 toggle
```

## Build

Uses PlatformIO in WSL2/Linux:

```bash
pio run -e tracker-t1000-e
```

Pre-built binaries are in `/artifacts/`.

## License

Part of the ViVSoft / LoR Mesh Devices hardware ecosystem. Built on the Meshtastic platform.
