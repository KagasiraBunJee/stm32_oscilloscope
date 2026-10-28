const fs=require('fs'),cp=require('child_process');
const dir='hardware/easyeda/output/';
const sets=[
 ['GND','VBIAS','3V3_A'],
 ['GND','VBIAS','3V3_A','CH1_SUM','CH2_SUM','CH3_SUM'],
 ['GND','VBIAS','3V3_A','CH1_SUM','CH2_SUM','CH3_SUM','CH1_BUF','CH2_BUF','CH3_BUF','PC0','PC1','PC2','REF_RAW','REF_MID']
];
let best=Infinity;const results=[];
for(let set=0;set<sets.length;set++)for(let seed=1;seed<=20;seed++){
 const out=dir+`rev3_coupled_${set}_${seed}.json`,nets=sets[set].join(',');
 cp.execFileSync(process.execPath,['hardware/easyeda/preview_rev2_routes.cjs'],{env:{...process.env,BOARD_INPUT:dir+'rev3_reopened.json',TRACK_INPUT:dir+'rev3_reopened_tracks.json',INCLUDE_GND:'1',HOLE_KEEPOUT_RADIUS:'118.2',ONLY_NETS:nets,RIP_NETS:nets,ORDER_SEED:String(seed*7919),ROUTE_OUTPUT:out,QUIET:'1'},stdio:'pipe'});
 const r=JSON.parse(fs.readFileSync(out)),score=r.failed.reduce((s,f)=>s+f.groups-1,0);
 results.push({set,seed,score,newSegments:r.routes.length,failed:r.failed.map(f=>({net:f.net,groups:f.groups})),out});
 if(score<best){best=score;console.log(JSON.stringify(results.at(-1)));}
 if(score===0)break;
}
results.sort((a,b)=>a.score-b.score);
fs.writeFileSync(dir+'rev3_coupled_search.json',JSON.stringify(results,null,2));
