// Explore scoped rerouting OFFLINE; never deletes copper in EasyEDA.
const fs=require('fs'),cp=require('child_process');
const sets=['PC0,PC2','PC0,PC2,VBIAS','PC0,PC2,3V3_A','PC0,PC2,PA1,PC1,CH3_SUM,CH3_ATT8','PC0,PC2,3V3_A,VBIAS,REF_RAW,REF_MID'];
let best=null,iteration=0;
for(const set of sets)for(let i=0;i<5;i++){
 const output='hardware/easyeda/output/reroute_'+iteration+'.json';
 const env={...process.env,TRACK_INPUT:'hardware/easyeda/output/routing_254_tracks.json',POWER_WIDTH:'10',QUIET:'1',RIP_NETS:set,ROUTE_OUTPUT:output,ORDER_SEED:i?String(i*29173):''};delete env.FRESH_ROUTE;delete env.ONLY_NETS;
 const r=cp.spawnSync(process.execPath,['hardware/easyeda/plan_top_routes.cjs'],{env,encoding:'utf8'});if(r.status)throw Error(r.stderr);
 const data=JSON.parse(fs.readFileSync(output,'utf8')),missing=data.failed.reduce((n,f)=>n+f.groups-1,0),length=data.routes.reduce((n,r)=>n+Math.hypot(r.startX-r.endX,r.startY-r.endY),0),score=missing*100000+length;
 if(!best||score<best.score){best={score,missing,length,set,i,segments:data.routes.length};fs.writeFileSync('hardware/easyeda/output/top_reroute_best.json',JSON.stringify(data,null,2));}
 console.log({iteration:iteration++,missing,best});
 if(!missing)process.exit(0);
}
