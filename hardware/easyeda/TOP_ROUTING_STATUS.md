# Top-only routing checkpoint — 2026-09-07

## Latest update: distributed STM32 connectors

The user authorized replacing the grouped J8 output header with distributed wire pairs. This supersedes the J8 position/pinout constraint and the older counts below. See [STM32_WIRING.md](STM32_WIRING.md).

- J8 removed from schematic and PCB; six labeled pairs J9-J14 added near their output circuits. Each has pin 1 = signal, pin 2 = GND.
- J9=PA1, J10=PC0, J11=PA2, J12=PC1, J13=PA3, J14=PC2.
- 72 components, 165 pads; original non-J8 component positions and nets preserved. Outline and mounting cutouts unchanged.
- 18 obsolete old-header leaf segments removed, 12 new local segments added: 248 top tracks total, no vias. J11/J13/J14 local signal connections remain unfinished.
- Placement gate passes at 72/100, seven ratsnest crossings, no overlap/tight/off-board or solder-access violations. Quality diagnostics remain advisory, not an electrical signoff.
- Native DRC: 24 connection items, one netlist mismatch, no clearance errors under the old rules. DFM still flags a pre-existing PA3 track near a cutout and other cleanup warnings. Full photoresist rule migration remains pending.
- Updated live project is saved. `output/split_*_after.json` contains pin/track/DRC evidence; the old import ZIP must not be used as the revised design.

The remaining sections describe the earlier centered-board checkpoint unless explicitly marked as fabrication targets.

**Not finished and not fabrication-ready.** Saved live PCB: `analog_front_end_import`, document `52caa9452ee66dee`.

## User constraints

- Strictly top-layer copper, including signal tracks and the GND pour. No bottom tracks, layer-change vias, or wire jumpers.
- U1 centered on the existing 70 × 90 mm board, at approximately (35.05 mm, 44.96 mm).
- Preserve the eight connector positions, orientations, and pinouts; preserve the existing outline and four mounting cutouts.
- Home-made PCB using photoresist, confirmed by the user. Hand-soldered assembly: 40 mil component gap and 60 mil large-pad access corridor. Factory minimum trace/space rules are not evidence of home-process capability.

## Photoresist fabrication revision

These are provisional design targets, NOT settings already applied to the live PCB:

- Signal tracks: 0.40 mm preferred; 0.30 mm minimum for short pad escapes after checking actual footprints.
- Different-net copper clearance: 0.30 mm minimum, including pad/track and pour gaps. Do not squeeze routes through smaller gaps to finish routing.
- Power tracks and routed GND connections: 0.60 mm preferred; review pad escapes separately.
- Copper-to-board-edge: 0.75 mm target. Preserve the existing mounting cutouts pending the already-required mechanical review.
- Only top copper; no vias, bottom copper, jumpers, or assumed plated-through holes. Verify that every header joint can actually be soldered to the top copper without its plastic body blocking access; changing connector side/orientation requires review of pin numbering and mechanics.
- Assume 35 um copper for planning only. Validate the chosen line/space with a small exposure/development/etch test coupon before making the full board.
- Deliver separate copper artwork, drill guide and assembly drawing at 1:1 scale with a scale-check mark. Confirm resist polarity (positive versus negative) before producing the exposure mask. Verify artwork orientation using U1 pin 1 and asymmetric connector numbering with the printed surface against the resist.
- Existing 0.254 mm tracks and earlier 0.1016/0.1524 mm clearance rules require reassessment and likely rerouting. Do not simply widen all tracks in place: that can create shorts. The last reported zero-clearance-error result applies only to the OLD rules.

These targets are engineering starting assumptions, not guaranteed photoresist process limits. Exposure, artwork contrast/contact, development and etching determine the achievable result. Manufacturer process references: https://www.bungard.de/images/downloads/anleitungen/pos_basis_manual_e.pdf and https://www.bungard.de/images/downloads/allgemein/katalog_en.pdf .

