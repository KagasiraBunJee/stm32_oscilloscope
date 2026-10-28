# Three-channel analog input front end

This is the pre-PCB reference design for the six physical inputs. Do not apply 12 V or ±15 V to the development board until this circuit has been assembled, current-limited, and validated against the supplied SPICE model. MCU ADC pins are not overvoltage tolerant.

## Routes

| Logical | Direct 0–3.3 V | Conditioned ±15 V |
|---|---|---|
| CH1 | PA1 / ADC IN1 | PC0 / ADC IN10 |
| CH2 | PA2 / ADC IN2 | PC1 / ADC IN11 |
| CH3 | PA3 / ADC IN3 | PC2 / ADC IN12 |

No analog multiplexer is used. The selected LVGL route chooses which physical ADC channel is sampled.

## Direct input, repeated three times

`Jx -> 100 Ω -> ADC_DIRECT`, with BAT54S external Schottky clamps from the protected node to analog 0 V and 3.3 V, and 1 nF C0G from the node to analog ground. The nominal one-pole bandwidth is:

`fc = 1 / (2π × 100 Ω × 1 nF) = 1.59 MHz`.

This route is only for low-impedance, ground-referenced 0–3.3 V sources. The clamps are transient protection, not permission to apply sustained out-of-range voltage. Start with a 1 kHz, 1 Vpp signal having 1.65 V DC offset.

## Conditioned ±15 V input, repeated three times

Nine 100 kΩ, 0.1% resistors in series connect `VIN_HI` to the summing node. A 100 kΩ, 0.1% resistor connects that node to a buffered 1.8333 V reference. Therefore:

`VNODE = VIN × 100k/(900k+100k) + VREF × 900k/(900k+100k)`

`VNODE = 0.1 × VIN + 0.9 × 1.8333 = 0.1 × VIN + 1.6500 V`.

Thus −15 V, 0 V, and +15 V map to approximately 0.15 V, 1.65 V, and 3.15 V. A TLV9064 powered from 3.3 V buffers the reference (U1A) and the three input nodes (U1B–D). Each output uses 33 Ω series resistance, BAT54S clamps, and 1 nF C0G at the ADC pin. Firmware calibration independently corrects gain and offset for all six paths.

A BAS70-04 dual low-leakage Schottky clamp is also fitted from each high-impedance summing node to the analog rails. The 900 kΩ series string limits op-amp input fault current; the external clamp must be characterized because its leakage becomes gain/offset error at this impedance.

The 1.8333 V reference is generated from analog 3.3 V by 80.0 kΩ over 100 kΩ, both 0.1%, with 100 nF to ground, then buffered. The theoretical divider value is `3.3 × 100/(80+100) = 1.8333 V`. The 900 kΩ input resistance limits fault current, but component voltage ratings, connector insulation, and safe probe practice still matter. The conditioned route is not CAT-rated and must never be connected to mains.

## BOM for three channels

| Quantity | Part | Requirement |
|---:|---|---|
| 3 | Input connector for direct route | Rated for intended low voltage |
| 3 | Input connector for ±15 V route | Clearly keyed/labeled |
| 30 | 100 kΩ resistor | 0.1%, 25 ppm/°C; 27 form three 900 kΩ strings, 3 bias arms |
| 3 | 100 Ω resistor | 1%, direct input series |
| 3 | 33 Ω resistor | 1%, buffer output isolation |
| 1 | 80.0 kΩ resistor | 0.1%, VREF upper arm |
| 1 | 100 kΩ resistor | 0.1%, VREF lower arm |
| 6 | BAT54S | Low-capacitance external rail clamps; characterize leakage |
| 3 | BAS70-04 | Low-leakage dual Schottky clamps at high-range summing nodes |
| 6 | 1 nF capacitor | C0G/NP0, 2%, ADC filters |
| 1 | 100 nF capacitor | C0G/X7R, VREF filtering |
| 1 | TLV9064 | Quad 10 MHz RRIO op amp, 3.3 V supply |
| 2 | 100 nF + 1 µF | Local op-amp/analog-rail decoupling |
| 1 | Ferrite bead | Separate filtered analog 3.3 V rail |

Use a star-like analog ground return, keep each ADC RC network close to the MCU, keep SDIO/FSMC edges away from summing nodes, and guard the 900 kΩ networks against contamination. Probe loading and the high-range 1 MΩ input network require frequency compensation to be measured on the assembled board; provide an optional 2–10 pF C0G footprint across the 900 kΩ string and tune it with a square wave.

## Bring-up checklist

1. Validate reference voltage and all six ADC nodes with the MCU disconnected.
2. Sweep conditioned inputs from −18 V to +18 V using a current-limited source; ensure ADC nodes stay inside 0–3.3 V.
3. Run the SPICE DC and AC sweeps and compare measured gain/offset.
4. Connect the MCU, test at low frequency, and store per-path calibration.
5. Measure bandwidth/noise/crosstalk before claiming the approximate 1 MHz useful bandwidth.

## Primary design references

- [ST STM32F405/407 datasheet](https://www.st.com/resource/en/datasheet/stm32f407ze.pdf): 36 MHz ADC clock at 2.4–3.6 V, 2.4 MS/s converters/7.2 MS/s triple interleaved, and analog input operating limits.
- [TI TLV9064 datasheet](https://www.ti.com/lit/ds/symlink/tlv9064.pdf): 1.8–5.5 V operation, 10 MHz unity-gain bandwidth, rail-to-rail input/output, input clamp and fault-current limits.
