# T1000-E Cached GPS Tracker — v3.0.2

Standalone Meshtastic GPS tracking firmware for the **Seeed SenseCAP T1000-E** (nRF52840). Logs waypoints to onboard flash independently of the phone, with full button + serial CLI control.

---

> [!WARNING]
> ### ⚠️ CRITICAL PRE-FLASHING WARNING: BACK UP FIRST!
>
> Before flashing this custom firmware onto your Seeed SenseCAP T1000-E, **you MUST back up your existing node configuration and factory firmware**:
>
> 1. **Export your Meshtastic configuration:**
>    Open a terminal with your T1000-E connected via USB and run:
>    ```bash
>    meshtastic --export-config > meshtastic_backup_config.yaml
>    ```
>    *This preserves your LoRa radio frequencies, channel keys, device name, owner info, and custom settings.*
>
> 2. **Save your `CURRENT.UF2`:**
>    - Double-click the side button on your T1000-E to enter UF2 Bootloader Mode (the drive will appear in File Explorer / Finder as `SENSECAP` or `NRF52BOOT`).
>    - Immediately copy the file named **`CURRENT.UF2`** from that drive and save it to a safe folder on your computer.
>    *This is a full backup of your currently installed firmware. If you ever need to roll back to stock or factory behavior, you can simply drop `CURRENT.UF2` back onto the bootloader drive.*
>
> **Do NOT skip these steps.** Flashing custom firmware can reset your flash storage and LittleFS partitions.

---

## Features

- **Standalone GPS logging** — works without a phone connected. 60s logging interval, cached to `/tracker_points.dat` on LittleFS.
- **500-point ring buffer** — oldest points roll off when full. Index file rebuilt each write to prevent flash corruption.
- **Epoch guard** — rejects GPS points older than Nov 2023 (< 1.7×10⁹) to prevent garbage data after cold starts.
- **Local-only routing** — all CLI commands are addressed to the device's own `nodeNum` (`destinationId=dest`); zero LoRa RF leakage. Tracker mode never broadcasts your position to the public mesh.
- **Hardware concurrency protection** — LittleFS operations are guarded by `concurrency::LockGuard g(spiLock)` to prevent SPI flash collisions during background logging and dumps.
- **Hardware buzzer gate control** — power-gated buzzer enable pin (`BUZZER_EN_PIN` on `P1.05`/Pin 37) prevents standby battery drain on the T1000-E.
- **Phone sync** — the `CachedPhoneTracker` module syncs valid breadcrumbs to the Meshtastic phone app queue on demand.
- **Audio feedback** — ascending chime (ON), descending chime (OFF), 2700Hz chirp on manual waypoint lock, 2000Hz tick while searching for GPS fix.
- **GPX export** — dump directly to `.gpx` with `tracker.bat` or `tracker_tool.py`.

## Button Gestures (v3.0.2)

| Gesture | Action | Audio Feedback |
|---|---|---|
| **1 click** | Manual waypoint log (powers GPS if needed) | High-pitch 2700Hz chirp on lock |
| **2 clicks** | Toggle Tracker Mode (ON / OFF) | Ascending chime (ON) / Descending chime (OFF) |
| **3 clicks** | Clear flash cache (resets index to 0/500) | Descending triple chime |
| **4 clicks** | GPS Toggle & Position Broadcast | Standard Meshtastic tone |
| **5 clicks** | Ping node | Sends local status & ping tone |

**Default state:** Tracker Mode is **OFF** after boot/flash. You must toggle it on (2 clicks or CLI command).

## CLI — Serial Commands

Connect via USB serial using `tracker.bat` (Windows interactive menu) or `tracker_tool.py`:

| Command | What it does |
|---|---|
| `tracker:status` | Show tracker state (ON/OFF), GPS fix status, battery, and point count |
| `tracker:on` | Enable continuous 60s logging (solid LED, ascending chime) |
| `tracker:off` | Disable continuous logging (sleeps GPS, cache preserved) |
| `tracker:sync` | Asynchronously force-sync cached points to the Meshtastic phone app |
| `tracker:clear` | Erase all cached points from flash (same as 3-click gesture) |
| `tracker:dump` | Dump all cached points (returns `$TRK,lat,lon,alt,epoch` lines) |
| `tracker:test` | Hardware test (tests buzzer PWM frequencies & LED blink) |

**Privacy Guarantee:** All commands route internally via `destinationId=localNodeNum` — nothing is broadcast over the LoRa mesh.

### Quick Start (Windows)

Launch the interactive controller by double-clicking `tracker.bat` or running:

```cmd
tracker.bat
```

Or execute direct commands:

```cmd
tracker.bat status
tracker.bat on
tracker.bat off
tracker.bat dump -o my_trip.gpx
tracker.bat clear
```

*Requires Python and `pip install meshtastic`.*

### GPX Export

```cmd
py tracker_tool.py dump -o tracklog.gpx
```

Generates a standard `.gpx` track file with latitude, longitude, elevation, and UTC timestamps ready for Google Earth, Gaia GPS, or CalTopo.

## Flashing Instructions

### Pre-requisite: Back Up Existing Firmware & Config
As noted in the warning above:
```bash
meshtastic --export-config > backup_config.yaml
```
Double-press the side button, and copy **`CURRENT.UF2`** from the `SENSECAP` drive to your PC.

### Flashing the UF2
1. Put the T1000-E into bootloader mode by **double-pressing the side button**.
2. A mass storage drive named `SENSECAP` (or `NRF52BOOT`) will appear on your computer.
3. Drag and drop `artifacts/firmware-tracker-t1000e-v3.0.2.uf2` onto that drive.
4. The device will flash, unmount automatically, and reboot into Tracker Firmware v3.0.2.

### Restoring Your Configuration
Once booted, re-apply your backed-up settings if needed:
```bash
meshtastic --configure backup_config.yaml
```

### Build from Source

```bash
pio run -e tracker-t1000-e
```

Requires the Meshtastic firmware source tree with the `CachedPhoneTracker` module and `src/input/ButtonThread.cpp` patched with `#ifdef TRACKER_T1000_E` gesture routing.

## Hardware Details (Seeed SenseCAP T1000-E)

| Component | Detail |
|---|---|
| **Board** | Seeed SenseCAP T1000-E Card Tracker |
| **MCU** | Nordic nRF52840 (ARM Cortex-M4F) |
| **GPS / GNSS** | LR1110 / Quectel High-sensitivity GNSS |
| **Buzzer PWM Pin** | P0.25 (GPIO 25) |
| **Buzzer Enable Pin** | P1.05 (Pin 37 — power-gating MOSFET) |
| **Status LED** | P0.15 |
| **Storage** | LittleFS on internal flash |
| **Target** | `tracker-t1000-e` |

## Repository Structure

```
├── artifacts/
│   └── firmware-tracker-t1000e-v3.0.2.uf2      # Prebuilt ready-to-flash binary
├── tracker.bat                                 # Interactive Windows CLI launcher & flasher
├── tracker_tool.py                             # Python serial CLI utility (direct node targeting)
├── src/
│   ├── input/
│   │   └── ButtonThread.cpp                    # 1–5 click hardware gesture routing
│   └── modules/optional/CachedPhoneTracker/
│       ├── CachedPhoneTracker.cpp              # SPILock concurrency, buzzer power gate, LittleFS ring buffer
│       └── CachedPhoneTracker.h                # Gesture & async state declarations
└── README.md                                   # Documentation & flashing guide
```

## License

Part of the **ViVSoft** hardware ecosystem. Built on the Meshtastic platform.
