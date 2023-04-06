#pragma once
#include <Arduino.h>
#include "defines.h"
#include "shockertasks.h"

#include "FS.h"
#include <LittleFS.h>

#define FS LittleFS

#include <ArduinoJson.h>

uint8_t amountInFolder(const char *dirname) {
    uint8_t amount = 0;
    if (FS.exists(dirname)) {
        File root = FS.open(dirname);
        File file = root.openNextFile();
        while (file) {
            // if (file.isDirectory())
            amount++;
            file = root.openNextFile();
        }
        root.close();
    }
    return amount;
}

uint8_t getIndexOfFileInDir(const char *dir, const char *filename) {
    File root = FS.open(dir);
    File file = root.openNextFile();
    uint8_t amount = 0;
    while (file) {
        if (strcmp(filename, file.name()) == 0) {
            file.close();
            return amount;
        }
        amount++;
        file = root.openNextFile();
    }
    root.close();
    return 255;
}

// Count the amount of shockers in /shockers
uint8_t shockerConfigCount() {
    uint8_t amount = amountInFolder("/shockers");
    return amount;
}

struct Shocker {
    uint16_t id;
    float shock_multiplier;
    float vibe_multiplier;
    uint8_t type;
    char name[SHOCKER_NAME_LENGTH];
    char role[SHOCKER_ROLE_LENGTH];
    void (*send)(void *);
};

struct StringComparator {
  bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};


// TODO: Have a doc detailing where to add new shocker types
// to different parts of the code
enum ShockerType {
    SHOCKER_TYPE_GENERIC = 0,
    SHOCKER_TYPE_PETRAINER,
};

enum ShockerListResult {
    SHOCKER_ADD_SUCCESS = 0,
    SHOCKER_ADD_ERROR,
    SHOCKER_ADD_EXISTS,

    SHOCKER_REMOVE_SUCCESS,
    SHOCKER_REMOVE_ERROR,
    SHOCKER_REMOVE_NOT_FOUND,

    SHOCKER_UPDATE_SUCCESS,
    SHOCKER_UPDATE_ERROR,
    SHOCKER_UPDATE_NOT_FOUND,
};

// Class for storing several shocker's parameters
// The class contains each shocker's ID, the maximum intensity, the shocker
// type, and a pointer to the shocker type's send() function
class ShockerList {
   public:
    Shocker shockers[MAX_SHOCKERS];
    uint8_t amount = 0;

    std::map<const char*, uint16_t, StringComparator> shockerRoleMap;

    uint8_t addShocker(uint16_t id, float shock_multiplier, float vibe_multiplier, const char* name, const char* role, uint8_t type,
                    void (*send)(void *)) {
        if (amount >= MAX_SHOCKERS) {
            return SHOCKER_ADD_ERROR;
        }
        for (uint8_t i = 0; i < amount; i++) {
            if (shockers[i].id == id) {
                return SHOCKER_ADD_EXISTS;
            }
        }
        shockers[amount].id = id;
        shockers[amount].shock_multiplier = shock_multiplier;
        shockers[amount].vibe_multiplier = vibe_multiplier;
        shockers[amount].type = type;
        strcpy(shockers[amount].name, name);
        strcpy(shockers[amount].role, role);
        shockers[amount].send = send;
        amount++;
        reorderShockers();
        return SHOCKER_ADD_SUCCESS;
    };

    uint8_t newShocker(uint16_t id, float shock_multiplier, float vibe_multiplier, const char* name, const char* role, uint8_t type) {
        uint8_t result = addShocker(id, shock_multiplier, vibe_multiplier, name, role, type, getShockerTaskFromType(type));
        if (result == SHOCKER_ADD_SUCCESS) {
            char filename[20];
            sprintf(filename, "/shockers/%d.json", id);
            File file = FS.open(filename, FILE_WRITE);
            if (!file) {
                debugPrintln("Failed to create file");
                return SHOCKER_ADD_ERROR;
            }
            StaticJsonDocument<512> shockerDoc;
            shockerDoc["id"] = id;
            shockerDoc["shock_multiplier"] = shock_multiplier;
            shockerDoc["vibe_multiplier"] = vibe_multiplier;
            shockerDoc["type"] = type;
            shockerDoc["name"] = name;
            shockerDoc["role"] = role;
            if (serializeJson(shockerDoc, file) == 0) {
                debugPrintln("Failed to write to file");
                return SHOCKER_ADD_ERROR;
            }
            file.close();
        }
        reorderShockers();
        return result;
    };

    uint8_t updateShocker(uint16_t id, float shock_multiplier, float vibe_multiplier, const char* name, const char* role, uint8_t type) {
        Shocker *shocker = getShocker(id);
        if (shocker != NULL) {
            shocker->shock_multiplier = shock_multiplier;
            shocker->vibe_multiplier = vibe_multiplier;
            shocker->type = type;
            strcpy(shocker->name, name);
            shocker->send = getShockerTaskFromType(type);
            char filename[20];
            sprintf(filename, "/shockers/%d.json", id);
            File file = FS.open(filename, FILE_WRITE);
            if (!file) {
                debugPrintf("Failed to create file for shocker %d\n", id);
                return SHOCKER_UPDATE_ERROR;
            }
            StaticJsonDocument<512> shockerDoc;
            shockerDoc["id"] = id;
            shockerDoc["shock_multiplier"] = shock_multiplier;
            shockerDoc["vibe_multiplier"] = vibe_multiplier;
            shockerDoc["type"] = type;
            shockerDoc["name"] = name;
            shockerDoc["role"] = role;
            if (serializeJson(shockerDoc, file) == 0) {
                debugPrintln("Failed to write to file");
                return SHOCKER_UPDATE_ERROR;
            }
            file.close();
        } else {
            return SHOCKER_UPDATE_NOT_FOUND;
        }
        reorderShockers();
        return SHOCKER_UPDATE_SUCCESS;
    };

