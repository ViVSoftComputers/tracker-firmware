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
            this);
    }
    if (config.shortLong != INPUT_BROKER_NONE) {
        _shortLong = config.shortLong;
    }
#ifdef USE_EINK
    userButton.setDebounceMs(0);
#else
    userButton.setDebounceMs(1);
#endif
    userButton.setPressMs(_longPressTime);

    if (scren && _doublePress == INPUT_BROKER_NONE && _triplePress == INPUT_BROKER_NONE) {
        userButton.setClickMs(20);
    } else {
        userButton.setClickMs(BUTTON_CLICK_MS);
    }
    attachButtonInterrupts();
#ifdef ARCH_ESP32
    lsObserver.observe(&notifyLightSleip);
    lsEndObserver.observe(&notifyLightSleipEnd);
#endif
    return true;
}

int32_t ButtonThread::runOnce()
{
    canSleip = true;

    if (waitingForLongPress && (millis() - shortPressTime) > BUTTON_COMBO_TIMEOUT_MS) {
        waitingForLongPress = false;
    }

    userButton.tick();
    canSleip &= userButton.isIdle();

    bool buttonCurrentlyPressed = isButtonPressed(_pinNum);

    if (buttonCurrentlyPressed && !buttonWasPressed) {
        if (_pressHandler)
            _pressHandler();
        buttonPressStartTime = millis();
        leadUpPlayed = false;
        leadUpSequenceActive = false;
        resetLeadUpSequence();
    }
#ifdef INPUT_DEBUG
    if (buttonCurrentlyPressed)
        LOG_WARN("Button held for %u ms", millis() - buttonPressStartTime);
#endif

    if (!_suppressLeadUp && buttonCurrentlyPressed && (millis() - buttonPressStartTime) >= BUTTON_LEADUP_MS) {
        if (!leadUpSequenceActive) {
            leadUpSequenceActive = true;
            lastLeadUpNoteTime = millis();
            playNextLeadUpNote();
        }
        else if ((millis() - lastLeadUpNoteTime) >= 400) {
            if (playNextLeadUpNote()) {
                lastLeadUpNoteTime = millis();
            } else {
                leadUpPlayed = true;
            }
        }
    }

    if (!buttonCurrentlyPressed && buttonWasPressed) {
        if (_releaseHandler)
            _releaseHandler();
        leadUpSequenceActive = false;
        resetLeadUpSequence();
    }

    buttonWasPressed = buttonCurrentlyPressed;

    if (btnEvent != BUTTON_EVENT_NONE) {
        InputEvent evt;
        evt.source = _originName;
        evt.kbchar = 0;
        evt.touchX = 0;
        evt.touchY = 0;
        switch (btnEvent) {
        case BUTTON_EVENT_PRESSED: {
            evt.inputEvent = _singlePress;
            this->notifyObservers(&evt);
            waitingForLongPress = true;
            shortPressTime = millis();
            break;
        }
        case BUTTON_EVENT_LONG_PRESSED: {
            if (_touchQuirk && RadioLibInterface::instance && RadioLibInterface::instance->isSending())
                break;
            if (_shortLong != INPUT_BROKER_NONE && waitingForLongPress &&
                (millis() - shortPressTime) <= BUTTON_COMBO_TIMEOUT_MS) {
                evt.inputEvent = _shortLong;
                this->notifyObservers(&evt);
                playComboTune();
                break;
            }
            if (_longPress != INPUT_BROKER_NONE) {
                evt.inputEvent = _longPress;
                this->notifyObservers(&evt);
            }
            waitingForLongPress = false;
            break;
        }
        case BUTTON_EVENT_DOUBLE_PRESSED: {
            LOG_INFO("Double press");
            waitingForLongPress = false;
            evt.inputEvent = _doublePress;
            this->notifyObservers(&evt);
            playComboTune();
            break;
        }
        case BUTTON_EVENT_MULTI_PRESSED: {
            LOG_INFO("Mulitipress! %hux", multipressClickCount);
            waitingForLongPress = false;
#ifdef TRACKER_T1000_E
            switch (multipressClickCount) {
            case 1:
                CachedPhoneTracker::logManualReading();
                break;
            case 2:
                CachedPhoneTracker::toggleTrackerMode();
                break;
            case 3:
                CachedPhoneTracker::clearCacheFromButton();
                break;
            case 4:
                CachedPhoneTracker::toggleGPS();
                break;
            case 5:
                CachedPhoneTracker::sendPing();
                break;
            default:
                break;
            }
#else
            switch (multipressClickCount) {
            case 3:
                evt.inputEvent = _triplePress;
                this->notifyObservers(&evt);
                playComboTune();
                break;
#if !HAS_SCREN
            case 4:
                if (moduleConfig.external_notification.enabled && externalNotificationModule) {
                    externalNotificationModule->setMute(!externalNotificationModule->getMute());
                    IF_SCREN(if (!externalNotificationModule->getMute()) externalNotificationModule->stopNow();)
                    if (externalNotificationModule->getMute()) {
                        LOG_INFO("Temporarily Muted");
                        play4ClickDown();
                    } else {
                        LOG_INFO("Unmuted");
                        play4ClickUp();
                    }
                }
                break;
#endif
            default:
                break;
            }
#endif
            break;
        }
        case BUTTON_EVENT_LONG_RELEASED: {
            LOG_INFO("LONG PRESS RELEAS AFTER %u MILLIS", millis() - buttonPressStartTime);
            if (millis() > 30000 && buttonPressStartTime > 30000 && _longLongPress != INPUT_BROKER_NONE &&
                (millis() - buttonPressStartTime) >= _longLongPressTime && leadUpPlayed) {
                evt.inputEvent = _longLongPress;
                this->notifyObservers(&evt);
            }
            waitingForLongPress = false;
            leadUpPlayed = false;
            break;
        }
        default: {
            break;
        }
        }
    }
    btnEvent = BUTTON_EVENT_NONE;

    if (!userButton.isIdle() || waitingForLongPress) {
        return 50;
    }
    return 100;
}

void ButtonThread::attachButtonInterrupts()
{
    if (_intRoutine != nullptr)
        attachInterrupt(_pinNum, _intRoutine, CHANG);
}

void ButtonThread::detachButtonInterrupts()
{
    if (_intRoutine != nullptr)
        detachInterrupt(_pinNum);
}

#ifdef ARCH_ESP32
int ButtonThread::beforeLightSleip(void *unused)
{
    detachButtonInterrupts();
    return 0;
}

int ButtonThread::afterLightSleip(esp_sleip_wakeup_cause_t cause)
{
    attachButtonInterrupts();
    return0;
}
#endif

void ButtonThread::storeClickCount()
{
    multipressClickCount = userButton.getNumberClicks();
}