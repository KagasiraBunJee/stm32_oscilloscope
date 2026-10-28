# Distributed STM32 connection pairs

The old J8 2x4 output header has been replaced in the live schematic and PCB by six separate 2.54 mm signal/GND pairs. They are distributed near their respective analog output circuits. The input connectors J1-J6 and power connector J7 are unchanged.

| PCB connector | Pin 1 (square pad) to STM32 | Pin 2 (round pad) | Measurement path |
|---|---|---|---|
| J9 | PA1 | GND | CH1 direct |
| J10 | PC0 | GND | CH1 conditioned |
| J11 | PA2 | GND | CH2 direct |
| J12 | PC1 | GND | CH2 conditioned |
| J13 | PA3 | GND | CH3 direct |
| J14 | PC2 | GND | CH3 conditioned |

All six pin-2 terminals share the same GND net. Connect to STM32 GND, never to a signal pin. Keep each external signal wire close to its ground-return wire, and keep the harness short. These are analog outputs TO the STM32; do not apply the measured high voltage to these terminals. Conditioned front-end validation is still outstanding.

## Home-made PCB assembly

This board uses only top copper for routing and GND pour. The external wires between boards are not routing jumpers on this PCB. The connector footprints use through-holes, but home fabrication does not provide plated barrels: solder wires directly to the top copper pads, or use an assembly arrangement that leaves the top solder joints accessible. Do not rely on a solder joint made only on the bare underside.

## Current Rev3 checkpoint

- The previous PCB was deleted by the user. The replacement has 72 components,
  165 pads and a fresh placement; the pin mapping above is unchanged.
- 70 x 90 mm outline with four 3.2 mm M3 mounting cutouts.
- Saved partial routing: 321 top track segments, one top GND pour, zero vias.
- VBIAS and several GND connections remain open. Native netlist reconciliation
  and the 0.75 mm edge-clearance rule also remain unresolved.
- NOT ready for fabrication or electrical use. See `REV3_STATUS.md` for evidence.

Use the live EasyEDA project `analog_front_end_import`, PCB `74199e97dae8261c`.
The old PCB UUID and import ZIP do not represent this revision.
