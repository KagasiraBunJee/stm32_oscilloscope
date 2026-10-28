# Continuous recording: firmware 2.0

PC0 / ADC1 channel 10, PC1 / ADC2 channel 11, and PC2 / ADC3 channel 12
are sampled simultaneously at 10,000 sets/s. Each set contains three unsigned
12-bit readings, stored as three little-endian 16-bit words in that order.
Raw throughput is 60 kB/s; sector-padded DATA records use 70 kB/s (252 MB/hour)
plus occasional metadata records and segment headers.
The existing positive-voltage 47k/10k divider calibration is unchanged.
These inputs are **not bipolar**; never apply negative voltage or connect
14 V directly to an MCU pin without its divider/protection.

Boot starts live measurement. Start requests PREPARING, mounts the card,
allocates a segment, and arms recording. The DMA half completing at the arm
boundary is excluded; the following complete half is the first recorded data.
Stop includes the half completing at the requested boundary, drains the queue,
writes SESSION_END, synchronizes/truncates/closes, and leaves live sampling on.
Pause is available during a recording: sampling stops at a half boundary,
queued data is drained and synchronized, and Resume starts a new acquisition
epoch in the same session. Stop also works while paused or pausing.
Start remains disabled until finalization completes. A persistent recording
error requires an explicit new Start. Changing display scale/view/trigger
does not alter raw recording. Routes/rate and calibration cannot change during
recording, including pause. This firmware profile fixes all three divider inputs
and 10 kS/s, including outside recording.

## Ownership and scheduling

DMA2 Stream0 fills 6,144 bytes in two 3,072-byte halves. Each contains 512 sets
(51.2 ms). The ADC interrupt copies a completed half into one of twelve owned
3,584-byte slots; it does no filesystem work, CRC, or measurement calculations.
The consumer releases a slot only after its complete write returns. A separate
display mailbox may be overwritten, but recording slots never are. Queue
capacity is 614.4 ms; longer card stalls can produce explicit whole-block gaps.
Indices remain 64-bit; 32-bit ring counters use independent slot indices so
integer wraparound does not change the twelve-slot ordering.

FatFS has one cooperative owner. SDIO transfers aligned SRAM buffers in multiple
sectors, with a 512-byte bounce buffer for CCM/unaligned buffers. Waits service
the display and touch with a re-entry guard. USB interrupt reception continues;
command dispatch is deferred until the filesystem call returns. Transfer timeout
is 5 s and aborts both SD and DMA. UI service frequency is not a hardware-verified
refresh-rate guarantee. Nominal display mailbox processing is about 19.6 Hz.

## Files and durability

Files are `OSC/SESSIONS/Rssssssss_nnnnnnnn.OSC`, where both identifiers are
eight hexadecimal digits (session, segment). Segment numbers start at zero.
Files are preallocated to 16 MiB using FatFS allocation; this does not guarantee
contiguous clusters. The next segment is prepared with approximately 1 MiB left.
Closing reserves eight records before rollover. Completed segments are truncated
to their used prefix. An unused next preallocation is removed only on clean Stop;
interrupted files are never automatically removed. Synchronization is requested
at least once per second and on Pause/Stop/rollover; a card stall can delay it.
The durable-byte counter counts synchronized headers and records, not unused
allocated bytes, and does not promise that the card has committed its internal cache.

Retention first removes validated legacy v1 captures (which predate v2), then
the lowest-ID fully completed v2 session with every linked segment CRC-validated.
It never removes active, incomplete, invalid, or unrelated files. Deleting a
multi-segment session proceeds from last to first. If interrupted, remaining
segments cannot falsely appear complete. Space exhaustion requests a normal
boundary Stop with STORAGE_FULL. Preparation failure leaves live mode running.

Recovery is demand-driven and incremental: each v2 list retry checks one record,
returning Busy until the prefix length is known. Mount does not scan v2 payloads.
Scanning stops at the first incomplete, structurally invalid, CRC-invalid, or
out-of-sequence record. No search past corruption is performed. Preallocated
space is ignored. A valid SEGMENT_END is not a clean session end; the chain must
reach SESSION_END. FAT/SD-controller corruption can still make files inaccessible.
Unsynchronized samples are not promised to survive loss of power.

## Wire representation

All integers are explicitly serialized little-endian. CRCs are conventional IEEE
CRC32 (reflected polynomial 0xEDB88320, initial/final XOR 0xffffffff).

### 512-byte segment header

