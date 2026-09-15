# ViVSoft Tracker Firmware (Meshtastic)

Custom Meshtastic firmware for Seeed Card Tracker T1000-E (nRF52840 + GNSS + Semtech LoRa) providing offline GPS track caching, gesture toggle, and dual-mode sync (Meshtastic App Position Log sync + USB/Serial GPX export).

ðŸ“– **Read the full write-up & deep-dive guide:** [Building an Offline GPS Data Logger for Meshtastic (Seeed Card Tracker T1000-E)](https://hub.lorameshdevices.com/blog/building-an-offline-gps-data-logger-for-meshtastic-seeed-card-tracker-t1000-e)

---

> [!WARNING]
> ### âš ï¸ Important: Back Up Your Existing Firmware First
> Before flashing custom firmware or any new UF2 onto your Seeed T1000-E, **always back up your current device configuration and existing firmware/flash**. 
> - Export your node configuration and keys using the Meshtastic mobile app or the Meshtastic CLI (`meshtastic --export-config > my_config.yaml`).
> - Keep a copy of your factory/stock T1000-E UF2 binary handy so you can restore your device at any time if needed.

---

## Button Gestures (T1000-E)

| Gesture | Action | Notes |
|---|---|---|
| **2 Clicks** (Double-press) | **Tracker Mode Toggle (ON / OFF)** | Turns tracking ON (solid green LED, ascending chime) or OFF (LED off, descending chime). Trackpoints in flash are preserved. |
| **3 Clicks** (Triple-press) | **Position Broadcast / GPS Toggle** | Meshtastic default triple-press behavior. |
| **4 Clicks** (Quad-press) | **Node Info / Position Ping** | Original 2-click ping moved here to keep quick 2-click gesture dedicated to tracker logging. |
| **Long Press (Hold)** | **Power / Shutdown** | Meshtastic default long-press power management. |

---

## Key Features & How It Works

### 1. 100% Stock Meshtastic Operation When OFF
- The tracker module boots **OFF** by default (`trackerModeActive = false`, LED is OFF).
- When OFF, it does not force GPS awake or interfere with standard Meshtastic device roles, smart broadcast intervals, sleep cycles, or LoRa mesh communications.

### 2. 2-Click Hardware Activation & Deactivation
- **Turn ON (2 Clicks):** Plays ascending tone (`play4ClickUp()`), turns LED solid ON, keeps GPS enabled, and begins logging 1 fix every 60 seconds.
- **Turn OFF (2 Clicks):** Plays descending tone (`play4ClickDown()`), turns LED OFF, returns GPS to Meshtastic power management.
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
| `tracker:on` | Activates tracker mode (same as 2 clicks up) |
| `tracker:off` | Deactivates tracker mode (same as 2 clicks down; preserves cache) |
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

2. Apply the minimal hook patch:
   ```bash
   git apply meshtastic-tracker-hook.patch
   ```

The patch touches only two locations in upstream code:
- `src/input/ButtonThread.cpp`: Hooks `BUTTON_EVENT_DOUBLE_PRESSED` to toggle tracker mode, and redirects 4-click to the ping event.
- `src/modules/Modules.cpp`: Instantiates `cachedPhoneTracker = new CachedPhoneTracker();`.

---

## Building & Flashing

### Compiling for T1000-E

In WSL2 / Ubuntu with PlatformIO:

```bash
cd ~/src/firmware
~/.local/bin/pio run -e tracker-t1000-e
```

The compiled UF2 binary is located at:
`.pio/build/tracker-t1000-e/firmware-tracker-t1000-e-2.8.1.471c07d.uf2`

### Flashing via UF2 Bootloader

1. Double-click the reset button or touch COM port at 1200 baud to enter bootloader mode (mounts as removable drive `D:` or `NRF52BOOT`).
2. Copy `firmware-tracker-t1000e.uf2` to the drive.
3. The device automatically flashes and reboots into normal operating mode.
