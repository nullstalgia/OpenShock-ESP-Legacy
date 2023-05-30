# OpenShock Installation and Setup

## Firmware Installation

- Download the latest "full" binary (.bin) from the [releases page](https://github.com/nullstalgia/OpenShock-ESP/releases) for your device.

[OpenShock Core](https://github.com/nullstalgia/OpenShock-Hardware/tree/main/Core) and other ESP32-S3 boards: Use the ESP32-S3 binaries

[PiShock Lite](http://pishock.com/) and other ESP32 boards: Use the ESP32 binaries

- In **Chrome** (most other browsers do not yet have WebSerial), go to Espressif's [Web ESPTool Page](https://espressif.github.io/esptool-js/).
- Click Connect and select your device from the list.
- Change the address to flash to `0x0`
- Click the "Choose File" button and select the .bin file you downloaded.
- Click Flash and wait for the process to complete.

## WiFi Setup

- With your phone or computer, connect to the "OpenShock" WiFi network emitted by the device after firmware installation.
- If it does not take you to the setup page automatically, navigate to `192.168.4.1` in your browser.
- Select "Setup Wifi", choose your WiFi network from the list, and enter the password.
- If the device was able to connect, the "OpenShock" WiFi network will disappear and the device will reset.
- ***!! Note !!*** - As of writing, due to a small bug, you will need to switch off and on your device after setting up the WiFi for the first time. This happens because the WiFi server accidentally holds onto port 80, but a quick power cycle will clear it out until the bug is squashed for good.

## Setting up/Pairing shockers

- Navigate to [openshock.local](http://openshock.local/) in your browser, if the device is connected to your network, you should should be able to use the hyperlinks.
    - If you are unable to connect to the device via that link, you can find the IP of the device with nmap, reading the device's USB Serial output, checking your router's Connected Devices, etc.
- Click on "[Devices](http://openshock.local/)" in the top menu.
- The top of the page has the form for adding new shockers. You'll need to fill out all parts of it for each shocker.
    -  ID: The ID the shocker listens for. You can click the dice next to the field to generate a random ID. If you are coming from an existing shocker solution, you can use the same IDs to avoid needing to pair your shockers again.
    - Shocker Type:
        - [Generic. Small shocker, common on Amazon and AliExpress.](https://github.com/nullstalgia/OpenShock-ESP/assets/20761757/6a1eb657-0f6b-4dea-ac97-c88486024e8c)
        - [Petrainers. Known working with PET998DB, PET998DRU models. Others may work as well, need user feedback.](https://github.com/nullstalgia/OpenShock-ESP/assets/20761757/d422439c-fa78-4055-bf4d-f8091790440b)
    - Maximum shock and vibe intensities: Sliders from 0% to 100%, these will take in incoming shock commands and scale them by this amount. This is especially helpful for turning down body parts that are more sensitive than others.
        - Example 1: Command of 100% intensity to a shocker with a maximum intensity of 50% will result in a shock of 50% intensity.
        - Example 2: Command of 50% intensity to a shocker with a maximum intensity of 100% will result in a shock of 50% intensity.
    - Name: The name of the shocker, just used for display purposes.
    - Role: Where on your body the shocker is located. This is used to abstract away the actual IDs of the shockers, so you can just send "LUA" for "Left Upper Arm" instead of "Shocker 4032". There are also some extra "Custom" roles in case we missed any.
- After clicking "Submit", the shocker will be added to the list below, but has not yet been paired.
- To pair a shocker, you must power the shocker on and then hold the power button down until the shocker beeps. While the shocker is in the pairing mode, click "Test/Pair" next to the shocker in the list. If the pairing was successful, the shocker will beep again/vibrate to indicate it has been paired.

## Other Settings

- [Settings](http://openshock.local/settings) Page
- OSC (Output Only)
    - OpenShock sends OSC packets on reciving commands, useful for shocker-reactive props and clothing on VRChat avatars.
    - OSC Port: The port to send OSC packets to. Default is `9000`.
    - OSC Host: The host IP address to send OSC packets to.
    - OSC Path: The prefix of the address OSC packets will be sent to. Default is `/avatar/parameters/OpenShock/Input/`
    - Enabled: Whether or not to send OSC packets.
    - Send highest intensity: Each time OSC is sent out, also send out the current highest shocker value to `/High/`
    - Send relative intensity: Each time OSC is sent out, instead of sending the current intensity from `0.0` to `1.0`, send the intensity relative to the shocker's maximum intensity. For example, if the shocker's maximum intensity is 50%, and the sent intensity is 25%, the resulting OSC packet will contain `0.5`.

    - ### Data from OSC:
    - All addresses are prefixed with the aforementioned OSC Path.
    - .../\<ShockerID>/Intensity - The current intensity of the shocker, float, `0.0` - `1.0`
    - .../\<ShockerID>/Method - The current "method" of the shocker, int, `1` - Shock, `2` - Vibrate
    - ### Dittos (same data, different address):
    - .../\<ShockerRole>/ (Same, but with the role instead of ID)
    - .../High/ (The current highest intensity of all shockers)


- BP.IO (Unfinished)
    - Allows reciving shock/vibrate commands from bp.io-connected sources.