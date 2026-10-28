# Raspberry Pi intercom development record

Updated 27 October 2026. Status: measured behaviour and prototype design notes. This is the working source for a future Raspberry Pi intercom build guide, not a completed installation manual.

## Objective and existing installation

We want to replace a Slinex SL-07IPHD indoor monitor while retaining the ML-20HD apartment door station and the building entrance connection. A Raspberry Pi 5 was initially considered; on 27 October the operator chose to start with an available Raspberry Pi 3, power supply and display. No other Pi audio or capture peripherals are currently available. The current monitor still supports the local experiments described here, but its Wi-Fi and Ethernet functions were reported not working. The cause of that network failure has not been diagnosed.

The building entrance is a separate device connected through an existing adapter. Photos identify a board marked MCLAUT. Its exact model, building-side protocol, and electrical specifications remain unknown. Retaining the adapter is the intended starting point, so we can investigate its monitor-side interface without implementing the building bus directly.

We have recorded both directions of speech, an incoming-call indication, panel power changes, and unlock pulses on the ML-20HD connection. This supports designing a first interface prototype. It does not establish the required drive currents or prove that the building adapter accepts identical signalling.

The shared chat URL supplied at the start could not be retrieved. Its unprovided contents are not treated as evidence in this document.

## Evidence and interpretation

This record uses three distinctions:

- **Measured:** present in saved samples or reported multimeter readings. Sample voltages use the saved nominal calibration unless stated otherwise.
- **User confirmed:** an action, audible behaviour, or recording identification confirmed by the person operating the intercom.
- **Proposed:** a working explanation or design decision that still requires electrical implementation and testing.

A valid file checksum establishes file integrity, not analog accuracy. A clean end marker does not imply that no samples were lost. Recording gaps are retained explicitly.

## Existing wire connections

The ML-20HD manual identifies these four monitor connections:

| Wire | Function | Current understanding |
|---|---|---|
| Red | Panel power | Monitor changes its voltage between standby and viewing or conversation |
| White | Audio | Both speech directions are present; large call and unlock excursions also occur |
| Yellow | Video | Analog video output; exact installed format has not been established |
| Black | Ground | Reference used for the ML-20HD measurements |

Do not connect a speaker, headphones, Raspberry Pi GPIO, microphone input, or audio line input directly across white and black. White carries DC and unlock excursions near 13 V as well as audio. The audio and control interfaces need appropriate coupling, current limiting, and protection.

The ML-20HD family manual documents AHD 1080p, AHD 720p, and CVBS PAL modes. Verify the installed panel revision and selected mode before choosing capture hardware. A PAL USB capture device does not inherently accept AHD.

The MCLAUT board photographs show:

| Side | Markings | Interpretation |
|---|---|---|
| Building-facing side | LN+, LN-, V, G | Line pair, video and video ground, inferred from markings |
| Monitor-facing side | A, G, V, + | Audio or signalling, ground, video, supply, inferred from markings |

These are not a verified manufacturer pinout. Physical order reverses when viewing the other side of the board. LN- must not be assumed to be GND. Trace terminal markings and actual wires rather than relying on colour alone.

## Measurement channels and equipment

The handmade oscilloscope uses an STM32F407. Continuous recording firmware samples all three divider-route inputs simultaneously at 10,000 sample sets per second:

| Saved array position | Physical ADC pin | Scope channel | Intercom connection in these experiments |
|---|---|---|---|
| First, index 0 | PC0 | CH1 | White |
| Second, index 1 | PC1 | CH2 | Not used to interpret the intercom |
| Third, index 2 | PC2 | CH3 | Red |

References in conversation to channel zero mean the first recorded input. Use physical pin names in new notes to avoid ambiguity.

Samples are unsigned 12-bit values stored in 16-bit words. Nominal external resolution at the x5.7 divider scale is approximately 4.593 mV per count. A 10 kS/s recording can capture useful voice-band information, but it is not a video measurement system. Frequencies above 5 kHz are not uniquely represented, and the current input does not provide a sharp anti-alias filter.

The oscilloscope and buffer are not isolated or mains-rated. Grounds may also connect through USB or other powered equipment. Only the identified ML-20HD low-voltage connection has been characterised in these experiments; do not transfer its ground assumptions to the building line.

## Initial measurements and the loading discovery

Initial multimeter observations were red approximately 4.8 V in standby and 13.6 V when viewing; white approximately 4.4 V in the early notes; yellow approximately 0 V in standby and a 0.6 V average when video was active. The yellow multimeter average does not identify video format or peak amplitude.

A later direct comparison in the same operating state was decisive:

| Measurement | White | Red |
|---|---:|---:|
| Scope disconnected from white | Approximately 4.81 V | Approximately 13.6 V |
| Original scope divider connected | Approximately 2.64 V | Approximately 13.6 V |

The original 47 kohm plus 10 kohm divider placed 57 kohm from white to ground. It materially loaded the white line. Under a simple Thevenin model, the DC source resistance in that state is approximately:

`57 kohm * (4.81 / 2.64 - 1) = 46.9 kohm`

This estimates the combined circuit's DC behaviour at that operating point. It is not a measurement of voice-frequency impedance or proof of which device supplies the bias.

Earlier white recordings near 2.7 V are therefore loaded measurements, not evidence that the unloaded line normally rests at 2.7 V. After fitting the buffer, recordings place white around 4.56 to 4.61 V, consistent with greatly reduced loading. Residual loading and calibration error remain possible.

## Recording format and interpretation

The first firmware saved a single 4096-sample snapshot when Stop was pressed. At 10 kS/s that is only 0.4096 seconds. The screen's roll history showed averages of earlier frames, but those earlier frames were not included in the saved snapshot. Consequently rec1 cannot reconstruct the multi-second call sequences that were performed before Stop.

