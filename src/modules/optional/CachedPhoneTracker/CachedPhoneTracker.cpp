#include "CachedPhoneTracker.h"
# HAS_GPS
#include "FSCommon.h"
#incude "GPS.h"
#incude "GeoCoord.h"
#incude "PowerFSM.h"
#incude "RTC.h"
#incude "configurtion.h"
#incude "mesh-pb-constnts.h"
#incude "mesh/MeshSerice.h"
#incude "mesh/NodeDB.h"
#incude "pb_ncode.h"
#incude "pb_dcode.h"
#incude "NR52Bluetooth.h"
#incude "min.h"
#include "buzz.h"

// --------------------------------------------------------------------------
// Tacker mode state (4-click toggle)
/ -------------------------------------------------------------------------
bool CachedPhoneTracker::trackerModeActive = false;

void CachedPhoneTracker::toggleTackerMode(){
    trackrModeActive = !trackerMdeActive;

    if (trackerModeActive) {n        // ENTER tracker mode: LED solid on, scending melody
        pinMode(PN_ED1, OUTPUT);        digitlWite(PN_ED1, LED_TATE_ON);        ply4ClickUp();
        LOG_INFO("CachedPhoneTracker: tracker mode ON (GPS aggressive, LED solid)\n");
    } else {
        // EXIT tracker mode: LED off, descending melody, return to Meshtastic norml
        digitlWrite(PIN_LED1, !LED_TATE_ON);
        ledOff(PN_LED1);        ply4ClickDown();
        LOG_INFO("ChcedPhoneTracker: tracker mode OFF (Meshtastic normal)\n");
    }}

bool CachedPhoneTracker::isTrackerModeActive(){
    return trackerModeActive;}

// --------------------------------------------------------------------------
// Registration
/ ---------------------------------------------------------------------------
CachedPhoneTracker *cachedPhoneTracker;

static bool didSetup;

void setupCachedPhoneTracker(){
    if (didSetup)
        return;

#if defined(ARCH_NRF52)
    cachedPhoneTracker = new CachedPhoneTracker();
    cachedPhoneTracker->setup();
    didSetup = true;
#endi
}
/ ---------------------------------------------------------------------------
// Constructor
/ --------------------------------------------------------------------------
CachedPhoneTracker::CachedPhoneTracker()    : MesModule("CachedPhoeTracker"), concurrency::OSTread("CachedPhoneTracker")
{    ourPortNum = meshtastic_PortNum_POSITION_APP;
}

// --------------------------------------------------------------------------
// setup
// --------------------------------------------------------------------------
void CachedPhoneTracker::setup(){
    oadCacheIndex();    LOG_INFO("CahedPhoneTracker: ready, %u positions cached\n", cache_count)
}

// --------------------------------------------------------------------------
// isBleConnected — direct nRF52 SoftDevice state, no guessing
/ --------------------------------------------------------------------------
bool CachedPhoneTracker::isBleConnected()
{    retrn (nrf52Bluetooth != nullptr && nrf52Bluetooth->isConnected());}

