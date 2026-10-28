# WinUSB protocol version 2

The device enumerates as `1209:F407`, interface 0, with bulk OUT `0x01` and bulk IN `0x81`, both 64-byte packets. Its Microsoft OS 2.0 descriptor selects WinUSB and publishes interface GUID `{5D4E3F66-6A90-4A57-9C0C-6F6C1B8A407E}`. It is not USB mass storage and does not create a drive letter.

## Frame

Every request has one response with the same command and sequence. Fields are packed and little-endian.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `0x3150534F` (`OSP1`) |
| 4 | 1 | protocol version `2` |
| 5 | 1 | header size `24` |
| 6 | 2 | command |
| 8 | 2 | status; zero in requests |
| 10 | 2 | flags; response bit 0 is set |
| 12 | 4 | caller-selected sequence |
| 16 | 4 | payload length, maximum 600 |
| 20 | 4 | IEEE CRC-32 |

CRC-32 is calculated over the complete header with `crc32=0`, continued over the payload. The reflected polynomial is `0xEDB88320` with conventional initial/final XOR. Multi-packet messages are concatenated according to `payload_length`.

Status values are: 0 OK, 1 bad magic, 2 bad version, 3 bad length, 4 bad CRC, 5 unknown command, 6 invalid argument, 7 busy, 8 no capture, 9 no card, 10 not found, 11 I/O error, 12 storage full, 13 failed.

## Commands

| ID | Name | Request payload | Response payload |
|---:|---|---|---|
| `0001` | Device info | none | version/capabilities/rates/state, 24 bytes |
| `0010` | Configure | `u32 rate, u16 level, u16 position, u8 mode, u8 edge, u8 enable_mask, u8 high_range_mask, i32 offset_uv[3], u32 uv_per_div[3]` | none |
| `0011` | Start | none | accepted preparation request; poll recording status |
| `0012` | Pause | none | none |
| `0013` | Resume | none | none |
| `0014` | Stop | none | accepted boundary-stop request; live sampling continues |
| `0020` | Capture info | none | sequence/rate/count/trigger/state, 24 bytes |
| `0021` | Capture read | `u32 sequence, u16 offset, u16 count, u8 capture_channel, 3 reserved` | same addressing header (12 bytes), then `u16` samples |
| `0022` | Measurements | none | `u32 sequence, u8 count, 3 reserved`, then `count` fixed 40-byte channel records from the `.OSC` specification |
| `0030` | Storage status | none | state, progress %, unsaved flag, RTC-valid flag |
| `0031` | RTC set | `u64` Unix UTC seconds | none |
| `0032` | Session list | `u32 cursor` | next cursor plus session info; Not Found ends paging |
| `0033` | Session info | `u32 session_id` | session info |
| `0034` | Session read | `u32 id, u32 offset, u16 length, u16 reserved` | same address header and bytes; length is capped to 588 |
| `0035` | Session delete | `u32 session_id` | none |

Session info is packed as `u32 id, u64 UTC, u32 total_length, u32 rate, u16 samples, u8 channels, u8 flags, char filename[64]` (88 bytes). The capture-info response is 24 bytes. The desktop implementation must use explicit packed decoding rather than its compiler’s default alignment.

File access/deletion returns Busy during preparing, recording, pause or finalization.
Idle live acquisition does not prevent downloads. Start/Stop/Pause/Resume are
deferred through the same recording service used by the UI; OK means request
accepted, not that the file is already open/closed. Poll 0040 for the outcome.
Version-1 read-only and legacy file commands remain supported with version-1
responses. Version-1 acquisition controls 0010–0014 are rejected with Bad Version,
because their old snapshot semantics no longer apply.
# Firmware 1.1 input-profile note

Route/high-range-mask value 1 now means the positive 0–14 V 47k/10k divider
on PC0/PC1/PC2, not the earlier bipolar front end. Configure now requires enable mask = high-range mask = 7 and rate = 10,000;
other profiles return Invalid Argument. During recording Configure returns Busy.
Outside recording it updates display settings without restarting acquisition.
Trigger position is now a display-block index in the range 0..511.
Device-info capability bit 6 identifies continuous recording; advertised rate
limits are 10,000 and live capture size is 512 samples per channel. Use the gain/offset
stored with each capture/session; do not hardcode the old bipolar conversion.

## Continuous recording extensions (protocol 2 only)

| ID | Request | Response |
|---:|---|---|
| 0040 | none | 72-byte recording status |
| 0041 | u32 directory cursor (start at 0) | u32 next cursor + 64-byte segment info |
| 0042 | u32 session, u32 segment, u32 offset, u32 length | same four u32 fields (actual length), then bytes; capped at 584 |
| 0043 | u32 session | delete fully validated completed v2 session, including all segments |

0041 validates one record per retry and returns Busy until that segment's valid
prefix is known. Repeat **the same cursor** on Busy; Not Found ends the directory.
A new cursor cancels the old scan. Ordering is FAT directory order, not chronology.
0042 exposes physical file bytes, including unused preallocation if requested:
download only the valid-prefix length from 0041 and validate again on the host.
An invalid file header returns Invalid Argument; advance that cursor explicitly
to inspect later entries. Interrupted files are never automatically deleted.
0043 refuses interrupted or invalid chains.

Status: u32 state/error/session/segment/queue_high_water/queue_depth at offsets
0/4/8/12/16/20; u64 recorded_sets/known_dropped_sets/unknown_gap_count/record_count/
durable_bytes/elapsed_us at 24/32/40/48/56/64. States: 0 live idle, 1 preparing,
2 armed (still show PREPARING), 3 recording, 4 pausing, 5 paused, 6 finalizing,
7 failed (persistent until a new Start). Errors: 0 none, 1 no card, 2 I/O,
3 storage full, 4 ADC. Counters count sets **per channel**, not total ADC words.

Segment info: u32 session +0, segment +4, valid-prefix length +8, flags +12;
u64 DATA sets in segment +16, UTC start seconds +24; reserved zero through +63.
Flags: 1 interrupted prefix, 2 SESSION_END encountered, 4 SEGMENT_END encountered.
Only an ordered chain starting at segment 0 and ending in a validated SESSION_END
constitutes a completed session. See [v2 format](OSC_V2_RECORDING.md).