Current recordings use OSC version 2. DATA records preserve raw simultaneous PC0, PC1 and PC2 sample sets. Sequence numbers, sample indices, calibration, checksums, gap markers and a final session marker allow validation and recovery. One 512-set DATA block spans 51.2 ms. Missing sets are not interpolated.

At three channels, raw sample throughput is 60 kB/s; sector-padded DATA records use approximately 70 kB/s. The queue provides finite buffering, so long storage stalls can cause marked whole-block losses. The file alone does not identify the underlying cause of a stall. The current recording losses still need investigation.

The examined files have no valid UTC anchor. Their apparent 2020 filesystem dates are not reliable event dates. Use time relative to session start. Session IDs have restarted between experiments, so identify a recording by its full folder and filename.

- [OSC2 format and firmware recording design](OSC_V2_RECORDING.md)
- [Legacy OSC1 format](OSC_FILE_FORMAT.md)
- [Recording verification status](RECORDING_VERIFICATION.md)

## Recording evidence index

| Folder and file | Sample timeline | Known missing duration | Principal evidence |
|---|---:|---:|---|
| rec1 SESSION_00000002 through 04 | 0.4096 s per file | Not a continuous history | Final snapshots only; red and white were recorded separately |
| rec2 R00000006_00000000.OSC | 74.2912 s | 0 s | Simultaneous white and red; two unlock events |
| rec3 R00000008_00000000.OSC | 73.2672 s | 0.2048 s | Incoming call, delayed Accept, two unlocks |
| rec4 R00000008_00000000.OSC | Same as rec3 | Same as rec3 | Exact duplicate of rec3 file, confirmed by SHA256 |
| rec4 R00000009_00000000.OSC | 91.6480 s | 0 s | User confirmed indoor and outdoor speech in extracted clips |
| rec5 R00000001_00000000.OSC | 62.6688 s | 1.5360 s | Buffered input; manual viewing, audio enabled, two unlocks, hang-up |
| rec6 R00000002_00000000.OSC | 91.1872 s | 2.8160 s | Buffered input; cleanly captured incoming-call event |
| rec7 R00000006_00000000.OSC | 95.3856 s | 0.1536 s | Buffered manual viewing; operator reports outdoor speech before Accept, indoor speech after Accept, then outdoor speech during conversation |
| rec8 R00000007_00000000.OSC | 42.4448 s | 0.7680 s | Manual viewing and Accept; indoor speech only; white-channel 1 kohm plus 47 nF filter reported fitted |
| rec9 R0000000F_00000000.OSC | 51.4048 s | 0 s | Manual activation three times without Accept; no monitor-origin sound reported; red measurement lead disconnected |

Timeline duration includes known missing sample intervals; recorded payload duration is shorter where gaps exist. Slight differences from the monotonic elapsed summary can reflect timestamp quantisation. The streaming files listed above have a clean SESSION_END. Their gap counts must still be considered separately from the error counter.

Evidence plots and summaries:

- [rec1 notes](../recordings/rec1/analysis/NOTES.txt)
- [rec2 plot](../recordings/rec2/analysis/R00000006_waveforms.png) and [summary](../recordings/rec2/analysis/R00000006_summary.json)
- [rec3 plot](../recordings/rec3/analysis/R00000008_waveforms.png) and [summary](../recordings/rec3/analysis/R00000008_summary.json)
- [rec4 audio activity](../recordings/rec4/analysis/R00000009_audio_activity.png)
- [rec5 plot](../recordings/rec5/analysis/R00000001_waveforms.png) and [summary](../recordings/rec5/analysis/R00000001_summary.json)
- [rec6 plot](../recordings/rec6/analysis/R00000002_waveforms.png) and [summary](../recordings/rec6/analysis/R00000002_summary.json)
- [rec7 plot](../recordings/rec7/analysis/R00000006_waveforms.png) and [summary](../recordings/rec7/analysis/R00000006_summary.json)

## Control behaviour established so far

### Incoming call

In rec6, white falls from its approximately 4.58 V bias to near zero. It remains below 1 V from 41.2559 to 41.3626 s, approximately 106.7 ms. Red crosses 8 V at 41.4138 s and rises toward 13.45 V. There are no missing samples during this initial event.

Thus red power-up follows the start of the white low excursion by approximately 158 ms. Rec2 and rec3 show the same sequence with the older, more heavily loaded input; rec3 white was below 1 V for approximately 120 ms.

This strongly associates the low excursion with an incoming visitor call. Measuring voltage on a shared connection does not independently prove which device sinks the current or exclude a multi-step handshake. Design a high-impedance receiver first; do not reproduce the call by directly shorting white to ground.

### Manual viewing

In rec5 red powers up at approximately 8.2056 s without a white near-zero call pulse. White has a smaller startup disturbance and returns toward its approximately 4.60 V bias. This separates manual activation from the observed incoming-call indication.

### Accept and conversation

The user reports that before Accept they can hear outside sounds, but the visitor cannot hear them. After Accept, indoor speech is transmitted too. No separate large, sustained white-line DC command for Accept has been identified. Button times were not recorded independently, and some candidate intervals contain gaps.

The working model is that the monitor enables its microphone transmit path on Accept while leaving receive audio enabled. This is sufficient to specify the intended user experience; it is not proof that all electrical aspects of the line remain unchanged.

### Unlock

Pressing Unlock is consistently associated with white rising near the operating red supply. In buffered rec5, white is approximately 13.29 V during the high state while red is approximately 13.45 V. Using a 12 V threshold, the two rec5 high intervals are approximately 2.0087 and 2.0086 s. They occur before the first recording gap. At an 8 V threshold, intervals are longer because of the decay after the high plateau.

