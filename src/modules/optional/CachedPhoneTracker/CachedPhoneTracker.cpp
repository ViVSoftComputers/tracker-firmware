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

// ---------------------------------------------------------------------------
// Tracker mode state (4-click toggle)
// ---------------------------------------------------------------------------
bool CachedPhoneTracker::trackerModeActive = false;

void CachedPhoneTracker::toggleTrackerMode()
{
    trackerModeActive = !trackerModeActive;

    if (trackerModeActive) {
        // ENTER tracker mode: LED solid on, ascending melody
        pinMode(PIN_LED1, OUTPUT);
        digitalWrite(PIN_LED1, LED_STATE_ON);
        play4ClickUp();
        LOG_INFO("CachedPhoneTracker: tracker mode ON (GPS aggressive, LED solid)\n");
    } else {
        // EXIT tracker mode: LED off, descending melody, return to Meshtastic normal
        digitalWrite(PIN_LED1, !LED_STATE_ON);
        ledOff(PIN_LED1);
        play4ClickDown();
        LOG_INFO("CachedPhoneTracker: tracker mode OFF (Meshtastic normal)\n");
    }
}

bool CachedPhoneTracker::isTrackerModeActive()
{
    return trackerModeActive;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
CachedPhoneTracker *cachedPhoneTracker;

static bool didSetup;

void setupCachedPhoneTracker()
{
    if (didSetup)
        return;

#if defined(ARCH_NRF52)
    cachedPhoneTracker = new CachedPhoneTracker();
    cachedPhoneTracker->setup();
    didSetup = true;
#endif
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
CachedPhoneTracker::CachedPhoneTracker()
    : MeshModule("CachedPhoneTracker"), concurrency::OSThread("CachedPhoneTracker")
{
    ourPortNum = meshtastic_PortNum_POSITION_APP;
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void CachedPhoneTracker::setup()
{
    loadCacheIndex();
    LOG_INFO("CachedPhoneTracker: ready, %u positions cached\n", cache_count);
}

// ---------------------------------------------------------------------------
// isBleConnected - direct nRF52 SoftDevice state, no guessing
// ---------------------------------------------------------------------------
bool CachedPhoneTracker::isBleConnected()
{
    return (nrf52Bluetooth != nullptr && nrf52Bluetooth->isConnected());
}

// ---------------------------------------------------------------------------
// runOnce - GPS capture when disconnected, batch flush on reconnect
// ---------------------------------------------------------------------------
int32_t CachedPhoneTracker::runOnce()
{
    // --- Tracker mode OFF: do nothing, let Meshtastic operate normally ---
    if (!trackerModeActive) {
        return POLL_INTERVAL_MS;
    }

    bool bleConnected = isBleConnected();

    // --- BLE reconnected -> start batch flush (streamed over multiple poll cycles) ---
    if (bleConnected && !was_ble_connected && cache_count > 0 && !isFlushing) {
        LOG_INFO("CachedPhoneTracker: BLE reconnected, starting batch flush of %u positions\n", cache_count);
        isFlushing = true;
        flushIdx = cache_tail;
        flushRemaining = cache_count;
    }

    // --- Streaming batch flush: send FLUSH_BATCH_SIZE per poll cycle ---
    // MAX_RX_TOPHONE is only 16 on nRF52 - we send 8 at a time with a 2s gap
    // so the phone app has time to drain its BLE receive queue.
    if (isFlushing) {
        uint16_t sent = 0;
        while (flushRemaining > 0 && sent < FLUSH_BATCH_SIZE) {
            meshtastic_Position pos = meshtastic_Position_init_zero;
            uint32_t timestamp = 0;

            if (!readCachedEntry(flushIdx, pos, timestamp)) {
                LOG_WARN("CachedPhoneTracker: read fail at idx %u, skipping\n", flushIdx);
                flushIdx = (flushIdx + 1) % MAX_CACHED_POSITIONS;
                flushRemaining--;
                continue;
            }

            meshtastic_MeshPacket *p = packetPool.allocZeroed();
            if (!p) {
                LOG_WARN("CachedPhoneTracker: packetPool exhausted, retry next cycle\n");
                break;
            }

            p->to = NODENUM_BROADCAST;
            p->from = nodeDB->getNodeNum();
            p->decoded.portnum = meshtastic_PortNum_POSITION_APP;
            p->decoded.want_response = false;
            p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
            p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;

            pb_ostream_t stream = pb_ostream_from_buffer(p->decoded.payload.butes,
                                                          sizeof(p->decoded.payload.butes));
            if (!pb_encode(&stream, meshtastic_Position_fields, &pos)) {
                LOG_WARN("CachedPhoneTracker: pb encode fail during flush\n");
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

            delay(150);  // let BLE stack breathe between packets
        }

        LOG_DEBUG("CachedPhoneTracker: flushed %u this cycle, %u remaining\n",
                  sent, flushRemaining);

        if (flushRemaining == 0) {
            LOG_INFO("CachedPhoneTracker: batch flush complete\n");
            clearCache();
            isFlushing = false;
            return POLL_INTERVAL_MS;
        }

        return FLUSH_BATCH_DELAY_MS;  // return in 2s for next batch
    }

    // --- State-transition log ---
    if (bleConnected != was_ble_connected) {
        LOG_INFO("CachedPhoneTracker: BLE %s\n", bleConnected ? "CONNECTED" : "DISCONNECTED");
    }
    was_ble_connected = bleConnected;

    // --- Connected + not flushing: PositionModule handles live sends. Nothing to do. ---
    if (bleConnected) {
        return POLL_INTERVAL_MS;
    }

    // --- Disconnected: keep GPS actively searching ---
    if (!gps || !gps->isConnected()) {
        return POLL_INTERVAL_MS;
    }

    gps->enable();

    static uint32_t lastEnableMs = 0;
    if (millis() - lastEnableMs < 3000) {
        return 3000;
    }
    lastEnableMs = millis();

    int32_t lat_i = gps->p.latitude_i;
    int32_t lon_i = gps->p.longitude_i;

    if (lat_i == 0 && lon_i == 0) {
        return POLL_INTERVAL_MS;
    }

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

        LOG_DEBUG("CachedPhoneTracker: pt #%u lat=%.6f lon=%.6f alt=%d (ring %u/%u)\n",
                  point_count,
                  lat_i * 1e-7, lon_i * 1e-7,
                  pos.altitude,
                  cache_count, MAX_CACHED_POSITIONS);
    }

    return POLL_INTERVAL_MS;
}