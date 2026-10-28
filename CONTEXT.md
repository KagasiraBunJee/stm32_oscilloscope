# Intercom project: working context

Last updated: 2026-10-28 (Europe/Kiev).

Read this file when resuming the project. The detailed evidence and development record are in [docs/RASPBERRY_PI_INTERCOM.md](docs/RASPBERRY_PI_INTERCOM.md). Distinguish user observations and measured results from proposed circuits. This file records progress; it does not mean the Raspberry Pi replacement is built or electrically validated.

## Status at a glance (2026-10-28)

| Area | State |
|---|---|
| Receive audio interface (white → EasyCAP) | Draft 1 BUILT on breadboard; bench tests and first intercom session PASSED (STM32 scope, no EasyCAP yet). Incoming-call pulse test pending |
| EasyCAP (MS2106, `534d:0021`) | Owned; characterised on the Windows PC (audio format, calibration, noise). Video not yet tested |
| Raspberry Pi 3 | NOT set up (no OS/Wi-Fi/SSH). Exact model unconfirmed |
| Transmit, call detection, unlock driver, panel supply, changeover relay | Not designed |
| Pi software | None |
| iPhone/remote calls | Future update; do not design yet (user priority) |

## Goal, priorities and hardware

Replace the indoor Slinex SL-07IPHD monitor, whose Wi-Fi/Ethernet do not work, while retaining the ML-20HD apartment door station and the separate building entrance intercom. The existing monitor still operates local calls, audio, video and unlock and serves as our reference.

PRIORITY (user, 2026-10-27): focus now on the LOCAL Pi + intercom/monitor connection only. The iPhone app is a future update.

Future goal (recorded, not designed): answer intercom calls on an iPhone over the internet (Pi on Wi-Fi, custom iOS app). Needs a transmit path onto white, call detection, echo handling and secure remote access (remote unlock is security-critical). User requirement: once a call is accepted remotely, the Slinex monitor must NOT play the call audio. Before Accept the monitor already plays outdoor audio, and it presumably ends unanswered calls after a timeout (unmeasured). Direction discussed, not decided: a fail-safe changeover relay (de-energised = panel on the monitor; energised = Pi takes the panel and must then power red itself).

Platform: originally Raspberry Pi 5 was discussed; the starting platform is the user's Raspberry Pi 3 with its power supply and display. Pi variant (3B/3B+), OS, display connection and remote access are unconfirmed. The PC is not portable and the intercom is far away, so intercom-side tests use portable equipment (STM32 scope on a power bank now; the Pi later).

The building entrance is a different system connected through an MCLAUT adapter. Photographs show terminals `LN+ LN- V G` on one side and `A G V +` on the monitor side. Exact adapter model and building protocol are unknown. Keep the adapter; do not assume LN- is ground or that identical monitor connectors guarantee identical electrical behaviour. Characterise this path separately.

The shared ChatGPT link could not be retrieved; do not assume its contents are available. The local documents and recordings are the usable project record.

## Confirmed ML-20HD observations

- Red: power; white: audio/control; yellow: video; black: ground.
- Red is approximately 4.4–4.8 V in standby and 13.5–13.6 V when active. Hang-up returns to standby, not necessarily zero.
- White standby: 4.8 V on a DMM (4.81 V earlier); the scope buffer's 1.25 MΩ input reads 4.56–4.6 V. Slow drifts to about 5.2 V; about 6.7 V for seconds after unlock. Carries speech in both directions.
- Incoming call: white drops to 0.00 V for 110–220 ms (fall 2–6 ms), followed by red power-up. In gap-free rec6: white low 41.2559–41.3626 s; red crosses 8 V at 41.4138 s.
- Manual camera activation gives a smaller positive white disturbance, without the near-zero pulse.
- Before Accept, indoor users can hear outside while the indoor microphone is muted. Accept enables outgoing speech and leaves receive active. An internal monitor routing change is the current interpretation; no separate large DC answer command is established.
- Unlock: white 4.6 → 13.3 V (max 13.52 V), rising faster than one 0.1 ms sample (≥45 V/ms measured), plateau 2.01 s; release falls to about 6.7 V within 1–2 ms. Conversation is interrupted during unlock.
- Speech on white (buffered sessions, 20 ms moving mean removed): 16–40 mV RMS; 99.9th percentile ±85–340 mV; switching transients up to about ±1.5 V. In rec12 loud speech peaked about ±0.5 V.
- No separate large white hang-up command is established.
- Do not reproduce controls by directly shorting white to red or ground. Required currents, impedances and a protected driver remain unknown. The white line's audio impedance is unmeasured (the about 47 kΩ figure is only a combined DC model).

