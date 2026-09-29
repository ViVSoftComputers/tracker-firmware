#include "LocalGpsTrackLogger.h"
#include "configuration.h"
#include "main.h"
#include "meshUtils.h"
#include "NodeDB.h"
#include "sleep.h"

// -----------------------------------------------------------------------------
// LocalGpsTrackLogger - GPS position logging for Heltec T096
// -----------------------------------------------------------------------------
// Standalone GPS track logger, persistent to LittleFS, with auto-sync
// to Meshtastic Position Log via BLE/Serial.
//
// Target: Heltec T096 (ESP32-S3)
// -----------------------------------------------------------------------------

static const uint32_t LOG_INTERVAL_MS = 60000;
static const uint32_t MAX_POINTS = 1024;
static const uint32_t MIN_EPOCH = 1700000000;

struct __attribute__((packed)) LogPoint {
    uint32_t timestamp;
    int32_t lat;
    int32_t lon;
    uint16_t alt;
    uint8_t hdop;
    uint8_t sats;
};

LocalGpsTrackLogger *localGpsTrackLogger;

LocalGpsTrackLogger::LocalGpsTrackLogger()
    : OSThread("LocalGpsTrackLogger"),
      _active(false),
      _lastLog(0),
      _count(0),
      _head(0)
{
}

bool LocalGpsTrackLogger::setup()
{
    localGpsTrackLogger = this;
    _restoreState();
    return true;
}

int32_t LocalGpsTrackLogger::runOnce()
{
    if (!_active || !gps || !gps->isConnected) {
        return 5000;
    }

    uint32_t now = millis();
    if (now - _lastLog < LOG_INTERVAL_MS) {
        return 1000;
    }

    auto pos = gps->getPosition();
    if (pos.timestamp < MIN_EPOCH) {
        return 1000;
    }

    _writePoint(pos);
    _lastLog = now;

    if (isPhoneAvailable()) {
        _syncPoints();
    }

    return 1000;
}

void LocalGpsTrackLogger::_writePoint(const GPSPosition &pos)
{
    LogPoint lp;
    lp.timestamp = pos.timestamp;
    lp.lat = pos.latitude_i;
    lp.lon = pos.longitude_i;
    lp.alt = pos.altitude;
    lp.hdop = pos.HDOP;
    lp.sats = pos.satellites;

    auto f = FSCom.open("/gps_track.dat", "ab");
    if (f) {
        f.write((uint8_t *)&lp, sizeof(lp));
        f.close();
        _count++;

        if (_count > MAX_POINTS) {
            _trimLog();
        }
    }
}

void LocalGpsTrackLogger::_trimLog()
{
    auto f = FSCom.open("/gps_track.dat", "rb");
    if (!f) return;

    size_t half = _count / 2;
    f.seek(half * sizeof(LogPoint));

    auto buf = (uint8_t *)malloc((_count - half) * sizeof(LogPoint));
    if (!buf) {
        f.close();
        return;
    }

    f.read(buf, (_count - half) * sizeof(LogPoint));
    f.close();

    FSCom.remove("/gps_track.dat");
    auto wf = FSCom.open("/gps_track.dat", "wb");
    if (wf) {
        wf.write(buf, (_count - half) * sizeof(LogPoint));
        wf.close();
    }

    free(buf);
    _count -= half;
}

void LocalGpsTrackLogger::_restoreState()
{
    if (FSCom.exists("/gps_track.dat")) {
        auto f = FSCom.open("/gps_track.dat", "rb");
        if (f) {
            _count = f.size() / sizeof(LogPoint);
            f.close();
        }
    }
}

bool LocalGpsTrackLogger::isPhoneAvailable()
{
    return service.isPhoneConnected;
}

void LocalGpsTrackLogger::_syncPoints()
{
    auto f = FSCom.open("/gps_track.dat", "rb");
    if (!f) return;

    LogPoint lp;
    while (f.read((uint8_t *)&lp, sizeof(lp)) == sizeof(lp)) {
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = lp.lat;
        pos.longitude_i = lp.lon;
        pos.altitude = lp.alt;
        pos.timestamp = lp.timestamp;
        pos.PDOP = lp.hdop;
        pos.sats_in_view = lp.sats;

        service.sendPositionToPhone(pos);
        delay(30);
    }
    f.close();
}

void LocalGpsTrackLogger::toggle()
{
    _active = !_active;
}

void LocalGpsTrackLogger::clearCache()
{
    FSCom.remove("/gps_track.dat");
    _count = 0;
}

uint32_t LocalGpsTrackLogger::count() const
{
    return _count;
}

bool LocalGpsTrackLogger::active() const
{
    return _active;
}

void setupLocalGpsTrackLogger()
{
    if (!localGpsTrackLogger) {
        localGpsTrackLogger = new LocalGpsTrackLogger();
        localGpsTrackLogger->setup();
    }
}

#endif // !MESHTASTIC_EXCLUDE_GPS
