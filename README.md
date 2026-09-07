# ViVSoft Tracker Firmware

Custom Meshtastic firmware modules for offline GPS tracking on Heltec V3 and T1000-E devices.

## Modules

### CachedPhoneTracker — T1000-E

**Problem:** When the Meshtastic Android app disconnects from the T1000-E (out of BLE range), the PositionModule silently drops all position-to-phone packets. You lose your entire track during BLE-disconnected operation.

**Solution:** A `MeshModule` + `OSThread` that monitors BLE connection state. When disconnected, it captures GPS positions into a LittleFS binary ring buffer (`/static/cached_positions.dat`). When the phone reconnects, the entire cache is flushed chronologically via `service->sendToPhone()` so your phone app sees the complete track.

| Parameter | Value |
|---|---|
| Poll interval | 30s (CPU/battery conscious) |
| Capture gate | >10m movement OR stationary >60s |
| Ring buffer | 500 entries, ~50KB on flash |
| BLE detection | `isToPhoneQueueEmpty()` with 2-sample hysteresis |
| Architecture guard | `#if defined(ARCH_NRF52)` — T1000-E only |

### LocalGpsTrackLogger — Heltec V3

Stands alone — no phone needed. Writes GPS fixes to LittleFS as CSV at `/static/tracklog.csv`. Serves the file over HTTP so you can pull it from any browser on the mesh.

| Parameter | Value |
|---|---|
| Poll interval | 5s |
| Capture gate | >10m movement OR stationary >60s |
| Output | CSV (`timestamp,date_time,latitude,longitude,altitude_m,hdop`) |
| Display | OLED card showing point count, lat/lon, altitude, HDOP, satellite count |
| Space safety | Stops logging when <50KB free |
| Architecture guard | `#if defined(ARCH_ESP32)` — Heltec V3 only |
| HTTP access | `http://<node-ip>/tracklog.csv` |

## Repository Structure

```
src/modules/optional/
  CachedPhoneTracker/
    CachedPhoneTracker.h
    CachedPhoneTracker.cpp
  LocalGpsTrackLogger/
    LocalGpsTrackLogger.h
    LocalGpsTrackLogger.cpp
tools/
  build_fw.sh          # Quick WSL2 PlatformIO build for Heltec V3
  export_gpx.py        # Convert tracklog.csv to GPX for Google Earth
```

## Build Environment

- **OS:** WSL2 / Ubuntu
- **Build system:** PlatformIO (`~/.local/bin/pio`)
- **Source:** Meshtastic firmware (this repo is a fork)

## Compiling

### Heltec V3 (ESP32-S3)

```bash
# In WSL2 Ubuntu:
cd ~/src/firmware
~/.local/bin/pio run -e heltec-v3

# Artifacts: .pio/build/heltec-v3/firmware.factory.bin
# Flash at 0x0 with esptool in DIO mode
```

### T1000-E (nRF52840)

```bash
# In WSL2 Ubuntu:
cd ~/src/firmware
~/.local/bin/pio run -e tracker-t1000-e

# Artifacts:
#   .pio/build/tracker-t1000-e/firmware.uf2  (drag-and-drop flash)
#   .pio/build/tracker-t1000-e/firmware.zip   (adafruit-nrfutil flash)
```

### Quick build script

```bash
# From Windows:
wsl -d Ubuntu bash -c "cd ~/src/firmware && bash tools/build_fw.sh"
```

## Flashing

### T1000-E

**Method 1 (easiest):** Double-press the button → "TRACKER" USB drive mounts → drag `.uf2` onto it.

**Method 2:** `adafruit-nrfutil dfu serial -pkg firmware.zip -p <COM_PORT>`

### Heltec V3

```bash
esptool.py --chip esp32s3 --port <COM_PORT> --before default_reset --after hard_reset \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 firmware.factory.bin
```

## Upgrading to New Meshtastic Versions

This repo is a fork of `meshtastic/firmware`. To pull in upstream changes:

```bash
cd ~/src/firmware
git fetch upstream
git merge upstream/master          # or upstream/develop
# Resolve any conflicts (rare — our modules are in src/modules/optional/)
~/.local/bin/pio run -e tracker-t1000-e   # test build
~/.local/bin/pio run -e heltec-v3         # test build
```