Proposed application states: STANDBY, RINGING, VIEWING, TALKING, UNLOCKING. Ringing/viewing: receive on, microphone muted. Talking: receive and transmit on. Unlock: mute both, operate bounded driver, wait for settling, restore prior state. Hang-up: mute and deactivate panel. These are requirements, not implemented Pi software. Full-duplex echo management is needed: transmitted audio returns in the receive path on the shared white wire.

## Receive audio interface (white → EasyCAP white RCA)

Design: [docs/RECEIVE_AUDIO_INTERFACE.md](docs/RECEIVE_AUDIO_INTERFACE.md); breadboard layout [PNG](hardware/receive_audio/receive_audio_breadboard.png)/[SVG](hardware/receive_audio/receive_audio_breadboard.svg) (generated by `hardware/receive_audio/draw_breadboard.py` + `render_breadboard.ps1`). Circuit: R1 100 k → C1 10 nF/≥50 V → node X; R2 1 M to VMID; 1N4148 clamps to VCC/GND; R3 10 k → MCP6022 unity follower on 5 V filtered by R8 100 Ω/C4 100 µF; VMID = R6/R7 100 k/100 k + C3 10 µF; output C2 10 µF → R4 100 Ω, R5 100 k to GND. The line sees no DC load and about 1.1 MΩ for audio; R1/R2 pass 91 % (−0.8 dB); high-pass 14 Hz; output always within 0…VCC.

Test results:
- Power-only DMM check (2026-10-28): VCC 4.8 V, VMID 2.39 V, node X/pin 3 2.2 V (meter loading of the 1 MΩ; true value equals VMID), pin 1 2.4 V, output about 10 mV. One early 0.7 V pin 1 reading was a misplaced probe.
- `recordings/rec10/R00000002_00000000.OSC` step test (142.5 s, 1.741 s gaps): CH3 read pin 1 × 5.7 because the scope red channel's 220 k at node B had lost its ground (fixed afterwards; protected by the scope's chain/clamps; true pin 1 = CH3/5.7, saturating at 3.3 V). About 40 touches; all recovered within 100 mV in 31–51 ms.
- `recordings/rec11/R00000006_00000000.OSC` rehearsal (75.4 s, 0.717 s gaps, CH3 true scale): baseline 2.462 V; 21 large steps; peaks 3.86–4.94 V and 0.00 V; recovery 31.7–53.3 ms. An open input picks up about 230 mV p-p mains hum; expected.
- Line loading at the intercom (STM32 on power bank, DMM across panel white/black): white 4.8 V with nothing connected, with black joined, and with white on the interface. No DC loading. Readings of 5.2/4.8 V seen earlier were artefacts of one-handed probe handling (body contact).
- `recordings/rec12/R00000001_00000000.OSC` FIRST INTERCOM SESSION (75.5 s, 0.256 s gaps at 49.87–50.18 s). Timeline inferred from the signal and CONFIRMED by the user: idle 0–10 s, viewing about 11 s, Accept about 21 s, speech 26–34 and 42–44 s, unlock 53.562–55.570 s (2.008 s), standby from about 62–63 s. Pin 1 baseline 2.34 V; speech up to about 100 mV RMS per second, lowest 1.29 V, no clipping; unlock spikes 4.68 V and 0.05 V, each back within 100 mV in 37–38 ms. Noise: standby 24 mV RMS (50 Hz 23 mV, 150 Hz 12 mV peak); active 15–16 mV RMS (50 Hz 5 mV), near the scope's own floor. User confirmed the Slinex monitor behaved normally (picture, sound, unlock). Listening WAVs are in `recordings/rec12/analysis/` (HP150/LP3k, gain 1.0, unlock spikes and gaps muted); the user found the cuts precise.
- Pending: incoming-call pulse recording (user will do it later with a helper); EasyCAP-at-intercom test (waits for the Pi).