    void removeShocker(uint16_t id) {
        for (uint8_t i = 0; i < amount; i++) {
            if (shockers[i].id == id) {
                for (uint8_t j = i; j < amount - 1; j++) {
                    shockers[j] = shockers[j + 1];
                }
                amount--;
                break;
            }
        }
    };

    uint8_t deleteShocker(uint16_t id) {
        Shocker *shocker = getShocker(id);
        if (shocker != NULL) {
            removeShocker(id);
            char filename[20];
            sprintf(filename, "/shockers/%d.json", id);
            if (!FS.remove(filename)) {
                debugPrintf("Failed to delete file for shocker %d\n", id);
                return SHOCKER_REMOVE_ERROR;
            }
            return SHOCKER_REMOVE_SUCCESS;
        }
        return SHOCKER_REMOVE_NOT_FOUND;
    };

    void clear() { amount = 0; };

    Shocker *getShocker(uint16_t id) {
        for (uint8_t i = 0; i < amount; i++) {
            if (shockers[i].id == id) {
                return &shockers[i];
            }
        }
        return NULL;
    };

    Shocker *getShockerByIndex(uint8_t index) {
        if (index < amount) {
            return &shockers[index];
        }
        return NULL;
    };

    Shocker *getShockerByRole(const char* role) {
        return getShocker(shockerRoleMap[role]);
    };

    uint16_t getShockerIDByRole(const char* role) {
        return shockerRoleMap[role];
    };

    // getShockerTaskFromType
    // Returns a pointer to the task function for the given shocker type
    TaskFunction_t getShockerTaskFromType(uint8_t type) {
        switch (type) {
            case SHOCKER_TYPE_GENERIC:
                return genericShockerTask;
            case SHOCKER_TYPE_PETRAINER:
                return petrainerShockerTask;
            default:
                return NULL;
        }
    };

    void readShockerFromFile(uint16_t id) {
        debugPrint("Attempting to read shocker file: ");
        debugPrintln(id);

        char filename[20];
        sprintf(filename, "/shockers/%d.json", id);
        if (FS.exists(filename)) {
            File file = FS.open(filename);
            StaticJsonDocument<512> shockerDoc;
            DeserializationError error = deserializeJson(shockerDoc, file);
            if (error) {
                debugPrintf("Error reading file: %s\n", filename);
                return;
            }
            file.close();
            float shock_multiplier = shockerDoc["shock_multiplier"];
            float vibe_multiplier = shockerDoc["vibe_multiplier"];
            uint8_t type = shockerDoc["type"];
            char name[SHOCKER_NAME_LENGTH];
            strcpy(name, shockerDoc["name"]);
            char role[SHOCKER_ROLE_LENGTH];
            if (shockerDoc.containsKey("role")) {
                strcpy(role, shockerDoc["role"]);
            } else {
                strcpy(role, "");
            }
            addShocker(id, shock_multiplier, vibe_multiplier, name, role, type, getShockerTaskFromType(type));
        }
    };

    void readShockersFromFiles() {
        // For each file in /shockers, get the ID from the filename and
        // run readShockerFromFile()
        // Example filename: 4287.json

        if (FS.exists("/shockers")) {
            File root = FS.open("/shockers");
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    char filename[20];
                    strcpy(filename, file.name());
                    char *idStr = strtok(filename, ".");
                    uint16_t id = atoi(idStr);
                    readShockerFromFile(id);
                }
                file = root.openNextFile();
            }
            root.close();
        } else {
            // Make the /shockers directory
            FS.mkdir("/shockers");
            readShockersFromFiles();
        }
    };

    void reorderShockers() {
        // Bubble sort shockers by ID
        if (amount > 1) {
            for (uint8_t i = 0; i < amount - 1; i++) {
                for (uint8_t j = 0; j < amount - i - 1; j++) {
                    if (shockers[j].id > shockers[j + 1].id) {
                        Shocker temp = shockers[j];
                        shockers[j] = shockers[j + 1];
                        shockers[j + 1] = temp;
                    }
                }
            }
        }

        // Map shocker IDs to their given roles
        // First, clear any existing entries
        shockerRoleMap.clear();
        for (uint8_t i = 0; i < amount; i++) {
            if (strlen(shockers[i].role) > 0)
                shockerRoleMap[shockers[i].role] = shockers[i].id;
        }
    }

    uint8_t size() { return amount; };

    String toJson() {
        StaticJsonDocument<2048> doc;
        JsonArray shockersArray = doc.createNestedArray("shockers");
        for (uint8_t i = 0; i < amount; i++) {
            JsonObject shocker = shockersArray.createNestedObject();
            shocker["id"] = shockers[i].id;
            shocker["shock_multiplier"] = shockers[i].shock_multiplier;
            shocker["vibe_multiplier"] = shockers[i].vibe_multiplier;
            shocker["name"] = shockers[i].name;
            shocker["role"] = shockers[i].role;
            shocker["type"] = shockers[i].type;
        }
        String json;
        serializeJson(doc, json);
        return json;
    }
};
