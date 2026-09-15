#include "CachedPhoneTracker.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "buzz.h"
#include "configuration.h"
#include "main.h"

#define LED_STATE_ON 1

bool CachedPhoneTracker::trackerModeActive = true;
CachedPhoneTracker *cachedPhoneTracker = nullptr;

void setupCachedPhoneTracker()
{
    if (cachedPhoneTracker == nullptr) {
        cachedPhoneTracker = new CachedPhoneTracker();
    }
}

CachedPhoneTracker::CachedPhoneTracker()
    : MeshModule("CachedPhoneTracker"), concurrency::OSThread("CachedPhoneTracker")
{
    ourPortNum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    loopbackOk = true;

    cache_count = 0;
    cache_head = 0;
    cache_tail = 0;
    last_lat_i = 0;
    last_lon_i = 0;
    last_capture_ms = 0;
    point_count = 0;

    loadCacheIndex();
    LOG_INFO("CachedPhoneTracker: initialized (restored %u cached points)\n", cache_count);

    setIntervalFromNow(3000);
}

bool CachedPhoneTracker::wantPacket(const meshtastic_MeshPacket *p)
{
    return (p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP);
}

void CachedPhoneTracker::toggleTrackerMode()
{
    trackerModeActive = !trackerModeActive;
    if (trackerModeActive) {
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, LED_STATE_ON);
        play4ClickUp();
        LOG_INFO("CachedPhoneTracker: Tracker Mode ON\n");
    } else {
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        play4ClickDown();
        LOG_INFO("CachedPhoneTracker: Tracker Mode OFF\n");
    }
}

void CachedPhoneTracker::replyText(const char *msg)
{
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    if (!p)
        return;
    p->to = nodeDB->getNodeNum();
    p->hop_limit = 0;
    p->from = nodeDB->getNodeNum();
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.payload.size = snprintf((char *)p->decoded.payload.bytes,
                                       sizeof(p->decoded.payload.bytes), "%s", msg);
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    service->sendToPhone(p);
}

int32_t CachedPhoneTracker::runOnce()
{
    if (!trackerModeActive) {
        return TRACKER_POLL_INTERVAL_MS;
    }

    if (gps && gps->isConnected()) {
        gps->enable();
    }

    int32_t lat_i = 0;
    int32_t lon_i = 0;
    if (gps && gps->isConnected()) {
        lat_i = gps->p.latitude_i;
        lon_i = gps->p.longitude_i;
    }

    if (lat_i == 0 && lon_i == 0) {
        return 3000;
    }

    uint32_t now = millis();
    bool movedFar = false;
    if (last_lat_i != 0 || last_lon_i != 0) {
        float dist = GeoCoord::latLongToMeter(
            (double)last_lat_i * 1e-7, (double)last_lon_i * 1e-7,
            (double)lat_i * 1e-7, (double)lon_i * 1e-7);
        movedFar = (dist >= TRACKER_MIN_MOVE_METERS);
    } else {
        movedFar = true;
    }

    bool stationaryTimeout = ((now - last_capture_ms) >= TRACKER_MAX_STATIONARY_INTERVAL_MS);

    if (movedFar || stationaryTimeout) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = lat_i;
        pos.longitude_i = lon_i;
        pos.altitude = gps->p.altitude;
        pos.HDOP = gps->p.HDOP;
        pos.sats_in_view = gps->p.sats_in_view;
        pos.timestamp = getValidTime(RTCQuality::RTCQualityGPS, true);

        appendToCache(pos);
        last_lat_i = lat_i;
        last_lon_i = lon_i;
        last_capture_ms = now;
        point_count++;

        digitalWrite(PIN_LED1, !LED_STATE_ON);
        playBeep();
        delay(60);
        digitalWrite(PIN_LED1, LED_STATE_ON);

        LOG_INFO("CachedPhoneTracker: captured #%u lat=%.6f lon=%.6f (cache=%u/%u)\n",
                 point_count, lat_i * 1e-7, lon_i * 1e-7, cache_count, MAX_CACHED_POSITIONS);
    }

    return TRACKER_POLL_INTERVAL_MS;
}

