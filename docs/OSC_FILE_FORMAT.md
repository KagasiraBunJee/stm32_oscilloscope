# `.OSC` session format version 1

All integer fields and samples are little-endian. A completed file consists of one 256-byte header followed by interleaved unsigned 12-bit ADC samples in 16-bit words. The unused upper four sample bits are zero.

## Header

| Offset | Size | Field | Unit / meaning |
|---:|---:|---|---|
| 0 | 4 | magic | `0x3143534F` (`OSC1`) |
| 4 | 2 | version | `1` |
| 6 | 2 | header_size | `256` |
| 8 | 4 | total_length | Header plus payload |
| 12 | 4 | session_id | Monotonically increasing ID |
| 16 | 8 | timestamp_utc | Unix seconds; meaningful when flag bit 0 is set |
| 24 | 4 | flags | bit 0 RTC valid, bit 1 trigger valid |
| 28 | 4 | firmware_version | `major << 16 | minor << 8 | patch` |
| 32 | 4 | sample_rate_hz | Actual rate per logical channel |
| 36 | 2 | sample_count | `4096` in this firmware |
| 38 | 2 | trigger_index | Index after trigger alignment |
| 40 | 2 | trigger_level_raw | ADC counts, 0–4095 |
| 42 | 1 | channel_count | 1–3 |
| 43 | 1 | data_layout | `1`: sample-major, then enabled capture channel |
| 44 | 1 | trigger_mode | 0 Auto, 1 Normal |
| 45 | 1 | trigger_edge | 0 rising, 1 falling |
| 46 | 1 | stop_reason | 0 touchscreen, 1 USB |
| 47 | 1 | reserved | zero |
| 48 | 120 | channel[3] | Three 40-byte channel records below |
| 168 | 4 | payload_size | `sample_count * channel_count * 2` |
| 172 | 4 | payload_crc32 | IEEE CRC-32 of payload |
| 176 | 4 | header_crc32 | IEEE CRC-32 of all 256 header bytes with this field zero |
| 180 | 76 | reserved | zero |

Each channel record is: logical channel `u8`, route `u8` (0 direct, 1 ±15 V), ADC channel `u8`, reserved `u8`, gain in µV/LSB Q16 `i32`, offset in µV `i32`, minimum `i32`, maximum `i32`, average `i32`, RMS `u32`, peak-to-peak `u32`, frequency in mHz `u32`, period in ns `u32`.

The first payload words are `sample0/channel0`, `sample0/channel1`, …, followed by `sample1/channel0`. CRC-32 uses reflected polynomial `0xEDB88320`, initial/final XOR `0xFFFFFFFF`.

## Atomic save and naming

Firmware writes `/OSC/SESSIONS/SESSION.TMP`, synchronizes and closes it, then renames it. Valid RTC names use `YYYYMMDD_HHMMSS_NNN.OSC`; otherwise `SESSION_NNNNNNNN.OSC`. Mount recovery removes the known temporary name. It neither removes nor interprets unknown files. Completed `.OSC` files with invalid structure or CRC are ignored.
# Firmware 2.0 note

New recordings use [OSC version 2](OSC_V2_RECORDING.md). The following v1
specification remains applicable to existing capture files and legacy downloads.

