#pragma once

#include "MeshModule.h"
#include "concurrency/OSThread.h"

// -----------------------------------------------------------------------------
// LocalGpsTrackLogger - GPS logging module for Heltec T096
// -----------------------------------------------------------------------------

class LocalGpsTrackLogger : public concurrency::OSThread
{
public:
    LocalGpsTrackLogger();
    bool setup();
    int32_t runOnce() override;

    void toggle();
    void clearCache();
    uint32_t count() const;
    bool active() const;

private:
    bool _active;
    uint32_t _lastLog;
    uint32_t _count;
    uint32_t _head;

    void _writePoint(const GPSPosition &pos);
    void _trimLog();
    void _restoreState();
    void _syncPoints();
    bool isPhoneAvailable();
};

extern LocalGpsTrackLogger *localGpsTrackLogger;
void setupLocalGpsTrackLogger();
