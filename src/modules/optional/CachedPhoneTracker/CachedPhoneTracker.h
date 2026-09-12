#pragma once

#include "modules/optional/tracker/PositionCache.h"
#include "MeshModule.h"
#include "concurrency/OSThread.h"

/**
 * CachedPhoneTracker — T1000-E firmware module
 *
 * Caches GPS positions to LittleFS while BLE is disconnected from a phone.
 * On reconnection, flushes the cache via BLE to the companion app.
 *
 * 4-click button toggle activates/deactivates tracker mode.
 * When OFF, the module is a complete no-op — Meshtastic runs unmodified.
 */
class CachedPhoneTracker : public MeshModule, public concurrency::OSThread
{
  public:
    CachedPhoneTracker();

    // Toggle tracker mode (called from ButtonThread on 4-click)
    static void toggleTrackerMode();

    // Check if tracker mode is currently active
    static bool isTrackerModeActive();

  protected:
    virtual int32_t runOnce() override;

  private:
    void flushCache();
    void cachePosition(double lat, double lon, int32_t alt, uint32_t ts);

    PositionCache *cache = nullptr;
    uint32_t lastCaptureMs = 0;
    uint32_t lastGpsEnableMs = 0;
    static bool trackerModeActive;
    static bool ledState;
};