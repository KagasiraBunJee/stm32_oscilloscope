const fs=require('fs'),dir='hardware/easyeda/output/';
const before=JSON.parse(fs.readFileSync(dir+'rev3_imported.json','utf8'));
const live=JSON.parse(fs.readFileSync(dir+'rev3_preapply.json','utf8'));
const candidate=JSON.parse(fs.readFileSync(dir+'rev3_candidate.json','utf8'));
const project='58ec8001acbe89a2f5750486ad8f94acdda71271a34abf7950a7348875b90d8c',doc='74199e97dae8261c';
if(live.components.length!==before.components.length)throw Error('component count changed');
for(const c of before.components){const l=live.components.find(x=>x.primitiveId===c.primitiveId);if(!l||['x','y','rotation','layer'].some(k=>Math.abs(l[k]-c[k])>0.01)||JSON.stringify(l.pads.map(p=>[p.padNumber,p.net]))!==JSON.stringify(c.pads.map(p=>[p.padNumber,p.net])))throw Error('Live changed '+c.designator);}
const steps=[];
const add=(id,action,payload)=>{steps.push({id:'target_'+id,action:'document.current',assert:{'$.uuid':'=='+doc,'$.parentProjectUuid':'=='+project}});steps.push({id,action,...(payload?{payload}:{})});};
for(const c of candidate.components)add('place_'+c.designator,'pcb.component.modify',{primitiveId:c.primitiveId,patch:{x:c.x,y:c.y,rotation:c.rotation,layer:1,locked:false}});
add('outline','pcb.outline.set',{points:candidate.outline.points,replace:true,lineWidth:1});
candidate.holes.forEach(([x,y],i)=>add('M3_'+i,'pcb.fill.create',{layer:12,points:Array.from({length:72},(_,k)=>[x+1.6/.0254*Math.cos(k*Math.PI/36),y+1.6/.0254*Math.sin(k*Math.PI/36)]),lineWidth:0,locked:true}));
add('save','pcb.save');
fs.writeFileSync(dir+'rev3_layout.playbook.json',JSON.stringify({version:1,meta:{project,doc,name:'Approved Rev3 placement and M3 cutouts'},defaults:{retry:0,continueOnError:false},steps},null,2));
console.log('Verified live baseline; generated',steps.length,'steps');
