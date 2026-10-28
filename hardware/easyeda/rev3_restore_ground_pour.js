// Restore GND after the native resize command removed the old zone but failed.
drc({allowedLayers:['TOP'],trackWidthMm:0.4,minTrackWidthMm:0.3,clearanceMm:0.3,edgeClearanceMm:0.75});
clearRouting({nets:['GND'],items:['zones']});
plane({net:'GND',layers:['TOP'],region:board(),stitching:false,zone:{clearanceMm:0.3,minThicknessMm:0.4,removeIslandsBelowMm2:1,padConnection:{mode:'thermal',thermalGapMm:0.3,spokeWidthMm:0.6}}});
runCopper();