Rec6 repeats this behaviour. Start/end crossings above 12 V are 72.6359 to 74.6444 s and 77.1311 to 79.1397 s, approximately 2.0085 and 2.0086 s edge-to-edge. Both contain marked internal gaps, so the missing plateau samples are not assumed constant.

An early audible estimate suggested one second, but recorded results support approximately two seconds in these experiments. The user also reports quiet relay clicks and loss of conversation audio during unlock. The silence could result from deliberate muting, the high drive disturbing the shared audio path, or both. The voltage traces alone do not separate these effects.

Do not implement unlock by directly connecting white to red. We have not measured driver current, output impedance, or the panel's acceptable injection current. A controlled and protected driver must be designed.

### Hang-up

Buffered rec5 red crosses below 8 V around 50.9404 s; rec6 does so around 84.0183 s. It returns to approximately 4.4 V, not zero. No comparable large white pulse has been established as necessary for hang-up.

The intended behaviour is to stop conversation and deactivate the panel. Whether the final replacement must actively reproduce the standby voltage, or can use a different bias arrangement, remains an electrical design question.

## Both speech directions are verified

The user listened to these rec4 session 9 clips and identified the sources:

| Interval | User confirmed source | Clip |
|---|---|---|
| 30 to 36 s | Speaking into the indoor monitor | [Indoor speech 1](../recordings/rec4/analysis/R00000009_00000000_listen_30-36s.wav) |
| 45 to 51 s | A repeated indoor-monitor utterance | [Indoor speech 2](../recordings/rec4/analysis/R00000009_00000000_listen_45-51s.wav) |
| 57 to 64 s | Speaking outside into the ML-20HD | [Outdoor speech](../recordings/rec4/analysis/R00000009_00000000_listen_57-64s.wav) |

This confirms that both microphone sources appear on the same white conductor in this installation. It does not determine the exact two-wire hybrid, bias, or coupling circuit inside either device.

The overview plots span many seconds and 0 to 14 V. Audio fluctuations are difficult to see at that scale; a flat-looking trace does not mean audio is absent. Raw samples retain the smaller variations. DC removal, filtering, a millivolt view, and audio export made them audible.

The preview exporter uses a first-order 150 Hz high-pass and a first-order 3 kHz low-pass. These are listening aids, not an anti-alias filter or a speech classifier. It deliberately silences large control excursions and missing-data intervals with margins. Gain is fixed, not automatic. Early previews used a narrow voltage mask that could also mute loud audio; the current version widens the mask. Original OSC files remain unchanged.

The old setup had approximately 9 to 10 mV RMS of filtered idle variation in selected windows. The buffered rec5 has approximately 19 to 22 mV RMS in selected quiet-looking windows. This includes circuit noise and interference; fitting the buffer has not by itself demonstrated improved audio signal-to-noise ratio. Direction, noise, and intelligibility must be validated separately.

### Follow-up audio sequence in rec7

The operator reports this order: manually activate viewing, speak into the ML-20HD with the indoor microphone muted, press Accept and speak into the monitor, then speak into the ML-20HD during the accepted conversation. Exact action times were not marked. This order is an operator note, not an automatic source classification.

Session 6 in rec7 spans 95.3856 s, validates with a clean end marker, and has one missing interval at 16.3328 to 16.4864 s. Red crosses above 8 V at 14.5649 s and below it at 84.9536 s. There are no sustained near-zero white call pulses or white excursions above 8 V, consistent with manual viewing and no unlocks. White is normally near 4.58 V; operating red is normally near 13.49 V.

Audio activity is present in later sections. Listen to the time-labelled clips before assigning exact stage boundaries:

- [18 to 42 s](../recordings/rec7/analysis/R00000006_listen_18-42s.wav)
- [42 to 49 s](../recordings/rec7/analysis/R00000006_listen_42-49s.wav)
- [50 to 60 s](../recordings/rec7/analysis/R00000006_listen_50-60s.wav)
- [67 to 77 s](../recordings/rec7/analysis/R00000006_listen_67-77s.wav)
- [Full preview](../recordings/rec7/analysis/R00000006_00000000_white_AC_preview.wav)

The preview uses the same fixed gain of 0.5 as the rec5 and rec6 exports and has no digital clipping. Missing samples and margins are silenced explicitly.

There is an unexplained common rise in recorded red and white levels around 60 to 70 s. Calibrated red peaks at approximately 15.63 V. It has not been established whether this is an actual panel-supply change or a measurement reference, ground, supply, or wiring effect. Do not infer a new control command or normal panel supply requirement from it. Validate with a simultaneous multimeter observation if it recurs. The waveform alone cannot establish the cause.

### Rec7 follow-up on obscured speech

The operator reports no audible content in the 18 to 42 s clip, useful audio in 42 to 49 s, speech becoming obscured toward the end of 50 to 60 s, interference around 60 to 67 s where the utterance should continue, and useful audio again in 67 to 77 s. These comments do not yet assign an exact indoor/outdoor source to every clip.

The earlier excerpts skipped 60 to 67 s; the original recording did not. Raw sample indices show no missing data anywhere from 40 to 80 s. The ADC samples do not reach either numeric rail; the listening export has no digital clipping and does not mute this interval. Therefore the gap in the excerpt list must be distinguished from the interference audible in the recorded interval. No automatic recovery of obscured words is claimed.

The operator did not touch the scope, buffer, cables, or power connection, but notes that doors opening or closing might have caused airflow near the wires. Airflow, microphone wind noise, contact movement, supply change and measurement reference effects are not established causes. Red and white both exhibit a baseline rise around the problem interval; this deserves a simultaneous multimeter comparison before interpreting it as a control signal.

Continuous listening files preserve the previously omitted interval:

