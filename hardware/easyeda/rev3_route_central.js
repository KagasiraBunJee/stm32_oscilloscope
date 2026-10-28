drc({allowedLayers:['TOP'],trackWidthMm:0.4,minTrackWidthMm:0.3,clearanceMm:0.3,edgeClearanceMm:0.75});
onlyNets('CH2_SUM','CH3_SUM','VBIAS');
for(const n of ['CH2_SUM','CH3_SUM','VBIAS'])signalNet(n,{allowedLayers:['TOP'],viaPreference:'forbid',priority:'high'});
runAll();
