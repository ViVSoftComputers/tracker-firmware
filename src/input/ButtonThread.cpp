#include "input/ButtonThread.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "mesh-pb-constants.h"
#include "modules/optional/CachedPhoneTracker/CachedPhoneTracker.h"

// Flag to defer button handling until confirmation from OS (currently unused for nRF52)
static bool buttonPressed = false;

using namespace concurrency;

ButtonThread::ButtonThread() : concurrency::OSThread("Button") {}

int32_t ButtonThread::runOne()
{
    // Minimal stub: button handling for nRF52 platforms is managed by
    // OneButton callbacks registered in the variant's init() or main().
    // The multi-click handler is attached via attachMultiClick().

    if (buttonPressed) {
        buttonPressed = false;
        LOG_DEBUG("Button pressed (deferred)\n");
    }

    return 100; // Wake every 100ms to poll
}

// Static handler for multi-click events — called from OneButton ISR context
// v3.0.0 Gesture Map:
//   1 click  → Manual waypoint log (powers GPS, chirps on lock)
//   2 clicks → Toggle Tracker Mode (ascending/descending chimes + LED)
//   3 clicks → Clear flash cache (buzzes "0/500")
//   4 clicks → GPS Toggle / Broadcast
//   5 clicks → Ping
void handleMultiClick(uint8_t clicks)
{
    switch (clicks) {
    case 1:
        LOG_DEBUG("Button: 1 click — manual waypoint log\n");
        CachedPhoneTracker::logManualReading();
        break;
    case 2:
        LOG_DEBUG("Button: 2 clicks — toggle tracker mode\n");
        CachedPhoneTracker::toggleTrackerMode();
        break;
    case 3:
        LOG_DEBUG("Button: 3 clicks — clear flash cache\n");
        CachedPhoneTracker::clearCacheFromButton();
        break;
    case 4:
        LOG_DEBUG("Button: 4 clicks — toggle GPS / broadcast\n");
        CachedPhoneTracker::toggleGPS();
        break;
    case 5:
        LOG_DEBUG("Button: 5 clicks — ping\n");
        CachedPhoneTracker::sendPing();
        break;
    default:
        LOG_DEBUG("Button: %u clicks (unhandled)\n", clicks);
        break;
    }
}