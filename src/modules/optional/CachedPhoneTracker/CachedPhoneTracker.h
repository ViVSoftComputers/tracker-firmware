#pragma once

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "GPS.h"
#include "GeoCoord.h"
#include <FSCommon.h>

#define MAX_CACHED_POSITIONS 500
#define TRACKER_MIN_MOVE_METERS 10.0f
#define TRACKER_MAX_STATIONARY_INTERVAL_MS 60000
#define TRACKER_POLL_INTERVAL_MS 5000

#define CACHE_PATH "/tracker_points.dat"
#define INDEX_PATH "/tracker_index.dat"

#pragma pack(push, 1)
struct TrackPoint {
    uint32_t timestamp;
    int32_t lat_i;
    int32_t lon_i;
    int16_t alt;
    uint16_t hdop;
    uint8_t sats;
    uint8_t flags;
};
#pragma pack(pop)

class CachedPhoneTracker : public MeshModule, private concurrency::OSThread
{
public:
    CachedPhoneTracker();

    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    static void toggleTrackerMode();
    static bool isTrackerModeActive() { return trackerModeActive; }

protected:
    virtual int32_t runOnce() override;

private:
    static bool trackerModeActive;
    uint16_t cache_count;
    uint16_t cache_head;
    uint16_t cache_tail;
    int32_t last_lat_i;
    int32_t last_lon_i;
    uint32_t last_capture_ms;
    uint32_t point_count;

    void appendToCache(const meshtastic_Position &pos);
    bool readCachedEntry(uint16_t index, TrackPoint &pt);
    void saveCacheIndex();
    void loadCacheIndex();
    void clearCache();
    void sendCacheViaText();
    void replyText(const char *msg);
};

extern CachedPhoneTracker *cachedPhoneTracker;
void setupCachedPhoneTracker();
