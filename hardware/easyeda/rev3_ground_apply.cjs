const fs=require('fs'),dir='hardware/easyeda/output/';
const candidate=JSON.parse(fs.readFileSync(dir+'rev3_ground_bias_preview.json','utf8'));
const steps=[],doc='74199e97dae8261c',project='58ec8001acbe89a2f5750486ad8f94acdda71271a34abf7950a7348875b90d8c';
candidate.routes.forEach((r,i)=>{if(r.net!=='GND'||r.layer!==1||r.lineWidth<.6/.0254-1e-5)throw Error('unexpected copper');steps.push({id:'target'+i,action:'document.current',assert:{'$.uuid':'=='+doc,'$.parentProjectUuid':'=='+project}},{id:'ground'+i,action:'pcb.line.create',payload:r});});
steps.push({id:'save',action:'pcb.save'});
fs.writeFileSync(dir+'rev3_ground.playbook.json',JSON.stringify({version:1,meta:{project,doc,name:'Verified collision-free top GND links'},defaults:{retry:0,continueOnError:false},steps},null,2));