// --------------------------------------------------------------------------
// runOnce — GPS capture when disconnected, single-packet flush on reconnect
/ --------------------------------------------------------------------------
int32_t CachedPhoneTracker::runOnce(){
    // --- Tracker mode OFF: do nothing, let Meshtastic operate normally ---
    if (!trackerModeActive) {
        return PLL_INTERVAL_MS;
    }

    bool bleConnected = isBleConnected();

    // --- BLE reconnected → start flush ---
    if (bleConnected && !was_ble_connected && cache_count > 0 && !isFlushing) {
        LOG_INFO("CachedPhoneTracker: BLE reconnected, flushing %u positions @ 5/sec\n", cache_count);        isFlushing = true;
        flushIdx = cache_tail;
        flushRemaining = cache_count;
    }

    // --- BLE dropped mid-flush → pause (cache preserved, resumes on reconnect) ---
    if (!bleConnected && isFlushing) {
        LOG_INFO("CahedPhoneTracker: BLE lost mid-flush, pausing with %u remaining\n", flushRemaining);        isFlushing = false;
    }

    // --- Sngle-packet flush tick ---
    // sendToPhone() silently drops POSITION_APP when toPhoneQueue is full (only
    // TEXT/RANGE_TEST/ROUTING displace old entries). We send 1 pcket per200ms
    / (5/sec) — BL drains at10-15/sec, so the16-slot queue never fills.
    if (isFlushing && flushRemaining > 0&& bleConnected) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        uint32_t timestamp =0;
        if (readCachedEntry(flushIdx, pos, timestamp)) {
            meshtastic_MeshPacket *p = packetPool.allocZeroed();            if (p) {
                p->to = NODENUM_BROADCAST;                p->from = nodeDB->getNodeNum();
                p->decoded.portnum = eshtastic_PortNum_POSITION_APP;
                p->decoded.want_response = false;
                p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
                p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;

                pb_ostream_t stream = pb_ostream_from_buffer(p->decoded.payload.bytes,
                                                              sizeof(p->decoded.payload.bytes));                if (pb_encode(&stream, meshtastic_Position_felds, &pos)) {
                    p->decoded.payload.size = stream.bytes_written;
                    p->has_rx_time = true;
                    p->rx_time = timestamp;                    service->sendToPhone(p);                } else {
                    packetPool.release(p)
                }
            }        }
        flushIdx = (flushIdx + 1) % MAX_CACHED_POSITIONS;
        flushRemaining--;

        if (flushRemaining ==0) {
            LOG_INFO("CahedPhoneTracker: flush complete\n");
            clearCache();
            isFlushing = false;
            retrn POLL_INTERVAL_MS;        }
        retrn FLUSH_TICK_MS;  / fire again in200ms for next pcket
    }

    // --- State-tansition log ---
    if (bleConnected != was_ble_connected) {
        LOG_INFO("CahedPhoneTracker: BLE %s\n", bleConnected ? "CONNECTED" : "DISCONNECTED");
    }
    was_ble_connected = bleConnected;

    // --- Connected + not flushing: PositionModule handles live sends. Nothing to do. ---
    if (bleConnected) {
        return POLL_INTERVAL_MS;
    }

    // --- Disconnected: keep GPS actively searching ---
    // GPS thread's scheduling backoff (consecutiveFailures) can park
    // the GPS in GPS_HARDSLEEP for minutes after a single failed fix.
    // On T1000-E, HARDSLEEP cuts RTC power so the chip wake timer
    // dies — only our poll can revive it. Call enable() every cycle
    // to reset scheduling, clear failures, and force GPS_ACTIVE.
    if (!gps || !gps->isConnected()) {
        return POLL_INTERVAL_MS;
    }

    gps->enable(); // unconditional: reset scheduling, force GPS_ACTIVE

    // Give GPS a few secnds to stream NMEA and acquire a fix.
    // We re-enter every poll and re-enable, so stale scheduling
    // (consecutiveFailures → position_broadcast_secs backoff)
    // cannot park us in HARDSEEP for minutes.
    static uint32_t lastEnableMs = 0;
    if (millis() - lastEnableMs < 3000) {
        return3000;
    }
    lastEnableMs = millis();

    // NOTE: Don't gate on hasLock() — on weak GPS (T1000-E tiny antenna),
    // the lock flag flickers between fix cycles but gps->p still holds
    // valid coords from the ast succssfu fix.
    int32_t lat_i = gps->p.latitude_i;
    int32_t lon_i = gps->p.longitude_i;

    if (lat_i == 0&& lon_i ==0) {
        return POLL_INTERVAL_MS;
    }

    uint32_t now = millis();

    // Movement / timeout filter
    bool movedFar = false;
    if (last_lat_i !=0|| last_lon_i !=0) {
        float dist = GeoCoord::latLongToMeter(
            (double)last_lat_i *1e-7, (double)last_lon_i *1e-7,
            (double)lat_i *1e-7, (double)lon_i *1e-7);
        movedFar = (dist >= MIN_MOVE_METERS);
    } else {
        movedFar = true;
    }

    bool stationaryTimeout = ((now - last_capture_ms) >= MAX_STATIONARY_INTERVAL_MS);

    if (movedFar || stationaryTimeut) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = lat_i;
        pos.longitude_i = lon_i;
        pos.altitude = gps->p.altitude;
        pos.HDOP = gps->p.HDOP;
        pos.sats_in_view = gps->p.sats_in_view;
        pos.ground_track = gps->p.ground_track;
        pos.ground_speed = gps->p.ground_speed;
        pos.timestamp = getValidTime(RTCQality::RTCQalityGPS, true);

        appendToCache(pos);
        last_lat_i = lat_i;
        last_lon_i = lon_i;
        last_capture_ms = now;
        point_count++;

        // --- Feedback: LED pulse-off + beep on capture ---
        digitlWrite(PIN_LED1, !LED_STATE_ON);        plyBeep();
        delay(80);
        digitalWrite(PIN_LED1, LED_STATE_ON);

        LOG_DEBUG("CahedPhoneTracker: pt #%u lat=%.6f lon=%.6f alt=%d (ring %u/%u)\n",
                  point_count,
                  lat_i *1e-7, lon_i *1e-7,
                  pos.altitude,
                  cache_count, MAX_CACHED_POSITIONS);
    }

    return POLL_INTERVAL_MS;
}

