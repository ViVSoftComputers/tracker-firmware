## Goal
Develop and maintain custom standalone GPS tracking firmware for the Seeed T1000-E (nRF52840). Requirements: (1) portable to upstream Meshtastic updates; (2) function as a stock Meshtastic node; (3) toggle tracking ON/OFF via button gestures; (4) persist tracked data in flash after power-off; (5) sync offline tracks to Meshtastic app position logs upon BLE reconnect; (6) log 1 fix/minute.

## Decisions
- **Architecture:** Implemented `CachedPhoneTracker` as an isolated module in `src/modules/optional/`.
- **Button Mapping (v2.0.0):**
  - 2-click: Toggle Tracker Mode (ON: ascending chime + LED; OFF: descending chime + LED off).
  - 3-click: GPS toggle / Position broadcast.
  - 4-click: Node Info / Ping.
- **Persistence:** Fixed LittleFS `FILE_O_WRITE` append bug by manually calling `FSCom.remove()` before overwriting index/cache files. 
- **Logging Guard:** Enforced `timestamp >= 1700000000` to prevent recording 1969/1970 epoch bugs from incomplete GPS/RTC locks.
- **Deployment:** Public GitHub repo `ViVSoftComputers/tracker-firmware` hosting UF2 releases and a patch file for upstream compatibility.

## State
- **Current Firmware:** v2.0.0 (LittleFS persistence fixed, epoch guard enabled, 60s capture interval).
- **Storage:** 500-point binary ring buffer in LittleFS.
- **Status:** Verified clean 0/500 cache after clear/reboot cycles on T1000-E.
- **Sync:** Points stream natively as `POSITION_APP` packets over BLE when phone connects.

## Files & Paths
- **Repository:** `https://github.com/ViVSoftComputers/tracker-firmware`
- **Hook Patch:** `meshtastic-tracker-hook.patch` (patches `Modules.cpp` and `ButtonThread.cpp`).
- **Binary:** `firmware-tracker-t1000e.uf2` (Compiled at v2.0.0).
- **Tools:** `tracker_tool.py` (CLI for dump/clear/test).
- **Workspace:** `C:\Users\vivso\Documents\Friday\DEV\Meshtastic\`

## Constraints
- **Portability:** Updates require applying `meshtastic-tracker-hook.patch` (40 lines total) to new Meshtastic releases.
- **Hardware:** T1000-E (no screen, single button).
- **Persistence:** Flash (`/tracker_index.dat`, `/tracker_points.dat`) is persistent; `tracker:clear` explicitly removes files before re-creating.