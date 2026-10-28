# Continuous recording verification

## Start-hang correction — 2026-10-25

On the attached board, Start was observed in PREPARING inside HAL
`SD_FindSCR`, while SysTick continued and fault registers were clear. The
screen remained on its previous LIVE label because SD negotiation does not
run the cooperative display hook. Start had remounted the already mounted
card; the SD handle was cleared without resetting the peripheral's data path.

Correction: reuse the healthy mount; before actual card reinitialization,
disable SD interrupts/data transfer, abort the two SD DMA streams, deinitialize
HAL, reset only SDIO (not shared DMA2), and clear pending SD interrupts before
clearing the handle. Failed DMA launches now invalidate disk status. Set the
HAL software data timeout to 1000 ms independently of its hardware clock-cycle
timeout. This bounds SCR polling, not the entire preparation/retention operation.

Validation: 15 host tests pass (including three new source-regression guards);
warning-free Release build, 303,284 flash bytes, unchanged 21,280-byte normal
SRAM headroom. ST-Link programming/read-back verification succeeded. After reset,
live capture progresses and storage reports READY. User Start/touch confirmation
is pending; do not interpret source guards as an SD hardware simulation.
Current BIN SHA256:
`647749D87483EC3BBE3E9AD4DEB020A9F93713F202C5D16FD3E8982C82D59394`.

The build/hash figures below describe the preceding firmware revision.

Implementation date: 2026-10-25.

## Host checks

`python -B -m unittest discover -s tests` passes 12 unittest cases, including two
compiled C harnesses. Those harnesses compile the actual `record_format.c` and
`recording.c`, not Python reimplementations. Hardware/HAL and FatFS are explicitly
mocked for the recording-service tests.

- Three-channel tuple ordering; 12-slot overflow; preservation of older entries;
  slot ownership after DMA source reuse; 32-bit ring-counter wrap; 64-bit indices.
- Actual C CRC known vector, every-byte record corruption, every truncated record
  length, encoder/parser agreement.
- Start/Stop boundaries, repeated sessions, pause/resume and stop from paused,
  SD absent, injected delayed writes and overflow, write failure, ADC epoch gap,
  real-size 16 MiB rollover, completed-only retention, full-card preparation failure.
- Host exporter parses a C-produced recording, reconciles final counts, and rejects
  corrupted/truncated headers, payloads, trailers and closing summaries while
  retaining the valid prefix. Existing legacy v1 tests remain passing.

Release builds with the existing CubeCLT CMake toolchain and warning flags.
The linker enforces at least 16 KiB normal-SRAM headroom, including the reserved
8 KiB stack and 512-byte heap.

## Build and attached-board checks

- Clean Release build passed with no compiler warnings. Final changed sources
  were rebuilt without warnings and the full host suite rerun after the clean build.
- Flash: 303,076 bytes of the 384 KiB firmware region; calibration sector 7 was
  not erased. Normal SRAM: 109,792 / 131,072 bytes, leaving **21,280 bytes**.
  CCM: 61,580 / 65,536 bytes. Linker headroom assertion passes.
- Final BIN SHA256:
  `0A2163342C30D0D8FD890DE533CB91BDE295196BF1C5F203DB1493BB7DFE687F`.
- ST-Link `48FF6F068365535047140781`, target ID 0x413, 512 KiB flash, 3.22 V:
  ELF programmed, read-back verification succeeded, MCU reset.
- Before programming, full flash backup saved as
  `build/pre_continuous_20261025.bin` (524,288 bytes).
- Read-only hot-plug SRAM checks after final reset: acquisition RUNNING (1),
  DMA-error flag clear, live capture sequence 530, configured rate 10000,
  sample count 512 and channel count 3; storage READY (1).
  This confirms live firmware activity and mounting, **not SD recording acceptance**.
- USB configured flag was 0 during inspection. Windows device enumeration query
  was denied by the local environment; registry history contains the VID/PID but
  did not expose the expected WinUSB interface. No USB transfers were verified.
  Connect/check the board's USB data port and Windows binding before USB bench tests.

No test recording was started on the physical card during this turn. SD delay,
rollover, retention and fault results above are from deterministic host mocks.
The physical device is left in live measurement, not recording.

## Hardware acceptance still required

These are not proven by the mocks:

1. Measure TIM2/ADC rate, tuple alignment and analog settling on PC0-PC2.
2. Record for one hour on the installed card: expect 36,000,000 sets per channel
   for exactly one hour of sample time (boundary-controlled user timing may add
   up to one block). Verify all raw values, CRCs, segment chain and no unexplained gaps.
3. Measure display refresh (target >=10 Hz), touch latency, queue high-water and
   durable progress during realistic card latency and retention/preallocation.
4. Physical card removal/reinsertion, controlled power interruption, FAT recovery,
   interrupted file download and preservation of complete synchronized blocks.
5. Windows enumeration, USB/UI control equivalence and downloaded sample comparison.

Do not treat this document as a production-readiness certificate. A card's internal
cache and FAT metadata can still be damaged by power loss. No high-voltage analog
front-end validation was performed or altered by this firmware change.

## 2026-10-25: recording/display scheduling correction

The previous firmware's physical recordings overflowed the queue. User-observed
Stop finalization took 1–2 seconds. A timed display service call reached 125 ms,
exceeding two 51.2 ms acquisition blocks. SD waits previously invoked display
processing immediately, even when a transfer would finish quickly.

- Drain queued records before display processing, with a 20 ms cooperative budget.
- Do not decode new graph captures inside SD wait hooks; continue touch/status
  service, guarded against re-entry. Give short SD transfers a 10 ms grace period.
- Limit display capture processing to 100 ms intervals; raw recording is unchanged.
- Persist CH1/CH2/CH3 view selection, color the selected readout and highlight its
  button. All three inputs remain enabled for recording.
- Preserve loss reporting, queue ownership, CRCs, Stop drain/sync/truncate/close,
  and existing file contents. No loss counter is hidden or reset by this fix.

18 tests pass (15 existing tests plus 3 source-level scheduling/UI guards; the
new guards are not runtime or throughput tests). Release build has no warnings.
Flash uses 303,692 bytes; SRAM uses 109,800 bytes, leaving 21,272 bytes; CCM is
61,580 bytes. Final BIN SHA256:
`10DC4DCC97AB41DF623AF8B8FBA94214CB6AE264AD1C4C0878B2B66FD10BE5E2`.
ST-Link programming and read-back verification succeeded. Post-reset SWD shows
acquisition RUNNING, recorder IDLE and storage READY. Calibration sector 7 retained.

Physical recording after this final update, channel-button appearance, improved
Stop latency, sustained zero drops and the >=10 Hz display target still require
bench verification. The 125 ms observed UI maximum means 10 Hz is not established.