- [50 to 70 s continuous](../recordings/rec7/analysis/R00000006_listen_continuous_50-70s.wav)
- [42 to 80 s continuous](../recordings/rec7/analysis/R00000006_listen_continuous_42-80s.wav)
- [60 to 67 s alone](../recordings/rec7/analysis/R00000006_listen_continuous_60-67s.wav)
- [42 to 80 s with an 80 Hz high-pass](../recordings/rec7/analysis/R00000006_listen_42-80s_HP80.wav)
- [Diagnostic results](../recordings/rec7/analysis/R00000006_audio_followup.json)

The first three are exact crops of the original filtered preview at the same gain. The 80 Hz version replaces its 150 Hz first-order high-pass with an 80 Hz first-order high-pass, retaining the same 3 kHz low-pass and fixed gain. It is a listening comparison, not reconstructed speech or proof of a hardware cause.

### Experimental noise reduction for rec7

The operator requested reducing background noise. The original captures and listening preview are preserved. The comparison tool tools/intercom_denoise.py estimates a fixed noise spectrum from 20 to 40 s, within the operator-reported quiet interval. It applies spectral power subtraction with a magnitude floor, smoothing and soft band limiting, retaining the existing time axis and fixed level without peak normalisation or automatic gain boost.

The mild variant lowers the RMS level in the selected quiet interval by approximately 5.6 dB; the stronger variant lowers it by approximately 8.0 dB. These are quiet-window level changes, not measured speech intelligibility or independently established signal-to-noise improvements. Neither export digitally clips. The changing interference in 60 to 67 s is reduced less than the steady background, and obscured words are not reconstructed. Quiet speech can be suppressed or acquire processing artifacts; operator listening is required before selecting a preferred version.

- [Original 42 to 80 s](../recordings/rec7/analysis/R00000006_listen_continuous_42-80s.wav)
- [Mild reduction 42 to 80 s](../recordings/rec7/analysis/R00000006_denoise_mild_42-80s.wav)
- [Stronger reduction 42 to 80 s](../recordings/rec7/analysis/R00000006_denoise_stronger_42-80s.wav)
- [Mild reduction 50 to 70 s](../recordings/rec7/analysis/R00000006_denoise_mild_50-70s.wav)
- [Stronger reduction 50 to 70 s](../recordings/rec7/analysis/R00000006_denoise_stronger_50-70s.wav)
- [Processing settings and measured levels](../recordings/rec7/analysis/R00000006_denoise_report.json)

Reproduce with:

```powershell
python -B tools/intercom_denoise.py recordings/rec7/analysis/R00000006_00000000_white_AC_preview.wav
```

This is an offline listening experiment, not a selected real-time Pi algorithm. It does not replace analog protection, anti-alias filtering, stable grounding, or acoustic echo cancellation.

### Rec8 indoor microphone test with the output filter

**Follow-up on interrupted words:** the operator confirms the voice is only partly audible despite clearer sound. In the 20 to 36 s interval, 0.6656 s of samples are actually missing and the current preview mutes approximately 0.7856 s including short guards. These losses can remove syllables; filtering cannot reconstruct them. In the current firmware source, recording_ingest increments known dropped sets when rec_queue_push reports a full queue. This identifies a recording backlog failure, not an established failure of the microphone or the RC filter. The specific source of the consumer delay requires timing diagnostics; SD latency, sync and main-loop/UI service must be distinguished rather than blaming one without measurement.

**Operator listening result:** the rec8 audio sounds much better and the voice is clearer than rec7, although background noise remains. This is user-confirmed subjective improvement with the reported 1 kohm plus 47 nF white-channel filter. It supports retaining that filter for the current measurement prototype; it does not establish a numerical speech SNR improvement or remove the recording gaps.

The operator reports manual activation, pressing Accept or Talk, then speaking only into the indoor monitor. They answered yes when asked whether the white-channel output filter had been changed to 1 kohm plus 47 nF; this is recorded as fitted for this session. Exact Accept and utterance timestamps remain unmarked.

The file recordings/rec8/R00000007_00000000.OSC validates with a clean end marker and spans 42.4448 s of sample timeline. It contains 0.7680 s of known losses across 14 gap intervals, including several at approximately 23 to 29 s where audio activity occurs. No ADC samples hit codes 0 or 4095. Red crosses above 8 V at 8.8927 s and below 8 V at 36.1191 s. Operating red is approximately 13.47 V, with a recorded maximum of approximately 13.60 V; the large rec7 voltage rise does not recur during this shorter session. White is normally near 4.56 to 4.58 V. There are no sustained near-zero call pulses or white high unlock pulses.

Using the same offline 150 Hz high-pass plus 3 kHz low-pass and the same 20 to 22 s interval, the median short-window AC RMS is approximately 20.10 mV in rec7 and 11.14 mV in rec8, a reduction of approximately 5.13 dB. This is an encouraging noise-level comparison, not a controlled A/B test or proof of improved speech intelligibility: source level, conditions, and actual silent periods were not independently standardised.

- [rec8 waveform and audio activity plot](../recordings/rec8/analysis/R00000007_waveforms.png)
- [rec8 summary](../recordings/rec8/analysis/R00000007_summary.json)
- [Full rec8 preview](../recordings/rec8/analysis/R00000007_00000000_white_AC_preview.wav)
- [18 to 32 s listening clip](../recordings/rec8/analysis/R00000007_listen_18-32s.wav)
- [20 to 36 s listening clip](../recordings/rec8/analysis/R00000007_listen_20-36s.wav)
- [29 to 35 s listening clip](../recordings/rec8/analysis/R00000007_listen_29-35s.wav)

