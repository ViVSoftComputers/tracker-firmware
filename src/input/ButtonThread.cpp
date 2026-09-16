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