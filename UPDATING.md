# Updating your OpenShock

## ! Warning !

Updating your OpenShock will reset all OpenShock settings and paired shockers. You will need to add your shockers and set up your OSC/BPIO settings again.

## Downloading the right binaries

If just starting out or updating via USB, you need the files labelled "full" which include the filesystem and firmware binaries in one package.

If you are already using OpenShock and want to update Over-the-Air, you can do so by uploading binaries to a page hosted by the device, you will need the "update" files labelled "firmware" and "filesystem" for your device.

[OpenShock Core](https://github.com/nullstalgia/OpenShock-Hardware/tree/main/Core) and other ESP32-S3 boards: Use the ESP32-S3 binaries

[PiShock Lite](http://pishock.com/) and other ESP32 boards: Use the ESP32 binaries

## Updating via OTA

If you are already using OpenShock and want to update Over-the-Air, you can do so by uploading binaries to a page hosted by the device

!! The order of the file uploads is significant, failure to upload the files in the correct order might result in you needing to reflash the device via USB !!

1. Go to the update page: [openshock.local/update](http://openshock.local/update) 
2. Upload the Filesystem binary first
3. Upload the Firmware binary second
4. Done! Go back to the [Devices](http://openshock.local/shockers/config) page to re-add your shockers.


## Updating via USB

- Download the latest "full" binary (.bin) from the [releases page](https://github.com/nullstalgia/OpenShock-ESP/releases) for your device.
- In **Chrome** (most other browsers do not yet have WebSerial), go to Espressif's [Web ESPTool Page](https://espressif.github.io/esptool-js/).
- Click Connect and select your device from the list.
- Change the address to flash to `0x0`
- Click the "Choose File" button and select the .bin file you downloaded.
- Click Flash and wait for the process to complete.