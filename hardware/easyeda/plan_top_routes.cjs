// Offline, conservative top-layer-only maze planner. Native DRC remains authoritative.
const fs=require('fs');
const read=p=>JSON.parse(fs.readFileSync(p,'utf8').replace(/^\uFEFF/,''));
const board=read('hardware/easyeda/output/routing_repair_baseline.json');
const original=read(process.env.TRACK_INPUT||'hardware/easyeda/output/repair_tracks.json');
const removedNets=(process.env.RIP_NETS||'').split(',').filter(Boolean);
let tracks=process.env.FRESH_ROUTE?[]:original.filter(t=>!removedNets.includes(t.net)).map(t=>({...t}));
const pads=board.components.flatMap(c=>c.pads.map(p=>({...p,ref:c.designator})));
const grid=2.5,W=1104,H=1418,N=W*H;
const holes=[[137.7953,-137.7953],[2618.1102,-137.7953],[137.7953,-3405.5118],[2618.1102,-3405.5118]];
function distSeg(x,y,t){let dx=t.endX-t.startX,dy=t.endY-t.startY,l=dx*dx+dy*dy,u=l?Math.max(0,Math.min(1,((x-t.startX)*dx+(y-t.startY)*dy)/l)):0;return Math.hypot(x-t.startX-u*dx,y-t.startY-u*dy);}
function blockedFor(net,width){
 const b=new Uint8Array(N),margin=6+width/2+2;
 function raster(x0,y0,x1,y1,fn){
  let xl=Math.max(0,Math.floor(x0/grid)),xr=Math.min(W-1,Math.ceil(x1/grid)),yl=Math.max(0,Math.floor(-y1/grid)),yr=Math.min(H-1,Math.ceil(-y0/grid));
  for(let y=yl;y<=yr;y++)for(let x=xl;x<=xr;x++)if(!fn||fn(x*grid,-y*grid))b[y*W+x]=1;
 }
 for(let y=0;y<H;y++)for(let x=0;x<W;x++)if(x*grid<25||x*grid>2730||y*grid<25||y*grid>3518)b[y*W+x]=1;
 for(const [x,y]of holes)raster(x-172,y-172,x+172,y+172,(a,c)=>Math.hypot(a-x,c-y)<172);
 for(const p of pads)if(p.net!==net)raster(p.x-p.width/2-margin,p.y-p.height/2-margin,p.x+p.width/2+margin,p.y+p.height/2+margin);
 for(const t of tracks)if(t.net!==net){let r=t.lineWidth/2+margin;raster(Math.min(t.startX,t.endX)-r,Math.min(t.startY,t.endY)-r,Math.max(t.startX,t.endX)+r,Math.max(t.startY,t.endY)+r,(x,y)=>distSeg(x,y,t)<=r);}
 return b;
}
class Heap{constructor(){this.a=[];}push(n,f){let a=this.a,i=a.length;a.push([n,f]);while(i){let p=(i-1)>>1;if(a[p][1]<=f)break;a[i]=a[p];i=p;}a[i]=[n,f];}pop(){let a=this.a,res=a[0],v=a.pop();if(a.length){let i=0;while(2*i+1<a.length){let c=2*i+1;if(c+1<a.length&&a[c+1][1]<a[c][1])c++;if(a[c][1]>=v[1])break;a[i]=a[c];i=c;}a[i]=v;}return res;}get length(){return this.a.length;}}
function search(a,z,b){
 const sx=Math.round(a[0]/grid),sy=Math.round(-a[1]/grid),ex=Math.round(z[0]/grid),ey=Math.round(-z[1]/grid),start=sy*W+sx,end=ey*W+ex;
 if(b[start]||b[end])return null;
 const ds=new Float64Array(N);ds.fill(Infinity);const prev=new Int32Array(N);prev.fill(-1);const closed=new Uint8Array(N),heap=new Heap();ds[start]=0;heap.push(start,0);
 const dirs=[[1,0],[-1,0],[0,1],[0,-1],[1,1],[1,-1],[-1,1],[-1,-1]];
 function h(x,y){let dx=Math.abs(x-ex),dy=Math.abs(y-ey);return Math.max(dx,dy)+.414213562*Math.min(dx,dy);}
 while(heap.length){const [u]=heap.pop();if(closed[u])continue;closed[u]=1;if(u===end){let path=[];for(let v=u;v>=0;v=prev[v])path.push([(v%W)*grid,-Math.floor(v/W)*grid]);path.reverse();path.unshift(a);path.push(z);return shorten(simplify(path),b);}
  const x=u%W,y=Math.floor(u/W);
  for(const [dx,dy]of dirs){const nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=W||ny>=H)continue;let v=ny*W+nx;if(b[v]||closed[v]||(dx&&dy&&(b[y*W+nx]||b[ny*W+x])))continue;const val=ds[u]+(dx&&dy?1.414213562:1);if(val<ds[v]){ds[v]=val;prev[v]=u;heap.push(v,val+h(nx,ny));}}
 }
 return null;
}
function simplify(ps){const out=[];for(const p of ps){if(out.length&&Math.hypot(p[0]-out.at(-1)[0],p[1]-out.at(-1)[1])<.001)continue;while(out.length>1){const a=out.at(-2),b=out.at(-1),cross=(b[0]-a[0])*(p[1]-b[1])-(b[1]-a[1])*(p[0]-b[0]);if(Math.abs(cross)>.001)break;out.pop();}out.push(p);}return out;}
function shorten(ps,blocked){
 function safe(a,z){const len=Math.hypot(a[0]-z[0],a[1]-z[1]),steps=Math.ceil(len/(grid/2));for(let k=0;k<=steps;k++){const t=steps?k/steps:0,x=Math.round((a[0]+t*(z[0]-a[0]))/grid),y=Math.round(-(a[1]+t*(z[1]-a[1]))/grid);if(x<0||y<0||x>=W||y>=H||blocked[y*W+x])return false;}return true;}
 let out=[ps[0]],i=0;while(i<ps.length-1){let found=false;for(let j=ps.length-1;j>i;j--){const a=ps[i],z=ps[j],dx=z[0]-a[0],dy=z[1]-a[1],m=Math.min(Math.abs(dx),Math.abs(dy));const corners=[[a[0]+Math.sign(dx)*m,a[1]+Math.sign(dy)*m],[z[0]-Math.sign(dx)*m,z[1]-Math.sign(dy)*m]];for(const c of corners)if(safe(a,c)&&safe(c,z)){out.push(c,z);i=j;found=true;break;}if(found)break;}if(!found){out.push(ps[++i]);}}
 return simplify(out);
}
function components(net){
 const ps=pads.filter(p=>p.net===net),ts=tracks.filter(t=>t.net===net),points=ps.map(p=>[p.x,p.y]);for(const t of ts)points.push([t.startX,t.startY],[t.endX,t.endY]);
 for(const t of ts){const length=Math.hypot(t.endX-t.startX,t.endY-t.startY),parts=Math.ceil(length/100);for(let k=1;k<parts;k++)points.push([t.startX+(t.endX-t.startX)*k/parts,t.startY+(t.endY-t.startY)*k/parts]);}
 const par=points.map((_,i)=>i);function root(i){while(par[i]!==i){par[i]=par[par[i]];i=par[i];}return i;}function join(a,b){par[root(a)]=root(b);}
 points.forEach((p,i)=>{points.forEach((q,j)=>{if(j<i&&Math.hypot(p[0]-q[0],p[1]-q[1])<.25)join(i,j);});ps.forEach((q,j)=>{if(Math.abs(p[0]-q.x)<=q.width/2&&Math.abs(p[1]-q.y)<=q.height/2)join(i,j);});});
 ts.forEach((t,k)=>{const a=ps.length+2*k;join(a,a+1);points.forEach((p,i)=>{if(distSeg(p[0],p[1],t)<=t.lineWidth/2+.1)join(i,a);});});
 const groups=new Map();points.forEach((p,i)=>{const k=root(i);if(!groups.has(k))groups.set(k,[]);const g=groups.get(k);if(!g.some(q=>q[0]===p[0]&&q[1]===p[1]))g.push(p);});return [...groups.values()];
}
const nets=[...new Set(pads.map(p=>p.net))].filter(n=>n&&n!=='GND'&&(!process.env.ONLY_NETS||process.env.ONLY_NETS.split(',').includes(n)));
function priority(n){return /^P[AC][0-3]$/.test(n)?-2:/3V3/.test(n)?-1:/_BUF$|_SUM$|REF_RAW|VBIAS|REF_MID/.test(n)?0:2;}
let seed=Number(process.env.ORDER_SEED||1);function rand(){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return(seed>>>0)/4294967296;}
const ordering=new Map(nets.map(n=>[n,rand()]));
nets.sort((a,b)=>Number(removedNets.includes(a))-Number(removedNets.includes(b))||(process.env.ORDER_SEED?ordering.get(a)-ordering.get(b):priority(a)-priority(b)||a.localeCompare(b)));
const failed=[],routes=[];
for(const net of nets){
 let groups=components(net),count=0,width=/3V3/.test(net)?Number(process.env.POWER_WIDTH||16):10;
 while(groups.length>1){
  const b=blockedFor(net,width),pairs=[];
  for(let i=0;i<groups.length;i++)for(let j=0;j<i;j++){const options=[];for(const a of groups[i])for(const z of groups[j])options.push({dist:Math.hypot(a[0]-z[0],a[1]-z[1]),best:[a,z]});options.sort((a,b)=>a.dist-b.dist);pairs.push(...options.slice(0,12));}
  pairs.sort((a,b)=>a.dist-b.dist);let path=null;
  for(const pair of pairs){path=search(...pair.best,b);if(path)break;}
  if(!path){failed.push({net,groups:groups.length,points:groups});break;}
  const batch=[];for(let i=1;i<path.length;i++)batch.push({net,layer:1,lineWidth:width,startX:path[i-1][0],startY:path[i-1][1],endX:path[i][0],endY:path[i][1]});tracks.push(...batch);routes.push(...batch);count++;
  const next=components(net);if(next.length>=groups.length)throw Error('No connectivity progress '+net);groups=next;
 }
 if(!process.env.QUIET)console.log(net,{remainingGroups:groups.length,newConnections:count});
}
fs.writeFileSync(process.env.ROUTE_OUTPUT||'hardware/easyeda/output/top_maze_candidate.json',JSON.stringify({routes,failed,trackCount:tracks.length,fresh:!!process.env.FRESH_ROUTE,removedNets},null,2));
console.log({newSegments:routes.length,failed:failed.map(f=>({net:f.net,groups:f.groups}))});
