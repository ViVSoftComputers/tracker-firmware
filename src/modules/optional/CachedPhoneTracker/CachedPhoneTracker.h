#pragma once

#include "MeshModule.h"
#include "concurrency/OSThread.h"

// -----------------------------------------------------------------------------
// CachedPhoneTracker - Standalone GPS tracking module
// -----------------------------------------------------------------------------
// Tracks GPS positions offline and syncs to the Meshtastic Position Log
// when a phone (BLE/Serial) reconnects.
//
// Features:
//   - 60-second interval position logging
//   - Ring buffer (2048 points) in LittleFS flat binary files
//   - 1-5 click button gestures for control
//   - Auto-sync to Meshtastic mobile app via POSITION_APP packets
//   - SPILock concurrency guards on all flash access
//   - Buzzer power gating for power efficiency
//
// Target: Seeed SenseCAP T1000-E (nRF52840)
// Version: v3.0.2
// -----------------------------------------------------------------------------

struct TrackPoint;

class CachedPhoneTracker : public concurrency::OSThread
{
public:
    CachedPhoneTracker();

    // Module lifecycle
    bool setup();
    int32_t runOnce() override;

    // Button gesture handler (dispatched from ButtonThread)
    void handleMultipress();

    // Get tracking state
    bool isTracking() const { return _tracking_enabled; }
    uint32_t getPointCount() const { return _point_count; }

    // Admin message handler (for CLI interaction)
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp);

protected:
    bool _tracking_enabled;

private:
    uint32_t _last_log_time;
    uint32_t _write_index;
    uint32_t _point_count;
    bool _dump_in_progress;
    bool _sync_pending;

    void _logCurrentPosition();
    void _writePoint(struct TrackPoint *tp);
    void _rotateLog();
    bool _readPoint(uint32_t index, struct TrackPoint *tp);
    void _saveIndex();
    void _loadIndex();
    void _pushPendingPositions();
    void _clearCache();
    bool isPhoneConnected();

    void _buzzerBeep(uint32_t duration_ms);
    void _buzzerMelody(uint32_t count, uint32_t on_ms, uint32_t off_ms);
};

extern CachedPhoneTracker *cachedPhoneTracker;
void setupCachedPhoneTracker();
