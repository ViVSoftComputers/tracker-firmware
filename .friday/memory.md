## Goal
Develop and maintain custom standalone GPS tracking firmware for the Seeed T1000-E (nRF52840). Requirements: (1) portable to upstream Meshtastic updates; (2) function as a stock Meshtastic node; (3) toggle tracking ON/OFF via button gestures; (4) persist tracked data in flash after power-off; (5) sync offline tracks to Meshtastic app position logs upon BLE reconnect; (6) log 1 fix/minute.

## Decisions
- **Architecture:** `CachedPhoneTracker` implemented as an isolated module in `src/modules/optional/`. 
- **Button Mapping (v2.0.0):** 2-click toggles Tracker Mode (ON/OFF); 3-click GPS/Position broadcast; 4-click Node Info/Ping.
- **Persistence:** Uses `/tracker_index.dat` and `/tracker_points.dat`. Fixed append-only LittleFS bug by calling `FSCom.remove()` before overwriting index files and validating index against physical file size.
- **Logging Guard:** Enforces `timestamp >= 1700000000` to prevent recording 1969/1970 epoch bugs from incomplete GPS locks.
- **Local CLI:** Commands (`tracker:status`, `on`, `off`, `clear`, `dump`, `sync`) use direct `destinationId=localNodeNum` addressing to bypass LoRa RF broadcasting; responses route via `service->sendToPhone(p)` to avoid mesh flooding.
- **Hardware Buzzer:** T1000-E buzzer support implemented using Pin 25 (PWM) and Pin 37 (Power Enable, active HIGH).

## State
- **Current Firmware:** v2.0.0 (Buzzer power management, 60s logging cadence, local-only CLI, epoch guard, LittleFS persistence fix).
- **Storage:** 500-point binary ring buffer.
- **Status:** Verified `0/500` cache post-reboot; buzzer toggles enabled with ascending/descending chimes; 1-minute heartbeat beeps (fix/no-fix confirmation).
- **Deployment:** GitHub `ViVSoftComputers/tracker-firmware` (Public).

## Files & Paths
- **Repository:** `https://github.com/ViVSoftComputers/tracker-firmware`
- **Hook Patch:** `meshtastic-tracker-hook.patch` (patches `Modules.cpp` and `ButtonThread.cpp`).
- **Binary:** `firmware-tracker-t1000e.uf2` (v2.0.0).
- **Tools:** `tracker_tool.py`, `tracker.bat` (CLI for dump/sync/manage).
- **Workspace:** `C:\Users\vivso\Documents\Friday\DEV\Meshtastic\`

## Constraints
- **Portability:** Updates require applying `meshtastic-tracker-hook.patch`.
- **Hardware:** T1000-E (Pin 25 buzzer signal, Pin 37 buzzer enable, 1-minute logging interval).
- **Messaging:** Default channel must be private; LongFast/Public channels should be slot 1+ to prevent tracking data/CLI commands from leaking.
- **Buzzer:** Logic must toggle Pin 37 HIGH, play tone on Pin 25, then toggle Pin 37 LOW.