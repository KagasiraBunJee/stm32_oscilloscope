# STM32F407 three-channel oscilloscope

Bare-metal oscilloscope firmware for the STM32F407ZET6 Black development board, an ILI9341 320×240 display on the verified 16-bit FSMC interface, and XPT2046 touch.

## Current firmware

- Official STM32F4 CMSIS/HAL, 8 MHz HSE, 144 MHz core, 36 MHz ADC and 48 MHz USB/SDIO clocks.
- Three simultaneous inputs, **PC0, PC1, and PC2 at 10,000 samples/s per channel**. This is the fixed current profile, not the earlier 7.2 MS/s capture mode.
- Unsigned 12-bit ADC readings stored in 16-bit words, in blocks of 512 three-channel sample sets. A twelve-slot recording queue is separate from the live display buffer.
- LVGL 9.5 touchscreen UI, roll/wave views, measurements, vertical scale/offset, and paused waveform inspection. Two partial RGB565 buffers use FSMC DMA transfers; the current flush callback waits for each transfer to finish.
- Continuous SD recording with Start/Pause/Resume/Stop, CRC-protected `.OSC` v2 records, explicit loss markers, segmented files, and recovery of valid interrupted prefixes.
- SDIO four-bit FAT16/FAT32 storage. Use a preformatted card; device-side formatting and exFAT are not supported.
- WinUSB control/live-data/session-file protocol (`1209:F407`) and RTC synchronization support. Windows enumeration and end-to-end USB transfers remain hardware acceptance items; no desktop GUI is included.
- D1 operational indicator and D2 heartbeat/error diagnostic.

## Measurement wiring

| Channel | STM32 input | ADC mapping | External signal range |
| --- | --- | --- | --- |
| CH1 | PC0 | ADC1 channel 10 | Positive 0–14 V through its divider |
| CH2 | PC1 | ADC2 channel 11 | Positive 0–14 V through its divider |
| CH3 | PC2 | ADC3 channel 12 | Positive 0–14 V through its divider |

For each channel, connect the signal through 47 kΩ to the ADC node, and connect
10 kΩ from that node to GND. Connect the signal source ground to board GND.
Nominal reconstruction gain is ×5.7; calibration is stored with the recording.
At 14 V, the ideal divider output is approximately 2.46 V.

**Never connect 14 V or a negative voltage directly to an MCU input.** The divider
is not isolation or complete overvoltage protection. This prototype is not rated
for mains, unknown transients, or floating/high-energy measurements. All channels
share ground; USB and ST-Link may also connect that ground to the computer.

The older PA1/PA2/PA3 direct-input and ±15 V conditioned-input designs are not the
active recording profile. Do not interpret historical design documents as proof
that those ranges are enabled or safe on the installed hardware.

See [Measurement input pins](docs/MEASUREMENT_PINS.md) for the channel wiring,
ADC mapping and wiring background. The fixed profile above and the
[v2 recording specification](docs/OSC_V2_RECORDING.md) describe current operation.

## Using the recorder

1. Power on: live measurement starts on all three channels without recording.
2. Press **Start** with a writable SD card inserted. `PREPARING` means the file is
   being allocated; `REC` means recording has started at an acquisition boundary.
3. **Pause** stops acquisition and recording, freezes the display, and synchronizes
   queued data. **Resume** continues the same session with an explicit pause marker.
4. **Stop** ends recording at a block boundary, drains queued data, writes the
   summary, synchronizes and closes the file. Live measurement continues.
5. **Wait for `FINALIZING` to return to `LIVE` before removing power or the card.**

CH1/CH2/CH3 buttons select the scale/readout channel; they do not disable the other
recording channels. Display settings and triggers do not discard or rearrange raw
recorded samples. Routes, sampling rate and calibration stay fixed during recording.

`lost` counts missing three-channel sample sets. A rising value means the recording
could not keep up; gap markers preserve the missing intervals, not their samples.
An SD failure ends recording with an error; a new recording requires explicit Start.

## Recording files and export

Files are stored under `/OSC/SESSIONS/` as `Rssssssss_nnnnnnnn.OSC`, where the
eight-digit hexadecimal fields identify the session and segment. Segments are
preallocated up to 16 MiB. Each contains channel mapping, calibration, sampling
rate, timing metadata, ordered raw-data blocks, sequence numbers and CRCs.

- Raw throughput: **60 kB/s** (216 MB/hour).
- Sector-padded DATA records: approximately **70 kB/s** (252 MB/hour), plus metadata.
- Stop writes a final session summary. Interrupted files retain only their valid
  completed-record prefix; unused preallocated space is not measurement data.
