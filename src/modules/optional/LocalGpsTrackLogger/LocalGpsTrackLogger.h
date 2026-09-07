#pragma once
#include "MeshModule.h"
#include "concurrency/OSThread.h"

#if HAS_SCREEN
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#endif

/**
 * @brief Offline GPS track logger with OLED display frame.
 *
 * Logs GPS fixes to LittleFS at /static/tracklog.csv and shows a live
 * summary card on the device display (point count, coordinates, HDOP, altitude).
 */
class LocalGpsTrackLogger : public MeshModule, protected concurrency::OSThread
{
  public:
    LocalGpsTrackLogger();

    /// Start the polling thread — must be called after construction.
    void setup();

  protected:
    // --- MeshModule overrides ---
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }

#if HAS_SCREEN
    virtual bool wantUIFrame() override { return true; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
#endif

    // --- OSThread override ---
    virtual int32_t runOnce() override;

  private:
    // GPS tracking state (updated by OSThread, read by drawFrame)
    int32_t last_lat_i = 0;
    int32_t last_lon_i = 0;
    uint32_t last_log_ms = 0;
    uint32_t point_count = 0;
    double last_lat_deg = 0.0;
    double last_lon_deg = 0.0;
    int32_t last_alt = 0;
    uint32_t last_hdop = 0;
    bool gps_has_lock = false;

    static constexpr uint32_t POLL_INTERVAL_MS = 5000;
    static constexpr uint32_t MAX_STATIONARY_INTERVAL_MS = 60000;
    static constexpr float MIN_MOVE_METERS = 10.0f;
    static constexpr const char *LOG_PATH = "/static/tracklog.csv";
};

extern LocalGpsTrackLogger *localGpsTrackLogger;

void setupLocalGpsTrackLogger();