For this export, tools/intercom_audio.py was revised to avoid silencing 250 ms on both sides of every data gap. Large control excursions retain that guard; gaps now use a 5 ms guard with short fades, and filter restart settling is 5 ms. This preserves more adjacent available speech. Actual missing data remain silence; no missing samples are reconstructed. Earlier exports were not regenerated, so compare previews with their individual processing metadata. The rec8 preview has no digital clipping.

### Rec9 viewing without Accept and with red measurement disconnected

The operator reports three manual activations without pressing Accept or Talk, no monitor-origin sound, and possible ML-20HD sounds. The red measurement lead was disconnected. Therefore PC2 is not evidence of panel power in this session; its near-zero samples must not be interpreted as the panel losing power. Only PC0 white is used to interpret intercom behaviour.

The recording R0000000F_00000000.OSC spans 51.4048 s and validates with a clean end marker, zero known drops and zero unknown gaps. Timing anchors match the sample indices. White has no ADC rail hits and normally rests near 4.59 V. There is a larger disturbance around 7 to 9 s, reaching approximately 6.98 V. No near-zero incoming-call pulse or white high unlock pulse is observed. White alone does not provide independent timestamps for all three reported manual activations.

After approximately 10 s, filtered white-channel AC activity is fairly steady: median short-window RMS is approximately 13.68 mV over 10 to 30 s and 13.45 mV over 30 to 50 s. Large speech-like bursts were not identified numerically in those periods. This does not establish that outdoor audio is absent; low-level ambient sound may be mixed with measurement noise. The session can be useful for listening comparisons but is not a guaranteed electronics-only noise reference.

The file still records three ADC channels at 10 kS/s each. Disconnecting the red measurement lead does not reduce sample throughput or queue size. Rec9 alone cannot establish causality. However, the operator subsequently reports repeating tests and observing interruptions with the red measurement lead connected. This repeated association is important evidence and must be investigated rather than dismissed because throughput is unchanged. The operator clarifies that these later interruptions were on the scope screen only and the runs were not saved. They are not evidence of additional DATA loss; matched saved runs are needed to test that relationship. The disconnected PC2 channel reaches zero often; those samples are expected for a ground-referenced unused input and are not classified as intercom clipping.

- [rec9 white waveform and audio activity plot](../recordings/rec9/analysis/R0000000F_waveforms.png)
- [rec9 summary](../recordings/rec9/analysis/R0000000F_summary.json)
- [Full white-channel preview](../recordings/rec9/analysis/R0000000F_00000000_white_AC_preview.wav)
- [5 to 11 s around the larger disturbance](../recordings/rec9/analysis/R0000000F_listen_5-11s.wav)
- [10 to 30 s listening comparison](../recordings/rec9/analysis/R0000000F_listen_10-30s.wav)
- [30 to 50 s listening comparison](../recordings/rec9/analysis/R0000000F_listen_30-50s.wav)

### Reported association between red measurement and interruptions

On 27 October the operator reports several tests in which connecting the red measurement lead produces interruptions. They clarify that the interruptions were seen only on the oscilloscope screen and those runs were not saved. Therefore this new observation establishes a display responsiveness issue, not additional proven sample loss or disturbance of live Slinex audio. Existing known sample gaps show an immediate queue-backlog mechanism in the current firmware; they do not establish why that backlog appears. Input-dependent display work, SD variability and electrical conditions remain possible contributors, not diagnoses.

Distinguish missing DATA samples from audible analog interference or UI delays. A grounded input and a driven input can change electrical or display behaviour while the acquisition data rate remains identical. Verify the buffer's second channel and common ground against the schematic; do not increase capacitors arbitrarily as a cure for digital gaps. Leave the original red connection between monitor and panel intact: any temporary disconnection refers only to the oscilloscope's RED_IN measurement branch.

A useful controlled comparison is matched runs with the red measurement branch disconnected versus connected, keeping white, ground, display, recording length and call actions unchanged. Label the runs and validate actual GAP counts before deciding whether live audio is also affected. If needed, a third bench comparison with a known lab source at RED_IN can separate response to a driven channel from response specific to the intercom connection; the scope buffer must stay powered and the source must enter through the full divider.

### Blackboard recs1 comparison and three activation candidates

The folder recordings/blackboard recs1 contains seven separate sessions: 09, 0A, 0B, 0C, 0D, 0E and 10 (hexadecimal session IDs). All validate and end cleanly. Red connection status below is inferred from the voltage on PC2; no operator wiring label for each individual file was supplied.

| Session | Timeline seconds | Known missing seconds | Observed channel condition |
|---|---:|---:|---|
| 09 | 65.6384 | 1.3312 | White near 4.56 V; red includes operating supply |
| 0A | 58.0608 | 0.7168 | White near 4.59 V; red includes operating supply, then falls to near zero |
| 0B | 57.7024 | 0 | White near 4.59 V; red apparently disconnected |
| 0C | 18.4832 | 0.8704 | White near 4.60 V; red near standby supply |
| 0D | 18.0736 | 0 | White near 4.60 V; red apparently disconnected |
| 0E | 66.7136 | 0.0512 | White near zero for much of run, then rises near end; red falls near end |
| 10 | 46.0288 | 0.1024 | White near 4.59 V; red apparently disconnected |

Session 10 is the best match for three manual activations. It has three reproducible positive white bias disturbances around 13.66, 25.76 and 37.33 s, each lasting approximately 0.36 s above a 4.72 V threshold applied to 10 ms block means. Alternating negative dips appear around 6.29, 21.74, 31.68 and 43.20 s. The pattern is consistent with repeated on/off toggling, possibly with viewing already active before the first dip. These are candidate action times, not independently verified button timestamps. None of the three positive candidates overlaps the two recording gaps.

The interpretation is supported by sessions 09 and 0A, where similar positive white disturbances coincide with recorded red power-up. A negative disturbance in 09 coincides with red power-down. This is stronger evidence for an electrical switching transient than the waveform alone, but does not prove its exact origin or exclude simultaneous audible clicks. The small dips do not reach near zero like the visitor-call pulse.

