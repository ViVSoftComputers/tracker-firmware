#pragma once
#include "MeshModule.h"
#include "concurrency/OSThread.h"

#if HAS_GPS
#include "mesh-pb-constants.h"

/**
 * @brief Cached position tracker for T1000-E and similar tracker devices.
 *
 * When the phone app is NOT connected via BLE, Meshtastic's PositionModule silently
 * drops position-to-phone packets (gated by isToPhoneQueueEmpty()). This module
 * captures those positions into a LittleFS ring buffer and flushes them to the
 * phone in chronological order when BLE reconnects.
 *
 * Storage: binary ring buffer at /static/cached_positions.dat
 * Each entry is a packed meshtastic_Position protobuf with a 4-byte timestamp prefix.
 */
class CachedPhoneTracker : public MeshModule, protected concurrency::OSThread
{
  public:
    CachedPhoneTracker();
    void setup();

    /// 4-click toggle: enter/exit aggressive GPS tracking mode
    static void toggleTrackerMode();
    static bool isTrackerModeActive();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }
    virtual int32_t runOnce() override;

  private:
    // --- Constants ---
    static constexpr uint32_t POLL_INTERVAL_MS = 30000;
    static constexpr uint32_t FLUSH_INTERVAL_MS = 30000;
    static constexpr uint32_t MAX_STATIONARY_INTERVAL_MS = 60000;
    static constexpr float MIN_MOVE_METERS = 10.0f;
    static constexpr uint16_t MAX_CACHED_POSITIONS = 500;
    static constexpr uint16_t ENTRY_HEADER_SIZE = 8;
    static constexpr const char *CACHE_PATH = "/static/cached_positions.dat";
    static constexpr const char *INDEX_PATH = "/static/cached_positions.idx";

    // --- Tracking state ---
    int32_t last_lat_i = 0;
    int32_t last_lon_i = 0;
    uint32_t last_capture_ms = 0;
    uint32_t point_count = 0;

    // --- Tracker mode state ---
    static bool trackerModeActive;

    // --- Cache state ---
    uint16_t cache_count = 0;
    uint16_t cache_head = 0;
    uint16_t cache_tail = 0;
    bool was_ble_connected = false;

    // --- Batch flush state ---
    // sendToPhone() silently drops POSITION_APP when toPhoneQueue is full (only
    // TEXT/RANGE_TEST/ROUTING displace old entries). So we send ONE packet per
    // cycle at 200ms — BLE drains at 10-15/sec, well within 5/sec safety margin.
    static constexpr uint32_t FLUSH_TICK_MS = 200;
    bool isFlushing = false;
    uint16_t flushIdx = 0;
    uint16_t flushRemaining =0;

    // --- Methods ---
    bool isBleConnected();
    void appendToCache(const meshtastic_Position &pos);
    bool readCachedEntry(uint16_t index, meshtastic_Position &pos, uint32_t &timestamp);
    void clearCache();
    void saveCacheIndex();
    void dumpCacheContents();
    void loadCacheIndex();
};

extern CachedPhoneTracker *cachedPhoneTracker;
void setupCachedPhoneTracker();

#else // HAS_GPS

void setupCachedPhoneTracker() {}

#endif // HAS_GPS