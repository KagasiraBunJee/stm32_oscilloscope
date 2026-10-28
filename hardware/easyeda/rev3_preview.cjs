const fs=require('fs');
const dir='hardware/easyeda/output/';
const src=JSON.parse(fs.readFileSync(dir+'rev3_imported.json','utf8'));
const opt=JSON.parse(fs.readFileSync(dir+'rev3_body_optimized.json','utf8'));
const poses=new Map(opt.placements.map(p=>[p.designator,p]));
const components=src.components.map(c=>{
 const p=poses.get(c.designator),r=(p.rotation-c.rotation)*Math.PI/180,co=Math.round(Math.cos(r)),si=Math.round(Math.sin(r));
 const xy=(x,y)=>[p.x+(x-c.x)*co-(y-c.y)*si,p.y+(x-c.x)*si+(y-c.y)*co];
 const corners=[[c.bbox.minX,c.bbox.minY],[c.bbox.minX,c.bbox.maxY],[c.bbox.maxX,c.bbox.minY],[c.bbox.maxX,c.bbox.maxY]].map(q=>xy(...q));
 return {...c,...p,bbox:{minX:Math.min(...corners.map(q=>q[0])),maxX:Math.max(...corners.map(q=>q[0])),minY:Math.min(...corners.map(q=>q[1])),maxY:Math.max(...corners.map(q=>q[1]))},pads:c.pads.map(q=>{const [x,y]=xy(q.x,q.y);return {...q,x,y,width:Math.abs(co)*q.width+Math.abs(si)*q.height,height:Math.abs(si)*q.width+Math.abs(co)*q.height};})};
});
const w=70/0.0254,h=90/0.0254;
const holes=[[3.5/0.0254,-3.5/0.0254],[(70-3.5)/0.0254,-3.5/0.0254],[3.5/0.0254,-(90-3.5)/0.0254],[(70-3.5)/0.0254,-(90-3.5)/0.0254]];
const edges=[];
const nets=new Map();
for(const c of components)for(const p of c.pads){if(!p.net||p.net==='GND'||p.net==='3V3_A')continue;if(!nets.has(p.net))nets.set(p.net,[]);nets.get(p.net).push({...p,ref:c.designator});}
for(const [net,pads] of nets){const done=new Set([0]);while(done.size<pads.length){let best=null;for(const a of done)for(let b=0;b<pads.length;b++)if(!done.has(b)){const d=Math.hypot(pads[a].x-pads[b].x,pads[a].y-pads[b].y);if(!best||d<best.d)best={a,b,d};}edges.push({net,a:pads[best.a],b:pads[best.b]});done.add(best.b);}}
const candidate={...src,components,holes,edges,metrics:opt.metrics,outline:{points:[[0,0],[w,0],[w,-h],[0,-h]],bbox:{minX:0,maxX:w,minY:-h,maxY:0}},silk:[],rules:{clearanceMil:.3/.0254,trackWidthMil:.4/.0254,powerWidthMil:.6/.0254,trackWidthMinMil:.3/.0254,copperToEdgeMil:.75/.0254,source:'user requirements'},routedLines:0};
fs.writeFileSync(dir+'rev3_candidate.json',JSON.stringify(candidate,null,2));
const audit={componentCount:components.length,padCount:components.reduce((a,c)=>a+c.pads.length,0),pinNetMismatches:[],bodyGapViolations:[],routingVerified:false};
const sch=JSON.parse(fs.readFileSync('C:/Users/kagas/AppData/Local/Temp/easyeda-copilot-mcp/sch-278287.json','utf8'));
const pins=new Map(sch.components.flatMap(c=>c.pins.map(p=>[c.designator+'.'+p.pin_number,p.signal_name])));
for(const c of components)for(const p of c.pads){const key=c.designator+'.'+p.padNumber;if(pins.get(key)!==p.net)audit.pinNetMismatches.push(key);pins.delete(key);}
audit.missingSchematicPins=[...pins.keys()];
for(let i=0;i<components.length;i++)for(let j=0;j<i;j++){const a=components[i].bbox,b=components[j].bbox;const gap=Math.hypot(Math.max(0,a.minX-b.maxX,b.minX-a.maxX),Math.max(0,a.minY-b.maxY,b.minY-a.maxY));if(gap<40)audit.bodyGapViolations.push({a:components[i].designator,b:components[j].designator,gap});}
fs.writeFileSync(dir+'rev3_candidate_audit.json',JSON.stringify(audit,null,2));
console.log(audit);
