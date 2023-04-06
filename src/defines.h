#pragma once
#include <Arduino.h>

//#define CONFIG_ASYNC_TCP_RUNNING_CORE 0 

#define SHOCK 1
#define VIBE 2
#define BEEP 3
#define LIGHT 4

#define SHOCKER_ID 4287

#define MAX_SHOCKERS 10
#define SHOCKER_NAME_LENGTH 45
#define SHOCKER_ROLE_LENGTH 6

#define TRANSMITTER_PIN 15
#define RGB_PIN 48
#define STATUS_PIN 35
#define EMERGENCY_STOP_PIN 13

#define LED_PIN 2

#define COMMAND_TIMEOUT 500

#define UPNP_ENABLE              true

#define UPNP_LISTEN_PORT         51569
#define UPNP_LEASE_DURATION      30000 // seconds

#define UPNP_FRIENDLY_NAME       "OpenShock"   // this name will appear in your router port forwarding section

#define MDNS_NAME                "openshock"   // You can access the web interface at http://<MDNS_NAME>.local

#define OTA_PASSWORD             "shockme"
//#define OTA_TIMEOUT              120000 // Time period in ms to allow OTA updates, comment out to disable

#include <WebSerialLite.h>

#define debugPrint(...) Serial.print( __VA_ARGS__ ); WebSerial.print( __VA_ARGS__ ); 
#define debugPrintln(...) Serial.println( __VA_ARGS__ ); WebSerial.println( __VA_ARGS__ ); 
#define debugPrintf(...) Serial.printf( __VA_ARGS__ ); WebSerial.printf( __VA_ARGS__ ); 
struct ShockerActionParameters {
  uint16_t id;
  uint8_t method;
  uint8_t intensity;
  float shock_multiplier;
  float vibe_multiplier;
  bool absolute_intensity;
  uint32_t duration;
  uint32_t expiration_time;
  char role[SHOCKER_ROLE_LENGTH];
  bool cutoff_now = false;
  void (*send_function)(void *);
};


// Stolen from @NicoHood and @st42 (github users) in a discussion about the
// map() function being weird
// https://github.com/arduino/ArduinoCore-API/issues/51#issuecomment-87432953
uint8_t map_float_with_clamp(float x, float in_min, float in_max, uint8_t out_min,
                    uint8_t out_max) {
  // if input is smaller/bigger than expected return the min/max out ranges
  // value
  if (x < in_min)
    return out_min;
  else if (x > in_max)
    return out_max;

  // map the input to the output range.
  // round up if mapping bigger ranges to smaller ranges
  else if ((in_max - in_min) > (out_max - out_min))
    return (x - in_min) * (out_max - out_min + 1) / (in_max - in_min + 1) +
           out_min;
  // round down if mapping smaller ranges to bigger ranges
  else
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}