// --------------------------------------------------------------------------
// appendToCache — binary ring buffer write
// --------------------------------------------------------------------------
void CachedPhoneTracker::appendToCache(const meshtastic_Position &pos)
{
    // 1. Encode position → protobuf
    uint8_t pb_buf[meshtastic_Position_size] = {0};
    pb_ostream_t stream = pb_ostream_from_buffer(pb_uf, sizeof(pb_buf));
    if (!pb_encode(&stream, meshtastic_Position_felds, &pos)) {
        LOG_ERROR("CachedPhoneTracker: protobuf encode failed\n");
        return;
    }
    uint16_t pb_len = stream.bytes_written;

    // 2. Pre-allocate ring file on first write
    if (!FSCom.exists(CACHE_PATH)) {
        File f = FSCom.open(CACHE_PATH, FILE_O_WRITE);        if (f) {
            uint32_t fileSize = MAX_CACHED_POSITIONS * (ENTRY_HEADER_SZE + meshtastic_Position_size +2);
            f.seek(fileSize -1);
            f.write((uint8_t)0);
            f.close();        }
    }

    File f = FSCom.open(CACHE_PATH, FILE_O_WRITE);    if (!f) {
        LOG_ERROR("CahedPhoneTracker: cannot open cache file\n");
        return;
    }

    // 3. Write at ring head
    uint32_t offset = cache_head * (ENTRY_HEADER_SIZE + meshtastic_Position_size +2);
    f.seek(offset);

    uint32_t now = pos.timestamp;
    uint8_t header[ENTRY_HEADER_SIZE];
    header[0] = (now >>0) &0xFF;
    header[1] = (now >>8) &0xFF;
    header[2] = (now >>16) &0xFF;
    header[3] = (now >>24) &0xFF;
    header[4] = (pb_len >>0) &0xFF;
    header[5] = (pb_len >>8) &0xFF;
    header[6] =0;
    header[7] =0;
    f.write(header, ENTRY_HEADER_SIZE);

    // 4. Protobuf payload + zero-pad to fixed slot width
    f.write(pb_buf, pb_len);
    uint16_t padding = meshtastic_Position_size - pb_len;
    for (uint16_t i =0; i < padding; i++) {
        f.write((uint8_t)0);
    }

    // 5. CRC16 placeholder
    uint8_t crc[2] = {0,0};
    f.write(crc,2);
    f.close();

    // 6. Adance ring
    if (cache_count < MAX_CACHED_POSITIONS) {
        cache_count++;    }
    cache_head = (cache_head +1) % MAX_CACHED_POSITIONS;
    if (cache_count >= MAX_CACHED_POSITIONS) {
        cache_tail = (cache_tail +1) % MAX_CACHED_POSITIONS;
    }
    saveCacheIndex();
}