void CachedPhoneTracker::appendToCache(const meshtastic_Position &pos)
{
    TrackPoint pt;
    pt.timestamp = pos.timestamp;
    pt.lat_i = pos.latitude_i;
    pt.lon_i = pos.longitude_i;
    pt.alt = (int16_t)pos.altitude;
    pt.hdop = (uint16_t)pos.HDOP;
    pt.sats = (uint8_t)pos.sats_in_view;
    pt.flags = 0;

    File f = FSCom.open(CACHE_PATH, FILE_O_WRITE);
    if (!f) {
        LOG_ERROR("CachedPhoneTracker: cannot open %s\n", CACHE_PATH);
        return;
    }

    uint32_t offset = (uint32_t)cache_head * sizeof(TrackPoint);
    f.seek(offset);
    f.write((uint8_t *)&pt, sizeof(TrackPoint));
    f.flush();
    f.close();

    if (cache_count < MAX_CACHED_POSITIONS) {
        cache_count++;
    }
    cache_head = (cache_head + 1) % MAX_CACHED_POSITIONS;
    if (cache_count >= MAX_CACHED_POSITIONS) {
        cache_tail = (cache_tail + 1) % MAX_CACHED_POSITIONS;
    }

    saveCacheIndex();
}

bool CachedPhoneTracker::readCachedEntry(uint16_t index, TrackPoint &pt)
{
    if (!FSCom.exists(CACHE_PATH))
        return false;

    File f = FSCom.open(CACHE_PATH, FILE_O_READ);
    if (!f)
        return false;

    f.seek((uint32_t)index * sizeof(TrackPoint));
    bool ok = (f.read((uint8_t *)&pt, sizeof(TrackPoint)) == sizeof(TrackPoint));
    f.close();
    return ok;
}

void CachedPhoneTracker::saveCacheIndex()
{
    File f = FSCom.open(INDEX_PATH, FILE_O_WRITE);
    if (!f)
        return;

    f.seek(0);
    uint16_t data[3] = {cache_count, cache_head, cache_tail};
    f.write((uint8_t *)data, sizeof(data));
    f.flush();
    f.close();
}

void CachedPhoneTracker::loadCacheIndex()
{
    if (!FSCom.exists(INDEX_PATH)) {
        cache_count = 0;
        cache_head = 0;
        cache_tail = 0;
        return;
    }

    File f = FSCom.open(INDEX_PATH, FILE_O_READ);
    if (!f)
        return;

    uint16_t data[3] = {0};
    if (f.read((uint8_t *)data, sizeof(data)) == sizeof(data)) {
        cache_count = data[0];
        cache_head = data[1];
        cache_tail = data[2];
        if (cache_count > MAX_CACHED_POSITIONS)
            cache_count = 0;
        if (cache_head >= MAX_CACHED_POSITIONS)
            cache_head = 0;
        if (cache_tail >= MAX_CACHED_POSITIONS)
            cache_tail = 0;
    }
    f.close();
}

void CachedPhoneTracker::clearCache()
{
    cache_count = 0;
    cache_head = 0;
    cache_tail = 0;
    if (FSCom.exists(CACHE_PATH)) {
        FSCom.remove(CACHE_PATH);
    }
    saveCacheIndex();
}

void CachedPhoneTracker::sendCacheViaText()
{
    if (cache_count == 0) {
        replyText("CACHE EMPTY");
        return;
    }

    uint16_t idx = cache_tail;
    for (uint16_t i = 0; i < cache_count; i++) {
        TrackPoint pt;
        if (readCachedEntry(idx, pt)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "$TRK,%.6f,%.6f,%d,%u,%u",
                     pt.lat_i * 1e-7, pt.lon_i * 1e-7,
                     pt.alt, pt.timestamp, pt.hdop);
            replyText(buf);
            delay(40);
        }
        idx = (idx + 1) % MAX_CACHED_POSITIONS;
    }

    char doneBuf[64];
    snprintf(doneBuf, sizeof(doneBuf), "DUMP COMPLETE: %u points", cache_count);
    replyText(doneBuf);
}

