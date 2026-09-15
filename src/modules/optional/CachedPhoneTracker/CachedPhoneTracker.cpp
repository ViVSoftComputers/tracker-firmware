#include "CachedPhoneTracker.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "buzz.h"
#include "configuration.h"
#include "main.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

#define LED_STATE_ON 1

// Default tracker mode to OFF on boot (Requirement 3)
bool CachedPhoneTracker::trackerModeActive = false;
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
    lastBleConnected = false;

    dumpActive = false;
    dumpCurIdx = 0;
    dumpTotalToSend = 0;
    dumpSentCount = 0;

    syncActive = false;
    syncCurIdx = 0;
    syncTotalToSend = 0;
    syncSentCount = 0;

    loadCacheIndex();
    LOG_INFO("CachedPhoneTracker: initialized (restored %u cached points, mode=%s)\n",
             cache_count, trackerModeActive ? "ON" : "OFF");

    setIntervalFromNow(3000);
}

bool CachedPhoneTracker::wantPacket(const meshtastic_MeshPacket *p)
{
    return (p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP);
}

void CachedPhoneTracker::toggleTrackerMode()
{
    trackerModeActive = !trackerModeActive;
    pinMode(PIN_LED1, OUTPUT);
    if (trackerModeActive) {
        digitalWrite(PIN_LED1, LED_STATE_ON);
        play4ClickUp();
        if (cachedPhoneTracker) {
            cachedPhoneTracker->last_capture_ms = 0; // Trigger capture as soon as GPS fixes
        }
        LOG_INFO("CachedPhoneTracker: Tracker Mode ON (1 pt/min)\n");
    } else {
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        play4ClickDown();
        LOG_INFO("CachedPhoneTracker: Tracker Mode OFF (cache preserved)\n");
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

bool CachedPhoneTracker::sendPositionToPhone(const TrackPoint &pt)
{
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    if (!p)
        return false;

    meshtastic_Position pos = meshtastic_Position_init_default;
    pos.latitude_i = pt.lat_i;
    pos.longitude_i = pt.lon_i;
    pos.has_latitude_i = true;
    pos.has_longitude_i = true;
    pos.altitude = pt.alt;
    pos.has_altitude = true;
    pos.time = pt.timestamp;
    pos.timestamp = pt.timestamp;
    pos.HDOP = pt.hdop;
    pos.sats_in_view = pt.sats;

    p->to = NODENUM_BROADCAST;
    p->from = nodeDB->getNodeNum();
    p->decoded.portnum = meshtastic_PortNum_POSITION_APP;
    p->decoded.want_response = false;
    p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
    p->hop_limit = 0;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p->decoded.payload.size = pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes),
                                                 &meshtastic_Position_msg, &pos);
    service->sendToPhone(p);
    return true;
}

int32_t CachedPhoneTracker::runOnce()
{
    // 1. Service active non-blocking text dump ($TRK stream for tracker_tool.py / GPX export)
    if (dumpActive) {
        if (dumpSentCount >= dumpTotalToSend) {
            char doneBuf[64];
            snprintf(doneBuf, sizeof(doneBuf), "DUMP COMPLETE: %u points", dumpSentCount);
            replyText(doneBuf);
            dumpActive = false;
            return TRACKER_POLL_INTERVAL_MS;
        }

        char msgBuf[230];
        msgBuf[0] = '\0';
        size_t offset = 0;
        uint8_t batchCount = 0;

        while (dumpSentCount < dumpTotalToSend && batchCount < 4) {
            TrackPoint pt;
            if (readCachedEntry(dumpCurIdx, pt)) {
                int written = snprintf(msgBuf + offset, sizeof(msgBuf) - offset,
                                       "%s$TRK,%.6f,%.6f,%d,%u,%u",
                                       batchCount > 0 ? "\n" : "",
                                       pt.lat_i * 1e-7, pt.lon_i * 1e-7,
                                       pt.alt, pt.timestamp, pt.hdop);
                if (written > 0 && (size_t)written < (sizeof(msgBuf) - offset)) {
                    offset += written;
                    batchCount++;
                } else {
                    break;
                }
            }
            dumpCurIdx = (dumpCurIdx + 1) % MAX_CACHED_POSITIONS;
            dumpSentCount++;
        }

        if (batchCount > 0) {
            replyText(msgBuf);
        }

        return 40; // 40ms yield to drain toPhoneQueue
    }

    // 2. Service active non-blocking position sync (native POSITION_APP packets for Meshtastic phone app)
    if (syncActive) {
        if (syncSentCount >= syncTotalToSend) {
            char doneBuf[64];
            snprintf(doneBuf, sizeof(doneBuf), "SYNC COMPLETE: %u points to phone app", syncSentCount);
            replyText(doneBuf);
            syncActive = false;
            LOG_INFO("CachedPhoneTracker: %s\n", doneBuf);
            return TRACKER_POLL_INTERVAL_MS;
        }

        if (service->isToPhoneQueueEmpty()) {
            TrackPoint pt;
            if (readCachedEntry(syncCurIdx, pt)) {
                sendPositionToPhone(pt);
            }
            syncCurIdx = (syncCurIdx + 1) % MAX_CACHED_POSITIONS;
            syncSentCount++;
        }

        return 60; // 60ms pacing between POSITION_APP packets for BLE
    }

    // 3. Auto-sync check on Bluetooth connection
#if !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (bluetoothStatus) {
        bool bleConnected = (bluetoothStatus->getConnectionState() == meshtastic::BluetoothStatus::ConnectionState::CONNECTED);
        if (bleConnected && !lastBleConnected) {
            LOG_INFO("CachedPhoneTracker: Phone connected via BLE. Auto-syncing %u cached points to phone app...\n", cache_count);
            if (cache_count > 0 && !syncActive && !dumpActive) {
                startSync();
            }
        }
        lastBleConnected = bleConnected;
    }
