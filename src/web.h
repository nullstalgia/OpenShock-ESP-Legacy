#pragma once
#include "ESPAsyncWebServer.h"
#include "defines.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "webincludes.h"
#include "shockertasks.h"
#include "taskloops.h"
#include "devices.h"
#include "osc.h"
#include "bpio.h"
#include <AsyncElegantOTA.h>

AsyncWebServer server(80);
AsyncWebServer websocketServer(8080);
AsyncWebSocket ws("/ws");

uint8_t timings_len;
rmt_data_t timings[100];

uint64_t lastTimeStamp = 0;

extern ShockerList shockerList;
extern SemaphoreHandle_t functionListMutex;
extern SemaphoreHandle_t shockerListMutex;

extern QueueHandle_t shockerActionQueue;

extern OSCHandler oscHandler;
extern BPIOHandler bpioHandler;

void cleanSocketClients() { ws.cleanupClients(); }

void sendCommandToShocker(uint16_t id, uint8_t method, uint8_t intensity,
                          uint32_t duration) {
    if (xSemaphoreTake(shockerListMutex, pdMS_TO_TICKS(10 * 1000)) == pdTRUE) {
        Shocker *shocker = shockerList.getShocker(id);
        xSemaphoreGive(shockerListMutex);
        if (shocker == NULL) {
            debugPrintf("Shocker %d not found!!\n", id);
            return;
        }

        ShockerActionParameters *params = new ShockerActionParameters;
        params->method = method;
        params->id = id;
        strcpy(params->role, shocker->role);

        // Since method 0 isn't used by anything,
        // I'm just letting it be another way to send a "stop"
        if (params->method == 0) {
            params->method = VIBE;
            params->intensity = 0;
            params->duration = 0;
        } else {
            params->intensity = intensity;
            params->duration = duration;
        }

        if (params->duration == 0) {
            params->expiration_time = millis() + COMMAND_TIMEOUT;
        } else {
            params->expiration_time = millis() + params->duration;
        }
        params->shock_multiplier = shocker->shock_multiplier;
        params->vibe_multiplier = shocker->vibe_multiplier;
        params->send_function = shocker->send;

        // Add the parameters to the queue
        if (xQueueSend(shockerActionQueue, &params, pdMS_TO_TICKS(10 * 1000)) !=
            pdTRUE) {
            debugPrintln("Failed to send to shockerActionQueue!!!");
        }

    } else {
        debugPrintln("Failed to take shockerListMutex!!!");
        // Print function parameters
        debugPrintf("id: %d, method: %d, intensity: %d, duration: %lu\n", id,
                    method, intensity, duration);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        // client connected
        debugPrintf("ws[%s][%u] connect\n", server->url(), client->id());

        // Send the shocker list
        if (xSemaphoreTake(shockerListMutex, pdMS_TO_TICKS(10 * 1000)) ==
            pdTRUE) {
            client->text(shockerList.toJson());
            xSemaphoreGive(shockerListMutex);
        } else {
            debugPrintln("Failed to take shockerListMutex!!!");
        }

        client->ping();
        // client->keepAlivePeriod(10);
    } else if (type == WS_EVT_DISCONNECT) {
        // client disconnected
        debugPrintf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    } else if (type == WS_EVT_ERROR) {
        // error was received from the other end
        debugPrintf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(),
                    *((uint16_t *)arg), (char *)data);
    } else if (type == WS_EVT_PONG) {
        // pong message was received (in response to a ping request maybe)
        debugPrintf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(),
                    len, (len) ? (char *)data : "");
    } else if (type == WS_EVT_DATA) {
        StaticJsonDocument<1024> doc;
        // String json = (char *)data;
        DeserializationError error = deserializeJson(doc, (char *)data, len);

        if (error) {
            debugPrint("deserializeJson() failed: ");
            debugPrintln(error.c_str());
            // debugPrintln("Bad JSON: ");
            // debugPrintln((char *)data);
            return;
        }

        // If "ids" is defined, repeat the command for each id
        // Otherwise just take the id from "id"

        // TODO: Check if it's a role and get the ID from the role and
        // send via the regular channels
        if (doc.containsKey("ids")) {
            JsonArray ids = doc["ids"].as<JsonArray>();
            for (JsonVariant id : ids) {
                if (id.is<uint16_t>()) {
                    sendCommandToShocker(id.as<uint16_t>(), doc["method"],
                                         doc["intensity"], doc["duration"]);
                } else {
                    uint16_t shockerID = shockerList.getShockerIDByRole(id.as<const char *>());

                    sendCommandToShocker(shockerID, doc["method"],
                                         doc["intensity"], doc["duration"]);
                }
                vTaskDelay(1);
            }
        } else {
            JsonVariant id = doc["id"];
            if (id.is<uint16_t>()) {
                sendCommandToShocker(doc["id"], doc["method"], doc["intensity"],
                                    doc["duration"]);
            } else {
                uint16_t shockerID = shockerList.getShockerIDByRole(id.as<const char *>());

                sendCommandToShocker(shockerID, doc["method"], doc["intensity"],
                                    doc["duration"]);
            }
            vTaskDelay(1);
        }
    }
}

