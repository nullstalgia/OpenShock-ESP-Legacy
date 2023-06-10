#include <Arduino.h>
#include "esp32-hal.h"
#include <WiFiManager.h>
#define WEBSERVER_H

#include <ESPmDNS.h>

#include "defines.h"

#include "rgb.h"

#include "taskloops.h"
#include "web.h"

#include "GenericShocker.h"
#include "Petrainer.h"

#include "RMTHandler.h"

#include "osc.h"

#include "bpio.h"

#include "upnp.h"

#include "ota.h"

#include "devices.h"

#include <EasyButton.h>

RMTHandler rmtHandler;
GenericShocker genericShocker(&rmtHandler);
Petrainer petrainerShocker(&rmtHandler);
basicRGB rgb;
OSCHandler oscHandler;
BPIOHandler bpioHandler;

ShockerList shockerList;
SemaphoreHandle_t shockerListMutex;

#define FS LittleFS
#define FORMAT_FS_IF_FAILED true

unsigned long lastTimeClearedClients = 0;

// Make a mutex for the function list
// TODO make this not global?
SemaphoreHandle_t functionListMutex;

bool upnpSuccess = false;

bool lastButtonState = HIGH;

void printIPAddress() {
    debugPrint("IP address: ");
    debugPrintln(WiFi.localIP());
}

void onButtonTriplePress() {
    debugPrintln("Triple pressed! Printing IP address...");
    printIPAddress();
}

EasyButton bootButton(0, 35U, true, true);

bool emergencyStopped = false;

void setup() {
    // Initialize the serial port
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    //USBSerial.begin(115200);


    debugPrintln("Starting up");

    // Set up STATUS LED
    pinMode(STATUS_PIN, OUTPUT);
    digitalWrite(STATUS_PIN, LOW);

    // Set up emergency stop button
    pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);

    bool res;
    WiFiManager wm;
    wm.setDarkMode(true);

    // Note to self: "sep" is the separator between the menu items
    const char * menu[] = {"wifi","wifinoscan","sep","info","update","exit"};
    wm.setMenu(menu, 6);
    res = wm.autoConnect("OpenShock");

    // TODO: FIX needing to reset after connecting to new AP

    if (!res) {
        debugPrintln("Failed to connect");
        // ESP.restart();
    } else {
        // if you get here you have connected to the WiFi
        debugPrintln("connected...yeey :)");
    }
    
    // Print out the MAC address in a readable format
    debugPrint("MAC: ");
    debugPrintln(WiFi.macAddress());

    // Set the RMT transmitter pin to output and LOW
    pinMode(TRANSMITTER_PIN, OUTPUT);
    digitalWrite(TRANSMITTER_PIN, LOW);

    // Create the mutexes
    functionListMutex = xSemaphoreCreateMutex();
    shockerListMutex = xSemaphoreCreateMutex();

    // Init LittleFS
    if (!FS.begin(FORMAT_FS_IF_FAILED)) {
        debugPrintln("LittleFS failed to mount");
        while (true) {
            rgb.setRGB(255, 0, 0);
            delay(1000);
            rgb.setRGB(0, 0, 0);
            delay(1000);
        }
    }

    // Even though I don't need to use the mutex,
    // consistency is king
    xSemaphoreTake(shockerListMutex, portMAX_DELAY);

    // Load the devices from the file system
    shockerList.readShockersFromFiles();

    xSemaphoreGive(shockerListMutex);

    // Configure the RMT transmitter
    rmtHandler.init(TRANSMITTER_PIN);

// Set up RGB
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    rgb.init(RGB_PIN);
#elif defined(CONFIG_IDF_TARGET_ESP32)
    rgb.init(LED_PIN);
#endif
    rgb.setBrightness(20);
    rgb.setRGB(0, 0, 255);

    setupWebServer();
    otaSetup(OTA_PASSWORD);
// setupOSC();
#if (UPNP_ENABLE)
    upnpResultQueue = xQueueCreate(2, sizeof(upnpNegotiationResult));
    // Create a task to start UPnP negotiation, otherwise we block setup
    xTaskCreatePinnedToCore(setupUPnPTask, "setupUPnPTask", 10000, NULL, 1,
                            NULL, 1);
