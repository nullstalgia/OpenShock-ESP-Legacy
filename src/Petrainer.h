#pragma once
#include <Arduino.h>
#include "RMTHandler.h"

class Petrainer {
    private:
        // Methods:
        // 0: Nothing
        // 1: Shock
        // 2: Vibration
        // 3: Beep

        const uint8_t encodedMethod[4] = {0, 0x7E, 0xBE, 0xDE};
        const uint8_t decodedMethod[4] = {0, 1, 2, 4};
        const uint16_t INIT[2] = {750, 750};
        const uint16_t ZERO[2] = {200, 750};
        const uint16_t ONE[2] = {200, 1500};
        const uint16_t PAUSE[2] = {200, 7000};
        RMTHandler* rmt = NULL;

    public:
        Petrainer(RMTHandler* rmt) {
            this->rmt = rmt;
        }
        rmt_data_t* timing_convert(uint8_t* data, int data_len, rmt_data_t* timings, uint8_t* timings_len) {
            int idx = 0;

            timings[idx].duration0 = INIT[0];
            timings[idx].level0 = 1;
            timings[idx].duration1 = INIT[1];
            timings[idx].level1 = 0;
            idx++;

            for (int i = 0; i < data_len; i++) {
                uint8_t byte_val = data[i];
                for (int j = 7; j >= 0; j--) {
                    uint8_t bit_pos = byte_val & (1 << j);
                    timings[idx].duration0 = bit_pos ? ONE[0] : ZERO[0];
                    timings[idx].level0 = 1;
                    timings[idx].duration1 = bit_pos ? ONE[1] : ZERO[1];
                    timings[idx].level1 = 0;
                    idx++;
                }
            }

            timings[idx].duration0 = PAUSE[0];
            timings[idx].level0 = 1;
            timings[idx].duration1 = PAUSE[1];
            timings[idx].level1 = 0;
            idx++;

            *timings_len = idx;
            return timings;
        }

        void generate_signal(uint16_t shockerid, uint8_t method, uint8_t intensity, rmt_data_t* timings, uint8_t* timings_len) {
            uint8_t data[5];
            data[0] = 0x80 | decodedMethod[method];
            data[1] = highByte(shockerid);
            data[2] = lowByte(shockerid);
            data[3] = intensity;
            data[4] = encodedMethod[method];

            timing_convert(data, 5, timings, timings_len);
        }  

        void send(uint16_t shockerid, uint8_t method, uint8_t intensity) {
            rmt_data_t timings[100];
            uint8_t timings_len;
            generate_signal(shockerid, method, intensity, timings, &timings_len);
            rmt->send(timings, timings_len, 2);
        }   
};
