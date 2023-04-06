#pragma once

#include <ArduinoOTA.h>
#include "defines.h"

bool otaEnabled = false;
unsigned long setupEndTime = 0;

void otaSetup(const char* const password) {
    // Only if password is set do we enable OTA
    if (password[0] == '\0') {
        return;
    }
    ArduinoOTA.setPassword(password);

    ArduinoOTA.onStart([]() {
        debugPrintln("OTA Start");
        LittleFS.end();
    });

    ArduinoOTA.onEnd([]() { debugPrintln("OTA End"); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        debugPrintf("OTA Progress: %u%%\n", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        debugPrintf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            debugPrintln("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            debugPrintln("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            debugPrintln("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            debugPrintln("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            debugPrintln("End Failed");
        }
    });

    otaEnabled = true;

    ArduinoOTA.begin();

    setupEndTime = millis();

    debugPrintln("OTA Ready");
}

void otaLoop(unsigned long loopMillis) {
    if (otaEnabled) {
#if defined(OTA_TIMEOUT)
        if (setupEndTime + OTA_TIMEOUT < loopMillis) {
            otaEnabled = false;
            debugPrintln("OTA Disabled after timeout, reset to enable again");
            return;
        }
#endif
        ArduinoOTA.handle();
    }
}