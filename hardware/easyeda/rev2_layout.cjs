// Fresh functional placement seed; generates a reviewable, target-pinned playbook.
const fs=require('fs'),dir='hardware/easyeda/output/';
const b=JSON.parse(fs.readFileSync(dir+'rev2_imported.json','utf8'));
const pose={U1:[1380,-1750,0],C8:[1150,-1750,90],R38:[1050,-1600,0],R39:[1220,-1460,90],C9:[1000,-1400,90],R37:[830,-1600,0],C7:[800,-1800,90],FB1:[580,-1750,0],J7:[360,-1750,0],
J1:[200,-3050,0],R1:[430,-3050,0],D1:[650,-3050,270],C1:[830,-3050,90],J9:[1050,-3000,270],
J2:[150,-2350,0],R2:[350,-2350,0],R3:[520,-2350,0],R4:[690,-2350,0],R5:[860,-2350,0],R6:[860,-2180,90],R7:[690,-2180,180],R8:[520,-2180,180],R9:[520,-1980,90],R10:[740,-1980,0],D2:[990,-1950,270],R11:[1150,-2150,90],R12:[1290,-2090,270],C2:[1280,-2310,90],D3:[1110,-2440,270],J10:[1310,-2610,270],
J3:[2400,-3250,90],R13:[2150,-3250,180],D4:[1940,-3250,90],C3:[1750,-3250,90],J11:[1530,-3250,0],
J4:[2600,-2800,0],R14:[2420,-2800,180],R15:[2220,-2800,180],R16:[2020,-2800,180],R17:[2020,-2600,90],R18:[2220,-2600,0],R19:[2420,-2600,0],R20:[2420,-2300,90],R21:[2210,-2150,180],R22:[1880,-1950,180],D5:[1680,-2110,90],R23:[1770,-1720,180],R24:[1510,-2110,270],C4:[1530,-2440,90],D6:[1760,-2440,90],J12:[1700,-2630,270],
J5:[200,-450,0],R25:[440,-450,0],D7:[650,-450,270],C5:[830,-450,90],J13:[1050,-450,0],
J6:[2640,-430,0],R26:[2440,-430,180],R27:[2240,-430,180],R28:[2040,-430,180],R29:[2040,-630,270],R30:[2240,-630,0],R31:[2440,-630,0],R32:[2440,-880,270],R33:[2200,-1030,180],R34:[1840,-1330,180],D8:[1650,-1500,90],R35:[1660,-1280,180],R36:[1560,-1100,90],C6:[1500,-900,90],D9:[1680,-850,90],J14:[1420,-650,90]};
const project='58ec8001acbe89a2f5750486ad8f94acdda71271a34abf7950a7348875b90d8c',doc='920cad72d3d6143e';
const steps=[];
const assertTarget=id=>steps.push({id:'target_'+id,action:'document.current',assert:{'$.uuid':'=='+doc,'$.parentProjectUuid':'=='+project}});
for(const c of b.components){const p=pose[c.designator];if(!p)throw Error('Missing '+c.designator);const [x,y,rotation]=p;assertTarget(c.designator);steps.push({id:'place_'+c.designator,action:'pcb.component.modify',payload:{primitiveId:c.primitiveId,patch:{x,y,rotation,locked:false}}});}
assertTarget('outline');steps.push({id:'outline',action:'pcb.outline.set',payload:{points:[[0,0],[2755.9055,0],[2755.9055,-3543.3071],[0,-3543.3071]],replace:true,lineWidth:1}});
assertTarget('save');steps.push({id:'save',action:'pcb.save'});
fs.writeFileSync(dir+'rev2_layout.playbook.json',JSON.stringify({version:1,meta:{project,doc,name:'Fresh functional single-sided layout'},defaults:{retry:0,continueOnError:false},steps},null,2));
console.log({parts:b.components.length,steps:steps.length});