Current test wiring: interface powered from the STM32 board's 5 V (board on a power bank); scope red-channel EXTERNAL input (before the 1 M + 33 k + 1 k chain) on U1 pin 1; panel black → common GND; panel white → interface input; panel red connected to nothing of ours. The scope's white channel (node A/CH1) is currently DISCONNECTED; check its 220 k ground when reconnecting.

Never directly connect white to an RCA or microphone input. EasyCAP does not eliminate the indoor microphone, speaker output or transmit-to-panel requirements.

## EasyCAP (MacroSilicon MS2106E)

- Chip marking read as "ws2106e" = MS2106E. On Windows: USB `534d:0021`, "AV TO USB2.0", serial 20200909; UVC camera (MI_00), audio (MI_02), vendor HID (MI_04); built-in drivers.
- Audio descriptor claims 1 ch/96 kHz/16-bit, but the stream is really 48 kHz stereo interleaved (confirmed: unplugging red silenced odd samples). EVEN samples = WHITE RCA (left), ODD = RED (right). Use the white input and keep even samples; the Pi needs the same handling (read as 48 kHz stereo, keep left, or an ALSA quirk).
- Calibration (Windows, current capture level; `tone_1k_46mVrms_raw_as_96k_mono.wav`): 46.2 mV AC (DMM) = 729 counts peak, so about 0.09 mV/count; full scale about 2.94 V peak (2.1 V RMS); 1 kHz harmonics −82 to −86 dBc. Valid only at that Windows level; re-measure on the Pi. A cheap DMM may read low at 1 kHz, so true full scale could be higher.
- Noise: idle floor about 1.2 counts RMS (−88.7 dBFS, about 0.1 mV) gives about 88 dB range. The background hiss heard earlier (−50 dBFS, mostly 150–1000 Hz) came from the CHARGING PHONE; it vanished with the charger unplugged. Lesson: supply/ground noise couples straight into this input.
- Test WAVs: `recordings/easycap_test/`. Recorder: PowerShell winmm script (in the session scratchpad, not in the repo).
- Linux: standard `uvcvideo` + `snd-usb-audio`. Reported, untested: UVC non-compliance quirk; possible missing native 720×576 PAL mode; related MS2100/MS2109 ALSA format quirks. Check on the Pi with `lsusb`, `v4l2-ctl --list-formats-ext`, `arecord -l`, `arecord --dump-hw-params`. Video formats not yet listed (no ffmpeg on the PC). CVBS PAL/NTSC only: confirm the ML-20HD is in CVBS mode, not AHD.
- Other candidates discussed earlier (not bought): Gembird SC-USB2.0-01 (CM108B) and SC-USB-01 for microphone input/output. The Pi 3 has a headphone output but no microphone input.

## Oscilloscope and measurement front end

STM32F407 homemade scope records three simultaneous channels at 10 ksample sets/s. PC0 = CH1 (sometimes called CH0 by the user), normally white. PC2 = CH3, normally red, currently U1 pin 1. PC1 unused. Nominal external scaling ×5.7; 4.59 mV per count.

Original 47 kohm/10 kohm divider loaded white severely: DMM approximately 4.81 V without scope, 2.64 V with scope. Red stayed 13.6 V.