The results strengthen the operator's reported association between connecting red and interruptions. In particular, 0C loses 0.8704 s with red voltage present while the similarly short 0D has near-zero red and no loss. However, 10 loses 0.1024 s despite apparently disconnected red. Red connection is therefore not a necessary condition for missing samples, and these runs do not identify the underlying firmware, UI, storage or electrical cause. Session 0E has different/possibly changing input conditions and should not be treated as a matched white+red trial.

- [All seven waveform plots](../recordings/blackboard%20recs1/analysis/recording_comparison.png)
- [Session 10 activation candidates](../recordings/blackboard%20recs1/analysis/R00000010_activation_candidates.png)
- [Detailed analysis notes](../recordings/blackboard%20recs1/analysis/ANALYSIS_NOTES.txt)
- [Machine readable comparison](../recordings/blackboard%20recs1/analysis/recording_comparison.json)
- [Candidate 1 audio 13 to 15 s](../recordings/blackboard%20recs1/analysis/R00000010_candidate_activation_1_13-15s.wav)
- [Candidate 2 audio 25 to 27 s](../recordings/blackboard%20recs1/analysis/R00000010_candidate_activation_2_25-27s.wav)
- [Candidate 3 audio 37 to 39 s](../recordings/blackboard%20recs1/analysis/R00000010_candidate_activation_3_37-39s.wav)

## Intended audio routing

Routing follows user actions and call state. It does not depend on deciding whether somebody is speaking.

| State | White audio to indoor speaker | Indoor microphone injected onto white |
|---|---|---|
| Standby | Disabled | Disabled |
| Incoming call before Accept | Enabled | Muted |
| Manual viewing before Talk | Enabled | Muted |
| Accepted conversation | Enabled | Enabled |
| Unlock interval | Muted | Muted |
| After unlock settles | Restore the previous state | Restore the previous state |
| Hang-up | Disabled | Disabled |

During unlock, keep the measurement/monitoring path protected and allow the line to return to its normal range before restoring conversation. The settling interval has not yet been chosen; use measured behaviour rather than an invented exact delay.

Voice activity detection may later help recording or noise handling, but it is not the mechanism for Accept. VAD also cannot identify indoor versus outdoor speech just from the shared wire. For live full-duplex operation, a transmit reference can support echo cancellation; the analog interface may also require a hybrid or subtraction arrangement. Avoid routing our own transmitted microphone signal straight back into the indoor speaker. Microphone/speaker acoustics must be tested for feedback.

If a second call arrives while one entrance is active, the application must distinguish call notification from active audio selection. Do not electrically tie the two entrances' audio lines or supplies together.

## MCP6022 measurement buffer

This is a measurement front end for the STM32 oscilloscope, not the finished Pi audio or control interface. It does not provide galvanic isolation, speaker drive, microphone injection, or certified surge protection.

The selected chip is MCP6022-I/SN, SOIC-8 on an adapter. Power it from STM32 3.3 V. The panel's red wire is a measured input, never the chip supply.

| Chip pin | Connection |
|---|---|
| 1 | Output A; direct feedback to pin 2; through 100 ohm to PC0 |
| 2 | Feedback from pin 1 |
| 3 | White divider junction, node A |
| 4 | Common GND |
| 5 | Red divider junction, node B |
| 6 | Feedback from pin 7 |
| 7 | Output B; direct feedback to pin 6; through 100 ohm to PC2 |
| 8 | 3.3 V |

For each input, the upper series chain is 1 Mohm plus 33 kohm plus 1 kohm. A 220 kohm resistor connects the junction to GND. This gives 1.254 Mohm input resistance and nominal x5.7 conversion. Node A/B means a solder junction, not an extra component. They must remain separate.

Four 1N4148 diodes are used instead of the originally proposed BAS70-04 devices. For each junction, one diode has anode at GND and cathode/stripe at the junction; the other has anode at the junction and cathode/stripe at 3.3 V. The silicon diodes clamp less tightly than the Schottky alternative, so this is not an identical protection substitution.

Connect a 100 nF ceramic capacitor and a 1 uF capacitor in parallel across pins 8 and 4. Place the ceramic closest to the chip. An electrolytic 1 uF is acceptable: positive to pin 8 and negative/stripe to pin 4. Each ADC connection has 100 ohm in series and 1 nF to GND after that resistor; feedback is taken before the resistor.

Remove the original PC0 and PC2 divider pairs when fitting this replacement; leave PC1 unchanged. Do not put the two dividers in series or leave the old 10 kohm shunts on the new outputs. The nominal x5.7 scaling matches existing firmware, so no nominal multiplier change or reflash is required for this exact circuit. Verify actual channel calibration.

Assembly evidence and troubleshooting:

- Initially pin 4 was not connected to GND. Abnormal voltages around 2.5 V were observed; connecting ground resolved that fault.
- A separate solder bridge between pins 3 and 4 pulled node A down. It measured approximately 2 ohm; removing the bridge resolved the fault.
- The user then reported approximately 3 mV on CH1 and CH3 without input, consistent with a small near-zero offset.
- A 3.4 V test produced approximately 0.6 V at the divider/buffer output and approximately 3.4 V on the corresponding scope channel, consistent with x5.7 scaling.
- The completed buffered setup was used for rec5 and rec6. DC behaviour was demonstrated; full bandwidth, oscillation, surge, and power sequencing qualification were not.

Bench test expectations are output = input / 5.7: 3.3 V becomes approximately 0.579 V; 13.6 V becomes approximately 2.386 V. Use a current-limited lab source only at the input terminal before the full resistor chain. Keep the intercom disconnected during bench tests. Keep the buffer powered while external signals are attached, and remove those signals before removing buffer power.

