// Focused coupled repair: open nets and the two SUM routes identified by
// router diagnostics as competing for the central routing corridor.
drc({allowedLayers:['TOP'],trackWidthMm:0.4,minTrackWidthMm:0.3,
 clearanceMm:0.3,edgeClearanceMm:0.75,
 via:{diameterMm:0.6,drillMm:0.3,minDiameterMm:0.6,minDrillMm:0.3}});
const selected=['3V3_A','PC1','VBIAS','CH2_SUM','CH3_SUM'];
onlyNets(...selected);
clearRouting({nets:selected,items:['tracks']});
powerNet('3V3_A',{trackWidthMm:0.6,minTrackWidthMm:0.3,allowedLayers:['TOP'],viaPreference:'forbid',priority:'high'});
for(const n of selected.filter(n=>n!=='3V3_A'))signalNet(n,{allowedLayers:['TOP'],viaPreference:'forbid',priority:'high'});
// Retain the existing top GND zone. No new polygons or vias.
runAll();
