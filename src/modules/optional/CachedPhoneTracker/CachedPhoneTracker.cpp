#include "CachedPhoneTracker.h"
#include "FSCom.h"
#include "GPS.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "buzz.h"
#include "configuration.h"
#include "graphics/Screen.h"
#include "led.h"
#include "main.h"
#include <cmath>

CachedPhoneTracker *cachedPhoneTracker = nullptr;

CachedPhoneTracker::CachedPhoneTracker()
    : MeshModule("tracker"), concurrency::OSThread("CachedPhoneTracker")
{
    trackerModeActive = true;
    wasConnectedToPhone = false;
    lastLoggedTime = 0;
    lastLat = 0;
    lastLon = 0;
    currentLat = 0;
    currentLon = 0;
    currentAlt = 0;
    currentHdop = 0;
    currentSats = 0;
    gpsStatus = meshtastic_Position_LocData_LocUnset;

    cache_count = 0;
    cache_head = 0;
    cache_tail = 0;

    dumpActive = false;
    dumpCurIdx = 0;
    dumpTotalToSend = 0;
    dumpSentCount = 0;

    loadCacheIndex();
    setInterval(TRACKER_POLL_INTERVAL_MS);
}

void CachedPhoneTracker::replyText(const char *text)
{
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    if (!p)
        return;

    p->to = NODENUM_BROADCAST;
    p->from = nodeDB->getNodeNum();
    p->id = generatePacketId();
    p->hop_limit = 0;
    p->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.want_response = false;

    size_t len = strlen(text);
    if (len > sizeof(p->decoded.payload.bytes))
        len = sizeof(p->decoded.payload.bytes);

    memcpy(p->decoded.payload.bytes, text, len);
    p->decoded.payload.size = len;

    service->sendToPhone(p);
}

bool CachedPhoneTracker::wantsPacket(const meshtastic_MeshPacket *p)
{
    return true;
}

meshtastic_MeshPacket_Priority CachedPhoneTracker::getPacketPriority(const meshtastic_MeshPacket *p)
{
    return meshtastic_MeshPacket_Priority_RELIABLE;
}

