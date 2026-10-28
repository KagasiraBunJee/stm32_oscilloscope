# Placement cleanup — 2026-09-07

> Superseded by [TOP_ROUTING_STATUS.md](TOP_ROUTING_STATUS.md): U1 is centered, the live placement gate passes at 84/100, and 254 top-layer track segments are saved. Routing is still incomplete. The checkpoints below are historical.

## Latest verified checkpoint (supersedes pending status below)

Saved successfully after reconnecting. R37/R38/R39 moves persisted. C9 is now (1775,-1675), rotation 0; C8 is (1800,-1810), rotation 90. Output resistors: R12 (1800,-2075), R24 (2350,-1975), R36 (2300,-1400). Conditioned-output filters: C2/D3 at x=2200/2000, y=-2810; C4/D6 at x=2200/2000, y=-3010; C6/D9 at x=2200/2000, y=-3210. All coordinates mil, top layer.

Readback: 67 components; all pad-to-net mappings preserved against baseline. Native DRC reports only 161 unconnected-pad errors, with no netlist or clearance error reported. No copper tracks have been added.

Layout lint: zero short/overlap/off-board/tight findings, 59 crossings, estimated ratsnest length 34617 mil (baseline 39873 mil, 60 crossings). Routing gate still FAILS (required score >=60 and crossings <=8). This is a partial placement cleanup, not routing-ready or fabrication-ready. Connectors, holes and outline were not edited.

## Earlier interrupted checkpoint

Baseline: `output/placement_before_cleanup.json` (67 components, all pad nets and geometry).

Completed typed moves (mil, top layer, rotation 0):

- R37: center/anchor (1475, -1350). Read back after reconnect; persisted.
- R38: center/anchor (1650, -1350). Verified by modify, then save/close/reopen succeeded.
- R39: center/anchor (1825, -1350). Verified by modify. Reload reached reopen, but the connector disconnected. Persistence after reopening needs verification.

No mounting holes, outline, connectors, nets, tracks or design rules were changed.

Pending planned capacitor moves: C9 to (1775, -1650), C8 to (1800, -1810), C8 rotation 90 degrees. These were NOT applied. Recheck geometry against fresh data before applying.

The EasyEDA connector repeatedly refreshed/disconnected, with new window IDs appearing. Final placement lint and DRC have not run. The latest health check showed only a home-page connection; opening the PCB failed due to another window replacement. Reopen the PCB and stabilize the connector before continuing. Do not report this placement pass as completed or routing-ready.