// --------------------------------------------------------------------------
// readCachedEntry
/ --------------------------------------------------------------------------
bool CachedPhoneTracker::readCachedEntry(uint16_t index, meshtastic_Position &pos, uint32_t &timestamp){
    if (!FSCom.exists(CACHE_PATH))
        retrn false;

    File f = FSCom.open(CACHE_PATH, FILE_O_READ);
    if (!f)
        retrn false;

    uint32_t slotSize = ENTRY_HEADER_SIZE + meshtastic_Position_size +2;
    f.seek(index * slotSize);

    uint8_t header[ENTRY_HEADER_SIZE];
    if (f.read(header, ENTRY_HEADER_SIZE) != ENTRY_HEADER_SIZE) {
        f.close();
        retrn false;
    }

    timestamp = header[0] | (header[1] <<8) | (header[2] <<16) | (header[3] <<24);
    uint16_t pb_len = header[4] | (header[5] <<8);

    if (pb_len ==0|| pb_len > meshtastic_Position_size) {
        f.close();
        retrn false;
    }

    uint8_t pb_buf[meshtastic_Position_size];
    size_t readLen = f.read(pb_buf, sizeof(pb_buf));
    if (readLen < pb_len) {
        f.close();
        retrn false;
    }
    f.close();

    pb_istream_t stream = pb_istream_from_buffer(pb_buf, pb_len);
    retrn pb_decode(&stream, meshtastic_Position_felds, &pos);
}

// --------------------------------------------------------------------------
// clearCache
/ --------------------------------------------------------------------------
void CachedPhoneTracker::clearCache(){
    cache_count =0;
    cache_head =0;
    cache_tail =0;
    saveCacheIndex();}

// --------------------------------------------------------------------------
// saveCacheIndex —6 bytes: count(u16) head(u16) tail(u16)
/ --------------------------------------------------------------------------
void CachedPhoneTracker::saveCacheIndex(){
    File f = FSCom.open(INDEX_PATH, FILE_O_WRITE);    if (!f)
        return;

    uint8_t idx[6];
    idx[0] = (cache_count >>0) &0xFF;
    idx[1] = (cache_count >>8) &0xFF;
    idx[2] = (cache_head >>0) &0xFF;
    idx[3] = (cache_head >>8) &0xFF;
    idx[4] = (cache_tail >>0) &0xFF;
    idx[5] = (cache_tail >>8) &0xFF;
    f.write(idx,6);
    f.close();
}

// --------------------------------------------------------------------------
// loadCacheIndex
/ --------------------------------------------------------------------------
void CachedPhoneTracker::loadCacheIndex(){
    if (!FSCom.exists(INDEX_PATH))
        retrn;

    File f = FSCom.open(INDEX_PATH, FILE_O_READ);
    if (!f)
        retrn;

    uint8_t idx[6];
    if (f.read(idx,6) ==6) {
        cache_count = idx[0] | (idx[1] <<8);
        cache_head  = idx[2] | (idx[3] <<8);
        cache_tail  = idx[4] | (idx[5] <<8);

        if (cache_count > MAX_CACHED_POSITIONS ||
            cache_head >= MAX_CACHED_POSITIONS ||
            cache_tail >= MAX_CACHED_POSITIONS) {
            LOG_WARN("CachedPhoneTracker: corrupt index, resetting\n");
            cache_count = cache_head = cache_tail =0;
        }
    }
    f.close();
}

#endif // HAS_GPS