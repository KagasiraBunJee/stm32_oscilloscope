# Measurement input pins

This firmware provides three logical oscilloscope channels. Each channel has
two separately wired physical input paths: a direct 0–3.3 V path and a
47 kΩ/10 kΩ positive 0–14 V divider path. Select the path for each enabled channel in the UI.
There is no analog multiplexer between the two pins.

## Pin assignment

| Logical channel | UI route | STM32 pin | ADC input | Signal at MCU pin | Description |
|---|---|---|---|---|---|
| CH1 | Direct | PA1 | ADC123_IN1 | 0–3.3 V only | Direct low-voltage measurement input for CH1 |
| CH1 | 14V | PC0 | ADC123_IN10 | 0–2.46 V nominal | CH1 divider midpoint |
| CH2 | Direct | PA2 | ADC123_IN2 | 0–3.3 V only | Direct low-voltage measurement input for CH2 |
| CH2 | 14V | PC1 | ADC123_IN11 | 0–2.46 V nominal | CH2 divider midpoint |
| CH3 | Direct | PA3 | ADC123_IN3 | 0–3.3 V only | Direct low-voltage measurement input for CH3 |
| CH3 | 14V | PC2 | ADC123_IN12 | 0–2.46 V nominal | CH3 divider midpoint |

All inputs are sampled as unsigned 12-bit values from 0 to 4095. The ideal
direct-path conversion, before per-path calibration, is:

`Vinput = ADC_raw × 3.3 V / 4095`

## Direct 0–3.3 V connection

Connect the signal through the documented protection/filter circuit to PA1,
PA2, or PA3. Connect the signal source ground to board GND. The source must be
ground-referenced and must remain between 0 V and 3.3 V at all times.

Recommended first test:

- CH1 UI route: `Direct`
- Signal: 1 kHz sine wave, 1 Vpp
- DC offset: 1.65 V
- Signal output: protected PA1 path
- Signal ground: board GND

The direct protection circuit is `signal -> 100 ohm -> ADC pin`, with a 1 nF
C0G capacitor to ground and external Schottky clamps to 0 V and 3.3 V. Do not
rely on the STM32 internal protection diodes for normal operation.

> **Warning:** Never connect 5 V, 12 V, ±15 V, a negative voltage, or mains
> voltage directly to PA1, PA2, or PA3. STM32 ADC pins are not overvoltage
> tolerant.

## Installed positive 0–14 V dividers (firmware 1.1)

Each input has its own 47 kΩ resistor from signal to PC0/PC1/PC2 and 10 kΩ
from that pin to GND. Source ground connects to STM32 GND. Never connect
14 V directly to an MCU pin. No negative voltage, transients or mains.
Keep the STM32 powered whenever an input signal is applied.

Nominal conversion: `Vin = ADC_raw * 3.3 / 4095 * 5.7`, zero offset.
At 14 V the ADC pin is nominally 2.456 V. The reported 13.5 V / 2.386 V
measurement was not associated with a specific channel, so all three retain
nominal calibration until individually measured with ADC readback.

Any enabled divider limits the entire acquisition to 10 kS/s/channel with
480-cycle ADC sampling. One divider channel uses dual simultaneous acquisition
of the same input and retains ADC1 samples only, avoiding interleaved bursts.
Boot defaults: CH1 14V, CH2/CH3 OFF, 10 kS/s, graph 0–16 V (4 V/div).
Calibration record version 2 ignores older bipolar calibration records;
the old flash contents are not erased automatically.

## Historical ±15 V front end — NOT used by current firmware

PC0, PC1, and PC2 are not raw ±15 V inputs. They receive the safe output of
three external analog conditioning circuits. Connect the measured high-voltage
signal to the conditioning-board input, then connect that circuit's output to
the corresponding PC pin. The conditioning circuit and STM32 board must share
analog ground.

The nominal transfer function is:

`Vadc = 0.1 × Vin + 1.65 V`

Therefore:

| External input | Expected ADC-pin voltage |
|---:|---:|
| −15 V | 0.15 V |
| 0 V | 1.65 V |
| +15 V | 3.15 V |

The inverse conversion used for calibration is:

`Vin = 10 × (Vadc − 1.65 V)`

The external circuit requires attenuation, a buffered 1.8333 V reference,
rail-to-rail buffering, filtering, and external clamps as specified in
[ANALOG_FRONT_END.md](ANALOG_FRONT_END.md). Do not apply high or negative
voltage until that circuit has been assembled and validated with a
current-limited source.

## Channel and sampling behavior

- Any logical channel can independently select `3V3` or `14V`.
- With any divider enabled, all channels are limited to 10 kS/s.
- Direct-only acquisition retains one-channel 7.2 MS/s interleaving and
  two/three-channel simultaneous conversion up to 2.4 MS/s per channel.
- Each physical path has independent gain and offset calibration storage.
- Each capture contains 4096 samples per enabled channel.

## Wiring checklist

1. Power off before changing measurement wiring.
2. Identify pins by the board silkscreen labels: PA1, PA2, PA3, PC0, PC1, PC2,
   and GND. Do not rely only on connector position or wire color.
3. Connect the signal ground to board GND before connecting the signal.
4. Select the matching channel and route in the UI.
5. Begin with a current-limited, low-frequency test signal.
6. Verify the voltage at the MCU ADC pin is within 0–3.3 V before increasing
   the source voltage.

This input design is for low-voltage bench measurements. It is not isolated,
not CAT-rated, and must never be connected directly to mains or another
hazardous-energy circuit.
