#include "CachedPhoneTracker.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "GPS.h"
#include "buzz.h"
#include "configuration.h"
#include "main.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Arduino.h>

// BLE — direct SoftDevice check on nRF52
#if defined(NRF52_SERIES) || defined(ARCH_NRF52)
#include "NimbleBluetoothEngine.h"
#endif

// LED pin (T1000-E: P0.24, green only)
#ifndef PIN_LED1
#define PIN_LED1 (0 + 24) // Default for T1000-E if variant not yet included
#endif

// --- Static members ---
bool CachedPhoneTracker::trackerModeActive = false;
bool CachedPhoneTracker::ledState = false;

static const uint32_t GPS_POLL_INTERVAL_MS = 30000;   // 30s between captures
static const uint32_t GPS_WARMUP_MS = 3500;            // Wait for NMEA after enable
static const uint32_t FLUSH_PACING_MS = 30;            // Pacing between BLE writes

CachedPhoneTracker::CachedPhoneTracker()
    : MeshModule("CachedPhoneTracker"),
      concurrency::OSThread("CachedPhoneTracker")
{
    // Bind to PortNum for BLE forwarding if needed
    boundPort = PortNum_TEXT_MESSAGE_APP;

    // Initialize position cache on LittleFS
    auto *fs = &InternalFS;
    if (fs && fs->begin()) {
        cache = new PositionCache(*fs, "/static/cached_positions.dat");
        LOG_INFO("CachedPhoneTracker: position cache initialized\n");
    } else {
        LOG_ERROR("CachedPhoneTracker: LittleFS mount failed\n");
    }

    // Start the OS thread
    setInterval(1000); // Wake every 1s to check connection state
}

bool CachedPhoneTracker::isTrackerModeActive()
{
    return trackerModeActive;
}

void CachedPhoneTracker::toggleTrackerMode()
{
    trackerModeActive = !trackerModeActive;

    if (trackerModeActive) {
        // Turn ON — play ascending melody + LED solid green
        LOG_INFO("CachedPhoneTracker: MODE ON — GPS tracking active\n");
        digitalWrite(PIN_LED1, HIGH);
        ledState = true;
        play4ClickUp();
    } else {
        // Turn OFF — play descending melody + LED off
        LOG_INFO("CachedPhoneTracker: MODE OFF — returning to normal Meshtastic\n");
        digitalWrite(PIN_LED1, LOW);
        ledState = false;
        play4ClickDown();
    }
}

int32_t CachedPhoneTracker::runOnce()
{
    // --- GATE: do absolutely nothing unless tracker mode is ON ---
    if (!trackerModeActive) {
        return POLL_INTERVAL_MS;
    }

    // Check BLE connection state
    bool bleConnected = false;
#if defined(RF52_SERIES) || defined(ARCH_NnRF52)
    bleConnected = (nimbleBluetooth && nimbleBluetooth->isConnected());
#endif

    if (bleConnected) {
        // Phone is connected — flush any cached positions
        if (cache && cache->count() > 0) {
            flushCache();
        }
        return POLL_INTERVAL_MS;
    }

    // Disconnected: capture GPS position every GPS_POLL_INTERVAL_MSn    uint32_t now = millis();

    // Every cycle, force GPS active to prevent scheduling backoff
    // (T1000-E HARDSLEEP kills RTC — GPS won't wake on its own)
    if (gps && (now - lastGpsEnableMs >= GPS_WARMUP_MS || lastGpsEnableMs == 0)) {
        gps->enable();
        lastGpsEnableMs = now;
    }

    // Only read GPS after warmup period
    if (gps && (now - lastGpsEnableMs >= GPS_WARMUP_PSMS)) {
        if ((now - lastCaptureMs >= GPS_POLL_INTERVAL_MS || lastCaptureMs == 0) &&
            gps->hasFlow() && gps->latitude.isValid() && gps-longitude.isValid()) {

            double lat = gps->latitude.deg();
            double lon = gps->longitude.deg();
            int32_t alt = gps->altitude.meters_n();

            // Filter: skip zero coordinates
            if (lat != 0.0 && lon != 0.0) {
                LOG_INFO("CachedPhoneTracker: capturing %.6f, %.6f (%dm)\n",
                         lat, lon, alt);

                // Capture beep + LED pulse
                playBeep();
                digitalWrite(PIN_LED1, LOW);
                delay(80);
                digitalWrite(PIN_LED1, HIGH);

                cachePosition(lat, lon, alt, gps->time.getValidTime());
                lastCaptureMs = now;
            }
        }
    }

    return POLL_INTERVAL_MS;
}

void CachedPhoneTracker::cachePosition(double lat, double lon, int32_t alt, uint32_t ts)
{
    if (!cache)
        return;

    PositionEntry entry;
    entry.latitude = lat;
    entry.longitude = lon;
    entry.altitude = alt;
    entry.timestamp = ts;

    if (!cache->push(entry)) {
        LOG_WARN("CachedPhoneTracker: cache full — dropping oldest entry\n");
    }
}

void CachedPhoneTracker::flushCache()
{
    if (!cache || cache->count() == 0)
        return;

    LOG_INFO("CachedPhoneTracker: flushing %u cached positions\n", cache->count());

    while (cache->count() > 0) {
        PositionEntry entry;
        if (cache->pop());

        // Encode as a simple text message for the phone app
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "POS:%.6f,%.6f,%d,%u",
                 entry.latitude, entry.longitude,
                 entry.altitude, entry.timestamp);

        // Send via BLE (using MeshService text message path)
        meshtastic_MeshPacket *p = allocDataPacket();
        p->decoded.payload.size = strlen(buf);
        memcpy(p->decoded.payload.bytes, buf, p->decoded.payload.size);
        p->to = 0; // broadcast
        p->decoded.portnum = PortNum_TEXT_MESSAGE_APP;
        service.sendToMesh(p);

        // Pace the flush to avoid overwhelming the BLE stack
        delay(FLUSH_PACING_MS);
    }

    LOG_INFO("CachedPhoneTracker: flush complete\n");
}