ProcessMessage CachedPhoneTracker::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const char *payload = (const char *)mp.decoded.payload.bytes;
    size_t plen = mp.decoded.payload.size;

    if (plen >= 14 && memcmp(payload, "tracker:status", 14) == 0) {
        char statusBuf[160];
        const char *gpsStatus = "NO_FIX";
        uint8_t sats = 0;
        float lat = 0.0f;
        float lon = 0.0f;
        if (gps && gps->isConnected()) {
            if (gps->hasLock()) {
                gpsStatus = "LOCKED";
            } else if (gps->p.latitude_i != 0 || gps->p.longitude_i != 0) {
                gpsStatus = "LAST_FIX";
            }
            sats = gps->p.sats_in_view;
            lat = gps->p.latitude_i * 1e-7;
            lon = gps->p.longitude_i * 1e-7;
        }

        snprintf(statusBuf, sizeof(statusBuf),
                 "STATUS: mode=%s, gps=%s, sats=%u, lat=%.6f, lon=%.6f, cache=%u/%u",
                 trackerModeActive ? "ON" : "OFF",
                 gpsStatus, sats, lat, lon,
                 cache_count, MAX_CACHED_POSITIONS);
        replyText(statusBuf);
        return ProcessMessage::STOP;
    }

    if (plen >= 10 && memcmp(payload, "tracker:on", 10) == 0) {
        trackerModeActive = true;
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, LED_STATE_ON);
        play4ClickUp();
        replyText("STATUS: Tracker mode ON");
        return ProcessMessage::STOP;
    }

    if (plen >= 11 && memcmp(payload, "tracker:off", 11) == 0) {
        trackerModeActive = false;
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        play4ClickDown();
        replyText("STATUS: Tracker mode OFF");
        return ProcessMessage::STOP;
    }

    if (plen >= 13 && memcmp(payload, "tracker:clear", 13) == 0) {
        clearCache();
        replyText("STATUS: Cache cleared (0/500)");
        return ProcessMessage::STOP;
    }

    if (plen >= 12 && memcmp(payload, "tracker:dump", 12) == 0) {
        sendCacheViaText();
        return ProcessMessage::STOP;
    }

    if (plen >= 12 && memcmp(payload, "tracker:test", 12) == 0) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        if (gps && (gps->p.latitude_i != 0 || gps->p.longitude_i != 0)) {
            pos.latitude_i = gps->p.latitude_i;
            pos.longitude_i = gps->p.longitude_i;
            pos.altitude = gps->p.altitude;
            pos.sats_in_view = gps->p.sats_in_view;
            pos.HDOP = gps->p.HDOP;
        } else {
            pos.latitude_i = 277935516;
            pos.longitude_i = -826546233;
            pos.altitude = 22;
            pos.sats_in_view = 8;
            pos.HDOP = 120;
        }
        pos.timestamp = getValidTime(RTCQuality::RTCQualityGPS, true);
        if (pos.timestamp == 0)
            pos.timestamp = 1789437732;

        appendToCache(pos);
        point_count++;

        digitalWrite(PIN_LED1, !LED_STATE_ON);
        playBeep();
        delay(60);
        digitalWrite(PIN_LED1, LED_STATE_ON);

        char testBuf[80];
        snprintf(testBuf, sizeof(testBuf), "STATUS: Test point recorded! (cache=%u/%u)",
                 cache_count, MAX_CACHED_POSITIONS);
        replyText(testBuf);
        return ProcessMessage::STOP;
    }

    return ProcessMessage::CONTINUE;
}
