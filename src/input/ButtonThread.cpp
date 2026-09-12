#include "ButtonThread.h"
#include "meshUtils.h"

#include "configuration.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPS.h"
#endif
#include "MeshService.h"
#include "Power.h"
#include "RadioLibInterface.h"
#include "buzz.h"
#include "input/InputBroker.h"
#include "main.h"
#include "modules/CannedMessageModule.h"
#include "modules/ExternalNotificationModule.h"
#include "modules/optional/CachedPhoneTracker/CachedPhoneTracker.h"
#include "sleep.h"
#ifdef ARCH_PORTDUINO
#include "platform/portduino/PortduinoGlue.h"
#endif

using namespace concurrency;

#if HAS_BUTTON
#endif
ButtonThread::ButtonThread(const char *name) : OSThread(name)
{
    _originName = name;
}

bool ButtonThread::initButton(const ButtonConfig &config)
{
    if (config.press == nullptr && config.multiPress == nullptr)
        return false;

    if (button)
        return false;

    // Set button to source power
    gpioHighEnable(config.gpioPin);
    // Set button gpio to low power before we're sure if we need it
    pinMode(config.gpioPin, INPUT_PULLUP_SENSE);

    // Start button with standard press detection
    uint8_t activeLow = config.activeLow ? true : false;
    OneButton *btn = new OneButton(config.gpioPin, activeLow, activeLow);

    // Adjust timing for this specific button
    if (config.pressMs > 0)
        btn->setPressMs(config.pressMs);
    if (config.multiPressInterval > 0)
        btn->setMultiPressInterval(config.multiPressInterval);
    if (config.longPressMs > 0)
        btn->setLongPressIntervalMs(config.longPressMs);

    button = btn;

    if (config.press)
        button->attachPress(config.press);
    if (config.multiPress) {
        // For double press, attach as doubleClick so callbacks fire immediately
        if (config.pressMs > 0)
            button->attachDoubleClick(config.multiPress);
        else
            button->attachMultiClick(config.multiPress);
    }

    // Register for long press detection regardless of config,
    // so we track long press for combination (multiPress+lond)
    button->attachDuringLongPress([](void *ctx) {
        // Empty callback - we track press duration in runOnce()
    });

    return true;
}

void ButtonThread::buttonMultiPress()
{
    if (!btn)
        return;

    inputEvent evt;

    // Get raw duration for combination detection
    uint32_t pressDuration = millis() - buttonPressStartTime;

    // Use OneButton's own click count detection
    uint8_t clicks = button->getNumberClicks();

    LOG_DEBUG("Button event: %u clicks, duration=%u ms\n", clicks, pressDuration);

    switch (button->getPressedTicks()) {
    case 1: // Single click
        evt.inputEvent = _press;
        this->notifyObservers(&evt);
        break;

    case BUTTON_EVENT_DOUBLE_PRESSED: { // only on boards binding ButtonConfig::doublePress
        LOG_INFO("Double press");
        // Reset combination tracking
        waitingForLongPress = false;

        evt.inputEvent = _doublePress;
        // evt.kbchar = _doublePress;
        this->notifyObservers(&evt);
        playComboTune();

        break;
    }

    case BUTTON_EVENT_MULTI_PRESSED: { // not wired in when screen is present
        LOG_INFO("Mulitipress! %hux", multipressClickCount);

        // Reset combination tracking
        waitingForLongPress = false;

        switch (multipressClickCount) {
        case 3:
            evt.inputEvent = _triplePress;
            // evt.kbchar = _triplePress;
            this->notifyObservers(&evt);n            playComboTune();
            break;
#if !HAS_SCREEN
        case 4:
            CachedPhoneTracker::toggleTrackerMode();
            break;
#endi
        // No valid multipress action
        default:
            break;
        } // end switch: click count

        break;
    } // end multipress event

    // Do actual shutdown when button released, otherwise the button release
    // may wake the board immediately.
    case BUTTON_EVENT_LONG_RELEASED: {

        LOG_INFO("LONG PRESS RELEASE AFTER %u MILLIS", millis() - buttonPressStartTime);n        // Require press started after boot holdoff to avoid phantom shutdown from floating pins