## Verified live state

- 67 components, 161 pads. All pad-to-net assignments preserved.
- 254 copper track segments, all on layer 1. No vias added.
- One existing top-layer GND pour, rebuilt after routing and retained.
- All six input headers and the power header have traces to their first series components.
- Placement gate: 84/100, four ratsnest crossings, zero shorts, overlaps, off-board components, tight pairs, or blocked solder-access corridors.
- Stage records successfully persisted with authorized filesystem access. No routing gates or clearance requirements bypassed.
- Latest native DRC: zero clearance errors, 28 Connection Error items, one Netlist Error. Connection items are not a count of whole nets or missing trace segments.

| Net | Native unconnected items |
|---|---:|
| GND | 12 |
| 3V3_A | 6 |
| CH3_SUM | 2 |
| PA1 | 2 |
| PC1 | 4 |
| CH3_ATT8 | 2 |

Twelve ground items remain isolated despite rebuilding the pour. Do not equate a visible GND pour with a connected ground return.

## Remaining work

1. Inspect native Auto Routing settings with the user, restrict routing to Top Layer only, and use native routing/rerouting for the remaining crowded paths. Current MCP cannot invoke/control that native interactive router. Preserve or explicitly review replacement of existing routed copper.
2. Rebuild GND and verify every ground island and signal/power net through native DRC.
3. Resolve native schematic/PCB comparison. Exported pin nets and device/footprint identities match the earlier schematic export; design-rule records differ, and D3 retains an extra `package` property. These differences are diagnostic evidence, not a proven explanation or a repaired mismatch. Do not blindly import/rebuild or replace the live board.
4. Finish silkscreen QA. Automatic alignment made labels upright, but the DFM tool still flags text/pad overlaps, including zero-size-bbox package attributes whose actual visibility needs inspection. Two acute-angle warnings were also present in the first short-route batch; final geometry still needs cleanup and rechecking.
5. Verify native DRC, DFM, post-route gate, saved state, and manufacturing exports before calling the board ready.

The existing mounting cutouts remain unchanged as requested; their imported dimensions still require mechanical review before manufacture. Analog input protection and bandwidth validation are separate outstanding work. Do not connect high voltage or mains based on this routing checkpoint.

## Files and recovery

- `output/routing_baseline.json`: pre-rearrangement component/pad geometry.
- `output/centered_top_routing_checkpoint.json`: current component/pad geometry, silk data, outline and routing count (the dump is NOT a native PCB project and does not contain track coordinates).
- `output/routing_254_tracks.json`: all current 254 track coordinates, nets and primitive IDs at capture time.
- `output/centered_top_routing_drc.json`: latest native DRC evidence.
- Applied additive batches and journals: `top_short_routes.playbook.json`, `top_header_routes.playbook.json`, and `top_remaining_best.playbook.json` under `output/`.
- Six erroneous newly-created segments from the first batch were removed by exact primitive ID and replaced/reworked; subsequent native DRC clearance errors are zero. Existing user components, holes, outline and connectors were not deleted.
- `top_fresh_candidate.json`, `top_outputs_first.json`, `order_*.json`, `reroute_*.json`, `top_reroute_best.json` and `top_completion_attempt.json` are OFFLINE experiments, not the current board and not safe-to-import completed designs. The proposed PC0/PC2 rip-up was NOT applied.
- The old `analog_front_end_import.zip` remains an outdated unrouted draft, not the current routed checkpoint.

## Tool behavior observed

The CLI short-router with global `--doc` created one segment, then failed subsequent document lookups. No blind retry was used. Remaining inspected segments were applied through a new ordinary typed-action playbook with an explicit project/document target, per-segment live document UUID/project assertions, zero retries, and stop-on-error. The daemon's placement/outline/routing gates remained enforced. Every batch was saved, reloaded, poured again and checked with native DRC.
