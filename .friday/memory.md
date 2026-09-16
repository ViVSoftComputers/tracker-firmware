## Goal
Develop and maintain custom standalone GPS tracking firmware for the Seeed T1000-E (nRF52840). Requirements: (1) portable to upstream Meshtastic updates; (2) function as a stock Meshtastic node; (3) toggle tracking ON/OFF via button gestures; (4) persist tracked data in flash after power-off; (5) sync offline tracks to Meshtastic app position logs upon BLE reconnect; (6) log 1 fix/minute.

## Decisions
- **Architecture:** `CachedPhoneTracker` implemented as an isolated module in `src/modules/optional/`.
- **Button Mapping (v2.0.0):** 2-click toggles Tracker Mode (ON/OFF); 3-click GPS/Position broadcast; 4-click Node Info/Ping.
- **Persistence:** LittleFS ring buffer; `clearCache()` and `saveCacheIndex()` explicitly call `FSCom.remove()` before writing to bypass append-only behavior.
- **Logging Guard:** Enforces `timestamp >= 1700000000` to prevent 1969/1970 epoch bugs from incomplete GPS locks.
- **Local CLI:** Commands (`tracker:status`, `on`, `off`, `clear`, `dump`, `sync`) use `destinationId=localNodeNum` to ensure zero LoRa RF broadcasting; responses routed via `service->sendToPhone(p)` to serial/BLE client.
- **Hardware Buzzer:** Uses Pin 25 (PWM) and Pin 37 (Enable, Active HIGH). Implemented `beepTone()` helper to sequence Power Enable (HIGH), Tone, Delay, Stop, Power Enable (LOW).

## Constraints
- **Hardware:** T1000-E (Pin 25 PWM, Pin 37 Power).
- **Messaging:** Default channel must be private; public/LongFast channels in slot 1+ to prevent telemetry leakage.
- **Sync:** Only log points with `gps->hasLock()` and valid timestamp (> 1.7e9).
- **CI/CD:** Upstream CI workflows removed from the repo to prevent spurious failing email notifications.

## State
- **Current Firmware:** v2.0.0 (Buzzer/PWM fix, Local-only CLI, Persistence fixes).
- **Storage:** 500-point binary ring buffer.
- **Status:** Verified `0/500` cache post-reboot; buzzer chimes (ascending ON / descending OFF); 1-minute heartbeat (2700Hz fix / 2000Hz searching).
- **Deployment:** GitHub `ViVSoftComputers/tracker-firmware` (Public); Release v2.0.0 active.

## Files & Paths
- **Repository:** `https://github.com/ViVSoftComputers/tracker-firmware`
- **Workspace:** `C:\Users\vivso\Documents\Friday\DEV\Meshtastic\`
- **Binary:** `firmware-tracker-t1000e.uf2` (v2.0.0)
- **CLI:** `tracker_tool.py`, `tracker.bat`
- **Patch:** `meshtastic-tracker-hook.patch` (Modules.cpp, ButtonThread.cpp)

## Open Questions
- Need to verify long-term power consumption in Tracker Mode.
- Evaluate if adding a "Status LED" heartbeat pulse during tracking adds significant battery drain.