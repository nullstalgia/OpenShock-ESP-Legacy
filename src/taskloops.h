#pragma once
#include <Arduino.h>
#include "defines.h"
#include "rgb.h"

// Define the maximum number of functions in the list
const int MAX_FUNCTIONS = 10;

// Define a struct to store each function, its expiration time, and a pointer to
// a ShockerActionParameters struct
struct FunctionInfo {
    void (*function)(void *);
    unsigned long expirationTime;
    void *data;
};

// Define an array of function pointers and expiration times
FunctionInfo functionList[MAX_FUNCTIONS];

extern basicRGB rgb;

// Define the index of the next available slot in the function list
int nextFunction = 0;

QueueHandle_t shockerActionQueue;

extern bool emergencyStopped;

bool lastEmergencyStopped = false;

// Add a function to the list with the specified expiration time and data
void addFunction(void (*function)(void *), unsigned long expirationTime,
                 void *data) {
    
    // Go through the current list and see if the ID is already in the list's parameters 
    // If it is, just update the data and expiration time
    uint16_t id = ((ShockerActionParameters *)data)->id;
    for (int i = 0; i < nextFunction; i++) {
        ShockerActionParameters *params = (ShockerActionParameters *)functionList[i].data;
        if (params->id == id) {
            functionList[i].expirationTime = expirationTime;

            // Release the memory allocated for the function's data
            delete params;

            functionList[i].data = data;
            return;
        }
    }
    
    // Make sure there's room in the list
    if (nextFunction >= MAX_FUNCTIONS) {
        return;
    }

    debugPrintf("Adding function for ID: %d\n", id);

    // Add the function, expiration time, and data to the list
    functionList[nextFunction].function = function;
    functionList[nextFunction].expirationTime = expirationTime;
    functionList[nextFunction].data = data;
    nextFunction++;
}

// Remove a function from the list
void removeFunction(int index) {
    
    debugPrintf("Removing function for ID: %d\n", ((ShockerActionParameters *)functionList[index].data)->id);
    
    // Release the memory allocated for the function's data
    delete (ShockerActionParameters *)functionList[index].data;

    // Shift all remaining functions down by one slot
    for (int i = index; i < nextFunction - 1; i++) {
        functionList[i] = functionList[i + 1];
    }
    // Decrement the next available slot index
    nextFunction--;
}

void functionLoop(void *parameter) {
    while (true) {
        // For each new action in the queue, add it to the list
        ShockerActionParameters *params;
        while (xQueueReceive(shockerActionQueue, &params, 0) == pdTRUE) {
            if (emergencyStopped) {
                delete params;
                continue;
            }
            addFunction(params->send_function, params->expiration_time, params);
        }

        // If the emergency stop button was just pressed, stop all functions
        if (emergencyStopped && !lastEmergencyStopped) {
            debugPrintln("Task loop: Emergency stop button pressed");
            for (int i = 0; i < nextFunction; i++) {
                ShockerActionParameters *params =
                    (ShockerActionParameters *)functionList[i].data;

                params->cutoff_now = true;
                functionList[i].function(functionList[i].data);

                // Set RGB to black
                rgb.setRGB(0, 0, 0);

                // Remove the function from the list
                removeFunction(i);
            }
        }

        // Check and call each function in the list
        for (int i = 0; i < nextFunction; i++) {
            if (emergencyStopped) {
                continue;
            }
            functionList[i].function(functionList[i].data);
            // If the function has expired, remove it from the list
            // and send a stop command to the shocker
            if (millis() >= functionList[i].expirationTime) {
                ShockerActionParameters *params =
                    (ShockerActionParameters *)functionList[i].data;

                params->cutoff_now = true;
                functionList[i].function(functionList[i].data);

                // Set RGB to black
                rgb.setRGB(0, 0, 0);

                // Remove the function from the list
                removeFunction(i);
            }
        }

        lastEmergencyStopped = emergencyStopped;

        // Sleep for a short period of time to avoid busy-waiting
        vTaskDelay(10);
    }
}