# Implementation status

## Firmware 2.0 update — 2026-10-25

Continuous PC0/PC1/PC2 recording at 10 kS/s replaces snapshot-on-Stop.
See [recording format and behavior](OSC_V2_RECORDING.md) and
[verification / remaining bench work](RECORDING_VERIFICATION.md).
The historical table below describes the previous feature set; its selectable
high-rate acquisition and snapshot controls are not the current UI/USB profile.

The firmware architecture in the project plan is implemented without an RTOS. Items marked hardware validation require the assembled analog front end, signal generator, SD card, or Windows USB host and cannot be proven by the cross-build alone.

| Area | Firmware status | Remaining bench validation |
|---|---|---|
| HAL/CMSIS and 144 MHz clock | Implemented | Verify clocks with MCO/debugger if desired |
| ILI9341 FSMC + DMA2 Stream1 | Implemented, preserving verified reset/timing | Sustained display/touch interaction |
| XPT2046 + LVGL 9.5 | Implemented | Touch calibration on the installed panel |
| 1/2/3-channel ADC DMA | Implemented | Rate, alignment, ENOB, bandwidth, crosstalk |
| Trigger and measurements | Implemented | Signal-generator accuracy sweep |
| Start/Pause/Resume/Stop and drag | Implemented | Touch workflow acceptance |
| SDIO/FAT16/FAT32/session retention | Implemented | Removal/full-card/power-interruption matrix |
| `.OSC` v1 and CRC validation | Implemented and host contract-tested | Sample-for-sample captured file comparison |
| LSE/RTC UTC sync and filenames | Implemented | LSE and backup-domain behavior |
| WinUSB bulk protocol | Implemented | Windows enumeration and disconnect recovery |
| Six calibration records | Implemented in reserved flash sector 7 | Per-path production calibration |
| Analog schematic/BOM/SPICE | Reference design supplied | Mandatory validation before high voltage |

The Windows GUI remains out of scope. Continuous recording is now implemented;
one-hour and power-interruption hardware acceptance remain required.

## Release verification (2026-09-01)

- Release and Debug CubeCLT builds completed with `-Wall -Wextra -Wpedantic` and no warnings.
- Seven host-side binary-contract, CRC, rate, linker-guard, and analog-transfer tests passed.
- Release memory: 265,908 bytes Flash, 95,792 bytes normal SRAM including the reserved 8 KiB stack, and 57,484 bytes CCM. Normal-SRAM headroom is 35,280 bytes.
- STM32CubeProgrammer detected device ID `0x413` at 3.21 V, programmed the Release HEX through ST-Link, verified it byte-for-byte, and reset the MCU.
- Windows WinUSB enumeration still requires the board's USB FS connector to be attached separately from ST-Link; no `1209:F407` USB device was present during this run.
