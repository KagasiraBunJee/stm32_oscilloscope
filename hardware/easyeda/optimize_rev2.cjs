// Offline candidate search only. Does not call EasyEDA or modify the board.
const fs = require('fs');
const src = JSON.parse(fs.readFileSync(process.env.PLACEMENT_SOURCE || 'hardware/easyeda/output/rev2_seed.json', 'utf8').replace(/^\uFEFF/, ''));
const placementOutput = process.env.PLACEMENT_OUTPUT || 'hardware/easyeda/output/rev2_optimized.json';
const cs = src.components;
const orig = cs.map(c => [c.x,c.y,c.rotation]);
let state = orig.map(p=>p.slice());
const movable = cs.map((c,i)=>i).filter(i=>cs[i].designator !== 'C8');
const ui=cs.findIndex(c=>c.designator==='U1'), ci=cs.findIndex(c=>c.designator==='C8');
const nets = new Map();
const pads = [];
cs.forEach((c,i)=>c.pads.forEach(p=>{
 const k=pads.length;
 pads.push({i,dx:p.x-c.x,dy:p.y-c.y,w:p.width,h:p.height,net:p.net});
 if(p.net && !(/^(?:[adp])?gnd$|^v(?:cc|dd|ss|in|out|bus|bat|sys|ref)\b|^[+-]?\d+v\d*$|^[+-]|gnd|vcc|vdd|vss/i.test(p.net))){
  if(!nets.has(p.net))nets.set(p.net,[]); nets.get(p.net).push(k);
 }
}));
const netNames=[...nets.keys()];
const groups=[...nets.values()];
const xy=pads.map(()=>[0,0]);
const boxes=cs.map(()=>[0,0,0,0]);
const holes=[[137.7953,-137.7953],[2618.1102,-137.7953],[137.7953,-3405.5118],[2618.1102,-3405.5118]];
function evaluate(){
 // U1 is movable; retain its pin-facing orientation.
 state[ci]=[state[ui][0]-230,state[ui][1],90]; // Preserve supply decoupler proximity.
 for(const b of boxes){b[0]=b[1]=Infinity;b[2]=b[3]=-Infinity;}
 // Respect actual footprint/body extents, not only the copper pad envelope.
 cs.forEach((c,i)=>{
  if(!c.bbox)return;
  const s=state[i],r=(s[2]-orig[i][2])*Math.PI/180,co=Math.round(Math.cos(r)),si=Math.round(Math.sin(r)),b=boxes[i];
  for(const x0 of [c.bbox.minX,c.bbox.maxX])for(const y0 of [c.bbox.minY,c.bbox.maxY]){
   const x=s[0]+(x0-c.x)*co-(y0-c.y)*si,y=s[1]+(x0-c.x)*si+(y0-c.y)*co;
   b[0]=Math.min(b[0],x);b[1]=Math.min(b[1],y);b[2]=Math.max(b[2],x);b[3]=Math.max(b[3],y);
  }
 });
 pads.forEach((p,k)=>{
  const s=state[p.i],r=(s[2]-orig[p.i][2])*Math.PI/180,co=Math.round(Math.cos(r)),si=Math.round(Math.sin(r));
  const x=s[0]+p.dx*co-p.dy*si,y=s[1]+p.dx*si+p.dy*co;
  xy[k][0]=x;xy[k][1]=y;
  const w=Math.abs(co)*p.w+Math.abs(si)*p.h,h=Math.abs(si)*p.w+Math.abs(co)*p.h,b=boxes[p.i];
  b[0]=Math.min(b[0],x-w/2);b[1]=Math.min(b[1],y-h/2);b[2]=Math.max(b[2],x+w/2);b[3]=Math.max(b[3],y+h/2);
 });
 let penalty=0,violations=0;
 for(let i=0;i<boxes.length;i++){
  const a=boxes[i];
  const out=Math.max(0,40-a[0],40+a[3],a[2]-2715,a[1]<-3503?-3503-a[1]:0);
  if(out>0){penalty+=100000+out*1000;violations++;}
  for(const h of holes){const dx=Math.max(a[0]-h[0],h[0]-a[2],0),dy=Math.max(a[1]-h[1],h[1]-a[3],0),gap=Math.hypot(dx,dy);if(gap<118){penalty+=100000+(118-gap)*1000;violations++;}}
  for(let j=0;j<i;j++){
   const b=boxes[j],dx=Math.max(a[0]-b[2],b[0]-a[2],0),dy=Math.max(a[1]-b[3],b[1]-a[3],0),gap=Math.hypot(dx,dy);
   if(gap<42){penalty+=100000+(42-gap)*1000;violations++;}
  }
 }
 const edges=[];let length=0,criticalPenalty=0;
 groups.forEach((g,ni)=>{
  const used=new Array(g.length).fill(false),dist=new Array(g.length).fill(Infinity),from=new Array(g.length).fill(-1);dist[0]=0;
  for(let t=0;t<g.length;t++){
   let u=-1,bd=Infinity;for(let v=0;v<g.length;v++)if(!used[v]&&dist[v]<bd){bd=dist[v];u=v;}
   if(u<0)break;used[u]=true;
   if(from[u]>=0){const a=xy[g[from[u]]],b=xy[g[u]],l=Math.sqrt(bd);edges.push([a[0],a[1],b[0],b[1],ni]);length+=l;if(/_SUM$|REF_RAW|_BUF$/.test(netNames[ni]))criticalPenalty+=l*4+Math.max(0,l-300)*100;}
   for(let v=0;v<g.length;v++)if(!used[v]){const a=xy[g[u]],b=xy[g[v]],d=(a[0]-b[0])**2+(a[1]-b[1])**2;if(d<dist[v]){dist[v]=d;from[v]=u;}}
  }
 });
 let crossings=0;
 for(let i=0;i<edges.length;i++)for(let j=0;j<i;j++){
  const a=edges[i],b=edges[j];if(a[4]===b[4])continue;
  const ax=a[2]-a[0],ay=a[3]-a[1],bx=b[2]-b[0],by=b[3]-b[1],d=ax*by-ay*bx;if(Math.abs(d)<1e-9)continue;
  const dx=b[0]-a[0],dy=b[1]-a[1],t=(dx*by-dy*bx)/d,u=(dx*ay-dy*ax)/d;
  if(t>1e-6&&t<1-1e-6&&u>1e-6&&u<1-1e-6)crossings++;
 }
 return {cost:penalty+crossings*1000+length*.08+criticalPenalty,crossings,length,violations,criticalPenalty};
}
let cur=evaluate(),best={...cur},bestState=state.map(s=>s.slice());
console.log('baseline',cur);
if(process.argv[2]){const p=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));state=p.placements.map(c=>[c.x,c.y,c.rotation]);cur=evaluate();best={...cur};bestState=state.map(s=>s.slice());}
let seed=Number(process.env.PLACEMENT_SEED || 20260907);function rand(){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return(seed>>>0)/4294967296;}
const steps=Number(process.env.SEARCH_STEPS||150000);
for(let it=0;it<steps;it++){
 const i=movable[Math.floor(rand()*movable.length)],prev=state[i].slice();
 const cycle=(it%50000)/50000,temp=350*(1-cycle)**3+4;
 const scale=rand()<.1?300:rand()<.4?100:25;
 if(rand()<.16&&i!==ui)state[i][2]=(state[i][2]+(rand()<.5?90:180))%360;
 else{state[i][0]=Math.round((state[i][0]+(rand()-.5)*2*scale)/5)*5;state[i][1]=Math.round((state[i][1]+(rand()-.5)*2*scale)/5)*5;}
 const next=evaluate();
 if(next.cost<cur.cost||rand()<Math.exp((cur.cost-next.cost)/temp)){cur=next;}else{state[i]=prev;}
 if(cur.cost<best.cost){best={...cur};bestState=state.map(s=>s.slice());}
 if((it+1)%25000===0){console.log(it+1,best);fs.writeFileSync(placementOutput,JSON.stringify({metrics:best,placements:cs.map((c,k)=>({designator:c.designator,primitiveId:c.primitiveId,x:bestState[k][0],y:bestState[k][1],rotation:bestState[k][2]}))},null,2));}
 if((it+1)%50000===0){state=bestState.map(s=>s.slice());cur=evaluate();}
}
