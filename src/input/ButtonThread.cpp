#include "input/ButtonThread.h"
#include "PowerFSM.h"
#include "buzz.h"
#include "mesh-pb-constants.h"
#include "modules/optional/CachedPhoneTracker/CachedPhoneTracker.h"

// Flag to defer button handling until confirmation from OS (currently unused for nRF52)
static bool buttonPressed = false;

using namespace concurrency;

ButtonThread::ButtonThread() : concurrency::OSThread("Button") {}

int32_t ButtonThread::runOnce()
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
void handleMultiClick(uint8_t clicks)
{
    switch (clicks) {
    case 1:
        LOG_DEBUG("Button: 1 click — sending text message\n");
        service.refreshLocalMeshNode();
        break;
    case 2:
        LOG_DEBUG("Button: 2 clicks — sending position\n");
        service.refreshLocalMeshNode();
        service.refreshMyNodeInfo();
        break;
    case 3:
        LOG_DEBUG("Button: 3 clicks — shutdown\n");
        powerFSM.trigger(EVENT_PRESS);
        break;
    case 4:
        LOG_DEBUG("Button: 4 clicks — toggle tracker mode\n");
        CachedPhoneTracker::toggleTrackerMode();
        break;
    case 5:
        LOG_DEBUG("Button: 5 clicks — reboot to DFU\n");
        screen->startBluetoothPinScreen(0);
        break;
    default:
        LOG_DEBUG("Button: %u clicks (unhandled)\n", clicks);
        break;
    }
}