#endif

    shockerActionQueue = xQueueCreate(100, sizeof(ShockerActionParameters*));

    oscHandler.setup();
    bpioHandler.setup();

    if (!MDNS.begin(MDNS_NAME)) {
        debugPrintln("Error setting up MDNS responder!");
    } else {
        debugPrint("mDNS responder started: ");
        debugPrintln(MDNS_NAME);

        // Add services to MDNS-SD
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("openshock", "tcp", 8080);
    }

    rgb.setRGB(0, 0, 0);

    // Init io0 button
    bootButton.begin();
    bootButton.onSequence(3, 2000, onButtonTriplePress);

    // Create a task to run the function loop on the second core
    xTaskCreatePinnedToCore(functionLoop, "functionLoop", 10000, NULL, 1, NULL,
                            1);

    delay(1000);

    // Set up watchdog timers
    esp_task_wdt_init(5, true);
    // enableLoopWDT();
    // enableCore0WDT();
    enableCore1WDT();

    digitalWrite(STATUS_PIN, HIGH);

    debugPrintln("OpenShock is ready!");
    printIPAddress();
}

unsigned long lastEmergencyStopLEDLoop = 0;
bool emergencyStopLEDState = false;
// Flash the RGB led on and off if the emergency stop button was pressed
void emergencyStopLEDLoop(unsigned long loopMillis) {
    if (emergencyStopped) {
        if (loopMillis - lastEmergencyStopLEDLoop > 100) {
            lastEmergencyStopLEDLoop = loopMillis;
            if (emergencyStopLEDState) {
                rgb.setRGB(0, 0, 0);
                digitalWrite(STATUS_PIN, LOW);
                emergencyStopLEDState = false;
            } else {
                rgb.setRGB(255, 0, 0);
                //digitalWrite(STATUS_PIN, HIGH);
                emergencyStopLEDState = true;
            }
        }
    }
}

void loop() {
    unsigned long loopMillis = millis();

    bootButton.read();

    // Check if the emergency stop button is pressed
    if (digitalRead(EMERGENCY_STOP_PIN) == LOW) {
        if (!emergencyStopped) {
            debugPrintln("Emergency stop button pressed!!!");
            emergencyStopped = true;
        }
    }

    emergencyStopLEDLoop(loopMillis);

    if ((WiFi.status() != WL_CONNECTED)) {
        digitalWrite(STATUS_PIN, LOW);
        rgb.setRGB(0, 255, 0);
        delay(1000);
        rgb.setRGB(0, 0, 0);
        delay(1000);

        // Reconnect to WiFi
        WiFiManager wm;
        wm.disconnect();
        wm.setDarkMode(true);
        wm.setConfigPortalTimeout(120);
        wm.autoConnect("OpenShock");
        digitalWrite(STATUS_PIN, HIGH);
    }

    // On button falling edge, set RGB to blue
    if (bootButton.wasPressed()) {
        rgb.setRGB(0, 0, 255);
    } else if (bootButton.wasReleased()) {
        rgb.setRGB(0, 0, 0);
    }

    // Check if we need to clear the socket clients
    if (loopMillis - lastTimeClearedClients > 1000) {
        lastTimeClearedClients = loopMillis;
        cleanSocketClients();
    }
#if (UPNP_ENABLE)
    upnpNegotiationResult result;
    if (xQueueReceive(upnpResultQueue, &result, 0) == pdTRUE) {
        debugPrint("UPnP Negotiation Result: ");
        // Decode the result from the enum
        switch (result) {
            case UPNP_SUCCESS:
                debugPrintln("SUCCESS");
                upnpSuccess = true;
                break;
            case UPNP_ALREADY_MAPPED:
                debugPrintln("ALREADY_MAPPED");
                upnpSuccess = true;
                break;
            case UPNP_FAILED:
                debugPrintln("FAILED");
                if (!upnpSuccess) upnpSuccess = false;
                break;
            case UPNP_ATTEMPTING:
                debugPrintln("ATTEMPTING");
                if (!upnpSuccess) upnpSuccess = false;
                break;
            case UPNP_ATTEMPTING_AGAIN:
                debugPrintln("ATTEMPTING AGAIN");
                if (!upnpSuccess) upnpSuccess = false;
                break;
        }

        if (upnpSuccess) informSharingServer();
    }
    if (upnpSuccess) maintainUPnPMaps();
#endif
    oscHandler.loop();
    bpioHandler.loop();
    otaLoop(loopMillis);
    // feedLoopWDT();

    // Can't busy wait the idle task, so just delay
    vTaskDelay(10);
}