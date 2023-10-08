#pragma once
#include <Arduino.h>
#include "esp32-hal.h"

class RMTHandler {
   public:
    // Set up the RMT transmitter
    rmt_obj_t* rmtSendObject = NULL;
    bool init(uint8_t gpio) {

        if ((rmtSendObject = rmtInit(gpio, RMT_TX_MODE, RMT_MEM_192)) == NULL) {
            debugPrintln("Shocker: init sender failed\n");
            return false;
        } else {
            float realTick = rmtSetTick(rmtSendObject, 1000);
            debugPrintf("Shocker: real tick set to: %fns\n", realTick);
            return true;
        }
    }

    void send(rmt_data_t* pulses, uint8_t timings_len, uint8_t repeat) {
        for (int i = 0; i < repeat; i++) {
            send(pulses, timings_len);
        }
    }

    void send(rmt_data_t* pulses, uint8_t timings_len) {
        rmtWriteBlocking(rmtSendObject, pulses, timings_len);
    }
};