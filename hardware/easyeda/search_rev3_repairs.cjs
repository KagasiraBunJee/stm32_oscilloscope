// Offline only: test focused rip-up candidates against current pad/track geometry.
const fs=require('fs'),cp=require('child_process');
const dir='hardware/easyeda/output/';
const tracks=JSON.parse(fs.readFileSync(dir+'rev3_reopened_tracks.json'));
const nets=[...new Set(tracks.map(t=>t.net))].filter(n=>n&&n!=='GND'&&n!=='VBIAS');
const results=[];
for(const net of nets){
 const out=dir+'rev3_trial_'+net+'.json';
 cp.execFileSync(process.execPath,['hardware/easyeda/preview_rev2_routes.cjs'],{env:{...process.env,BOARD_INPUT:dir+'rev3_reopened.json',TRACK_INPUT:dir+'rev3_reopened_tracks.json',INCLUDE_GND:'1',HOLE_KEEPOUT_RADIUS:'118.2',ONLY_NETS:'VBIAS,GND,'+net,RIP_NETS:net,ROUTE_OUTPUT:out,QUIET:'1'},stdio:'pipe'});
 const r=JSON.parse(fs.readFileSync(out));
 const score=r.failed.reduce((s,f)=>s+f.groups-1,0);
 results.push({net,score,newSegments:r.routes.length,failed:r.failed.map(f=>({net:f.net,groups:f.groups}))});
}
results.sort((a,b)=>a.score-b.score||a.newSegments-b.newSegments);
fs.writeFileSync(dir+'rev3_repair_search.json',JSON.stringify(results,null,2));
console.log(JSON.stringify(results.slice(0,8),null,2));
