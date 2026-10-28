const fs=require('fs'),cp=require('child_process');
let best=null;
for(let i=0;i<20;i++){
 const output='hardware/easyeda/output/order_'+i+'.json';
 const env={...process.env,TRACK_INPUT:'hardware/easyeda/output/routing_next_tracks.json',QUIET:'1',ROUTE_OUTPUT:output,ORDER_SEED:i?String(i*27139):''};
 delete env.FRESH_ROUTE;delete env.ONLY_NETS;
 const r=cp.spawnSync(process.execPath,['hardware/easyeda/plan_top_routes.cjs'],{env,encoding:'utf8'});if(r.status)throw Error(r.stderr);
 const data=JSON.parse(fs.readFileSync(output,'utf8'));const missing=data.failed.reduce((n,f)=>n+f.groups-1,0),length=data.routes.reduce((n,r)=>n+Math.hypot(r.startX-r.endX,r.startY-r.endY),0),score=missing*100000+length;
 if(!best||score<best.score){best={score,missing,length,order:i,segments:data.routes.length};fs.writeFileSync('hardware/easyeda/output/top_remaining_best.json',JSON.stringify(data,null,2));}
 console.log({order:i,missing,segments:data.routes.length,best});
 if(!missing)break;
}
