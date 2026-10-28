// Offline placement and short-lead preview. Does not mutate EasyEDA.
const fs=require('fs');
const base=JSON.parse(fs.readFileSync('hardware/easyeda/output/split_pcb_before.json','utf8')).result.components;
const tracks=JSON.parse(fs.readFileSync('hardware/easyeda/output/routing_254_tracks.json','utf8'));
const parts=base.filter(c=>c.designator!=='J8');
const pads=parts.flatMap(c=>c.pads);
const distance=(x,y,t)=>{let dx=t.endX-t.startX,dy=t.endY-t.startY,u=Math.max(0,Math.min(1,((x-t.startX)*dx+(y-t.startY)*dy)/(dx*dx+dy*dy)));return Math.hypot(x-t.startX-u*dx,y-t.startY-u*dy);};
const equal=(a,b)=>Math.hypot(a[0]-b[0],a[1]-b[1])<0.15;
// Retire only leaf branches ending at the old connector. Stop at a retained pad or junction.
const removed=new Set();
for(const p of base.find(c=>c.designator==='J8').pads){let at=[p.x,p.y];for(let n=0;n<100;n++){
 if(pads.some(q=>q.net===p.net&&Math.hypot(at[0]-q.x,at[1]-q.y)<0.15))break;
 const touching=tracks.filter(t=>t.net===p.net&&!removed.has(t.primitiveId)&&distance(...at,t)<0.15);
 if(touching.length!==1)break;const t=touching[0];
 if(!equal(at,[t.startX,t.startY])&&!equal(at,[t.endX,t.endY]))break;
 removed.add(t.primitiveId);at=equal(at,[t.startX,t.startY])?[t.endX,t.endY]:[t.startX,t.startY];
}}
let live=tracks.filter(t=>!removed.has(t.primitiveId));
const obstacles=[...parts];const chosen=[];
function safePoint(x,y,net,r){
 if(x-r<30||x+r>2725||y+r>-30||y-r< -3513)return false;
 if([[137.7953,-137.7953],[2618.1102,-137.7953],[137.7953,-3405.5118],[2618.1102,-3405.5118]].some(h=>Math.hypot(x-h[0],y-h[1])<166+r))return false;
 if(pads.some(p=>p.net!==net&&Math.abs(x-p.x)<p.width/2+r+12&&Math.abs(y-p.y)<p.height/2+r+12))return false;
 if(live.some(t=>t.net!==net&&distance(x,y,t)<r+t.lineWidth/2+12))return false;
 return true;
}
function path(a,b,net,width){
 const dx=b[0]-a[0],dy=b[1]-a[1],m=Math.min(Math.abs(dx),Math.abs(dy));
 const opts=[[a,[a[0]+Math.sign(dx)*m,a[1]+Math.sign(dy)*m],b],[a,[b[0]-Math.sign(dx)*m,b[1]-Math.sign(dy)*m],b]];
 for(const pts of opts){let ok=true;for(let i=1;i<pts.length&&ok;i++){const n=Math.ceil(Math.hypot(pts[i][0]-pts[i-1][0],pts[i][1]-pts[i-1][1])/3);for(let j=0;j<=n;j++){let u=n?j/n:0;if(!safePoint(pts[i-1][0]+u*(pts[i][0]-pts[i-1][0]),pts[i-1][1]+u*(pts[i][1]-pts[i-1][1]),net,width/2)){ok=false;break;}}}if(ok)return pts;}
}
for(let i=0;i<6;i++){
 const target=process.env.HEADER_TARGETS?.split(',')[i]||'C'+(i+1),c=base.find(c=>c.designator===target),net=['PA1','PC0','PA2','PC1','PA3','PC2'][i];
 let best;
 for(let x=Math.max(160,c.x-350);x<=Math.min(2590,c.x+350);x+=10)for(let y=Math.max(-3390,c.y-350);y<=Math.min(-160,c.y+350);y+=10)for(const rot of [0,90,180,270]){
  const rad=rot*Math.PI/180,p1={x:x-50*Math.cos(rad),y:y-50*Math.sin(rad),net,width:70.9,height:70.9},p2={x:x+50*Math.cos(rad),y:y+50*Math.sin(rad),net:'GND',width:70.9,height:70.9};
  const hw=rot%180?55:105,hh=rot%180?105:55,b={minX:x-hw,maxX:x+hw,minY:y-hh,maxY:y+hh};
  if(obstacles.some(o=>!(b.maxX+40<o.bbox.minX||b.minX-40>o.bbox.maxX||b.maxY+40<o.bbox.minY||b.minY-40>o.bbox.maxY)))continue;
  if(!safePoint(p1.x,p1.y,net,35.45)||!safePoint(p2.x,p2.y,'GND',35.45))continue;
  const p=c.pads.find(p=>p.net===net),g=c.pads.find(p=>p.net==='GND');
  const sp=path([p1.x,p1.y],[p.x,p.y],net,0.4/0.0254),gp=path([p2.x,p2.y],[g.x,g.y],'GND',0.6/0.0254);
  if(!sp||!gp){const score=1000+Math.hypot(p.x-p1.x,p.y-p1.y)+Math.hypot(g.x-p2.x,g.y-p2.y);if(!best||score<best.score)best={ref:'J'+(i+9),net,x,y,rotation:rot,bbox:b,pads:[p1,p2],signalPath:[],groundPath:[],score,needsReroute:true};continue;}
  // Check the proposed signal and ground against each other's pad and lead.
  const segs=sp.slice(1).map((z,k)=>({startX:sp[k][0],startY:sp[k][1],endX:z[0],endY:z[1]}));
  if(segs.some(t=>distance(p2.x,p2.y,t)<35.45+12+0.2/0.0254))continue;
  let crossing=false;for(let k=1;k<gp.length;k++){let n=Math.ceil(Math.hypot(gp[k][0]-gp[k-1][0],gp[k][1]-gp[k-1][1])/3);for(let j=0;j<=n;j++){let u=n?j/n:0,xx=gp[k-1][0]+u*(gp[k][0]-gp[k-1][0]),yy=gp[k-1][1]+u*(gp[k][1]-gp[k-1][1]);if(Math.hypot(xx-p1.x,yy-p1.y)<35.45+12+0.3/0.0254||segs.some(t=>distance(xx,yy,t)<12+0.5/0.0254))crossing=true;}}if(crossing)continue;
  const score=Math.hypot(p.x-p1.x,p.y-p1.y)+Math.hypot(g.x-p2.x,g.y-p2.y);
  if(!best||score<best.score)best={ref:'J'+(i+9),net,x,y,rotation:rot,bbox:b,pads:[p1,p2],signalPath:sp,groundPath:gp,score};
 }
 if(!best)throw Error('No safe local header slot for '+net);
 chosen.push(best);obstacles.push(best);pads.push(...best.pads);
 for(const [pts,net,w] of [[best.signalPath,best.net,0.4/0.0254],[best.groundPath,'GND',0.6/0.0254]])for(let k=1;k<pts.length;k++)if(!equal(pts[k],pts[k-1]))live.push({net,layer:1,lineWidth:w,startX:pts[k-1][0],startY:pts[k-1][1],endX:pts[k][0],endY:pts[k][1]});
}
const out={headers:chosen,removeTracks:tracks.filter(t=>removed.has(t.primitiveId)),routes:live.filter(t=>!t.primitiveId)};
fs.writeFileSync('hardware/easyeda/output/split_header_plan.json',JSON.stringify(out,null,2));
console.log(JSON.stringify({headers:chosen.map(({pads,bbox,signalPath,groundPath,...r})=>r),removed:out.removeTracks.length,newTracks:out.routes.length},null,2));
