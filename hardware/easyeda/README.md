# Analog front end A0 — EasyEDA Pro import package

Status: engineering draft, NOT released for fabrication.

The generator creates Standard-format JSON schematic and placed PCB documents,
packaged together for EasyEDA Pro's File > Import > EasyEDA Standard import.
This is the documented Pro import route, not a native Pro project archive.

Run `python hardware/easyeda/generate.py` from the project root.
Run `python hardware/easyeda/test_generate.py` for the import-link regression tests.

## Import repair (2026-09-07)

The first import used different instance IDs for schematic and PCB components.
The live schematic passed its connectivity check and all 161 PCB pad nets matched
the schematic, but native PCB DRC reported a netlist mismatch. The generator now
uses the schematic instance ID for its PCB twin. J8 was also moved 5.08 mm toward
the board interior to increase clearance from the bottom-right mounting cutout.
The generated archive is still an unrouted draft, not a live-project backup.

Live-project follow-up: all 67 component instance IDs were repaired and saved,
and moving J8 removed the native clearance violation. Native DRC still reports
161 unconnected pads and one netlist mismatch after Import Changes. Exported
netlists agree on pin nets but differ in device metadata (generic PCB footprint
devices versus named schematic devices). Do not assume the ID repair resolves
the full mismatch. The live hand-soldering placement gate also fails routability
(60 ratline crossings, score 0/100); no copper has been routed.

The four imported mounting cutouts measure approximately 6.4 mm across in Pro,
not the intended 3.2 mm. Replacement was blocked by safety review and no cutouts
were deleted. The generated HOLE encoding remains pending correction and import
verification. The 70 x 90 mm outer board outline is a separate polyline.

## Scope

Three direct 0–3.3 V inputs and three conditioned ±15 V inputs. Six independent
two-pin signal/ground headers accept short leads from panel-mounted connectors.
The outputs are PA1/PA2/PA3 and PC0/PC1/PC2 respectively. J7 supplies 3.3 V and
GND. This board shares ground with the STM32 and is not isolated.

The nominal high-range transfer is VADC = VIN/10 + V3V3/2. Thus at 3.3 V supply,
−15/0/+15 V gives 0.15/1.65/3.15 V. At a 3.21 V supply, the offset is 1.605 V,
not 1.65 V: calibrate using the actual rail/reference voltage.

U1 is TLV9064IDR, SOIC-14. Amplifier A buffers the 5/9 supply reference; B/C/D
buffer CH1/2/3. Nine series 100k resistors plus a 100k bias arm implement each
attenuator. Two 40k precision resistors replace the draft's single 80k reference
resistor. No firmware changes are included.

PCB proposal: 70 × 90 mm, two copper layers, 1.6 mm FR4, four Ø3.2 mm holes at
(3.5,3.5), (66.5,3.5), (3.5,86.5), (66.5,86.5) mm. This is an additional stacked
board; the existing 28 mm-high case has NOT been verified to accommodate it.

## Files and validation limits

- `output/analog_front_end_import.zip`: schematic and PCB together.
- `output/components.json`: every reference, value, footprint, position and pin net.
- `output/BOM.md`: grouped parts list; passive specifications are procurement
  requirements, not verified supplier stock codes.
- `output/placement.svg`: assembly-placement preview (not a copper plot).
- `output/connectivity.json`: authoritative pin-to-net table for import checking.

The schematic uses explicitly numbered functional block symbols with net labels.
Each symbol pin number matches its PCB pad number. Footprints are embedded;
they do not depend on personal cloud libraries. The placed PCB is UNROUTED.
No Gerbers or pick-and-place release files are supplied for this draft.

Before release: verify successful Pro import, compare schematic netlist with
connectivity.json, verify manufacturer footprints/pin 1, complete routing and
ground plane, run ERC/DRC, verify enclosure clearances, and bench-test a channel.
Do not treat JSON syntax or a generated drawing as ERC/DRC validation.

## Electrical review still required

The inherited 900k/100k network has 90k Thevenin resistance. Even 10 pF total
node capacitance gives a pole near 177 kHz. This draft does NOT establish 1 MHz
bandwidth. Compensation and probe loading need vendor-model AC simulation and
measurement; no compensation capacitors are populated in A0.

BAT54S at outputs and BAS70-04 at summing nodes are rail clamps, not guaranteed
ADC-safe sustained-overvoltage protection. Their forward drops, leakage and
rail injection must be evaluated; supply-off inputs can back-power the MCU.
Connect only in-range signals while powered during prototype testing. The
direct connector is never a 5 V input. An ideal op-amp SPICE model cannot validate
overload behavior. The earlier suggestion to sweep ±18 V with the MCU attached
is not a release test: disconnect the MCU while characterizing overrange.

## References

- https://prodocs.easyeda.com/en/import-export/import-easyeda/
- https://docs.easyeda.com/en/DocumentFormat/EasyEDA-Format-Standard/
- https://www.ti.com/lit/ds/symlink/tlv9064.pdf
- https://assets.nexperia.com/documents/data-sheet/BAT54S.pdf
- https://www.infineon.com/part/BAS70-04
