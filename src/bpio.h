#pragma once
#include <Arduino.h>
#include "defines.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>
#include "devices.h"
#include "shockertasks.h"
#include "taskloops.h"
#include <WebSocketsClient.h>

#define FS LittleFS

extern ShockerList shockerList;

WebSocketsClient webSocket;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED:
			debugPrintf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
        {
			debugPrintf("[WSc] Connected to url: %s\n", payload);

			// send message to BPIO's WSDM server identifying this device
			// webSocket.sendTXT("Connected");
            // await ws.send(json.dumps({ "identifier": "OpenShock", "address": DEVICE_ADDRESS, "version": 0}))
            
            StaticJsonDocument<200> doc;
            doc["identifier"] = "OpenShock";

            // Use the MAC address of the ESP32, remove the colons
            String macStr = WiFi.macAddress();
            macStr.replace(":", "");
            doc["address"] = macStr;

            doc["version"] = 0;

            String json;
            serializeJson(doc, json);
            webSocket.sendTXT(json);
        }
        break;
		case WStype_TEXT:
			debugPrintf("[WSc] get text: %s\n", payload);

            

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			debugPrintf("[WSc] get binary length: %u\n", length);
            // The first message we expect back is being asked what device
            // type we are with the message "DeviceType;"
            // We need to respond with "Z:MACADDRESS:10"

            // We also recieve vibrate and battery request messages
            // "Vibrate:1" - vibrate at 1/20th power
            // "Battery" - respond with "90;" (90% battery)

            if (strncmp((char *)payload, "DeviceType;", 11) == 0) {
                // Use the MAC address of the ESP32, remove the colons
                String macStr = WiFi.macAddress();
                macStr.replace(":", "");
                String response = "Z:" + macStr + ":10";
                webSocket.sendBIN((uint8_t *)response.c_str(), response.length());
            } else if (strncmp((char *)payload, "Vibrate:", 8) == 0) {
                // Vibrate at the specified power
                uint8_t intensity = atoi((char *)payload + 8);
                debugPrintf("BPIO: Shock at %d\n", intensity);

                // // For each shocker, add a one second shock at the specified power
                
                // // Grab the shockerlist mutex
                // xSemaphoreTake(shockerListMutex, portMAX_DELAY);
                // for (uint8_t i = 0; i < shockerList.size(); i++) {
                //     intensity = (intensity/20)*100;
                //     uint16_t id = 0;

                //     sendCommandToShocker(i, SHOCK, intensity, 1000);
                // }
                // xSemaphoreGive(shockerListMutex);
            } else if (strncmp((char *)payload, "Battery", 7) == 0) {
                // Respond with the battery level
                String response = "90;";
                webSocket.sendBIN((uint8_t *)response.c_str(), response.length());
            }


			// hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:		
            debugPrintf("[WSc] error(%u): %s\n", length, payload);
            break;
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}

}

class BPIOHandler {
   public:
    BPIOHandler(uint16_t port = 54817, String host = "", bool enabled = false)
        : bpioPort(port), bpioHost(host), bpioEnabled(enabled) {}

    void setup() {
        // Read port and parameters to subscribe to from bpio.json in LittleFS
        File bpioFile = FS.open("/config/bpio.json", "r");
        if (!bpioFile) {
            Serial.println("Failed to open bpio.json");
            bpioFile.close();
            return;
        }

        StaticJsonDocument<1024> bpioDoc;
        DeserializationError bpioError = deserializeJson(bpioDoc, bpioFile);
        if (bpioError) {
            debugPrint("deserializeJson on bpio.json() failed: ");
            debugPrintln(bpioError.c_str());
            bpioFile.close();
            return;
        }

        bpioFile.close();

        // Set up BPIO settings
        bpioPort = bpioDoc["port"];
        bpioHost = bpioDoc["host"].as<String>();
        bpioEnabled = bpioDoc["enabled"];

        // Print out all the settings
        debugPrintln("BPIO settings:");
        debugPrint("  Port: ");
        debugPrintln(bpioPort);
        debugPrint("  Host: ");
        debugPrintln(bpioHost);
        debugPrint("  Enabled: ");
        debugPrintln(bpioEnabled);

        if (bpioEnabled) {
            webSocket.begin(bpioHost, bpioPort, "/");
            webSocket.onEvent(webSocketEvent);
	        webSocket.setReconnectInterval(5000);
        }
    }

    void loop()
    {
        if (bpioEnabled)
            webSocket.loop();
        
    }

    void setConfig(uint16_t port, String host, bool enabled)
    {
        bpioPort = port;
        bpioHost = host;
        bpioEnabled = enabled;

        // Save the new configuration to the bpio.json file

        StaticJsonDocument<1024> bpioDoc;
        bpioDoc["port"] = bpioPort;
        bpioDoc["host"] = bpioHost;
        bpioDoc["enabled"] = bpioEnabled;

        File bpioFile = FS.open("/config/bpio.json", "w");
        if (bpioFile) {
            serializeJson(bpioDoc, bpioFile);
            bpioFile.close();
        }
    }

    String toJson()
    {
        StaticJsonDocument<1024> bpioDoc;
        bpioDoc["port"] = bpioPort;
        bpioDoc["host"] = bpioHost;
        bpioDoc["enabled"] = bpioEnabled;

        String bpioJson;
        serializeJson(bpioDoc, bpioJson);
        return bpioJson;
    }

    private:
        uint16_t bpioPort;
        String bpioHost;
        bool bpioEnabled;

};