- [Full soldering and bench-test instructions](MCP6022_SOLDERING_GUIDE.txt)
- [PNG schematic](../hardware/mcp6022/mcp6022_1n4148_schematic.png)
- [Zoomable SVG schematic](../hardware/mcp6022/mcp6022_1n4148_schematic.svg)

### Proposed hardware audio filtering experiment

Status: originally proposed after rec7; the operator reports fitting the 1 kohm plus 47 nF white output filter for rec8. The original 100 ohm plus 1 nF output has a nominal 1.59 MHz corner and is not an audio-band anti-alias filter. Rec8 provides an initial recording comparison, not full analog validation.

A first controlled experiment is to replace only the white-channel output resistor with 1 kohm and its ADC-side capacitor with 47 nF ceramic. Connect MCP6022 pin 1 through 1 kohm to PC0, and 47 nF from PC0 to common GND. Keep feedback directly from pin 1 to pin 2 before the resistor. Leave the high-resistance divider, clamps, supply bypass capacitors, and red channel unchanged.

The calculated corner is approximately 3.39 kHz. The nominal response is about -0.36 dB at 1 kHz, -2.51 dB at 3 kHz and -5.02 dB at 5 kHz. It preserves DC and slow call/unlock behaviour while reducing higher-frequency components. A single pole provides limited rejection around the 5 kHz Nyquist frequency and is not a complete high-performance anti-alias design. Actual response, stability and ADC settling require bench verification.

Power off before modifications and test with a known DC voltage before reconnecting the intercom. Do not remove the 100 nF plus 1 uF supply bypass capacitors or place 47 nF at the high-resistance input node. Compare the same speech/silence test before and after; this does not establish a cure for the unexplained rec7 baseline rise.

For the eventual Pi interface, retain an independent DC/control monitoring path and design a protected AC-coupled audio path with suitable bias, voice-band filtering and codec interfacing. A series coupling capacitor alone is not enough: unlock edges can pass through it. In-band noise, unstable references, contact faults and analog overload require diagnosis rather than arbitrary extra capacitance.

## Proposed Raspberry Pi architecture

### Initial platform and audio peripherals

The initial prototype will use the existing Raspberry Pi 3, supply and display. Exact Pi variant, installed OS and display interface have not yet been confirmed. Start with one ML-20HD receive-audio path, then transmit and controls; video is a later integration step.

A provisional low-cost audio arrangement is a USB audio adapter for intercom-side capture/playback, a separate USB microphone for the indoor user, and powered AUX speakers driven from the Pi's analog audio output where present. A CM108B-based adapter has a mono ADC, so one adapter alone cannot independently capture both the white-line receive signal and an indoor microphone. The additional microphone provides the second capture source. Multiple USB audio clocks and acoustic echo control require software validation. An interface with multiple independent inputs/outputs is an alternative; no specific retail device has been selected.

All intercom-side audio connections still require the designed protection, level matching and DC/control separation. Neither the existing scope buffer output with its DC bias nor the panel wires should be connected directly to an unverified sound-card microphone input. A generic headset adapter is not a finished intercom interface.


```text
ML-20HD ---------------------- dedicated analog interface -----+
                                                              |
Building line -- MCLAUT adapter -- dedicated analog interface --+-- Raspberry Pi 5
                                                                  display and controls
                                                                  microphone and speaker
                                                                  local network access
```

The Pi supplies application logic, display, recording, networking, and audio processing. Dedicated interface hardware handles the intercom voltages. GPIO must not directly contact the panel wires.

| Block | Requirement | Still to establish |
|---|---|---|
| Panel supply | Independently controlled, protected supply for each connection | Current in standby, viewing, IR illumination, conversation and unlock; exact standby bias circuit |
| Call detector | High-impedance receiver for white low excursions | Input thresholds, hysteresis, filtering, and any handshake dependency |
| Unlock driver | Timed, current-limited drive toward the operating supply | Source current, switching topology, isolation from audio circuitry, fault behaviour |
| Audio receive | DC blocking, suitable input impedance, protected codec input | Signal amplitudes and impedance versus frequency under normal loading |
| Audio transmit | Microphone/codec output coupled through a controlled driver | Injection level, source impedance, coupling topology and mute behaviour |
| Audio processing | State-dependent mute, playback, echo control | Full-duplex performance and delay |
| Video capture | Decoder/capture hardware matched to installed video mode | Actual panel format, termination, capture compatibility and simultaneous-channel needs |
| Power and grounding | Stable Pi supply, analog rails and controlled return paths | Ground potential differences, wiring layout, fault and power sequencing behaviour |

A USB or I2S audio codec is a possible component of the eventual design, not a direct replacement for the analog interface. PAL capture is a possible route if the installed panel is configured to CVBS; retaining AHD requires an appropriate decoder/capture solution. No specific codec or capture device has been selected or tested.

Each entrance should have independent detection and control. The building adapter may present a compatible four-wire monitor interface, but identical connectors do not prove identical video format, bias, pulse requirements or source impedance. Verify it independently before applying the ML-20HD driver design.

### Capturing HD video from the ML-20HD

The documented 720p and 1080p modes are AHD analog HD, not higher-resolution CVBS PAL. A PAL/EasyCAP-type input cannot be assumed to decode them, and a USB capture device advertised as 1080p may merely refer to its output or HDMI capability. Verify AHD support on the input side and the exact panel frame rate.

Two possible paths are AHD directly into a USB UVC capture module, or AHD into an HDMI converter followed by an HDMI-to-USB capture device. The Pi HDMI connector is for display output, not camera capture. CSI bridge designs exist but require compatible decoder hardware and drivers; a decoder chip with a MIPI output alone is not a ready-to-use Pi camera module.

