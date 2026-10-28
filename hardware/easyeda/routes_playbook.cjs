// Convert an inspected top-only route candidate to typed actions with target assertions.
const fs=require('fs');
const input=process.argv[2],output=process.argv[3],baseline=Number(process.argv[4]);
const data=JSON.parse(fs.readFileSync(input,'utf8').replace(/^\uFEFF/,''));
if(data.fresh)throw Error('Fresh-board plans require separately reviewed copper replacement.');
const project='58ec8001acbe89a2f5750486ad8f94acdda71271a34abf7950a7348875b90d8c',doc='52caa9452ee66dee';
const target=i=>({id:'target_'+i,action:'document.current',assert:{'$.uuid':'=='+doc,'$.parentProjectUuid':'=='+project}});
const steps=[target('baseline'),{id:'baseline',action:'pcb.line.list',assert:{'$.count':'=='+baseline}}];
data.routes.forEach((r,i)=>{if(r.layer!==1||r.lineWidth<9.8425)throw Error('Unsupported layer or undersized track');const {net,layer,lineWidth,startX,startY,endX,endY}=r;steps.push(target(i),{id:'track_'+i,action:'pcb.line.create',payload:{net,layer,lineWidth,startX,startY,endX,endY},assert:{'$.primitiveId':'exists'}});});
steps.push({id:'save',action:'pcb.save',assert:{'$.saved':'true'}});
fs.writeFileSync(output,JSON.stringify({version:1,meta:{name:input,project,doc},defaults:{retry:0,continueOnError:false},steps},null,2));
console.log({segments:data.routes.length,steps:steps.length,output});
