@echo off

REM Check if version number argument was passed
IF "%~1"=="" (
    echo Usage: %~nx0 version
    exit /B 1
)

SET version=%~1

REM Check if output directory exists. If not, create it
IF NOT EXIST "output\" (
    mkdir output
)

REM Copy files with new versioned names
copy ".pio\build\esp32\littlefs.bin" "output\update-esp32-filesystem-%version%.bin"
copy ".pio\build\esp32\firmware.bin" "output\update-esp32-firmware-%version%.bin"
copy ".pio\build\esp32-s3-devkitc-1\littlefs.bin" "output\update-esp32-s3-filesystem-%version%.bin"
copy ".pio\build\esp32-s3-devkitc-1\firmware.bin" "output\update-esp32-s3-firmware-%version%.bin"

REM Execute esptool commands and copy the output files with new versioned names
esptool.py --chip esp32 merge_bin -o full_esp32_with_fs.bin --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 .pio\build\esp32\bootloader.bin 0x8000 .pio\build\esp32\partitions.bin 0x10000 .pio\build\esp32\firmware.bin 0x310000 .pio\build\esp32\littlefs.bin
copy "full_esp32_with_fs.bin" "output\full-esp32-with-fs-%version%.bin"

esptool.py --chip esp32s3 merge_bin -o full_esp32-s3_with_fs.bin --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0000 .pio\build\esp32-s3-devkitc-1\bootloader.bin 0x8000 .pio\build\esp32-s3-devkitc-1\partitions.bin 0x10000 .pio\build\esp32-s3-devkitc-1\firmware.bin 0x670000 .pio\build\esp32-s3-devkitc-1\littlefs.bin
copy "full_esp32-s3_with_fs.bin" "output\full-esp32-s3-with-fs-%version%.bin"

echo All files updated and copied to output folder with version: %version%
