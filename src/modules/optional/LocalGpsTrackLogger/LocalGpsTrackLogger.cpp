#include "LocalGpsTrackLogger.h"
#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_GPS && defined(ARCH_ESP32)
#include "FSCommon.h"
#include "SPILock.h"
#include "GPS.h"
#include "GeoCoord.h"
#include "RTC.h"
#include <Arduino.h>
#include <time.h>

#if HAS_SCREEN
#include "graphics/SharedUIDisplay.h"
#include "graphics/ScreenFonts.h"
#endif

// ──────────────────────────────────────────────────────────
// Constructor — registers with MeshModule AND OSThread
// ──────────────────────────────────────────────────────────

LocalGpsTrackLogger::LocalGpsTrackLogger()
    : MeshModule("LocalGpsTrackLogger", meshtastic_PortNum_UNKNOWN_APP),
      concurrency::OSThread("LocalGpsTrackLogger")
{
    LOG_INFO("LocalGpsTrackLogger initialized. Log target: %s", LOG_PATH);
}

// ──────────────────────────────────────────────────────────
// OSThread — GPS polling + CSV logging
// ──────────────────────────────────────────────────────────

int32_t LocalGpsTrackLogger::runOnce()
{
    gps_has_lock = (gps != nullptr && gps->hasLock());

    if (!gps_has_lock) {
        return POLL_INTERVAL_MS;
    }

    int32_t lat_i = gps->p.latitude_i;
    int32_t lon_i = gps->p.longitude_i;

    // Skip invalid coordinates (0, 0)
    if (lat_i == 0 && lon_i == 0) {
        return POLL_INTERVAL_MS;
    }

    uint32_t now_ms = millis();
    bool is_first_point = (point_count == 0);

    if (!is_first_point) {
        float dist = GeoCoord::latLongToMeter(
            (double)last_lat_i * 1e-7, (double)last_lon_i * 1e-7,
            (double)lat_i * 1e-7, (double)lon_i * 1e-7);
        uint32_t elapsed_ms = now_ms - last_log_ms;

        // If we moved less than MIN_MOVE_METERS and stationary interval has not elapsed, skip
        if (dist < MIN_MOVE_METERS && elapsed_ms < MAX_STATIONARY_INTERVAL_MS) {
            return POLL_INTERVAL_MS;
        }
    }

    // ── Update cached display values ──
    last_lat_deg = (double)lat_i * 1e-7;
    last_lon_deg = (double)lon_i * 1e-7;
    last_alt = gps->p.has_altitude ? gps->p.altitude : 0;
    last_hdop = gps->p.HDOP;

    // ── Write point to flash filesystem ──
    concurrency::LockGuard g(spiLock);
    if (!FSBegin()) {
        LOG_WARN("TrackLogger: Filesystem not mounted");
        return POLL_INTERVAL_MS;
    }

    // Safety check: ensure at least 50KB free space remains
    if (FSCom.totalBytes() - FSCom.usedBytes() < 51200) {
        LOG_WARN("TrackLogger: Filesystem low on space (<50KB free)");
        return 30000;
    }

    FSCom.mkdir("/static");
    bool need_header = !FSCom.exists(LOG_PATH);

    File f = FSCom.open(LOG_PATH, need_header ? FILE_WRITE : FILE_APPEND);
    if (!f) {
        LOG_ERROR("TrackLogger: Unable to open %s", LOG_PATH);
        return POLL_INTERVAL_MS;
    }

    if (need_header) {
        f.println("timestamp,date_time,latitude,longitude,altitude_m,hdop");
    }

    uint32_t rtc_sec = getTime(false);
    char time_str[32] = "N/A";
    if (rtc_sec > 0) {
        time_t rawtime = (time_t)rtc_sec;
        struct tm *dt = gmtime(&rawtime);
        if (dt) {
            snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02dZ",
                     dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday,
                     dt->tm_hour, dt->tm_min, dt->tm_sec);
        }
    }

    f.printf("%u,%s,%.7f,%.7f,%d,%u\n", rtc_sec, time_str, last_lat_deg, last_lon_deg, last_alt, last_hdop);
    f.flush();
    f.close();

    last_lat_i = lat_i;
    last_lon_i = lon_i;
    last_log_ms = now_ms;
    point_count++;

    LOG_INFO("TrackLogger: Stored fix #%u (%.6f, %.6f, alt=%dm, hdop=%u)",
             point_count, last_lat_deg, last_lon_deg, last_alt, last_hdop);

    return POLL_INTERVAL_MS;
}

// ───────────────────────────────────────────────────────────
// Display Frame — OLED card showing live track stats
// ──────────────────────────────────────────────────────────

#if HAS_SCREEN
void LocalGpsTrackLogger::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;  // unused — we are a static data card

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);

    // ── Header ──
    const char *header = gps_has_lock ? "GPS Track Log" : "GPS Track (no lock)";
    graphics::drawCommonHeader(display, x, y, header);

    const int *textPos = graphics::getTextPositions(display);
    int line = 1;

    char buf[40];

    // Row 1: Point count
    if (point_count == 0) {
        display->drawString(x + 2, textPos[line++], "Waiting for GPS lock...");
    } else {
        snprintf(buf, sizeof(buf), "Points: %u", point_count);
        display->drawString(x + 2, textPos[line++], buf);

        // Row 2: Lat/Lon
        snprintf(buf, sizeof(buf), "Lat: %.6f", last_lat_deg);
        display->drawString(x + 2, textPos[line++], buf);

        snprintf(buf, sizeof(buf), "Lon: %.6f", last_lon_deg);
        display->drawString(x + 2, textPos[line++], buf);

        // Row 3: Altitude + HDOP
        snprintf(buf, sizeof(buf), "Alt: %dm  HDOP: %u", last_alt, last_hdop);
        display->drawString(x + 2, textPos[line++], buf);

        // Row 4: GPS status
        snprintf(buf, sizeof(buf), "Sats: %u", gps->p.sats_in_view);
        display->drawString(x + 2, textPos[line++], buf);
    }

    graphics::drawCommonFooter(display, x, y);
}
#endif // HAS_SCREEN

// ──────────────────────────────────────────────────────────
// Setup — starts the OSThread poller
// ──────────────────────────────────────────────────────────

void LocalGpsTrackLogger::setup()
{
    LOG_DEBUG("LocalGpsTrackLogger: starting poll thread");
    setIntervalFromNow(0);
}

// ──────────────────────────────────────────────────────────
// Module singleton + setup hook
// ──────────────────────────────────────────────────────────

LocalGpsTrackLogger *localGpsTrackLogger = nullptr;

void setupLocalGpsTrackLogger()
{
    LOG_INFO("Registering LocalGpsTrackLogger optional module");
    localGpsTrackLogger = new LocalGpsTrackLogger();
    localGpsTrackLogger->setup();
}

#else // MESHTASTIC_EXCLUDE_GPS

void setupLocalGpsTrackLogger()
{
    // GPS excluded from build; no-op
}

#endif // !MESHTASTIC_EXCLUDE_GPS