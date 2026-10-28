// Offline preparation. No calls to the live editor.
const fs=require('fs');
const base='hardware/easyeda/output/';
const board=JSON.parse(fs.readFileSync(base+'reroute_synced_baseline.json','utf8'));
const plan=JSON.parse(fs.readFileSync(base+'full_placement_candidate.json','utf8'));
const poses=new Map(plan.placements.map(p=>[p.primitiveId,p]));
for(const c of board.components){
 const p=poses.get(c.primitiveId); if(!p)throw Error('Missing '+c.designator);
 const a=(p.rotation-c.rotation)*Math.PI/180,co=Math.round(Math.cos(a)),si=Math.round(Math.sin(a));
 const xy=(x,y)=>({x:p.x+(x-c.x)*co-(y-c.y)*si,y:p.y+(x-c.x)*si+(y-c.y)*co});
 const b=c.bbox, corners=[[b.minX,b.minY],[b.minX,b.maxY],[b.maxX,b.minY],[b.maxX,b.maxY]].map(v=>xy(...v));
 c.bbox={minX:Math.min(...corners.map(v=>v.x)),maxX:Math.max(...corners.map(v=>v.x)),minY:Math.min(...corners.map(v=>v.y)),maxY:Math.max(...corners.map(v=>v.y))};
 c.pads=c.pads.map(v=>({...v,...xy(v.x,v.y),width:Math.abs(co)*v.width+Math.abs(si)*v.height,height:Math.abs(si)*v.width+Math.abs(co)*v.height}));
 Object.assign(c,{x:p.x,y:p.y,rotation:p.rotation,locked:false});
}
board.silk=[];board.routedLines=0;
fs.writeFileSync(base+'full_placement_preview.json',JSON.stringify(board,null,2));
console.log(JSON.stringify(plan.metrics));
