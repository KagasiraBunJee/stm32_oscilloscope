# Rev3 single-sided PCB rebuild — PARTIAL ROUTING, NOT FABRICATION READY

## September 8 reopened session — supersedes earlier recovery notes

Project reopen restored Agent lookup of PCB1 / 74199e97dae8261c. Native line.list
confirmed 415 TOP segments, including 94 GND segments: the prior 91 additions
DID persist. Copilot's wire summary omitted these additions; its earlier 321
count was not authoritative. Do not recreate them. Sources: rev3_reopened.json
and rev3_reopened_tracks.json in output/.

Offline focused single-net and 60 coupled-routing candidates were tested. None
completed all affected networks. Supply-route removal opens GND paths but leaves
new supply breaks after rerouting. These candidates were NOT applied. This does
not prove a top-only layout impossible; placement and routing must change together.
Search evidence: output/rev3_repair_search.json and rev3_coupled_search.json.

The document-lookup fault recurred immediately after the first region.create.
Only the top-edge 31 mil no-wire/no-fill/no-pour strip was added (primitive
c7ef42c89dfe4e20); the other three strips were NOT created. Existing tracks
were retained. Copilot saved the PCB and checkpoint z37vjgkbo7fjweop.
This reproduces the connector/document failure after a write, not user deletion.
Do not ask for another identical reopen cycle as if it were a durable fix.
Copilot rebuilt the pour after the new keepout (pcb-dsl:ae5c20d3) and saved it.
Final native DRC is back to 15 connection errors only; no clearance/netlist
errors reported. Evidence: output/rev3_reopened_final_drc.json. The PCB is NOT
ready to fabricate, and edge-margin verification remains incomplete.

## Earlier September 8 recovery checkpoint (historical)

User confirmed Import Changes; native netlist warning is now cleared.
Latest native DRC reports 15 connection items: 12 GND and 3 VBIAS.
GND representatives: C9.2, C8.2, J14.2, primitive e467, D3.1, J12.2,
J7.2, R39.2, C4.2, J4.2, D5.1, D6.1. VBIAS: U1.2, R35.2, R23.2.
No clearance or free-copper-region errors are reported under current rules;
the 0.75 mm edge-rule discrepancy below remains unresolved.

A guarded 91-segment GND addition reported success, but persistence cannot be
reconciled with Copilot's subsequent segment counts. Do NOT replay its playbook.
The native pour-fit operation removed the old zone and then failed document
resolution. Copilot restored one TOP GND pour, removed isolated islands, and
saved the PCB. Recovery operation: pcb-dsl:52339013. Readback retains 72 parts,
zero vias and one pour. This is not a connectivity pass.

Blocking editor state: Agent document.current identifies PCB 74199e97dae8261c,
but doc ls omits all PCBs. Saving and Copilot save/close/reopen synchronization
did not resolve this. Further edits are paused pending a project-level reopen
and confirmation that this same PCB exists in the project tree. Do not delete
or recreate it. Recovery snapshot: output/rev3_recovery_readback.json.

## Earlier live checkpoint (historical counts)

User approved placement. Applied 72 parts, 70 x 90 mm outline, four 3.2 mm
polygonal mounting cutouts and four 3 mm radius top-copper hardware keepouts.
Saved and reloaded. Copilot and EasyEDA Agent were both used.

Local placement corrections: C9 center x=1045 mil, y=-1600; R23 anchor
x=1680,y=-2000,rotation=180; R35 anchor x=1530,y=-1700,rotation=90.
Native placement gate passes with no shorts, overlaps, off-board parts, tight
pairs or obstructed 60 mil solder-access corridors. Current ratsnest heuristic
score is 76/100 with six crossings; this is NOT a connectivity pass.

Current routed readback: **321 top-only track segments, 0 vias, one TOP GND pour**.
Reported routed widths are 0.40 and 0.60 mm. No bottom routing reported.
Native DRC after save/reload: **17 connection items plus one netlist warning**:

- VBIAS: U1.2, R35.2, R23.2.
- GND: C9.2, C8.2, D3.1, J12.2, J10.2, D4.1, J7.2, R39.2, C4.2,
  J4.2, D5.1, D6.1, J14.2, J6.2.