## GPX Export

Convert Heltec V3 track log to GPX for Google Earth / GIS:

```bash
# From the node's HTTP server:
python tools/export_gpx.py http://192.168.87.45/tracklog.csv -o my_track.gpx

# From a local file:
python tools/export_gpx.py tracklog.csv -o my_track.gpx -n "My Hike"
```

## Lessons Learned

### T1000-E / nRF52

1. **`numSatellites` doesn't exist.** The field is `sats_in_view`. `numSatellites` is only declared on ESP32. Use `gps->p.sats_in_view` on nRF52.

2. **`pb_decode.h` must be explicitly included.** On ESP32 it comes in transitively; nRF52 needs `#include "pb_decode.h"` directly.

3. **nRF52 GPS is I2C-based (LC76G).** The GNSS module communicates over I2C, not UART like ESP32 boards. The `GPS` singleton abstracts this, but the GPS power-on sequence is different and slower — give it extra time on cold start.

4. **LittleFS on nRF52 uses `FSCom` like ESP32.** Same API. The ring buffer pre-allocation trick (`seek(fileSize-1); write(0)`) works identically.

5. **`OSThread` on nRF52 must not block.** Our `runOnce()` returns `POLL_INTERVAL_MS` immediately on every fast-path failure (no GPS, no lock, standby). Never spin-wait for BLE or GPS — let the thread framework schedule you.

6. **BLE connection detection is quirky.** `isToPhoneQueueEmpty()` returns `true` even when the phone is connected but not requesting positions. We use a 2-sample hysteresis to avoid false reconnection detection (was 5000ms polling windows, now 30000ms — still fine).

### Heltec V3 / ESP32-S3

1. **HAS_SCREEN for the display frame.** The `drawFrame()` override only compiles under `#if HAS_SCREEN`. Without it, the module silently works but shows no display card.

2. **SPI lock around filesystem writes.** `concurrency::LockGuard g(spiLock)` before any `FSCom` access prevents flash contention with the radio stack.

3. **FSBegin() before every write.** The filesystem can unmount between poll cycles (power management). Always call `FSBegin()` in `runOnce()`, not just in `setup()`.

4. **`FILE_APPEND` mode.** Writing incrementally with `FILE_APPEND` is far more flash-friendly than rewriting the entire CSV each time. The CSV header is written only once (`need_header` check).

5. **50KB safety floor.** `totalBytes() - usedBytes() < 51200` → stop logging. This prevents filesystem corruption from total exhaustion.

### Architecture

1. **MeshModule + OSThread dual inheritance works.** Both modules inherit from both base classes. Registration via `new` + `setup()` in the `setup*()` factory functions. No core file modifications needed.

2. **Module registration is automatic.** Meshtastic's `bin/optional-modules.py` build hook scans `src/modules/optional/*/` and generates registration calls. Just drop the folder in and rebuild.

3. **`HAS_GPS` and `ARCH_*` guards are mandatory.** Without them, missing GPS headers on non-GPS variants will fail the build. Each module has both a header-level `#if HAS_GPS` and a cpp-level `#if defined(ARCH_NRF52)` / `#if defined(ARCH_ESP32)`.

4. **Don't fight PositionModule — complement it.** CachedPhoneTracker only activates when `isToPhoneQueueEmpty()` (BLE disconnected). When BLE is connected, PositionModule handles live sends normally. No interference, no duplicated positions.

5. **30s poll interval is the sweet spot.** We started at 5s (matching LocalGpsTrackLogger) but that's too aggressive for a coin-cell tracker. At 30s the nRF52 spends 99%+ of its time in deep sleep. The 30s flush delay on reconnect means worst case you wait half a minute for the cache to start draining, but battery life wins.

## License

This project is a fork of Meshtastic firmware and inherits its licensing. See the upstream [LICENSE](https://github.com/meshtastic/firmware/blob/master/LICENSE) for details.

## Author

ViVSoft — [ViVSoftComputers](https://github.com/ViVSoftComputers)