| Offset | Type | Meaning |
|---:|---|---|
| 0 | u32 | 0x3243534f, `OSC2` |
| 4, 6 | u16, u16 | version 2, header bytes 512 |
| 8, 12, 16 | u32 each | session ID, segment number, firmware version |
| 20 | u32 | bit 0: UTC valid |
| 28, 32, 36 | u32 each | sample sets/s (10000), channels (3), record bytes (3584) |
| 40, 48, 56 | u64 each | start UTC seconds, monotonic start microseconds, first record sequence |
| 64, 96, 128 | 32 bytes each | channel descriptors below |
| 160 | u64 | expected next sample index at segment start |
| 168, 172 | u32 each | timer clock 72000000 Hz, timer ticks/set 7200 |
| 176, 180, 184, 188 | u32 each | initial display trigger mode, edge, raw level, position |
| 508 | u32 | CRC over bytes 0..507 |

Unassigned fields are zero. Channel descriptors: four u8 bytes = ASCII `C`,
pin number 0/1/2, ADC channel 10/11/12, divider profile 1; i32 gain in
microvolts/LSB Q16 at +4; i32 offset microvolts at +8; u32 display microvolts/div
at +12; i32 display offset at +16; remaining bytes zero.
Voltage = `(trunc(raw*gain/65536) + offset)/1e6` volts.
Each activated segment repeats the original session metadata and calibration.
First-header activation may rewrite its CRC before the first DATA write.

### 3,584-byte sector-aligned record (all record types)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | u32 | 0x324b4c42, `BLK2` |
| 4, 6 | u16 each | type, flags (GAP bit 0 = unknown length) |
| 8, 12 | u32 each | total length 3584, DATA sample-set count (otherwise 0) |
| 16, 24 | u64 each | record sequence, first sample index |
| 32, 36 | u32 each | acquisition epoch, payload byte count |
| 40 | u64 | monotonic time in microseconds |
| 48, 52 | u32 each | session ID, segment number |
| 56, 60 | u32 each | payload CRC, header CRC over bytes 0..59 |
| 64 | bytes | payload, then zero padding |
| 3568 | u32 | completion magic 0x32444e45, `END2` |
| 3572 | u64 | repeated record sequence |
| 3580 | u32 | CRC over bytes 0..3579, including padding and trailer |

Record sequences start at zero and continue across segments. DATA indices start
at zero at the recording boundary and advance even if a whole queue block is
dropped. Unknown ADC losses do **not** advance by an invented count: an ERROR
and unknown GAP separate acquisition epochs. Counters track completed sets;
sample index is not an uninterrupted time axis across unknown gaps.

DATA time is the epoch's timer-start anchor plus its sample offset ×100 us.
Anchors use an extended millisecond HAL tick (1 ms absolute resolution); intra-
epoch spacing comes from TIM2, not filesystem time. Actual analog aperture and
oscillator accuracy require bench characterization. Control markers use the same
monotonic clock. UTC is a session-start anchor only; changing RTC does not change
sample timing. Recorded counts, sequence, durations and sample indices are u64.

| Type | ID | Payload |
|---|---:|---|
| DATA | 1 | exactly 3072 bytes: 512 PC0/PC1/PC2 tuples, no trigger rotation/decimation |
| GAP | 2 | known: u64 first missing index, u64 missing set count; unknown: empty, flag 1 |
| ERROR | 3 | 16 bytes: u32 error code; reserved bytes; optional u64 accumulated fault count at +8 |
| PAUSE | 4 | 112-byte checkpoint summary; marker time is synchronization time |
| RESUME | 5 | u64 intentional pause duration in microseconds, including drain/sync wait |
| SEGMENT_END | 6 | empty; next segment is required |
| SESSION_END | 7 | final 112-byte summary |

Summary offsets: u64 recorded sets +0, known dropped sets +8, unknown gap markers
+16, pauses +24, error count +32, elapsed us +40, known acquisition us +48,
record total including this marker +56; u32 segment count +64, stop reason +68;
u64 intentional pause us +72; remainder zero. Acquisition duration is known
completed plus known-dropped sets ×100 us; unknown-loss duration is not fabricated.
Stop reason uses recording error codes: 0 user Stop, 1 no card, 2 I/O, 3 storage
full, 4 ADC. An I/O failure can prevent any closing marker; absence of SESSION_END
means interrupted, regardless of the filename. Pauses in the PAUSE checkpoint
count previously completed pauses; final summary includes the current pause.

## Host validation/export

`python tools/osc_recording.py R00000001_00000000.OSC R00000001_00000001.OSC --csv session.csv`

Supply all segments of one session. Output reports clean/interrupted status,
valid-prefix bytes, record and sample counts. CSV retains DATA raw readings and
calibrated volts, epoch, sample index, time, and explicit marker rows. It never
interpolates losses. A single v1 file is also accepted. `tools/read_osc.py` remains
the legacy v1 visualization tool, not a mixed-directory/v2 reader.

## Acceptance status

See `RECORDING_VERIFICATION.md`. Software tests and a successful build are not
a claim of one-hour, power-cut, SD-controller, USB, or analog validation.
