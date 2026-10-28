# Full single-sided revision — 2026-09-07

Status: synchronization resolved after the user applied the native import. Live PCB saved; 24 connection items remain, with no netlist mismatch. Placement and copper have not been changed in this revision.

## Recovery point

Native EasyEDA board copy created and verified in the board list:

- Backup board: `analog_front_end_schematic_1`
- Backup schematic: `40d9fba187bc25d1`
- Backup PCB: `a47fd7b413d2d507`
- Original working PCB: `52caa9452ee66dee` in `analog_front_end_import`

The native copy operation copies both schematic and PCB. The backup's individual primitive contents have not yet been independently read back.

## Synchronization blocker

The original board's schematic binding points to `240b9c508235f5e1`, as expected. A single native import-changes attempt timed out (`DISPATCH_FAILED`, connector deadline). Subsequent component/pad/position readback was identical to the pre-attempt data: 72 components. Native DRC still reports 24 Connection Error items and one Netlist Error.

The installed connector's import-confirm helper searches only Chinese dialog/button text (`确认导入信息` / `应用修改`). The user uses an English UI; this is a likely explanation for the unconfirmed import, not proof of the currently visible dialog. Inspect the import comparison before applying it. Do not retry blindly or rip up copper until synchronization is verified.

## Authorized revision constraints

## Placement/routing feasibility checkpoint

`reroute_synced_baseline.json` captures the synchronized live board. An offline 150,000-step placement search, with all components except the coupled decoupler independently movable, reduced estimated signal crossings from seven to three and ratline length from 26,221 to 21,533 mil. `full_placement_preview.json` passes the offline overlap/40-mil pair-spacing checks, but hand-solder access has not yet been checked live.

The top-only copper preview using 0.40/0.60 mm widths and 0.30 mm clearance did NOT complete: the first ordering left 14 nets fragmented, including 3V3_A, CH1_SUM, CH2_SUM, REF_RAW, and VBIAS. GND pour connectivity is not modeled by that preview. Candidate geometry is therefore NOT applied and is not a completed routing result. Native interactive routing is not exposed through the connected typed API; the next assisted step is to inspect native Auto Routing settings for an exclusively top-layer trial. Do not start it with default two-layer settings.

## Preserved requirements

All components may move/rotate. Keep values, footprints, electrical pin assignments, outline, and mounting holes. Top copper only, no vias or PCB jumpers. Preserve six distributed signal/GND output pairs. Target 0.40 mm signal tracks (0.30 mm short escapes), 0.60 mm power/GND routing, 0.30 mm different-net clearance, and 0.75 mm copper clearance to board/cutout edges.

Completion requires zero native connection/netlist/clearance errors plus independent net reconciliation and top-copper verification. The current board is not ready to etch.
