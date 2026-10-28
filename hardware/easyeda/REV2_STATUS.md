# Fresh top-only PCB checkpoint

Live PCB: `920cad72d3d6143e` (`PCB1`), board currently reported by EasyEDA as `analog_front_end_schematic_2`, project `analog_front_end_import`. The previously requested board name was not retained by the native creation/import flow. Old PCB UUIDs and backup PCB UUIDs were deleted by the user; do not target them.

## Implemented

- Imported the current main schematic: 72 components and 165 pads. Independent designator/pin/net comparison found zero mismatches.
- Fresh functional placement and refinement, all top-side. Native hand-solder routing gate passes: 40 mil component spacing, 60 mil access, no overlaps or off-board parts; score 96, one estimated signal crossing.
- 70 x 90 mm outline; four polygonal M3 cutouts constructed with 3.2 mm diameter at the original corner centers, per explicit user confirmation. Four top no-wire/no-pour/no-fill regions reserve copper clearance around those cutouts. The API confirms four cutouts but does not return their dimensions in fill.list; exported dimensional verification remains pending.
- 262 top copper segments: 0.40 mm signals and 0.60 mm power. No vias. The offline planner uses 0.30 mm different-net clearance plus raster margin, and at least 0.75 mm edge/cutout clearance. Native global signal-clearance rules have NOT yet been migrated to enforce this entire specification.
- One top GND pour, rebuilt; pour gap rule raised and read-back verified at 0.3048 mm. Board-edge inset exceeds 0.75 mm.
- Designators aligned and top view enabled. Label cleanup is incomplete.

## Outstanding — NOT READY TO ETCH

Native DRC after routing/pour: 12 Connection Error items on CH2_HIGH, CH3_ATT8, CH3_SUM, GND (D8/D9), PC0, PC2. A Netlist Error reappeared after PCB rule/geometry edits despite the independent pin/net match; the exact native import comparison still requires review. Do not assume or suppress it.

DFM: zero errors, 115 warnings, including 13 acute angles, 20 coupling warnings, 81 silk-over-pad findings and one connector-clearance warning. No track-over-pad, dangling copper, duplicate segments, or clearance errors under the checked rules. These counts do NOT establish complete manufacturing acceptance.

Further connected routing needs a native interactive routing/settings review; the connected typed API cannot operate EasyEDA's interactive router. Keep Top as the only routable copper layer and prohibit vias/jumpers. Do not silently accept a bottom-layer result or reduce specified clearances.

Source and evidence: `output/rev2_source_schematic.json`, `output/rev2_checkpoint.json`, `output/rev2_routes.playbook.json` and journal, `output/rev2_drc.json`, `output/rev2_dfm.json`. The dump is not a native backup and excludes actual copper coordinates; the successfully applied route playbook carries them.

No analog/high-voltage validation or etching artwork release has been completed.
