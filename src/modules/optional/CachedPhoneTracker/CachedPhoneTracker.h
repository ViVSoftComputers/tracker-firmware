#pragma once

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "GPS.h"
#include "GeoCoord.h"
#include "BluetoothStatus.h"
#include <FSCommon.h>

#define MAX_CACHED_POSITIONS 500
#define TRACKER_LOG_INTERVAL_MS 60000 // Log once every 60 seconds (1 minute)
#define TRACKER_POLL_INTERVAL_MS 3000 // Poll interval while tracker is idle

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
    static void logManualReading();
    static void clearCacheFromButton();
    void captureManualPoint();
    void clearCache();

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
    bool lastBleConnected;

    // Manual single-click reading state
    bool manualReadingPending;
    uint32_t manualReadingRequestedTime;

    // Asynchronous non-blocking text dump state ($TRK lines for GPX / tracker_tool.py)
    bool dumpActive;
    uint16_t dumpCurIdx;
    uint16_t dumpTotalToSend;
    uint16_t dumpSentCount;

    // Asynchronous non-blocking position sync state (native POSITION_APP packets for Meshtastic app)
    bool syncActive;
    uint16_t syncCurIdx;
    uint16_t syncTotalToSend;
    uint16_t syncSentCount;

    void appendToCache(const meshtastic_Position &pos);
    bool readCachedEntry(uint16_t index, TrackPoint &pt);
    void saveCacheIndex();
    void loadCacheIndex();
    void startDump();
    void startSync();
    bool sendPositionToPhone(const TrackPoint &pt);
    void replyText(const char *msg);
};

extern CachedPhoneTracker *cachedPhoneTracker;
void setupCachedPhoneTracker();