String processTemplating(const String &var) {
    if (var == "HEADER-ADD") {
        return header_addendum;
    } else if (var == "LASTTIMESTAMP") {
        return String(lastTimeStamp);
    } else if (var == "HEAP") {
        return String(esp_get_free_heap_size());
    } else if (var == "TASKS") {
        return String(uxTaskGetNumberOfTasks());
    }

    return String();
}

void sendCommandFromRequest(AsyncWebServerRequest *request, uint8_t method) {
    uint8_t intensity = request->getParam("intensity")->value().toInt();
    uint32_t duration = request->getParam("duration")->value().toInt();
    uint16_t id = request->getParam("id")->value().toInt();
    uint64_t timestamp = atoll(request->getParam("timestamp")->value().c_str());

    // If the timestamp is less than the last timestamp, then we should
    // ignore this request
    if (timestamp < lastTimeStamp) {
        request->send(200, "text/plain", "Old Request");
        return;
    }

    lastTimeStamp = timestamp;
    sendCommandToShocker(id, method, intensity, duration);
}

void setupWebServer() {
    server.serveStatic("/res", LittleFS, "/res/");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Send index.html as text
        request->send(LittleFS, "/index.html", "text/html", false);
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/settings.html", "text/html", false);
    });

    server.on("/slider", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/slider.html", "text/html", false);
    });

    // server.on("/shockers", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     request->send(LittleFS, "/shockers.html", "text/html", false);
    // });

    server.on("/shockers/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/shockersconfig.html", "text/html", false);
    });

    server.on("/shockersconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/shockersconfig.html", "text/html", false);
    });

    // TODO: Give these a better endpoint
    server.on("/shock", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("intensity") && request->hasParam("duration") &&
            request->hasParam("id") && request->hasParam("timestamp")) {
            sendCommandFromRequest(request, SHOCK);
            // Return response with how much memory is free
            request->send(200, "text/plain", String(esp_get_free_heap_size()));
            // request->send(200, "text/plain", "Shock");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/vibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("intensity") && request->hasParam("duration") &&
            request->hasParam("id") && request->hasParam("timestamp")) {
            sendCommandFromRequest(request, VIBE);

            request->send(200, "text/plain", String(esp_get_free_heap_size()));
            // request->send(200, "text/plain", "Vibrate");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // Endpoint: /shockers/amount
    // Returns the amount of shockers
    server.on("/shockers/amount", HTTP_GET, [](AsyncWebServerRequest *request) {
        xSemaphoreTake(shockerListMutex, portMAX_DELAY);
        uint8_t amount = shockerList.size();
        xSemaphoreGive(shockerListMutex);
        request->send(200, "text/plain", String(amount));
    });

    // Endpoint: /shockers
    // Returns a JSON array of shockers
    server.on("/shockers", HTTP_GET, [](AsyncWebServerRequest *request) {
        xSemaphoreTake(shockerListMutex, portMAX_DELAY);
        String json = shockerList.toJson();
        xSemaphoreGive(shockerListMutex);
        request->send(200, "application/json", json);
    });

    // Endpoint: /shockers/new
    // Adds a new shocker
    // Params:
    //  - name: The name of the shocker
    //  - type: The type of the shocker
    //  - id: The id of the shocker
    //  - shock_multiplier: The shock multiplier of the shocker (float)
    //  - vibe_multiplier: The vibe multiplier of the shocker (float)
    //  - role: The role of the shocker
    server.on("/shockers/new", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("name", true) &&
            request->hasParam("role", true) &&
            request->hasParam("type", true) && request->hasParam("id", true) &&
            request->hasParam("shock_multiplier", true) &&
            request->hasParam("vibe_multiplier", true)) {
            String name = request->getParam("name", true)->value();
            String role = request->getParam("role", true)->value();
            uint8_t type = request->getParam("type", true)->value().toInt();
            uint16_t id = request->getParam("id", true)->value().toInt();
            float shock_multiplier =
                request->getParam("shock_multiplier", true)->value().toFloat();
            float vibe_multiplier =
                request->getParam("vibe_multiplier", true)->value().toFloat();

            xSemaphoreTake(shockerListMutex, portMAX_DELAY);
            uint8_t result =
                shockerList.newShocker(id, shock_multiplier, vibe_multiplier,
                                       name.c_str(), role.c_str(), type);
            xSemaphoreGive(shockerListMutex);

            if (result == SHOCKER_ADD_EXISTS) {
                request->send(400, "text/plain", "Shocker already exists");
                return;
            } else if (result == SHOCKER_ADD_ERROR) {
                request->send(400, "text/plain", "Error adding shocker");
                return;
            }

            request->send(200, "text/plain", "Shocker added");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // Endpoint: /shockers/delete
    // Removes a shocker
    // Params:
    //  - id: The id of the shocker
    server.on(
        "/shockers/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (request->hasParam("id", true)) {
                uint16_t id = request->getParam("id", true)->value().toInt();

                xSemaphoreTake(shockerListMutex, portMAX_DELAY);
                uint8_t result = shockerList.deleteShocker(id);
                xSemaphoreGive(shockerListMutex);

                if (result == SHOCKER_REMOVE_NOT_FOUND) {
                    request->send(400, "text/plain", "Shocker not found");
                    return;
                } else if (result == SHOCKER_REMOVE_ERROR) {
                    request->send(400, "text/plain", "Error removing shocker");
                    return;
                }

                request->send(200, "text/plain", "Shocker deleted");
            } else {
                request->send(400, "text/plain", "Bad Request");
            }
        });

    // Endpoint: /shockers/update
    // Updates a shocker
    // Params:
    //  - name: The name of the shocker
    //  - type: The type of the shocker
    //  - id: The id of the shocker
    //  - shock_multiplier: The shock multiplier of the shocker (float)
    //  - vibe_multiplier: The vibe multiplier of the shocker (float)
    server.on(
        "/shockers/update", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (request->hasParam("name", true) &&
                request->hasParam("role", true) &&
                request->hasParam("type", true) &&
                request->hasParam("id", true) &&
                request->hasParam("shock_multiplier", true) &&
                request->hasParam("vibe_multiplier", true)) {
                String name = request->getParam("name", true)->value();
                String role = request->getParam("role", true)->value();
                uint8_t type = request->getParam("type", true)->value().toInt();
                uint16_t id = request->getParam("id", true)->value().toInt();
                float shock_multiplier =
                    request->getParam("shock_multiplier", true)
                        ->value()
                        .toFloat();
                float vibe_multiplier =
                    request->getParam("vibe_multiplier", true)
                        ->value()
                        .toFloat();

                xSemaphoreTake(shockerListMutex, portMAX_DELAY);
                uint8_t result = shockerList.updateShocker(
                    id, shock_multiplier, vibe_multiplier, name.c_str(),
                    role.c_str(), type);
                xSemaphoreGive(shockerListMutex);

                if (result == SHOCKER_UPDATE_NOT_FOUND) {
                    request->send(400, "text/plain", "Shocker not found");
                    return;
                } else if (result == SHOCKER_UPDATE_ERROR) {
                    request->send(400, "text/plain", "Error updating shocker");
                    return;
                }

                request->send(200, "text/plain", "Shocker updated");
            } else {
                request->send(400, "text/plain", "Bad Request");
            }
        });

    server.on("/osc/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/osc.html", "text/html");
    });

    // Endpoint: /osc/get
    // Returns the osc settings as JSON
    server.on("/osc/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = oscHandler.toJson();
        request->send(200, "application/json", json);
    });

    // Endpoint: /osc/set
    // Saves new OSC settings
    // Params:
    //  - host: The ip of the OSC server
    //  - port: The port of the OSC server
    //  - path: The prefix of the path we send to over OSC
    server.on("/osc/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("host", true) &&
            request->hasParam("port", true) &&
            request->hasParam("path", true) &&
            request->hasParam("enabled", true) &&
            request->hasParam("sendHighest", true) &&
            request->hasParam("sendRelative", true)) {
            String host = request->getParam("host", true)->value();
            uint16_t port = request->getParam("port", true)->value().toInt();
            String path = request->getParam("path", true)->value();
            bool enabled = false;
            bool sendHighest = false;
            bool sendRelative = false;
            if (request->getParam("enabled", true)->value() == "true") {
                enabled = true;
            }
            if (request->getParam("sendHighest", true)->value() == "true") {
                sendHighest = true;
            }
            if (request->getParam("sendRelative", true)->value() == "true") {
                sendRelative = true;
            }

            oscHandler.setConfig(port, host.c_str(), path.c_str(), enabled,
                                 sendHighest, sendRelative);

            request->send(200, "text/plain", "OSC settings saved");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/bpio/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/bpio.html", "text/html");
    });

    // Endpoint: /bpio/get
    // Returns the bpio settings as JSON
    server.on("/bpio/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = bpioHandler.toJson();
        request->send(200, "application/json", json);
    });

    // Endpoint: /bpio/set
    // Saves new BPIO settings
    // Params:
    //  - host: The ip of the BPIO server
    //  - port: The port of the BPIO server
    //  - enabled: Whether or not to enable BPIO
    server.on("/bpio/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("host", true) &&
            request->hasParam("port", true) &&
            request->hasParam("enabled", true)) {
            String host = request->getParam("host", true)->value();
            uint16_t port = request->getParam("port", true)->value().toInt();
            bool enabled = false;
            if (request->getParam("enabled", true)->value().equals("true")) {
                enabled = true;
            }

            bpioHandler.setConfig(port, host.c_str(), enabled);

            request->send(200, "text/plain", "BPIO settings saved");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // TODO: Redo these to use the new function taskloop
    // server.on("/beep", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     if (request->hasParam("id")) {
    //         uint16_t id = request->getParam("id")->value().toInt();
    //         request->send(200, "text/plain", "Beep");
    //     } else {
    //         request->send(400, "text/plain", "Bad Request");
    //     }
    // });

    // server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     if (request->hasParam("state") && request->hasParam("id")) {
    //         bool state = request->getParam("state")->value().toInt();
    //         uint16_t id = request->getParam("id")->value().toInt();
    //         request->send(200, "text/plain", "Led");
    //     } else {
    //         request->send(400, "text/plain", "Bad Request");
    //     }
    // });

    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(esp_get_free_heap_size()));
    });

    server.on("/tasks", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(uxTaskGetNumberOfTasks()));
    });

    // Page for uploading new firmware
    // server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     request->send(LittleFS, "/update.html", "text/html");
    // });

    // TODO: When making the L-Remote style controls with a slider,
    // make sure to throttle inputs coming at at the webserver level

    ws.onEvent(onEvent);

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);

    // Updating firmware is accessible at "<IP Address>/update" in browser
    AsyncElegantOTA.begin(&server);

    websocketServer.addHandler(&ws);

    websocketServer.begin();
    server.begin();
}