For Pi 3 USB 2.0, prefer a capture device that compresses to MJPEG or another supported compressed format before USB transfer. Raw 1080p30 YUY2 would require approximately 124 MB/s, exceeding USB 2.0 capacity. Raw 720p30 YUY2 is about 55 MB/s and is also impractical on the shared bus in normal use. Start validation at 720p with a modest frame rate; Linux/UVC support alone does not guarantee Pi 3 live-display performance or acceptable latency.

Manufacturer examples, not tested purchases:

- [Brando A2UC-XP AHD-to-USB module](https://www.bus-dvr.com/cn/ahd-zhuan-usb-shi-pin-cai-ji-ka-xing-hao-a2uc-xp.html) lists AHD 720p/1080p inputs, UVC/UAC, Linux support, and MJPEG/YUY2 output. Confirm the exact model/revision, USB 2.0 modes and panel frame-rate support before buying.
- [ezcap267P](https://www.ezcap.com/ezcap267p) explicitly lists AHD input, UVC and Linux, but specifies USB 3.0. Do not assume suitability for Pi 3 without confirmed USB 2.0 fallback formats.
- [Clinton CE-HDA2HDMI](https://www.clintonelectronics.com/products/accessories/signal-converters/ce-hda2hdmi/) is an example of an actual AHD-to-HDMI converter. It still requires a separate USB capture device for the Pi.

Connect only the verified video input and video ground; do not feed video through the slow oscilloscope divider. If a module offers a 5 V camera power output, leave it disconnected from the ML-20HD red supply. Verify video termination and grounding: paralleling two terminated receivers can overload the panel output, so retaining the monitor while capturing may require a suitable loop-through or distribution buffer.

## Application state model

Proposed states are STANDBY, RINGING, VIEWING, TALKING and UNLOCKING. UNLOCKING remembers the prior state and restores it after the line settles, unless Hang-up or a fault requests standby.

- A qualified incoming-call event identifies the entrance, notifies the user, activates viewing as appropriate and enables receive audio while the indoor microphone remains muted.
- Manual viewing selects an entrance and activates its viewing path without requiring a panel call pulse.
- Accept or Talk enables the microphone transmit path and leaves receive audio active.
- Unlock mutes conversation, requests one bounded unlock interval through the protected driver and waits for the line to settle before restoring the prior audio state.
- Hang-up mutes audio immediately and requests panel deactivation through the designed supply/control interface.

These are software requirements, not implemented Pi functionality. Initial outputs should be inactive; reboot, application failure, and loss of Pi power must not cause an unintended unlock. The electrical design must enforce the default independently of normal application operation.

## Remaining work before a build guide can claim completion

1. Stabilise continuous recording or explicitly bound losses for all tests. rec5 and rec6 contain marked drops despite clean end markers.
2. Determine the bias source, DC current and voice-frequency impedance at the monitor/panel interface. The earlier 47 kohm estimate is only a combined DC model.
3. Design protected power, receive, transmit and unlock circuits. Use a bench surrogate and current limits before connecting the panel; do not test by shorting white to ground or red.
4. Select and verify a Pi-compatible audio codec and video decoder/capture path.
5. Implement and test the state model, including receive-before-Accept, microphone mute, unlock mute and restoration, hang-up, and default inactive outputs.
6. Verify the building adapter separately with a documented model/terminal map and measured behaviour. Keep the apartment and building paths independently controllable.
7. Validate two-way speech, echo/feedback, call reliability, video startup, actual lock actuation and recovery from power interruptions. Record firmware versions, wiring revisions and results.
8. Turn validated steps into an installation guide with a final schematic, BOM, software setup, calibration, enclosure layout and troubleshooting procedure.

## Reproducing the analyses

Use the full path so reused session numbers do not collide:

```powershell
python -B tools/osc_recording.py recordings/rec6/R00000002_00000000.OSC
python -B tools/intercom_audio.py recordings/rec6/R00000002_00000000.OSC --gain 0.5
```

The audio script writes derived files under that recording folder's analysis directory. It processes the first recorded channel as white; confirm wiring before reusing it. Its masks and sample rate assumptions are specific to these experiments, and it currently requires a completed recording. It is not a general-purpose audio extraction library.

For multiple OSC2 segments, supply all segments from one session to the validator. Do not combine unrelated session files in one validation call. For legacy rec1 snapshots use tools/read_osc.py instead. Original recordings should remain unchanged; store plots, summaries, filtered audio and operator notes separately.

For future tests, record the physical channel wiring, hardware revision, diode type, reference ground, firmware, divider/calibration, exact actions and approximate times. Audio previews with deliberate muting must not be treated as evidence of silence in the original line.

## Primary references

- [Slinex ML-20HD family manual](https://cdn.slinex.com/Manuals_en/Slinex_ML-20HD_v2_User_Manual_eng.pdf): wire functions and documented video modes; confirm applicability to the installed revision.
- [Slinex SL-07IPHD manual](https://cdn.slinex.com/Manuals_en/Slinex_SL-07IPHD_User_Manual_eng.pdf): manufacturer manual location; the full large PDF was not retrieved during this investigation.
- [Microchip MCP6022 datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/20001685E.pdf): pinout, supply limits, input protection and capacitive load guidance.
- [Nexperia 1N4148 datasheet](https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf): diode characteristics and package polarity.
- [Nexperia BAS70-04 datasheet](https://assets.nexperia.com/documents/data-sheet/BAS70-04.pdf): originally considered alternative, not the discrete 1N4148 wiring variant.
- [Tektronix probe loading explanation](https://www.tek.com/en/documents/application-note/how-oscilloscope-probes-affect-your-measurement): why measurement resistance and capacitance can alter a circuit.
