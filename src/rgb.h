#pragma once
#include <Arduino.h>
#include "defines.h"
// Since every damn RGB library want to use the RMT peripheral, and doesn't actually check if the channel is already in use, we have to do it ourselves.
// This is a very basic RGB library that uses the RMT peripheral NICELY, and is based on the example from the ESP32 Arduino core.
// Mostly stolen from https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/RMT/RMTWriteNeoPixel/RMTWriteNeoPixel.ino

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#define NR_OF_LEDS   1
#define NR_OF_ALL_BITS 24*NR_OF_LEDS

class basicRGB {
    private:
        rmt_obj_t *rmtSendObject = NULL;
        rmt_data_t led_data[NR_OF_ALL_BITS];
        int led_index = 0;
        uint8_t brightness = 255;
        uint8_t pin;
    public:

    void init(uint8_t p) { 
        pin = p;
        if ((rmtSendObject = rmtInit(pin, RMT_TX_MODE, RMT_MEM_64)) == NULL)
        {
            debugPrintln("RGB: init sender failed\n");
        }
        float realTick = rmtSetTick(rmtSendObject, 100);
        debugPrintf("RGB: real tick set to: %fns\n", realTick);
        delay(100);
    }
    void setBrightness(uint8_t b) {
        brightness = b;
    }
    void setGRB(uint8_t g, uint8_t r, uint8_t b) {
            g = (uint8_t)((uint16_t)g * brightness / 255);
            r = (uint8_t)((uint16_t)r * brightness / 255);
            b = (uint8_t)((uint16_t)b * brightness / 255);

            int led, col, bit;
            int i = 0;
            uint8_t colors[3] = { g, r, b };
            for (led=0; led<NR_OF_LEDS; led++) {
                for (col=0; col<3; col++ ) {
                    for (bit=0; bit<8; bit++){
                        if ( (colors[col] & (1<<(7-bit))) && (led == led_index) ) {
                            led_data[i].level0 = 1;
                            led_data[i].duration0 = 8;
                            led_data[i].level1 = 0;
                            led_data[i].duration1 = 4;
                        } else {
                            led_data[i].level0 = 1;
                            led_data[i].duration0 = 4;
                            led_data[i].level1 = 0;
                            led_data[i].duration1 = 8;
                        }
                        i++;
                    }
                }
            }
            // make the led travel in the pannel
            if ((++led_index)>=NR_OF_LEDS) {
                led_index = 0;
            }

            // Send the data
            rmtWriteBlocking(rmtSendObject, led_data, NR_OF_ALL_BITS);
        //delay(100);
    }
    void setRGB(uint8_t r, uint8_t g, uint8_t b) {
        setGRB(g, r, b);
    }
    void rgbFromMethod(uint8_t method){
            if (method == SHOCK) {
            setRGB(255, 0, 0);
        } else if (method == VIBE) {
            setRGB(0, 255, 0);
        } else if (method == BEEP) {
            setRGB(0, 0, 255);
        } else if (method == LIGHT) {
            setRGB(255, 255, 255);
        }
    }
};

#elif defined(CONFIG_IDF_TARGET_ESP32)

// Just a basic LED on/off controller that uses the same methods as the RGB class.
class basicRGB {
    private:
        uint8_t pin;
    public:
        void init(uint8_t p) {
            pin = p;
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
        void setBrightness(uint8_t b) {
            // Do nothing
        }
        void setRGB(uint8_t r, uint8_t g, uint8_t b) {
            digitalWrite(pin, r || g || b);
        }
        void rgbFromMethod(uint8_t method){
            if (method == SHOCK) {
                setRGB(0, 255, 0);
            } else if (method == VIBE) {
                setRGB(255, 0, 0);
            } else if (method == BEEP) {
                setRGB(0, 0, 255);
            } else if (method == LIGHT) {
                setRGB(255, 255, 255);
            }
        }
};

#endif