# OpenShock

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K3K3JDV5G)

OpenShock is an open source solution for controlling shock collars with an ESP32 or it's derivatives. It is designed to be used in combination with apps like VRChat, but can be used outside of VRChat as well.

The firmware is written in the Arduino framework under PlatformIO. Other technologies include Python, JS (jQuery), and Bootstrap.

## Features

#### [Showcase Video](https://streamable.com/wfniv1)

- Low latency 
- Free and open source
- Able to send unique commands to up to 5 collars simultaneously
- Easy to build VRChat props for!
- [Companion Script for VRChat](https://github.com/nullstalgia/OpenShock-VRC)
- OSC output built in! (Open Sound Control)
- Emergency Stop functionality (on boards with E-Stop buttons)
- Compatible with existing shock collar solutions

# How to use

## Hardware Requirements

Prebuilt:

- ESP32+ with a 433Mhz transmitter ([OpenShock Core](https://github.com/nullstalgia/OpenShock-Hardware/tree/main/Core), PiShock Lite)

DIY:

- ESP32+
- 433Mhz transmitter (connect Signal to IO15)
- 10k resistor (Signal pulldown to GND)

(ESP32 and ESP32-S3 have been tested and work. Other chips are unknown.)

## Usage

1. Flash the firmware to your ESP32 using PlatformIO or the supplied binaries.<sup>If flashing via PlatformIO, don't forget to upload the filesystem.</sup>
2. Connect device to your WiFi by connecting to the "OpenShock" AP and navigate to "192.168.4.1" in your browser if not brought to the setup page automatically.
3. Choose your SSID and enter password. The unit will reset. (Note! There is currently a bug where the WiFi connection server does not let go of port 80, requiring a single power cycle after successful setup.)
4. Navigate to openshock.local <sup>If mdns is not supported on your network, locate the Openshock device via nmap or similar means.</sup>
5. Set up and pair your shockers in Devices (/shockers/config)
6. Enjoy using scripts to control your shockers with low latency!