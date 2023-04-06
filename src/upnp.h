#pragma once
#if (UPNP_ENABLE)
#include <Arduino.h>
#include "defines.h"
#include <WiFi.h>
#include <TinyUPnP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <HTTPClient.h>


// enum for the port mapping result
enum upnpNegotiationResult {
    UPNP_NOT_STARTED,
    UPNP_ATTEMPTING,
    UPNP_ATTEMPTING_AGAIN,
    UPNP_SUCCESS,
    UPNP_ALREADY_MAPPED,
    UPNP_FAILED,
};

QueueHandle_t upnpResultQueue;

TinyUPnP tinyUPnP(
    10000);  // -1 means blocking, preferably, use a timeout value (ms)

void negotiatePortMappings() {
    portMappingResult portMappingAdded;
    tinyUPnP.addPortMappingConfig(WiFi.localIP(), 8080, UPNP_LISTEN_PORT,
                                  RULE_PROTOCOL_TCP, UPNP_LEASE_DURATION,
                                  UPNP_FRIENDLY_NAME);

    uint8_t tries = 0;
    uint8_t maxTries = 2;
    uint32_t timeout = 10000;
    bool portMappingSuccess = false;

    upnpNegotiationResult upnpResult = UPNP_ATTEMPTING;
    xQueueSend(upnpResultQueue, &upnpResult, 0);
    while (tries < maxTries && portMappingAdded != SUCCESS && portMappingAdded != ALREADY_MAPPED) {
        vTaskDelay(10);
        portMappingAdded = tinyUPnP.commitPortMappings();
        debugPrintln("");
        if (portMappingAdded != SUCCESS && portMappingAdded != ALREADY_MAPPED) {
            if (maxTries >= tries + 1){
            debugPrint("Port mapping failed, trying again ");
            debugPrint(maxTries - tries);
            debugPrint(" more times in ");
            debugPrint(timeout / 1000);
            debugPrintln(" seconds...");
            delay(timeout);
            }
            tries++;
        } else if (portMappingAdded == SUCCESS) {
            debugPrintln("Port mapping successful!");
            portMappingSuccess = true;
            upnpResult = UPNP_SUCCESS;
            xQueueSend(upnpResultQueue, &upnpResult, 0);
        } else if (portMappingAdded == ALREADY_MAPPED) {
            debugPrintln("Port mapping already exists!");
            portMappingSuccess = true;
            upnpResult = UPNP_ALREADY_MAPPED;
            xQueueSend(upnpResultQueue, &upnpResult, 0);
        }
        vTaskDelay(10);
    }
    if (portMappingSuccess != true) {
        debugPrintln("Port mapping failed, giving up!");
        upnpResult = UPNP_FAILED;
        xQueueSend(upnpResultQueue, &upnpResult, 0);
    }
}

void setupUPnPTask(void* pvParameters) {
    negotiatePortMappings();
    vTaskDelete(NULL);
}

void maintainUPnPMaps() { tinyUPnP.updatePortMappings(60000, NULL); }

// Send a GET request to the OpenShock Code Sharing Server to add a new port
// mapping Endpoint: dev.???.com/inialize/<port> Returns: 200 if
// successful

void informSharingServer() {
    // HTTPClient http;
    // http.begin("http://dev.???.com/initialize/" +
    //            String(UPNP_LISTEN_PORT));
    // int httpCode = http.GET();
    // http.end();
}

#endif