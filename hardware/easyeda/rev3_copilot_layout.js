// New single-sided layout; schematic and footprints remain authoritative.
// Mounting holes are NOT boardHole primitives: that API exports vias.
// Reserve four 7x7 mm corner areas; add true 3.2 mm NPTH cutouts later.
board.rect(70, 90, { layers: ["top"], defaultLayer: "top", clearance: 1.0, edge: 1.5 });
for (const corner of ["top_left", "top_right", "bottom_left", "bottom_right"]) {
  constraintRegion("Mount_" + corner, { layers: ["top"], shape: region.rect({
    anchor: anchor("board." + corner), width: 7, height: 7
  }) });
}
block("Core", ["U1", "C8"], "analog");
block("Supply", ["J7", "FB1", "C7"], "power");
block("Reference", ["R37", "R38", "R39", "C9"], "analog", {
  placement: "satellite", attachTo: "Core", anchor: pin("U1", "3")
});
near(comp("U1"), anchor("board.center"), "high");
bypass(["C8"], pin("U1", "4"), "critical", { gap: 1 });
near(pin("FB1", "2"), pin("U1", "4"), "high");
criticalPair(pin("C9", "1"), pin("U1", "3"), { maxDistance: 5 });
// Low-power signal buffer and passive divider: no thermal vias/spreader.
// Thermal and overvoltage validation remain separate from routing acceptance.
for (let ch = 0; ch < 3; ch++) {
  const b = 12 * ch, j = 2 * ch, d = 3 * ch, c = 2 * ch;
  block("Direct" + ch, ["J"+(j+1), "R"+(b+1), "C"+(c+1), "D"+(d+1), "J"+(9+j)], "analog");
  block("Attenuator" + ch, ["J"+(j+2), ...Array.from({length:9}, (_,i)=>"R"+(b+i+2))], "analog");
  block("Sum" + ch, ["R"+(b+11), "D"+(d+2)], "analog", {
    placement: "satellite", attachTo: "Core", anchor: pin("U1", ["5","10","12"][ch])
  });
  block("Output" + ch, ["R"+(b+12), "C"+(c+2), "D"+(d+3), "J"+(10+j)], "analog", {
    placement: "satellite", attachTo: "Core", anchor: pin("U1", ["7","8","14"][ch])
  });
  criticalPair(pin("R"+(b+10), "2"), pin("U1", ["5","10","12"][ch]), { maxDistance: 8 });
  criticalPair(pin("R"+(b+12), "1"), pin("U1", ["7","8","14"][ch]), { maxDistance: 6 });
}
for (const j of [1,2,3,4,5,6,7,9,10,11,12,13,14]) component("J"+j).role("connector").top();
component("U1").role("main_ic").top();
silkscreen.designators({ height: 1.1, margin: 0.3 });
// Focused, non-applying mechanics preview before full placement approval.
solver({ grid: 1, ignoredSignals: ["GND"], compactness: "normal", preview: true,
  placeOnlyComponents: ["J1","J2","J3","J4","J5","J6","J7","J9","J10","J11","J12","J13","J14"] });
