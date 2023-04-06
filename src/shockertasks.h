#pragma once
#include <Arduino.h>
#include "defines.h"
#include "GenericShocker.h"
#include "Petrainer.h"
#include "rgb.h"
#include "taskloops.h"
#include "osc.h"

extern GenericShocker genericShocker;
extern Petrainer petrainerShocker;
extern basicRGB rgb;
extern SemaphoreHandle_t functionListMutex;
extern OSCHandler oscHandler;

float calculateIntensity(void *parameter) {
    ShockerActionParameters *params = (ShockerActionParameters *)parameter;
    float out_intensity = params->intensity;
    float max_intensity = 100;
    if (params->method == SHOCK) {
        
        // Clamp the multiplier to between 0 and 1
        if (params->shock_multiplier > 1 || params->shock_multiplier < 0) {
            params->shock_multiplier = 1;
        }
        out_intensity = params->intensity * params->shock_multiplier;
        max_intensity = max_intensity * params->shock_multiplier;
    } else if (params->method == VIBE) {
        
        if (params->vibe_multiplier > 1 || params->vibe_multiplier < 0) {
            params->vibe_multiplier = 1;
        }

        out_intensity = params->intensity * params->vibe_multiplier;
        max_intensity = max_intensity * params->vibe_multiplier;
    }
    if (params->absolute_intensity) {
        out_intensity = params->intensity;
        if (out_intensity > max_intensity) {
            out_intensity = max_intensity;
        }
    }
    return out_intensity;
}

// These are the functions that are looped in core 1
void genericShockerTask(void *parameter) {
    ShockerActionParameters *params = (ShockerActionParameters *)parameter;

    uint16_t id = params->id;
    uint8_t method = params->method;
    const char *role = params->role;
    float intensity = 0;
    float multiplier = 1;

    rgb.rgbFromMethod(method);

    if (params->cutoff_now) {
        intensity = 0;
    } else {
        intensity = calculateIntensity(params);
    }

    if (params->method == SHOCK) {
        multiplier = params->shock_multiplier;
    } else if (params->method == VIBE) {
        multiplier = params->vibe_multiplier;
    }
    
    genericShocker.send(id, method, intensity);

    oscHandler.publishMethodAndIntensity(id, method, intensity, role, multiplier);
}

void petrainerShockerTask(void *parameter) {
    ShockerActionParameters *params = (ShockerActionParameters *)parameter;

    uint16_t id = params->id;
    uint8_t method = params->method;
    const char *role = params->role;
    float intensity = 0;
    float multiplier = 1;

    rgb.rgbFromMethod(method);

    if (params->cutoff_now) {
        intensity = 0;
    } else {
        intensity = calculateIntensity(params);
    }

    if (params->method == SHOCK) {
        multiplier = params->shock_multiplier;
    } else if (params->method == VIBE) {
        multiplier = params->vibe_multiplier;
    }

    petrainerShocker.send(id, method, intensity);

    oscHandler.publishMethodAndIntensity(id, method, intensity, role, multiplier);
}