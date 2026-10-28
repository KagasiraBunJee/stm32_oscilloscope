# MCP routing repair status — 2026-09-07

> Historical diagnosis. See [TOP_ROUTING_STATUS.md](TOP_ROUTING_STATUS.md) for the current centered layout, 254 saved top-layer track segments, and remaining native DRC findings. Do not use this older checkpoint as current routing status.

Target PCB: `52caa9452ee66dee`, project `analog_front_end_import`.

User authorized debug API diagnosis and metadata repair, excluding mounting-hole changes.

## Verified findings

- 67 components and 161 pads remain present.
- PCB pin-to-net mappings in the exported native netlist match the schematic export.
- PCB and schematic device metadata differ; 19 instances also reference different footprint library IDs. These differences are not yet proven to be the cause of the native netlist error.
- Native `pcb_Net.setNetlist` was tested with only the schematic Channel ID applied to the PCB netlist. It returned true, but the Channel ID remained empty after save/reload. This is not a successful repair.
- Native DRC still reports 161 unconnected-pad errors and one netlist mismatch.
- Placement gate: score 0/100, 60 crossing airwires; no shorts, overlaps, off-board components or tight pairs reported by layout-lint.
- The four mounting cutouts e1–e4 remain present and were not edited.
- No copper tracks were added during this repair attempt. Routing gates were not bypassed.

## Artifacts

- `output/pcb_before_metadata_repair.enet`: native PCB netlist backup.
- `output/pcb_metadata_repair.enet`: proposed Channel-ID-only netlist; NOT a routed board or confirmed applied repair.

## Next diagnostic

Inspect EasyEDA Pro's native Import Changes difference dialog. The programmatic import previously returned `no-dialog`, and metadata update success flags are not evidence that the mismatch was repaired. Do not replace device/footprint bindings or rebuild the board without reviewing the difference and scope first.
