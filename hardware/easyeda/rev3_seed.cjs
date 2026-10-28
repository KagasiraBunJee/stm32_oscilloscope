// Offline geometry generation only. Never calls EasyEDA.
const fs=require('fs');
const src=JSON.parse(fs.readFileSync('hardware/easyeda/output/rev3_imported.json','utf8'));
const pose={U1:[1380,-1750,0],C8:[1150,-1750,90],C7:[850,-1750,90],FB1:[620,-1750,0],J7:[350,-1700,0],
 R37:[750,-1350,0],R38:[970,-1350,0],R39:[1180,-1350,90],C9:[980,-1530,0]};
const put=(ref,x,y,r=0)=>{if(pose[ref])throw Error('duplicate '+ref);pose[ref]=[x,y,r];};
// Direct paths stay independent of the op amp. Signal/GND headers distributed.
for(const [b,j,d,c,x,y] of [[0,1,1,1,250,-3000],[12,3,4,3,1550,-3200],[24,5,7,5,250,-450]]){
 put('J'+j,x,y);put('R'+(b+1),x+220,y);put('D'+d,x+430,y,270);
 put('C'+c,x+640,y,90);put('J'+(j+8),x+880,y);
}
// Three attenuator chains fan toward their actual U1 input pins.
for(const [b,j,x,y,s] of [[0,2,220,-2450,1],[12,4,2520,-2650,-1],[24,6,2520,-550,-1]]){
 put('J'+j,x,y);
 for(let i=0;i<9;i++){
  const row=Math.floor(i/5),col=row ? 8-i : i;
  put('R'+(b+2+i),x+s*(220+col*190),y+(b===24?-1:1)*row*230,row ? (s===1?180:0):(s===1?0:180));
 }
}
Object.assign(pose,{
 R11:[1120,-2120,90],D2:[950,-1950,270],R12:[1290,-2100,270],C2:[1290,-2350,90],D3:[1080,-2600,270],J10:[1310,-2800,270],
 R23:[1810,-1710,180],D5:[1670,-2110,90],R24:[1510,-2100,270],C4:[1530,-2400,90],D6:[1750,-2420,90],J12:[1770,-2750,270],
 R35:[1740,-1310,180],D8:[1670,-1540,90],R36:[1500,-1260,90],C6:[1430,-1050,90],D9:[1710,-1040,90],J14:[1470,-800,90]});
if(Object.keys(pose).length!==src.components.length)throw Error('count mismatch');
const transform=(x,y,c,p)=>{const a=(p[2]-c.rotation)*Math.PI/180,co=Math.round(Math.cos(a)),si=Math.round(Math.sin(a));return [p[0]+(x-c.x)*co-(y-c.y)*si,p[1]+(x-c.x)*si+(y-c.y)*co];};
src.components=src.components.map(c=>{
 const p=pose[c.designator];if(!p)throw Error('missing '+c.designator);
 const pts=[[c.bbox.minX,c.bbox.minY],[c.bbox.minX,c.bbox.maxY],[c.bbox.maxX,c.bbox.minY],[c.bbox.maxX,c.bbox.maxY]].map(q=>transform(...q,c,p));
 return {...c,x:p[0],y:p[1],rotation:p[2],bbox:{minX:Math.min(...pts.map(q=>q[0])),maxX:Math.max(...pts.map(q=>q[0])),minY:Math.min(...pts.map(q=>q[1])),maxY:Math.max(...pts.map(q=>q[1]))},pads:c.pads.map(q=>{const [x,y]=transform(q.x,q.y,c,p),swap=(p[2]-c.rotation)%180!==0;return {...q,x,y,width:swap?q.height:q.width,height:swap?q.width:q.height};})};
});
fs.writeFileSync('hardware/easyeda/output/rev3_seed.json',JSON.stringify(src,null,2));
console.log('Fresh seed, real imported geometry:',src.components.length);
