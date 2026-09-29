# Meshtastic Cached GPS Tracker Firmware

Standalone GPS tracking firmware additions for two Meshtastic devices:

- **Seeed SenseCAP T1000-E** (nRF52840), with physical-button gestures and a compact tracker enclosure.
- **Heltec T096**, with an on-device tracker status card and menu controls.

Both targets use the `CachedPhoneTracker` module for locally cached GPS points and synchronization with the Meshtastic phone app. Device-specific controls and hardware details are called out separately below.

> **Source-tree note:** This repository is a lightweight source overlay, not a complete Meshtastic firmware checkout. Apply the changes to the matching full Meshtastic source tree before building. Build environments and firmware behavior can depend on the Meshtastic revision and local patches; confirm the target and features in the checkout you are using.

---

## Before flashing: make backups

Custom firmware can replace the installed firmware and may affect device configuration or flash storage. Before flashing either device:

1. **Export your Meshtastic configuration** while connected to the device:
   ```bash
   meshtastic --export-config > meshtastic_backup_config.yaml
   ```
2. **Back up the currently installed firmware** if the device exposes a bootloader drive with `CURRENT.UF2`. Enter bootloader mode using the device-specific procedure, then copy that file somewhere safe.
3. Keep the firmware image and device target clearly identified. Never flash a T1000-E image to a T096 or vice versa.

After flashing, restore configuration if needed:
```bash
meshtastic --configure meshtastic_backup_config.yaml
```

## Shared tracker behavior

- GPS points are cached in LittleFS so logging can continue without the phone connected.
- The tracker uses a 60-second interval while continuous tracking is enabled.
- Cached points can be inspected, cleared, exported, and synchronized to the Meshtastic phone app using the supported firmware controls/tools.
- The tracker is intended for local device control. Do not assume that every Meshtastic position or radio setting is private; review your channel, position-broadcast, and device configuration before use.
- T1000-E hardware uses SPI locking around shared flash operations and a power-gated buzzer.

## Device controls

### Seeed SenseCAP T1000-E

The physical button is **P0.06**. Its firmware gestures are:

| Button gesture | Action |
|---|---|
| **1 click** | Log a waypoint manually (GPS may be powered if needed) |
| **2 clicks** | Toggle continuous Tracker Mode on/off |
| **3 clicks** | Clear the cached point data |
| **4 clicks** | GPS toggle / Meshtastic position action, according to the configured firmware behavior |
| **5 clicks** | Local node status/ping action |

Tracker Mode defaults to **OFF** after boot in the documented T1000-E build. Audio feedback is provided for tracker actions. The T1000-E mapping and buzzer behavior are board-specific; do not apply these pin numbers to the T096.

**T1000-E hardware mapping**

| Component | Mapping |
|---|---|
| User button | P0.06 |
| Status LED | P0.15 (`PIN_LED1`) |
| Buzzer PWM | P0.25 (Pin 25) |
| Buzzer power enable | P1.05 (Pin 37) |
| MCU | Nordic nRF52840 |
| PlatformIO environment | `tracker-t1000-e` |

**Build and flash**

From the full Meshtastic source tree with the tracker changes applied:
```bash
pio run -e tracker-t1000-e
```

Use the UF2 produced for this target. Enter the T1000-E bootloader using the device's double-press procedure, confirm the expected bootloader drive, and copy the UF2 to it.

### Heltec T096

The T096 implementation adds a **Tracker** status card to the on-device screen carousel. The card displays whether tracking is on or off and the cached point count. Press **SELECT** while viewing that card to open its tracker menu:

- **Start Tracking** when tracking is off; **Stop Tracking** when it is on.
- **Clear Cache**, followed by a confirmation choice before cached points are erased.
- **Back** to leave the tracker menu.

These are screen/menu controls, not the T1000-E's 1–5 click gesture mapping. Use the T096's normal display-button navigation to move through and select menu options. The T096 does not use the T1000-E pin table above.

**Build and flash**

From the full Meshtastic source tree with the T096 tracker and screen changes applied:
```bash
pio run -e heltec-mesh-node-t096
```

Flash only the resulting image intended for the Heltec T096, following the board's documented bootloader procedure.

## Serial tools and GPX export

The accompanying `tracker.bat` and `tracker_tool.py` utilities provide serial status/control and GPX export where included in the source package. Typical commands are:

```cmd
tracker.bat status
tracker.bat dump -o tracklog.gpx
```

Or, with Python and the Meshtastic package installed:
```cmd
py tracker_tool.py dump -o tracklog.gpx
```

Check the utility's help output and the version-matched source package for supported commands. GPX exports contain location and time data; store and share them accordingly.

## Source layout

The complete build requires a full Meshtastic firmware source checkout. This overlay contains the project-specific changes, including the `CachedPhoneTracker` module, T1000-E button gesture routing, and T096 screen/menu integration. Build from the full source tree, not from this overlay by itself.

## License and attribution

This project is part of the **ViVSoft** hardware ecosystem and is built on the Meshtastic platform. Refer to the applicable Meshtastic and third-party component licenses in the full firmware source tree.
