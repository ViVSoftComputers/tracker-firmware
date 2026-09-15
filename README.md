# ViVSoft Tracker Firmware (Meshtastic)

Custom Meshtastic firmware for Seeed Card Tracker T1000-E (nRF52840 + GNSS + Semtech LoRa) providing offline GPS track caching, gesture toggle, and dual-mode sync (Meshtastic App Position Log sync + USB/Serial GPX export).

---

## Key Features & How It Works

### 1. 100% Stock Meshtastic Operation When OFF
- The tracker module boots **OFF** by default (`trackerModeActive = false`, LED is OFF).
- When OFF, it does not force GPS awake or interfere with standard Meshtastic device roles, smart broadcast intervals, sleep cycles, or LoRa mesh communications.

### 2. 4-Click Hardware Activation & Deactivation
- **Turn ON (4 Clicks):** Plays ascending 4-click tone (`play4ClickUp()`), turns LED solid ON, keeps GPS enabled, and begins logging.
- **Turn OFF (4 Clicks):** Plays descending 4-click tone (`play4ClickDown()`), turns LED OFF, returns GPS to Meshtastic power management.
- **Track Preserved:** Toggling OFF **never erases** the recorded track. All points remain safely stored in flash memory until explicitly cleared.

### 3. Balanced Logging Cadence (1 Fix / Minute)
- Logs 1 fix every **60 seconds** while tracker mode is active and GPS has a valid lock.
- Uses a compact 18-byte binary struct (`TrackPoint`) in a LittleFS ring buffer (`/tracker_points.dat` + `/tracker_index.dat`).
- 500-entry capacity = **over 8.3 hours** of continuous outdoor breadcrumbs in ~9 KB of flash.

### 4. Meshtastic Mobile App Position Log Sync
- **Offline Problem Solved:** When walking without your phone, stock Meshtastic cannot store past positions, losing historical breadcrumbs on the phone app's map.
- **Native Sync:** When you reconnect your phone via Bluetooth (or send `tracker:sync` in chat), the module streams cached points as native `meshtastic_PortNum_POSITION_APP` packets directly to the phone via `service->sendToPhone()`.
- The official Meshtastic Android/iOS app receives each historical point with its original timestamp, latitude, longitude, and altitude, populating the node's **Position Log** and breadcrumb map track for direct in-app visualization and GPX export.

### 5. CLI / USB Tool for Direct GPX Export
- Non-blocking streaming of `$TRK` text lines over USB/Serial via `tracker_tool.py dump`.
- Generates a clean standard `tracklog.gpx` ready for Google Earth, Strava, or GIS software.

---

## Commands

Available via Meshtastic app text message (local node) or USB Serial CLI:

| Command | Action |
|---|---|
| `tracker:status` | Returns mode (`ON`/`OFF`), GPS lock, satellite count, coordinates, and cache count (e.g. `cache=371/500`) |
| `tracker:on` | Activates tracker mode (same as 4 clicks up) |
| `tracker:off` | Deactivates tracker mode (same as 4 clicks down; preserves cache) |
| `tracker:sync` | Streams cached points to the connected Meshtastic phone app as native `POSITION_APP` packets |
| `tracker:dump` | Streams points in `$TRK` format for PC tool export |
| `tracker:clear` | Resets the ring buffer and wipes cached trackpoints |
| `tracker:test` | Injects a test coordinate into flash cache |

---

## Portability & Upstream Patching

To apply this module to any clean or future upstream Meshtastic firmware release:

1. Copy directory:
   ```bash
   cp -r src/modules/optional/CachedPhoneTracker/ <new-firmware-root>/src/modules/optional/
   ```

2. Apply the minimal 40-line hook patch:
   ```bash
   git apply meshtastic-tracker-hook.patch
   ```

The patch touches only two locations in upstream code:
- `src/input/ButtonThread.cpp`: Hooks `case 4` to toggle tracker mode.
- `src/modules/Modules.cpp`: Instantiates `cachedPhoneTracker = new CachedPhoneTracker();`.

---

## Building & Flashing

### Compiling for T1000-E

In WSL2 / Ubuntu with PlatformIO:

```bash
cd ~/src/firmware
~/.local/bin/pio run -e tracker-t1000-e
```

**Artifact:** `.pio/build/tracker-t1000-e/firmware.uf2`

### Flashing (UF2 Bootloader)

1. Connect the Seeed T1000-E via USB.
2. **Double-click the button** — the device mounts as a removable USB drive (e.g. `D:\`).
3. Drag and drop `firmware-tracker-t1000e.uf2` onto the drive.
4. The drive automatically unmounts and reboots into the updated firmware on `COM3`.

---

## Python Tool Usage (`tracker_tool.py`)

Run commands directly from Windows terminal:

```bash
# Check status
python tracker_tool.py status --port COM3

# Trigger phone app sync
python tracker_tool.py sync --port COM3

# Dump track and export GPX
python tracker_tool.py dump --port COM3 -o tracklog.gpx

# Clear cache after backup
python tracker_tool.py clear --port COM3
```

---

## Repository Structure

```
src/modules/optional/
  CachedPhoneTracker/
    CachedPhoneTracker.h      # Module interface & ring-buffer definitions
    CachedPhoneTracker.cpp    # GPS logging, BLE sync, & serial CLI handler
meshtastic-tracker-hook.patch # Upstream integration patch
tracker_tool.py               # Serial CLI management and GPX exporter tool
tracker.bat                   # Convenient Windows batch wrapper
```

## License

This project is built on Meshtastic firmware and inherits its GPL v3 licensing. See upstream [LICENSE](https://github.com/meshtastic/firmware/blob/master/LICENSE) for details.
