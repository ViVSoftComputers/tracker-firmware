#include "ButtonThread.h"
#include "configuration.h"
#include "input/UserButton.h"
#include "main.h"
#include "modules/optional/CachedPhoneTracker/CachedPhoneTracker.h"

namespace concurrency
{
extern BinarySemaphore mainDelay;
}

// -----------------------------------------------------------------------------
// T1000-E: Force multi-click registration unconditionally
// -----------------------------------------------------------------------------
// The T1000-E has a single user button (P0.06). ButtonThread::attachMultiClick()
// is called in setup() regardless of whether the CachedPhoneTracker module is
// enabled, so that 1-5 click gestures always work for tracker control.
//
// On non-T1000-E platforms, the standard dynamic registration via
// ButtonThread::wakeupScreen() continues to fire (calling attachMultiClick
// only when the screen module is available).
// -----------------------------------------------------------------------------

namespace
{
UserButton userButton;
}

ButtonThread::ButtonThread() : OSThread("Button"), concurrency::PeriodicTask(getName(), 50)
{
    userButton.init();
}

int32_t ButtonThread::runOnce()
{
#if defined(BUTTON_PIN)
    userButton.tick();
#endif

    return 50;
}

void ButtonThread::attachMultiClick(ButtonEventCallback callback)
{
    userButton.setMultipressCallback(callback);
}

int ButtonThread::getClickCount()
{
    return userButton.getNumberClicks();
}

void ButtonThread::wakeupScreen()
{
    static bool firstWake = true;
    if (firstWake) {
        firstWake = false;
#if !defined(TRACKER_T1000_E)
        // Standard: only wire multi-click when screen module is available.
        userButton.setMultipressCallback([]{ screen->onLongPress(); });
#endif
    }
}

#if defined(TRACKER_T1000_E)
// Force unconditional multi-click registration for T1000-E
// Called from setup() after module initialization
void ButtonThread::setup()
{
    userButton.setMultipressCallback([]{ cachedPhoneTracker->handleMultipress(); });
}
#endif

int multipressClickCount = 0;

static void multipressInterrupt()
{
    multipressClickCount = userButton.getNumberClicks();
}