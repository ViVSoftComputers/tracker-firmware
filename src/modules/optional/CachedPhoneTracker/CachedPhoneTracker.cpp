#include "CachedPhoneTracker.h"

#if HAS_GPS

#include "FSCommon.h"
#include "GPS.h"
#include "GeoCoord.h"
#include "PowerFSM.h"
#include "RTC.h"
#include "configuration.h"
#include "mesh-pb-constants.h"
#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "NRF52Bluetooth.h"
#include "main.h"
#include "buzz.h"

bool CachedPhoneTracker::trackerModeActive = false;

void CachedPhoneTracker::toggleTrackerMode()
{
    trackerModeActive = !trackerModeActive;
    if (trackerModeActive) {
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, LED_STATE_ON);
        play4ClickUp();
        LOG_INFO("CachedPhoneTracker: tracker mode ON\n");
    } else {
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        ledOff(PIN_LED1);
        play4ClickDown();
        LOG_INFO("CachedPhoneTracker: tracker mode OFF\n");
    }
}

bool CachedPhoneTracker::isTrackerModeActive()
{
    return trackerModeActive;
}

CachedPhoneTracker *cachedPhoneTracker;
static bool didSetup;

void setupCachedPhoneTracker()
{
    if (didSetup) return;
#if defined(ARCH_NRF52)
    cachedPhoneTracker = new CachedPhoneTracker();
    cachedPhoneTracker->setup();
    didSetup = true;
#endif
}

CachedPhoneTracker::CachedPhoneTracker()
    : MeshModule("CachedPhoneTracker"), concurrency::OSThread("CachedPhoneTracker")
{
    ourPortNum = meshtastic_PortNum_POSITION_APP;
}

void CachedPhoneTracker::setup()
{
    loadCacheIndex();
    LOG_INFO("CachedPhoneTracker: ready, %u cached\n", cache_count);
}

bool CachedPhoneTracker::isBleConnected()
{
    return (nrf52Bluetooth != nullptr && nrf52Bluetooth->isConnected());
}

int32_t CachedPhoneTracker::runOnce()
{
    if (!trackerModeActive) return POLL_INTERVAL_MS;

    bool bleConnected = isBleConnected();

    if (bleConnected && !was_ble_connected && cache_count > 0 && !isFlushing) {
        LOG_INFO("CachedPhoneTracker: starting batch flush of %u\n", cache_count);
        isFlushing = true;
        flushIdx = cache_tail;
        flushRemaining = cache_count;
    }

    if (isFlushing) {
        uint16_t sent = 0;
        while (flushRemaining > 0 && sent < FLUSH_BATCH_SIZE) {
            meshtastic_Position pos = meshtastic_Position_init_zero;
            uint32_t timestamp = 0;
            if (!readCachedEntry(flushIdx, pos, timestamp)) {
                flushIdx = (flushIdx + 1) % MAX_CACHED_POSITIONS;
                flushRemaining--;
                continue;
            }
            meshtastic_MeshPacket *p = packetPool.allocZeroed();
            if (!p) break;
            p->to = NODENUM_BROADCAST;
            p->from = nodeDB->getNodeNum();
            p->decoded.portnum = meshtastic_PortNum_POSITION_APP;
            p->decoded.want_response = false;
            p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
            p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
            pb_ostream_t stream = pb_ostream_from_buffer(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes));
            if (!pb_encode(&stream, meshtastic_Position_fields, &pos)) {
                packetPool.release(p);
                flushIdx = (flushIdx + 1) % MAX_CACHED_POSITIONS;
                flushRemaining--;
                continue;
            }
            p->decoded.payload.size = stream.bytes_written;
            p->has_rx_time = true;
            p->rx_time = timestamp;
            service->sendToPhone(p);
            sent++;
            flushIdx = (flushIdx + 1) % MAX_CACHED_POSITIONS;
            flushRemaining--;
            delay(150);
        }
        LOG_DEBUG("CachedPhoneTracker: flushed %u, %u remaining\n", sent, flushRemaining);
        if (flushRemaining == 0) {
            LOG_INFO("CachedPhoneTracker: batch flush complete\n");
            clearCache();
            isFlushing = false;
            return POLL_INTERVAL_MS;
        }
        return FLUSH_BATCH_DELAY_MS;
    }

    if (bleConnected != was_ble_connected)
        LOG_INFO("CachedPhoneTracker: BLE %s\n", bleConnected ? "CONNECTED" : "DISCONNECTED");
    was_ble_connected = bleConnected;

    if (bleConnected) return POLL_INTERVAL_MS;

    if (!gps || !gps->isConnected()) return POLL_INTERVAL_MS;

    gps->enable();
    static uint32_t lastEnableMs = 0;
    if (millis() - lastEnableMs < 3000) return 3000;
    lastEnableMs = millis();

    int32_t lat_i = gps->p.latitude_i;
    int32_t lon_i = gps->p.longitude_i;
    if (lat_i == 0 && lon_i == 0) return POLL_INTERVAL_MS;

    uint32_t now = millis();
    bool movedFar = false;
    if (last_lat_i != 0 || last_lon_i != 0) {
        float dist = GeoCoord::latLongToMeter(
            (double)last_lat_i * 1e-7, (double)last_lon_i * 1e-7,
            (double)lat_i * 1e-7, (double)lon_i * 1e-7);
        movedFar = (dist >= MIN_MOVE_METERS);
    } else {
        movedFar = true;
    }
    bool stationaryTimeout = ((now - last_capture_ms) >= MAX_STATIONARY_INTERVAL_MS);

    if (movedFar || stationaryTimeout) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = lat_i;
        pos.longitude_i = lon_i;
        pos.altitude = gps->p.altitude;
        pos.HDOP = gps->p.HDOP;
        pos.sats_in_view = gps->p.sats_in_view;
        pos.ground_track = gps->p.ground_track;
        pos.ground_speed = gps->p.ground_speed;
        pos.timestamp = getValidTime(RTCQuality::RTCQualityGPS, true);
        appendToCache(pos);
        last_lat_i = lat_i;
        last_lon_i = lon_i;
        last_capture_ms = now;
        point_count++;
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        playBeep();
        delay(80);
        digitalWrite(PIN_LED1, LED_STATE_ON);
    }
    return POLL_INTERVAL_MS;
}