- Completed old oscilloscope sessions may be deleted automatically when space is
  needed. Interrupted sessions and unrelated files are not automatic retention targets.
- Recovery cannot guarantee survival of unsynchronized data or prevent FAT/card
  corruption after power loss. Back up important recordings.

Validate a copied recording or export its valid samples and markers to CSV:

```powershell
python tools/osc_recording.py recordings/rec2/R00000006_00000000.OSC
python tools/osc_recording.py recordings/rec2/R00000006_00000000.OSC --csv recordings/session6.csv
```

For a multi-segment session, pass all its segment paths to the same invocation.
Do not combine different sessions. The tool also reads legacy v1 captures.
Exported voltages use each file's calibration. WinUSB file access uses protocol
commands, not a Windows drive letter, and returns Busy while recording is active,
paused or finalizing.

## Layout

```text
Core/          HAL startup, clock, interrupts, main and configuration
Drivers/       Official STM32F4 CMSIS and HAL drivers
Middlewares/   LVGL 9.5, FatFS and ST USB Device core
App/           Oscilloscope application and touchscreen UI
Components/    Board-agnostic ILI9341 and XPT2046 drivers
Services/      Acquisition, calibration, buses, storage, RTC and WinUSB
docs/          Protocols, file format, analog design and status
hardware/      Analog models, EasyEDA projects/scripts and wiring illustrations
peripheral_docs/ Board and peripheral reference documents
tools/         Recording validator/exporter and supporting utilities
tests/         Host C harnesses, binary-contract and source-regression tests
recordings/    Local measurement files and analysis (Git-ignored)
```

## Build and test

Install CMake (3.20 or later), Ninja, the GNU Arm embedded toolchain and Python 3.
The CMake toolchain automatically locates the GNU tools installed by the STM32
VS Code extension under `%LOCALAPPDATA%/stm32cube/bundles`. Host C tests also need
GCC available on PATH; the Windows test harness has a Strawberry Perl GCC fallback.

```powershell
cmake --preset Release
cmake --build --preset Release --parallel 8
python -B -m unittest discover -s tests
```

Outputs are `build/Release/oscilloscope.elf`, `.hex`, and `.bin`. The linker fails if less than 16 KiB of normal SRAM remains after static allocations and reserved heap/stack.

## Flash with ST-Link

Stop recording and wait for `LIVE` first. Install STM32CubeProgrammer and adjust
the versioned path below to your installation. This example writes firmware and
resets the board; do not use a full-chip erase if you want to preserve calibration.

```powershell
& "$env:LOCALAPPDATA\stm32cube\bundles\programmer\2.23.0\bin\STM32_Programmer_CLI.exe" `
  -c port=SWD -w build\Release\oscilloscope.hex -v -rst
```

## Verification and limitations

The 2026-10-25 scheduling update passed 18 host tests, a warning-free Release build,
and ST-Link read-back verification. That build left 21,272 bytes of normal SRAM
headroom; those figures are a recorded checkpoint, not a guarantee for future edits.

Subsequent local-file checks established:

- Session 6: 74.2912 seconds, 742,912 samples per channel, all three channels present,
  zero dropped sets, valid CRCs and a clean final summary.
- Session 7: 57.088 seconds of valid three-channel data recovered without gaps in
  that prefix; no final summary. It was reported as possibly disconnected before
  finalization completed.

These short recordings do not establish one-hour reliability, accuracy/bandwidth,
all SD failure scenarios, USB interoperability, or sustained 10 Hz display refresh.
See the [verification report](docs/RECORDING_VERIFICATION.md) for outstanding tests.

## Git housekeeping

Firmware sources, bundled libraries, hardware designs, documentation and shared IDE
settings are intended to be tracked. `.gitignore` excludes build output, local
recordings/analysis, caches, temporary files and machine-local agent settings.
Ignored files remain on disk and need a separate backup. `.gitattributes` defines
text normalization and binary-file handling. No remote or initial commit is assumed.

Third-party libraries retain their own license notices; review them before redistribution.

## Documentation

- [USB protocol](docs/USB_PROTOCOL.md)
- [Continuous recording and `.OSC` v2 format](docs/OSC_V2_RECORDING.md)
- [Legacy `.OSC` v1 file format](docs/OSC_FILE_FORMAT.md)
- [Recording verification and outstanding acceptance tests](docs/RECORDING_VERIFICATION.md)
- [Analog front end, BOM and validation](docs/ANALOG_FRONT_END.md)
- [Implementation and bench-test status](docs/IMPLEMENTATION_STATUS.md)
- [Raspberry Pi intercom development record](docs/RASPBERRY_PI_INTERCOM.md)