#endif

    // 4. If tracker module is OFF, do not force GPS awake; Meshtastic handles power & GPS normally (Requirement 2 & 3)
    if (!trackerModeActive) {
        return TRACKER_POLL_INTERVAL_MS;
    }

    // 5. Tracker mode is ON: keep GPS enabled and log once every 60 seconds (Requirement 6)
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
    // Check if 1 minute (60 seconds) has elapsed since last capture
    if (last_capture_ms != 0 && (now - last_capture_ms < TRACKER_LOG_INTERVAL_MS)) {
        return 3000;
    }

    meshtastic_Position pos = meshtastic_Position_init_default;
    pos.latitude_i = lat_i;
    pos.longitude_i = lon_i;
    pos.has_latitude_i = true;
    pos.has_longitude_i = true;
    pos.altitude = gps->p.altitude;
    pos.has_altitude = true;
    pos.HDOP = gps->p.HDOP;
    pos.sats_in_view = gps->p.sats_in_view;
    pos.time = getValidTime(RTCQuality::RTCQualityGPS, true);
    if (pos.time == 0)
        pos.time = getValidTime(RTCQuality::RTCQualityDevice, true);
    pos.timestamp = pos.time;

    appendToCache(pos);
    last_lat_i = lat_i;
    last_lon_i = lon_i;
    last_capture_ms = now;
    point_count++;

    // Brief LED blink to acknowledge logged waypoint
    digitalWrite(PIN_LED1, !LED_STATE_ON);
    delay(50);
    digitalWrite(PIN_LED1, LED_STATE_ON);

    LOG_INFO("CachedPhoneTracker: logged 1-min point #%u lat=%.6f lon=%.6f (cache=%u/%u)\n",
             point_count, lat_i * 1e-7, lon_i * 1e-7, cache_count, MAX_CACHED_POSITIONS);

    return 3000;
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
    if (!f)
        return;

    uint32_t writePos = (uint32_t)cache_head * sizeof(TrackPoint);
    f.seek(writePos);
    f.write((uint8_t *)&pt, sizeof(TrackPoint));
    f.flush();
    f.close();

    cache_head = (cache_head + 1) % MAX_CACHED_POSITIONS;
    if (cache_count < MAX_CACHED_POSITIONS) {
        cache_count++;
    } else {
        cache_tail = (cache_tail + 1) % MAX_CACHED_POSITIONS;
    }

    saveCacheIndex();
}

bool CachedPhoneTracker::readCachedEntry(uint16_t index, TrackPoint &pt)
{
    if (index >= MAX_CACHED_POSITIONS || !FSCom.exists(CACHE_PATH))
        return false;

    File f = FSCom.open(CACHE_PATH, FILE_O_READ);
    if (!f)
        return false;

    uint32_t readPos = (uint32_t)index * sizeof(TrackPoint);
    f.seek(readPos);
    bool ok = (f.read((uint8_t *)&pt, sizeof(TrackPoint)) == sizeof(TrackPoint));
    f.close();
    return ok;
}

void CachedPhoneTracker::saveCacheIndex()
{
    File f = FSCom.open(INDEX_PATH, FILE_O_WRITE);
    if (!f)
        return;

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
    dumpActive = false;
    syncActive = false;
    if (FSCom.exists(CACHE_PATH)) {
        FSCom.remove(CACHE_PATH);
    }
    saveCacheIndex();
    LOG_INFO("CachedPhoneTracker: Cache cleared\n");
}

void CachedPhoneTracker::startDump()
{
    if (cache_count == 0) {
        replyText("CACHE EMPTY");
        return;
    }

    dumpActive = true;
    dumpCurIdx = cache_tail;
    dumpTotalToSend = cache_count;
    dumpSentCount = 0;
    setIntervalFromNow(10);
}

void CachedPhoneTracker::startSync()
{
    if (cache_count == 0) {
        replyText("SYNC: CACHE EMPTY");
        return;
    }

    syncActive = true;
    syncCurIdx = cache_tail;
    syncTotalToSend = cache_count;
    syncSentCount = 0;
    setIntervalFromNow(10);
    LOG_INFO("CachedPhoneTracker: Starting sync of %u points to phone app\n", syncTotalToSend);
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
        if (!trackerModeActive) {
            toggleTrackerMode();
        }
        replyText("STATUS: Tracker mode ON (1 pt/min)");
        return ProcessMessage::STOP;
    }

    if (plen >= 11 && memcmp(payload, "tracker:off", 11) == 0) {
        if (trackerModeActive) {
            toggleTrackerMode();
        }
        replyText("STATUS: Tracker mode OFF (cache preserved)");
        return ProcessMessage::STOP;
    }

    if (plen >= 13 && memcmp(payload, "tracker:clear", 13) == 0) {
        clearCache();
        replyText("STATUS: Cache cleared (0/500)");
        return ProcessMessage::STOP;
    }

    if (plen >= 12 && memcmp(payload, "tracker:sync", 12) == 0) {
        startSync();
        return ProcessMessage::STOP;
    }

    if (plen >= 12 && memcmp(payload, "tracker:dump", 12) == 0) {
        startDump();
        return ProcessMessage::STOP;
    }

    if (plen >= 12 && memcmp(payload, "tracker:test", 12) == 0) {
        meshtastic_Position pos = meshtastic_Position_init_default;
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
        pos.has_latitude_i = true;
        pos.has_longitude_i = true;
        pos.has_altitude = true;
        pos.time = getValidTime(RTCQuality::RTCQualityGPS, true);
        if (pos.time == 0)
            pos.time = 1789437732;
        pos.timestamp = pos.time;

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
