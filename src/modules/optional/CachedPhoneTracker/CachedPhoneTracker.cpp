#include "CachedPhoneTracker.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "Router.h"
#include "airtime.h"
#include "configuration.h"
#include "main.h"
#include "mesh-pb-constants.h"
#include "meshUtils.h"
#include "modules/PositionModule.h"
#include "sleep.h"

// -----------------------------------------------------------------------------
// CachedPhoneTracker - Standalone GPS tracking with Meshtastic position sync
// -----------------------------------------------------------------------------
// Logs GPS positions locally (LittleFS) and syncs to Meshtastic position log
// when BLE/Serial reconnects. Supports 1-5 click button gestures.
//
// Target: Seeed SenseCAP T1000-E (nRF52840)
// Version: v3.0.2
// -----------------------------------------------------------------------------

// --- Constants ---
static const uint32_t min_valid_epoch = 1700000000; // Nov 2023 guard
static const uint32_t LOG_INTERVAL_SECONDS = 60;
static const uint32_t MAX_LOG_POINTS = 2048;

// Flat binary log entry (18 bytes)
struct __attribute__((packed)) TrackPoint {
    uint32_t timestamp;
    int32_t latitudeI;
    int32_t longitudeI;
    uint16_t altitude;
    uint8_t hdop;
    uint8_t sats;
};

// --- File paths (LittleFS root) ---
static const char *POINTS_PATH = "/tracker_points.dat";
static const char *INDEX_PATH = "/tracker_index.dat";

