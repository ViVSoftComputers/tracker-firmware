# There is also now a V2 of this project available 

Here --> https://github.com/ViVSoftComputers/tracker-firmware-2

# Meshtastic Cached GPS Tracker Firmware

Standalone GPS tracking firmware additions for two Meshtastic devices:

- **Seeed SenseCAP T1000-E** (nRF52840), with physical-button gestures and a compact tracker enclosure.
- **Heltec T096**, with an on-device tracker status card and menu controls.

Both targets use the `CachedPhoneTracker` module for locally cached GPS points and synchronization with the Meshtastic phone app. Device-specific controls and hardware details are called out separately below.

> **Source-tree note:** This repository is a lightweight source overlay, not a complete Meshtastic firmware checkout. Apply the changes to the matching full Meshtastic source tree before building. Build environments and firmware behavior can depend on the Meshtastic revision and local patches; confirm the target and features in the checkout you are using.


### Links: (Note: As an affiliate, I earn from qualifying purchases)

* Heltec T096 - https://www.lorameshdevices.com/meshtastic-lora-t096-development-board-nrf52840-sx1262-supports-lora-bluetooth-integrated-l1-l5-gnss-compatible-with-meshcore-lorawan-arduino-ultra-low-power-for-off-grid-communication-tracker-v2-with-nrf52840
* Seeed T1000-E - https://www.lorameshdevices.com/sensecap-card-tracker-t1000-e-for-meshtastic
* LoRameshDevices.com: https://www.lorameshdevices.com