void CachedPhoneTracker::appendToCache(const meshtastic_Position &pos)
{
    uint8_t pb_buf[meshtastic_Position_size] = {0};
    pb_ostream_t stream = pb_ostream_from_buffer(pb_buf, sizeof(pb_buf));
    if (!pb_encode(&stream, meshtastic_Position_fields, &pos)) return;
    uint16_t pb_len = stream.bytes_written;
    if (!FSCom.exists(CACHE_PATH)) {
        File f = FSCom.open(CACHE_PATH, FILE_O_WRITE);
        if (f) {
            uint32_t fileSize = MAX_CACHED_POSITIONS * (ENTRY_HEADER_SIZE + meshtastic_Position_size + 2);
            f.seek(fileSize - 1);
            f.write((uint8_t)0);
            f.close();
        }
    }
    File f = FSCom.open(CACHE_PATH, FILE_O_WRITE);
    if (!f) return;
    uint32_t offset = cache_head * (ENTRY_HEADER_SIZE + meshtastic_Position_size + 2);
    f.seek(offset);
    uint32_t now = pos.timestamp;
    uint8_t header[ENTRY_HEADER_SIZE];
    header[0] = (now >> 0) & 0xFF;
    header[1] = (now >> 8) & 0xFF;
    header[2] = (now >> 16) & 0xFF;
    header[3] = (now >> 24) & 0xFF;
    header[4] = (pb_len >> 0) & 0xFF;
    header[5] = (pb_len >> 8) & 0xFF;
    header[6] = 0;
    header[7] = 0;
    f.write(header, ENTRY_HEADER_SIZE);
    f.write(pb_buf, pb_len);
    uint16_t padding = meshtastic_Position_size - pb_len;
    for (uint16_t i = 0; i < padding; i++) f.write((uint8_t)0);
    uint8_t crc[2] = {0, 0};
    f.write(crc, 2);
    f.close();
    if (cache_count < MAX_CACHED_POSITIONS) cache_count++;
    cache_head = (cache_head + 1) % MAX_CACHED_POSITIONS;
    if (cache_count >= MAX_CACHED_POSITIONS) cache_tail = (cache_tail + 1) % MAX_CACHED_POSITIONS;
    saveCacheIndex();
}

bool CachedPhoneTracker::readCachedEntry(uint16_t index, meshtastic_Position &pos, uint32_t &timestamp)
{
    if (!FSCom.exists(CACHE_PATH)) return false;
    File f = FSCom.open(CACHE_PATH, FILE_O_READ);
    if (!f) return false;
    uint32_t slotSize = ENTRY_HEADER_SIZE + meshtastic_Position_size + 2;
    f.seek(index * slotSize);
    uint8_t header[ENTRY_HEADER_SIZE];
    if (f.read(header, ENTRY_HEADER_SIZE) != ENTRY_HEADER_SIZE) { f.close(); return false; }
    timestamp = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
    uint16_t pb_len = header[4] | (header[5] << 8);
    if (pb_len == 0 || pb_len > meshtastic_Position_size) { f.close(); return false; }
    uint8_t pb_buf[meshtastic_Position_size];
    size_t readLen = f.read(pb_buf, sizeof(pb_buf));
    if (readLen < pb_len) { f.close(); return false; }
    f.close();
    pb_istream_t stream = pb_istream_from_buffer(pb_buf, pb_len);
    return pb_decode(&stream, meshtastic_Position_fields, &pos);
}

void CachedPhoneTracker::clearCache()
{
    cache_count = 0;
    cache_head = 0;
    cache_tail = 0;
    saveCacheIndex();
}

void CachedPhoneTracker::saveCacheIndex()
{
    File f = FSCom.open(INDEX_PATH, FILE_O_WRITE);
    if (!f) return;
    uint8_t idx[6];
    idx[0] = (cache_count >> 0) & 0xFF;
    idx[1] = (cache_count >> 8) & 0xFF;
    idx[2] = (cache_head >> 0) & 0xFF;
    idx[3] = (cache_head >> 8) & 0xFF;
    idx[4] = (cache_tail >> 0) & 0xFF;
    idx[5] = (cache_tail >> 8) & 0xFF;
    f.write(idx, 6);
    f.close();
}

void CachedPhoneTracker::loadCacheIndex()
{
    if (!FSCom.exists(INDEX_PATH)) return;
    File f = FSCom.open(INDEX_PATH, FILE_O_READ);
    if (!f) return;
    uint8_t idx[6];
    if (f.read(idx, 6) == 6) {
        cache_count = idx[0] | (idx[1] << 8);
        cache_head  = idx[2] | (idx[3] << 8);
        cache_tail  = idx[4] | (idx[5] << 8);
        if (cache_count > MAX_CACHED_POSITIONS || cache_head >= MAX_CACHED_POSITIONS || cache_tail >= MAX_CACHED_POSITIONS) {
            LOG_WARN("CachedPhoneTracker: corrupt index, resetting\n");
            cache_count = cache_head = cache_tail = 0;
        }
    }
    f.close();
}

#endif