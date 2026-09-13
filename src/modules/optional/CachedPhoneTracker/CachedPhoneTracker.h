#pragma once
#include "MeshModule.h"
#include "concurrency/OSThread.h"

#if HAS_GPS
#include "mesh-pb-constants.h"

class CachedPhoneTracker : public MeshModule, protected concurrency::OSThread
{
  public:
    CachedPhoneTracker();
    void setup();

    static void toggleTrackerMode();
    static bool isTrackerModeActive();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }
    virtual int32_t runOnce() override;

  private:
    // --- Constants ---
    static constexpr uint32_t POLL_INTERVAL_MS = 30000;
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
    static constexpr uint16_t FLUSH_BATCH_SIZE = 8;
    static constexpr uint32_t FLUSH_BATCH_DELAY_MS = 2000;
    bool isFlushing = false;
    uint16_t flushIdx = 0;
    uint16_t flushRemaining = 0;

    // --- Methods ---
    bool isBleConnected();
    void appendToCache(const meshtastic_Position &pos);
    bool readCachedEntry(uint16_t index, meshtastic_Position &pos, uint32_t &timestamp);
    void clearCache();
    void saveCacheIndex();
    void loadCacheIndex();
};

extern CachedPhoneTracker *cachedPhoneTracker;
void setupCachedPhoneTracker();

#else

void setupCachedPhoneTracker() {}

#endif