// --- Singleton ---
CachedPhoneTracker *cachedPhoneTracker;

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
CachedPhoneTracker::CachedPhoneTracker()
    : concurrency::OSThread("CachedPhoneTracker"),
      _tracking_enabled(false),
      _last_log_time(0),
      _write_index(0),
      _point_count(0),
      _dump_in_progress(false),
      _sync_pending(false)
{
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
bool CachedPhoneTracker::setup()
{
    cachedPhoneTracker = this;
    
    // Initialize LittleFS access
    fsInit();
    
    // Restore tracking state from disk
    _loadIndex();
    
    LOG_INFO("CachedPhoneTracker: setup complete. tracking=%s, points=%u\n",
             _tracking_enabled ? "ON" : "OFF", _point_count);
    
    return true;
}

int32_t CachedPhoneTracker::runOnce()
{
    if (!_tracking_enabled) {
        return 5000; // Check every 5s when idle
    }
    
    uint32_t now = getTime();
    
    // Log position every LOG_INTERVAL_SECONDS
    if (now - _last_log_time >= LOG_INTERVAL_SECONDS) {
        _logCurrentPosition();
        _last_log_time = now;
    }
    
    // If connected to phone, push pending sync
    if (_sync_pending && isPhoneConnected()) {
        _pushPendingPositions();
    }
    
    return 1000; // Check every second when tracking
}

// -----------------------------------------------------------------------------
// GPS Position Logging
// -----------------------------------------------------------------------------
void CachedPhoneTracker::_logCurrentPosition()
{
    if (!gps || !gps->isConnected) {
        LOG_DEBUG("CachedPhoneTracker: GPS not available, skipping log\n");
        return;
    }
    
    auto p = gps->getPosition();
    
    if (p.timestamp < min_valid_epoch) {
        LOG_DEBUG("CachedPhoneTracker: stale GPS fix, skipping\n");
        return;
    }
    
    TrackPoint tp;
    tp.timestamp = p.timestamp;
    tp.latitudeI = p.latitude_i;
    tp.longitudeI = p.longitude_i;
    tp.altitude = p.altitude;
    tp.hdop = p.HDOP;
    tp.sats = p.satellites;
    
    spiLock->lock();
    _writePoint(&tp);
    spiLock->unlock();
    
    // Beep confirmation
    _buzzerBeep(50);
    
    LOG_INFO("CachedPhoneTracker: logged point %u, lat=%.6f lon=%.6f\n",
             _point_count,
             p.latitude_i * 1e-7,
             p.longitude_i * 1e-7);
}

void CachedPhoneTracker::_writePoint(TrackPoint *tp)
{
    File f = FSCom.open(POINTS_PATH, FILE_APPEND);
    if (!f) {
        LOG_ERROR("CachedPhoneTracker: failed to open points file\n");
        return;
    }
    
    f.write((uint8_t *)tp, sizeof(TrackPoint));
    f.close();
    
    _point_count++;
    
    if (_point_count > MAX_LOG_POINTS) {
        _rotateLog();
    }
    
    _saveIndex();
}

void CachedPhoneTracker::_rotateLog()
{
    LOG_INFO("CachedPhoneTracker: rotating log (max %u points)\n", MAX_LOG_POINTS);
    
    // Read all points after the first half
    uint32_t half = _point_count / 2;
    uint32_t keep = _point_count - half;
    
    File rf = FSCom.open(POINTS_PATH, FILE_READ);
    if (!rf) return;
    
    rf.seek(half * sizeof(TrackPoint));
    
    uint8_t *buf = (uint8_t *)malloc(keep * sizeof(TrackPoint));
    if (!buf) {
        rf.close();
        return;
    }
    
    rf.read(buf, keep * sizeof(TrackPoint));
    rf.close();
    
    // Rewrite file with kept points
    FSCom.remove(POINTS_PATH);
    File wf = FSCom.open(POINTS_PATH, FILE_WRITE);
    if (wf) {
        wf.write(buf, keep * sizeof(TrackPoint));
        wf.close();
    }
    
    free(buf);
    _point_count = keep;
    _saveIndex();
}

// -----------------------------------------------------------------------------
// TrackPoint Data Access
// -----------------------------------------------------------------------------
bool CachedPhoneTracker::_readPoint(uint32_t index, TrackPoint *tp)
{
    if (index >= _point_count) return false;
    
    File f = FSCom.open(POINTS_PATH, FILE_READ);
    if (!f) return false;
    
    f.seek(index * sizeof(TrackPoint));
    size_t read = f.read((uint8_t *)tp, sizeof(TrackPoint));
    f.close();
    
    return read == sizeof(TrackPoint);
}

// -----------------------------------------------------------------------------
// Index Persistence
// -----------------------------------------------------------------------------
void CachedPhoneTracker::_saveIndex()
{
    File f = FSCom.open(INDEX_PATH, FILE_WRITE);
    if (!f) return;
    
    f.write((uint8_t *)&_point_count, sizeof(_point_count));
    f.write((uint8_t *)&_write_index, sizeof(_write_index));
    f.write((uint8_t *)&_tracking_enabled, sizeof(_tracking_enabled));
    f.close();
}

void CachedPhoneTracker::_loadIndex()
{
    if (!FSCom.exists(INDEX_PATH)) return;
    
    File f = FSCom.open(INDEX_PATH, FILE_READ);
    if (!f) return;
    
    f.read((uint8_t *)&_point_count, sizeof(_point_count));
    f.read((uint8_t *)&_write_index, sizeof(_write_index));
    f.read((uint8_t *)&_tracking_enabled, sizeof(_tracking_enabled));
    f.close();
    
    // Sanity check
    if (_point_count > MAX_LOG_POINTS) {
        _point_count = 0;
        _write_index = 0;
    }
}

// -----------------------------------------------------------------------------
// Phone Sync (Meshtastic Position Log integration)
// -----------------------------------------------------------------------------
bool CachedPhoneTracker::isPhoneConnected()
{
    // Check if any BLE or serial client is connected
    return service.isPhoneConnected;
}

void CachedPhoneTracker::_pushPendingPositions()
{
    if (_dump_in_progress) return;
    
    LOG_INFO("CachedPhoneTracker: starting position sync (%u points)\n", _point_count);
    _dump_in_progress = true;
    
    for (uint32_t i = 0; i < _point_count && _dump_in_progress; i++) {
        TrackPoint tp;
        if (!_readPoint(i, &tp)) continue;
        
        meshtastic_Position pos = meshtastic_Position_init_zero;
        pos.latitude_i = tp.latitudeI;
        pos.longitude_i = tp.longitudeI;
        pos.altitude = tp.altitude;
        pos.timestamp = tp.timestamp;
        pos.PDOP = tp.hdop;
        pos.sats_in_view = tp.sats;
        
        // Route directly to local node (no RF)
        service.sendPositionToPhone(pos);
        
        // Throttle to avoid flooding
        delay(50);
    }
    
    _dump_in_progress = false;
    _sync_pending = false;
    
    LOG_INFO("CachedPhoneTracker: sync complete\n");
}

// -----------------------------------------------------------------------------
// Buzzer Control
// -----------------------------------------------------------------------------
// T1000-E: Buzzer PWM on P0.25 (Pin 25), Power enable on P1.05 (Pin 37)
static const uint8_t BUZZER_PWM_PIN = 25;
static const uint8_t BUZZER_EN_PIN = 37;

void CachedPhoneTracker::_buzzerBeep(uint32_t duration_ms)
{
    digitalWrite(BUZZER_EN_PIN, HIGH);
    delayMicroseconds(100); // Power gate stabilization
    
    tone(BUZZER_PWM_PIN, 2400, duration_ms);
    delay(duration_ms + 5);
    
    digitalWrite(BUZZER_EN_PIN, LOW);
}

void CachedPhoneTracker::_buzzerMelody(uint32_t count, uint32_t on_ms, uint32_t off_ms)
{
    for (uint32_t i = 0; i < count; i++) {
        _buzzerBeep(on_ms);
        if (i < count - 1) delay(off_ms);
    }
}

// -----------------------------------------------------------------------------
// Button Gesture Handler (1-5 clicks)
// -----------------------------------------------------------------------------
void CachedPhoneTracker::handleMultipress()
{
    int clicks = multipressClickCount;
    LOG_INFO("CachedPhoneTracker: %d-click gesture\n", clicks);
    
    switch (clicks) {
    case 1:
        // 1-click: Log waypoint
        _buzzerBeep(30);
        _logCurrentPosition();
        break;
        
    case 2:
        // 2-click: Toggle tracker
        _tracking_enabled = !_tracking_enabled;
        _buzzerMelody(2, 100, 100);
        spiLock->lock();
        _saveIndex();
        spiLock->unlock();
        LOG_INFO("CachedPhoneTracker: tracking %s\n",
                 _tracking_enabled ? "ENABLED" : "DISABLED");
        break;
        
    case 3:
        // 3-click: Clear cache
        _buzzerMelody(3, 80, 80);
        _clearCache();
        break;
        
    case 4:
        // 4-click: GPS toggle / broadcast
        _buzzerMelody(4, 60, 60);
        if (gps) {
            // Toggle GPS and broadcast position
            gps->forceWake(true);
        }
        break;
        
    case 5:
        // 5-click: Node info / ping
        _buzzerMelody(5, 50, 50);
        LOG_INFO("CachedPhoneTracker: node info - tracking=%s, points=%u\n",
                 _tracking_enabled ? "ON" : "OFF", _point_count);
        break;
        
    default:
        LOG_WARN("CachedPhoneTracker: unhandled click count %d\n", clicks);
        break;
    }
}

void CachedPhoneTracker::_clearCache()
{
    spiLock->lock();
    
    if (FSCom.exists(POINTS_PATH)) {
        FSCom.remove(POINTS_PATH);
    }
    
    _point_count = 0;
    _write_index = 0;
    _saveIndex();
    
    spiLock->unlock();
    
    _buzzerBeep(200);
    LOG_INFO("CachedPhoneTracker: cache cleared\n");
}

// -----------------------------------------------------------------------------
// CLI / Admin Message Handler
// -----------------------------------------------------------------------------
ProcessMessage CachedPhoneTracker::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Only process messages addressed to us
    if (mp.to != nodeDB->getNodeNum()) {
        return ProcessMessage::CONTINUE;
    }
    
    // Only process admin messages
    if (mp.which_payload_variant != meshtastic_MeshPacket_decoded_tag) {
        return ProcessMessage::CONTINUE;
    }
    
    auto &decoded = mp.decoded;
    if (decoded.which_payload_variant != meshtastic_Data_admin_tag) {
        return ProcessMessage::CONTINUE;
    }
    
    // Route to local processing only (no RF relay)
    LOG_INFO("CachedPhoneTracker: received admin message from 0x%x\n", mp.from);
    
    return ProcessMessage::STOP; // Don't forward
}

// -----------------------------------------------------------------------------
// External setup hook
// -----------------------------------------------------------------------------
void setupCachedPhoneTracker()
{
    if (!cachedPhoneTracker) {
        cachedPhoneTracker = new CachedPhoneTracker();
        cachedPhoneTracker->setup();
    }
}