MeshModule::ProcessMessage CachedPhoneTracker::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return ProcessMessage::CONTINUE;

    if (mp.decoded.portnum == meshtastic_PortNum_POSITION_APP) {
        meshtastic_Position pos;
        if (pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, &meshtastic_Position_msg, &pos)) {
            if (pos.latitude_i != 0 || pos.longitude_i != 0) {
                currentLat = pos.latitude_i;
                currentLon = pos.longitude_i;
                currentAlt = (int16_t)pos.altitude;
                currentHdop = (uint16_t)pos.HDOP;
                currentSats = (uint8_t)pos.sats_in_view;
                gpsStatus = pos.location_source;
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        char msg[256];
        size_t len = mp.decoded.payload.size;
        if (len >= sizeof(msg))
            len = sizeof(msg) - 1;
        memcpy(msg, mp.decoded.payload.bytes, len);
        msg[len] = '\0';

        char *cmd = msg;
        while (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n')
            cmd++;

        if (strcasecmp(cmd, "tracker:on") == 0) {
            setTrackerMode(true);
            replyText("TRACKER MODE ON");
            return ProcessMessage::STOP;
        }

        if (strcasecmp(cmd, "tracker:off") == 0) {
            setTrackerMode(false);
            replyText("TRACKER MODE OFF");
            return ProcessMessage::STOP;
        }

        if (strcasecmp(cmd, "tracker:status") == 0) {
            char reply[160];
            const char *gpsStr = "NO_FIX";
            if (gpsStatus == meshtastic_Position_LocData_LocFixed)
                gpsStr = "LOCKED";
            else if (gpsStatus == meshtastic_Position_LocData_LocLastKnown)
                gpsStr = "LAST_FIX";

            snprintf(reply, sizeof(reply),
                     "STATUS: mode=%s, gps=%s, sats=%d, lat=%.6f, lon=%.6f, cache=%u/%u",
                     trackerModeActive ? "ON" : "OFF", gpsStr, currentSats,
                     currentLat * 1e-7, currentLon * 1e-7,
                     cache_count, MAX_CACHED_POSITIONS);
            replyText(reply);
            return ProcessMessage::STOP;
        }

        if (strcasecmp(cmd, "tracker:dump") == 0) {
            startDump();
            return ProcessMessage::STOP;
        }

        if (strcasecmp(cmd, "tracker:clear") == 0) {
            clearCache();
            replyText("CACHE CLEARED");
            return ProcessMessage::STOP;
        }

        if (strcasecmp(cmd, "tracker:test") == 0) {
            meshtastic_Position testPos = meshtastic_Position_init_default;
            testPos.latitude_i = 277936130;
            testPos.longitude_i = -826546630;
            testPos.altitude = 15;
            testPos.time = getTime();
            testPos.HDOP = 120;
            testPos.sats_in_view = 8;
            testPos.location_source = meshtastic_Position_LocData_LocFixed;
            appendToCache(testPos);
            replyText("INJECTED TEST WAYPOINT");
            return ProcessMessage::STOP;
        }
    }

    return ProcessMessage::CONTINUE;
}

void CachedPhoneTracker::loadCacheIndex()
{
    FSCom f;
    if (f.open(CACHE_FILE_INDEX, "r")) {
        uint16_t data[3];
        if (f.read((uint8_t *)data, sizeof(data)) == sizeof(data)) {
            cache_count = data[0];
            cache_head = data[1];
            cache_tail = data[2];
            if (cache_count > MAX_CACHED_POSITIONS) cache_count = 0;
            if (cache_head >= MAX_CACHED_POSITIONS) cache_head = 0;
            if (cache_tail >= MAX_CACHED_POSITIONS) cache_tail = 0;
        }
        f.close();
    }
}

void CachedPhoneTracker::saveCacheIndex()
{
    FSCom f;
    if (f.open(CACHE_FILE_INDEX, "w")) {
        uint16_t data[3] = {cache_count, cache_head, cache_tail};
        f.write((uint8_t *)data, sizeof(data));
        f.flush();
        f.close();
    }
}

void CachedPhoneTracker::clearCache()
{
    if (dumpActive) {
        dumpFile.close();
        dumpActive = false;
    }

    cache_count = 0;
    cache_head = 0;
    cache_tail = 0;
    saveCacheIndex();

    FSCom f;
    if (f.open(CACHE_FILE_POINTS, "w")) {
        f.close();
    }
}

void CachedPhoneTracker::appendToCache(const meshtastic_Position &pos)
{
    TrackPoint pt;
    pt.lat_i = pos.latitude_i;
    pt.lon_i = pos.longitude_i;
    pt.alt = (int16_t)pos.altitude;
    pt.timestamp = pos.time ? pos.time : getTime();
    pt.hdop = (uint16_t)pos.HDOP;
    pt.sats = (uint8_t)pos.sats_in_view;
    pt.flags = 0;

    FSCom f;
    const char *mode = (cache_count == 0 && cache_head == 0) ? "w+" : "r+";
    if (!f.open(CACHE_FILE_POINTS, mode)) {
        if (!f.open(CACHE_FILE_POINTS, "w+"))
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
    if (index >= MAX_CACHED_POSITIONS)
        return false;

    FSCom f;
    if (!f.open(CACHE_FILE_POINTS, "r")) {
        return false;
    }

    uint32_t offset = (uint32_t)index * sizeof(TrackPoint);
    f.seek(offset);
    size_t r = f.read((uint8_t *)&pt, sizeof(TrackPoint));
    f.close();
    return (r == sizeof(TrackPoint));
}

void CachedPhoneTracker::startDump()
{
    if (cache_count == 0) {
        replyText("CACHE EMPTY");
        return;
    }

    if (dumpActive) {
        dumpFile.close();
    }

    if (!dumpFile.open(CACHE_FILE_POINTS, "r")) {
        replyText("ERROR: CANNOT OPEN CACHE FILE");
        return;
    }

    dumpActive = true;
    dumpCurIdx = cache_tail;
    dumpTotalToSend = cache_count;
    dumpSentCount = 0;

    setIntervalFromNow(10);
}

void CachedPhoneTracker::sendCacheViaText()
{
    startDump();
}

void CachedPhoneTracker::toggleTrackerMode()
{
    setTrackerMode(!trackerModeActive);
}

void CachedPhoneTracker::setTrackerMode(bool enable)
{
    trackerModeActive = enable;
    if (trackerModeActive) {
        playTune(tune_ringtone);
        ledBlink(LED_GREEN, 3, 100);
    } else {
        playTune(tune_low_battery);
        ledBlink(LED_RED, 2, 200);
    }
}

int32_t CachedPhoneTracker::runOnce()
{
    // Handle non-blocking streaming dump if active
    if (dumpActive) {
        if (dumpSentCount >= dumpTotalToSend) {
            dumpFile.close();
            dumpActive = false;
            char doneBuf[64];
            snprintf(doneBuf, sizeof(doneBuf), "DUMP COMPLETE: %u points", dumpSentCount);
            replyText(doneBuf);
            return TRACKER_POLL_INTERVAL_MS;
        }

        char msgBuf[230];
        msgBuf[0] = '\0';
        size_t offsetStr = 0;
        uint8_t batchCount = 0;

        while (dumpSentCount < dumpTotalToSend && batchCount < 4) {
            TrackPoint pt;
            uint32_t offsetBytes = (uint32_t)dumpCurIdx * sizeof(TrackPoint);
            dumpFile.seek(offsetBytes);
            size_t r = dumpFile.read((uint8_t *)&pt, sizeof(TrackPoint));

            if (r == sizeof(TrackPoint)) {
                int written = snprintf(msgBuf + offsetStr, sizeof(msgBuf) - offsetStr,
                         "%s$TRK,%.6f,%.6f,%d,%u,%u",
                         batchCount > 0 ? "\n" : "",
                         pt.lat_i * 1e-7, pt.lon_i * 1e-7,
                         pt.alt, pt.timestamp, pt.hdop);
                if (written > 0 && (size_t)written < sizeof(msgBuf) - offsetStr) {
                    offsetStr += written;
                    batchCount++;
                }
            }
            dumpCurIdx = (dumpCurIdx + 1) % MAX_CACHED_POSITIONS;
            dumpSentCount++;
        }

        if (batchCount > 0) {
            replyText(msgBuf);
        }

        // Yield for 40ms to let SerialConsole drain to USB/phone before sending the next packet!
        return 40;
    }

    if (!trackerModeActive) {
        return TRACKER_POLL_INTERVAL_MS;
    }

    uint32_t now = getTime();
    if (now == 0) {
        return TRACKER_POLL_INTERVAL_MS;
    }

    bool hasPosition = (currentLat != 0 || currentLon != 0);
    if (!hasPosition && gps && gps->hasValidLocation()) {
        currentLat = gps->getLatitude();
        currentLon = gps->getLongitude();
        currentAlt = (int16_t)gps->getAltitude();
        currentHdop = (uint16_t)(gps->getHDOP() * 100);
        currentSats = (uint8_t)gps->getNumSatellites();
        gpsStatus = meshtastic_Position_LocData_LocFixed;
        hasPosition = true;
    }

    if (!hasPosition) {
        return TRACKER_POLL_INTERVAL_MS;
    }

    double dLat = (currentLat - lastLat) * 1e-7 * 111319.9;
    double dLon = (currentLon - lastLon) * 1e-7 * 111319.9 * cos(currentLat * 1e-7 * 0.0174532925);
    double dist = sqrt(dLat * dLat + dLon * dLon);

    bool shouldLog = false;
    if (lastLoggedTime == 0) {
        shouldLog = true;
    } else if (dist >= 10.0) {
        shouldLog = true;
    } else if ((now - lastLoggedTime) >= 60) {
        shouldLog = true;
    }

    if (shouldLog) {
        meshtastic_Position p = meshtastic_Position_init_default;
        p.latitude_i = currentLat;
        p.longitude_i = currentLon;
        p.altitude = currentAlt;
        p.time = now;
        p.HDOP = currentHdop;
        p.sats_in_view = currentSats;
        p.location_source = gpsStatus;

        appendToCache(p);

        lastLat = currentLat;
        lastLon = currentLon;
        lastLoggedTime = now;

        ledBlink(LED_GREEN, 1, 50);
    }

    return TRACKER_POLL_INTERVAL_MS;
}
