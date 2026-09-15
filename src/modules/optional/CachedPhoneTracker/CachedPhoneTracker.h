#pragma once

#include "FSCom.h"
#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "configuration.pb.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"

#define CACHE_FILE_POINTS "/tracker_points.dat"
#define CACHE_FILE_INDEX  "/tracker_index.dat"
#define MAX_CACHED_POSITIONS 500
#define TRACKER_POLL_INTERVAL_MS 5000

struct __attribute__((packed)) TrackPoint {
    int32_t lat_i;
    int32_t lon_i;
    int16_t alt;
    uint32_t timestamp;
    uint16_t hdop;
    uint8_t sats;
    uint8_t flags;
};

class CachedPhoneTracker : public MeshModule, public concurrency::OSThread
{
  public:
    CachedPhoneTracker();

    virtual int32_t runOnce() override;

  protected:
    virtual bool wantsPacket(const meshtastic_MeshPacket *p) override;
    virtual meshtastic_MeshPacket_Priority getPacketPriority(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool trackerModeActive;
    bool wasConnectedToPhone;
    uint32_t lastLoggedTime;
    int32_t lastLat;
    int32_t lastLon;

    int32_t currentLat;
    int32_t currentLon;
    int16_t currentAlt;
    uint16_t currentHdop;
    uint8_t currentSats;
    meshtastic_Position_LocData gpsStatus;

    uint16_t cache_count;
    uint16_t cache_head;
    uint16_t cache_tail;

    // Asynchronous Dump Streaming state
    bool dumpActive;
    uint16_t dumpCurIdx;
    uint16_t dumpTotalToSend;
    uint16_t dumpSentCount;
    FSCom dumpFile;

    void toggleTrackerMode();
    void setTrackerMode(bool enable);
    void appendToCache(const meshtastic_Position &pos);
    void clearCache();
    void loadCacheIndex();
    void saveCacheIndex();
    bool readCachedEntry(uint16_t index, TrackPoint &pt);
    void startDump();
    void sendCacheViaText();
    void replyText(const char *text);
};

extern CachedPhoneTracker *cachedPhoneTracker;
