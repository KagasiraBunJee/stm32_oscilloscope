// User: handmade photoresist, strictly top copper, no vias or jumpers.
// EasyEDA requires two logical copper layers; BOTTOM must remain unused.
// Provisional substrate 1.6mm FR4 / 1oz; no controlled impedance promised.
stack({boardThicknessMm:1.6,fallbackCopperThicknessOz:1,layers:[
 {kind:'copper',name:'TOP'}, {kind:'copper',name:'BOTTOM'}
]});
drc({allowedLayers:['TOP'],trackWidthMm:0.40,minTrackWidthMm:0.30,
 clearanceMm:0.30,edgeClearanceMm:0.75,
 via:{diameterMm:0.6,drillMm:0.3,minDiameterMm:0.6,minDrillMm:0.3}});
const names=['3V3_IN','3V3_A','GND','REF_RAW','REF_MID','VBIAS','PA1','PA2','PA3','PC0','PC1','PC2'];
for(let c=1;c<=3;c++){names.push(`CH${c}_DIRECT`,`CH${c}_HIGH`,`CH${c}_SUM`,`CH${c}_BUF`);for(let i=1;i<=8;i++)names.push(`CH${c}_ATT${i}`);}
for(const n of names) {
 if(['3V3_IN','3V3_A','GND'].includes(n))powerNet(n,{allowedLayers:['TOP'],viaPreference:'forbid',trackWidthMm:0.6,minTrackWidthMm:0.3});
 else signalNet(n,{allowedLayers:['TOP'],viaPreference:'forbid',priority:/REF_RAW|VBIAS|_SUM$|_BUF$/.test(n)?'high':'normal'});
}
plane({net:'GND',layers:['TOP'],region:board(),stitching:false,zone:{clearanceMm:0.30,minThicknessMm:0.4,padConnection:{mode:'thermal',thermalGapMm:0.3,spokeWidthMm:0.6,spokeCount:4},removeIslandsBelowMm2:1}});
runAll();
