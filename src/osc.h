#pragma once
#include <Arduino.h>
#include "defines.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoOSCWiFi.h>
#include "shockertasks.h"
#include "taskloops.h"

#define FS LittleFS

// TODO: Refactor to use bundles and queues.

class OSCHandler {
   public:
    OSCHandler(uint16_t port = 9000, String host = "", String path = "",
               bool enabled = false, bool sendHighest = false, bool sendRelative = false)
        : oscPort(port), oscHost(host), oscPath(path), oscEnabled(enabled), oscSendRelative(sendRelative), oscReady(false) {}

    void setup() {
        // Read port and parameters to subscribe to from osc.json in LittleFS
        File oscFile = FS.open("/config/osc.json", "r");
        if (!oscFile) {
            Serial.println("Failed to open osc.json");
            oscFile.close();
            return;
        }

        StaticJsonDocument<1024> oscDoc;
        DeserializationError oscError = deserializeJson(oscDoc, oscFile);
        if (oscError) {
            debugPrint("deserializeJson on osc.json() failed: ");
            debugPrintln(oscError.c_str());
            oscFile.close();
            return;
        }

        oscFile.close();

        // Set up OSC
        oscPort = oscDoc["port"];
        oscHost = oscDoc["host"].as<String>();
        oscPath = oscDoc["path"].as<String>();
        oscEnabled = oscDoc["enabled"];
        oscSendHighest = oscDoc["sendHighest"];
        oscSendRelative = oscDoc["sendRelative"];
        // Ensure trailing slash on oscPath
        if (!oscPath.endsWith("/")) {
            oscPath += "/";
        }

        // Initialize lastSentMethod and lastSentIntensity maps to 255 for all
        // IDs 255 is an invalid value for method and intensity, so this ensures
        // that the first message sent for each ID will be sent no matter what
        for (uint16_t i = 0; i < MAX_SHOCKERS; i++) {
            lastSentMethod[i] = 255;
            lastSentIntensity[i] = 255;
        }

        // Print out all the settings
        debugPrintln("OSC settings:");
        debugPrint("  Port: ");
        debugPrintln(oscPort);
        debugPrint("  Host: ");
        debugPrintln(oscHost);
        debugPrint("  Path: ");
        debugPrintln(oscPath);
        debugPrint("  Enabled: ");
        debugPrintln(oscEnabled);
        debugPrint("  Send highest: ");
        debugPrintln(oscSendHighest);
        debugPrint("  Send relative: ");
        debugPrintln(oscSendRelative);

        if (oscEnabled) {
            oscReady = true;
        }
    }

    void publishMethodAndIntensity(uint16_t id, uint8_t method,
                                   uint8_t intensity, const char* role, float multiplier) {
        if (!oscReady) return;
        // Check if method and intensity are different from last sent values for
        // this ID
        if (method != lastSentMethod[id] ||
            intensity != lastSentIntensity[id]) {
            if (intensity == 0) {
                method = 0;
            }
            
            publishMethod(id, method, role);
            publishIntensity(id, intensity, role, multiplier);
        }
    }

    void loop() {
        if (oscReady) OscWiFi.update();
    }

    void setConfig(uint16_t port, String host, String path, bool enabled, bool sendHighest, bool sendRelative) {
        oscPort = port;
        oscHost = host;
        oscPath = path;
        oscEnabled = enabled;
        oscSendHighest = sendHighest;
        oscSendRelative = sendRelative;
        if (!oscPath.endsWith("/")) {
            oscPath += "/";
        }

        // Save the new configuration to the osc.json file
        StaticJsonDocument<1024> oscDoc;
        oscDoc["port"] = oscPort;
        oscDoc["host"] = oscHost;
        oscDoc["path"] = oscPath;
        oscDoc["enabled"] = oscEnabled;
        oscDoc["sendHighest"] = oscSendHighest;
        oscDoc["sendRelative"] = oscSendRelative;
        File oscFile = FS.open("/config/osc.json", "w");
        if (oscFile) {
            serializeJson(oscDoc, oscFile);
            oscFile.close();
        }
    }

    String toJson() {
        StaticJsonDocument<1024> oscDoc;
        oscDoc["port"] = oscPort;
        oscDoc["host"] = oscHost;
        oscDoc["path"] = oscPath;
        oscDoc["enabled"] = oscEnabled;
        oscDoc["sendHighest"] = oscSendHighest;
        oscDoc["sendRelative"] = oscSendRelative;
        String oscJson;
        serializeJson(oscDoc, oscJson);
        return oscJson;
    }

   private:
    uint16_t oscPort;
    String oscHost;
    String oscPath;
    bool oscEnabled;
    bool oscSendHighest;
    bool oscSendRelative;
    bool oscReady;
    std::map<uint16_t, uint8_t>
        lastSentMethod;  // Map of last sent method for each ID
    std::map<uint16_t, uint8_t>
        lastSentIntensity;  // Map of last sent intensity for each ID

    void publishMethod(uint16_t id, uint8_t method, const char* role) {
        OscWiFi.send(oscHost, oscPort, oscPath + String(id) + "/Method",
                     method);
        // If role is set, send to role path as well
        // oscPath/<role>/Method
        if (strlen(role) > 0) {
            OscWiFi.send(oscHost, oscPort, oscPath + role + "/Method", method);
        }
        lastSentMethod[id] = method;
    }

    void publishIntensity(uint16_t id, uint8_t intensity, const char* role, float multiplier) {
        if (oscSendRelative) {
            // Calculate the relative intensity value
            float relativeIntensity = (intensity / (multiplier * 100)) * 100;
            intensity = static_cast<uint8_t>(round(relativeIntensity));
        }

        float intensityFloat = intensity / 100.0;
        OscWiFi.send(oscHost, oscPort, oscPath + String(id) + "/Intensity",
                     intensityFloat);
        // If role is set, send to role path as well
        // oscPath/<role>/Intensity
        if (strlen(role) > 0) {
            OscWiFi.send(oscHost, oscPort, oscPath + role + "/Intensity",
                         intensityFloat);
        }
        lastSentIntensity[id] = intensity;
        // Send highest intensity
        if (oscSendHighest) {
            uint8_t highestIntensity = 0;
            for (const auto& pair : lastSentIntensity) {
                if (pair.second > highestIntensity) {
                    // Ignore invalid value
                    if (pair.second == 255) continue;
                    
                    highestIntensity = pair.second;
                }
            }
            intensityFloat = highestIntensity / 100.0;
            OscWiFi.send(oscHost, oscPort, oscPath + "High/Intensity",
                    intensityFloat);
        }
    }
};