- No copper-clearance violation reported under the active native rules.
- Independent reconciliation of all 165 pad nets against the captured schematic
  still reports no missing pins or mismatches. The native netlist warning remains
  unresolved and must not be suppressed based on that audit.

**Rule discrepancy:** requested edge clearance 0.75 mm is present in router DSL,
but native compact rule readback reports Track-to-Board = 0.30 mm. Therefore a
native zero-clearance-error result does NOT prove the requested edge rule.
The final edge/cutout geometry and native rule configuration still need checking.

The GND pour is fragmented by signal/power routing. The router excludes GND from
maze routing, so its open-net count does not capture these ground failures.
No top-only physical impossibility has been proven. More return-path and VBIAS
placement/routing repair is required; do not fabricate or apply high voltage.

Latest evidence: `output/rev3_routed_checkpoint.json`,
`output/rev3_copilot_readback.json`, `output/rev3_native_drc.json`.
Routing operations: `pcb-dsl:87f0d16c` (first), `pcb-dsl:ae40dbc8` (coupled repair),
`pcb-dsl:c8965db1` (central clear), `pcb-dsl:4ca031ca` (central reroute).
Their routing results and first copper application are preserved under `output/`.
Completed layout and keepout playbooks must NOT be replayed.

Next interaction: native Import Changes reconciliation; wait for user confirmation
if the dialog is open, then re-read before continuing. No final deliverable or
manufacturing pass exists yet. Existing partial copper is retained as a work checkpoint.

---

## Historical preview checkpoint (not the current editor state)

Target project: analog_front_end_import

Target PCB UUID: `74199e97dae8261c`.
Source schematic UUID: `240b9c508235f5e1`; page `30968987fbc1647d`.
The user deleted the previous PCB; do not target its obsolete UUID.

## Current editor versus local candidate

- New PCB created through EasyEDA Copilot and linked to the source schematic.
- Schematic import manually confirmed by user; 72 components and 165 pads read back.
- Current PCB remains in its imported placement. No candidate placement, outline,
  mounting cutouts, copper routing, or GND pour has been applied in this stage.
- Both the all-component and selected-component Copilot footprint queries failed
  with `fetch failed`. Placement operation `pcb-layout:d6174b0c` terminated with
  the same error. Do not attempt to assemble it.
- EasyEDA Agent successfully retrieved local footprint bounding boxes, pad sizes,
  positions, rotations and net assignments into `output/rev3_imported.json`.
- Offline geometry-based candidate: `output/rev3_candidate.json`.
- Preview: `output/rev3_placement_preview.png`. Lines are guides, not copper.

## Candidate checks

- All 165 PCB pads reconciled against the captured source schematic: no missing
  pins or mismatched nets (`output/rev3_candidate_audit.json`).
- All 72 components on top. Six distributed STM32 signal/GND connectors preserved.
- No footprint-bounding-box pair closer than 40 mil in the candidate.
- Search reports zero modeled edge/mounting-clearance violations and one
  straight-line ratsnest crossing. These checks do not prove routability.
- Native solder-access, connectivity and DRC checks have NOT run on the candidate.
- GND and analog supply omitted from preview guide lines for readability only;
  their complete copper networks remain required, not exempted from routing.

## Required next steps

1. Obtain placement/mechanical preview approval before applying the candidate.
2. Re-read the exact live document and primitive IDs. Apply only if unchanged
   from the imported snapshot; preserve any intervening user edits.
3. Create a 70 x 90 mm rectangle and four true 3.2 mm unplated mounting cutouts,
   centered 3.5 mm from each corner. Do NOT use Copilot `boardHole`, whose
   documented export is an unnetted via. Reserve M3 hardware space.
4. Verify placement, hand-solder access and schematic/PCB reconciliation.
5. Route on top ONLY: 0.40 mm signals, 0.30 mm short escapes only, 0.60 mm
   supply/GND where pads allow, >=0.30 mm different-net clearance and >=0.75 mm
   copper-to-edge/cutout clearance. No bottom tracks, vias or PCB wire jumpers.
6. Rebuild top GND pour, save/reload and run native connectivity/DRC. Require zero
   airwires, shorts, clearance errors and netlist mismatches before completion.

Not ready for etching or high-voltage use. Analog validation remains separate.
