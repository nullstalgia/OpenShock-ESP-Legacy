#pragma once
#include <Arduino.h>
#include "RMTHandler.h"

class GenericShocker {
    private:
        // Methods:
        // 0: Nothing
        // 1: Shock
        // 2: Vibration
        // 3: Beep
        // 4: Light
        const uint8_t decodedMethod[5] = {0, 1, 2, 3, 4};
        const uint16_t INIT[2] = {1400, 800};
        const uint16_t ZERO[2] = {300, 800};
        const uint16_t ONE[2] = {800, 300};
        RMTHandler* rmt = NULL;

    public:
        GenericShocker(RMTHandler* rmt) {
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

            for (int i = 0; i < 3; i++) {
                timings[idx].duration0 = ZERO[0];
                timings[idx].level0 = 1;
                timings[idx].duration1 = ZERO[1];
                timings[idx].level1 = 0;
                idx++;
            }

            *timings_len = idx;
            return timings;
        }

        void generate_signal(uint16_t shockerid, uint8_t method, uint8_t intensity, rmt_data_t* timings, uint8_t* timings_len) {
            uint8_t data[5];
            data[0] = highByte(shockerid);
            data[1] = lowByte(shockerid);
            // Technically we don't need to do this, but it keeps consistency with the Petrainer class
            data[2] = decodedMethod[method];
            data[3] = min(intensity, (uint8_t)99);

            uint16_t checksum = 0;
            for (int i = 0; i < 4; i++) {
                checksum += data[i];
            }
            data[4] = checksum & 0xFF;

            timing_convert(data, 5, timings, timings_len);
        }  

        void send(uint16_t shockerid, uint8_t method, uint8_t intensity, uint32_t duration) {
            rmt_data_t timings[100];
            uint8_t timings_len;
            generate_signal(shockerid, method, intensity, timings, &timings_len);

            // Keep sending the command for the duration in milliseconds
            uint32_t start = millis();
            while (millis() - start < duration) {
                rmt->send(timings, timings_len, 2);
            }
            
            // Send a stop command
            generate_signal(shockerid, method, 0, timings, &timings_len);
            rmt->send(timings, timings_len, 2);
        }

        void send(uint16_t shockerid, uint8_t method, uint8_t intensity) {
            rmt_data_t timings[100];
            uint8_t timings_len;
            generate_signal(shockerid, method, intensity, timings, &timings_len);
            rmt->send(timings, timings_len, 2);
        }   
};