The user assembled a high-impedance MCP6022-I/SN buffer on an SOIC-to-DIP adapter:

- Pin 8 = STM32 3.3 V; pin 4 = common ground. Supply bypass 100 nF ceramic plus 1 uF in parallel; electrolytic positive at pin 8.
- Pin 1 directly feeds pin 2; pin 7 directly feeds pin 6 (unity followers).
- Each external input passes through series 1 Mohm + 33 kohm + 1 kohm to its node. Each node has 220 kohm to ground. White node A goes to pin 3; red node B to pin 5. Input resistance 1.254 Mohm, nominal attenuation 5.7.
- Each node has two 1N4148 clamps: lower anode ground/cathode stripe at node; upper anode node/cathode stripe at 3.3 V. Not equivalent to a fully qualified surge protection design.
- Original PC0/PC2 47 kohm/10 kohm divider pairs removed with this replacement; PC1 unchanged.
- White output: pin 1 through 1 kohm to PC0; 47 nF from PC0 to ground (corner about 3.39 kHz). User confirmed before rec8.
- Red output: pin 7 through 100 ohm to PC2; 1 nF from PC2 to ground.
- Do not place 47 nF at the high-resistance input node: that would remove speech bandwidth.

Assembly faults fixed: pin 4 initially ungrounded; a solder bridge between pins 3 and 4 pulled node A down; (2026-10-28) the red node B 220 k lost its ground during rewiring and was re-grounded. Apply test voltage only before the full input resistor chain, with buffer powered and common ground.

Instructions: [soldering guide](docs/MCP6022_SOLDERING_GUIDE.txt), [schematic PNG](hardware/mcp6022/mcp6022_1n4148_schematic.png), [SVG](hardware/mcp6022/mcp6022_1n4148_schematic.svg). These assets show the original output filters; use the white-filter update above.

## Recordings

Original `.OSC` files stay unchanged. Derived WAVs, plots, JSON and CSV are under each recording folder's `analysis` directory. Use full paths: session numbers are reused.

OSC1 legacy files save only the last 4096-sample snapshot (0.4096 s at 10 kS/s); rec1 holds only these. OSC2 records continuous three-channel data with explicit gaps and end metadata. DATA blocks contain 512 sets (51.2 ms). Known gaps are real missing samples. UTC in examined files was invalid; filesystem dates are unreliable.

- rec2 `R00000006_00000000.OSC`: 74.2912 s, no gaps; call and two unlocks (divider era: white loaded to about 2.7 V). rec2 `R00000007_00000000.OSC`: INTERRUPTED session (no session end; 57.09 s valid prefix, then invalid record; left at the 16 MiB preallocated size); contains a call and two unlocks; not otherwise analysed.
- rec3 `R00000008_00000000.OSC`: 73.2672 s, 0.2048 s missing. Call, about ten-second wait before Accept, two unlocks.
- rec4 session 008 duplicates rec3 exactly. Session 009: 91.648 s, no gaps; speech at 30–36 and 45–51 s into the indoor monitor, 57–64 s outdoors into ML-20HD.
- rec5 `R00000001_00000000.OSC`: manual viewing, Accept, two unlocks, hang-up; 62.6688 s, 1.536 s missing. First buffered measurements.
- rec6 `R00000002_00000000.OSC`: incoming call; 91.1872 s, 2.816 s missing. Call pulse gap-free; unlock interiors contain gaps.
- rec7 `R00000006_00000000.OSC`: 95.3856 s, 0.1536 s missing at 16.3328–16.4864. Useful voice at 42–49 and 67–77 s; interference 60–67 s with a correlated slow white/red baseline rise (cause unresolved). No gaps or clipping at 40–80 s.
- rec8 `R00000007_00000000.OSC`: white 1 kohm/47 nF filter; indoor speech; 42.4448 s, 0.768 s missing. Clearer voice but incomplete words (gaps).
- rec9 `R0000000F_00000000.OSC`: 51.4048 s, no gaps, red disconnected; three manual activations without Accept (not identifiable).
- `recordings/blackboard recs1`: seven tests with/without red measurement; analysis folder has comparisons. Red-connected sessions generally lose more data, cause not established. `R00000010_00000000.OSC`: strong three-activation candidate (positive white transients at 13.6592–14.0176, 25.7636–26.1208, 37.3348–37.692 s).
- rec10, rec11, rec12: receive-interface tests (see above). `recordings/easycap_test/`: EasyCAP WAVs.

