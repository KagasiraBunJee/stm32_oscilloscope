const fs=require('fs'),d='hardware/easyeda/output/';
const b=JSON.parse(fs.readFileSync(d+'rev3_candidate.json','utf8'));
const project='58ec8001acbe89a2f5750486ad8f94acdda71271a34abf7950a7348875b90d8c',doc='74199e97dae8261c';
const steps=[];b.holes.forEach(([x,y],i)=>{
 steps.push({id:'target'+i,action:'document.current',assert:{'$.uuid':'=='+doc,'$.parentProjectUuid':'=='+project}});
 steps.push({id:'keepout'+i,action:'pcb.region.create',payload:{layer:1,ruleType:['no-wires','no-fills','no-pours'],locked:true,points:Array.from({length:72},(_,k)=>[x+3/.0254*Math.cos(k*Math.PI/36),y+3/.0254*Math.sin(k*Math.PI/36)])}});
});steps.push({id:'save',action:'pcb.save'});
fs.writeFileSync(d+'rev3_keepouts.playbook.json',JSON.stringify({version:1,meta:{project,doc,name:'M3 hardware top copper exclusions'},defaults:{retry:0,continueOnError:false},steps},null,2));
