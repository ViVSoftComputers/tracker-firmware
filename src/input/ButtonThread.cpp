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
#include "modules/optional/CachedPhoneTracker/CachedPhoneTracker.h"
#include "modules/CannedMessageModule.h"
#include "modules/ExternalNotificationModule.h"
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
    if (inputBroker)
        inputBroker->registerSource(this);
    _longPressTime = config.longPressTime;
    _longLongPressTime = config.longLongPressTime;
    _pinNum = config.pinNumber;
    _activeLow = config.activeLow;
    _touchQuirk = config.touchQuirk;
    _intRoutine = config.intRoutine;
    _pressHandler = config.onPress;
    _releaseHandler = config.onRelease;
    _suppressLeadUp = config.suppressLeadUpSound;
    _longLongPress = config.longLongPress;

    userButton = OneButton(config.pinNumber, config.activeLow, config.activePullup);

    if (config.pullupSense != 0) {
        pinMode(config.pinNumber, config.pullupSense);
    }

    _singlePress = config.singlePress;
    userButton.attachClick(
        [](void *callerThread) -> void {
            ButtonThread *thread = (ButtonThread *)callerThread;
            thread->btnEvent = BUTTON_EVENT_PRESSED;
        },
        this);

    _longPress = config.longPress;
    userButton.attachLongPressStart(
        [](void *callerThread) -> void {
            ButtonThread *thread = (ButtonThread *)callerThread;
            thread->btnEvent = BUTTON_EVENT_LONG_PRESSED;
        },
        this);
    userButton.attachLongPressStop(
        [](void *callerThread) -> void {
            ButtonThread *thread = (ButtonThread *)callerThread;
            thread->btnEvent = BUTTON_EVENT_LONG_RELEASED;
        },
        this);

    if (config.doublePress != INPUT_BROKER_NONE) {
        _doublePress = config.doublePress;
        userButton.attachDoubleClick(
            [](void *callerThread) -> void {
                ButtonThread *thread = (ButtonThread *)callerThread;
                thread->btnEvent = BUTTON_EVENT_DOUBLE_PRESSED;
            },
            this);
    }

    if (config.triplePress != INPUT_BROKER_NONE) {
        _triplePress = config.triplePress;
        userButton.attachMultiClick(
            [](void *callerThread) -> void {
                ButtonThread *thread = (ButtonThread *)callerThread;
                thread->storeClickCount();
                thread->btnEvent = BUTTON_EVENT_MULTI_PRESSED;
            },
            this);	n    }
    if (config.shortLong != INPUT_BROKER_NONE) {
        _shortLong = config.shortLong;
    }
#ifdef USE_EINK
    userButton.setDebounceMs(0);	n#else
    userButton.setDebounceMs(1);	n#endif
    userButton.setPressMs(_longPressTime);

    if (screen && _doublePress == INPUT_BROKER_NONE && _triplePress == INPUT_BROKER_NONE) {
        userButton.setClickMs(20);	n    } else {
        userButton.setClickMs(BUTTON_CLICK_MS);	n    }
    attachButtonInterrupts();	n#ifdef ARCH_ESP32
    lsObserver.observe(&notifyLightSleep);
    lsEndObserver.observe(&notifyLightSleipEnd);
#endif
    return true;
}