Recorder dropouts: the queue holds 12 blocks (about 0.61 s); overflow drops blocks when SD/UI service stalls. Still present in rec10–rec12 (0.26–1.74 s per session). [docs/RECORDING_VERIFICATION.md](docs/RECORDING_VERIFICATION.md) records a 2026-10-25 scheduling fix programmed by ST-Link (build/Release dated 2026-10-25 20:37), but every v2 recording reports firmware 0x00020000, so files cannot tell which build made them. Bump `OSC_FIRMWARE_VERSION` on every flash. Root cause not confirmed; not cured.

Tools: `tools/osc_recording.py` validates/exports OSC files; `tools/read_osc.py` legacy snapshots; `tools/intercom_audio.py` CH1 (white) audio with HP150/LP3000 and gap/control masks (gap margin 5 ms, 2 ms fades; check each export's metadata); `tools/interface_audio.py` CH3 (interface pin 1) listening WAVs, args `file.OSC out_dir gain [clip_start clip_end ...]`, mutes gaps and >1.5 V control steps; `tools/intercom_denoise.py` experimental rec7 spectral subtraction. Do not interpret muted previews as raw-line silence.

## Video options

ML-20HD family documentation lists AHD 1080p/720p and CVBS PAL modes; confirm the installed revision and setting. Ordinary PAL/EasyCAP capture does not decode AHD. Possible HD paths: AHD-to-USB UVC capture, or AHD-to-HDMI then HDMI-to-USB capture. Pi HDMI is output only. A bare MIPI decoder chip is not a ready-to-use Pi CSI module.

On Pi 3 USB 2.0, prefer compression inside the capture device (MJPEG). Raw 1080p30 YUY2 (about 124 MB/s) is impossible on USB 2.0; raw 720p30 (about 55 MB/s) is impractical. Examples discussed, NOT tested or bought: Brando A2UC-XP, ezcap267P, Clinton CE-HDA2HDMI (links in the main development document).

Verify video grounding and termination: paralleling monitor and capture can double the 75-ohm termination. Do not connect a capture module's 5 V supply to the panel's red line.

## Next useful work (local system only)

1. Set up the Pi 3: Raspberry Pi OS via Raspberry Pi Imager with Wi-Fi and SSH preconfigured; confirm model and display. Then EasyCAP on the Pi: USB ID, video formats, 48 kHz stereo handling, re-measured input level; then receive audio/video at the intercom through the interface (interface 5 V from the Pi GPIO header, pin 2 = 5 V, pin 6 = GND).
2. Incoming-call recording through the interface (needs a helper at the panel).
3. Measurements without new hardware: monitor ring and viewing timeouts (stopwatch, ideally recorded); panel red current in standby and active (DMM in series, carefully); white-line audio impedance in talk mode (small injected tone through a known resistor) for the transmit design.
4. Design transmit (Pi audio out onto white), call detection to a GPIO, unlock driver, and, if the Pi takes over from the monitor, the panel supply plus fail-safe changeover relay.
5. Pi indoor-station software with the application states, USB microphone and speaker; two-way speech and echo tests. Then characterise the building adapter separately.

Housekeeping: README still describes Firmware 1.1 with the 47 k/10 k dividers and has an empty "Project layout" heading.

Do not claim completed Pi software, validated transmit/control drivers, a cured recorder dropout problem, or a finished installation guide. Update the detailed development record as new evidence arrives.
