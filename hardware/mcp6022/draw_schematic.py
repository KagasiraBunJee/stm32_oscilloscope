"""Generate an exact vector wiring schematic and a drawing scene for PNG export."""
from pathlib import Path
import json,html
out=Path('hardware/mcp6022');out.mkdir(exist_ok=True)
scene=[]
BLUE='#1565a8';RED='#b53730';INK='#172938';GREEN='#26704a';GRAY='#647480'
def line(x1,y1,x2,y2,color=INK,width=3):scene.append(dict(kind='line',x1=x1,y1=y1,x2=x2,y2=y2,color=color,width=width))
def text(x,y,t,size=22,color=INK):scene.append(dict(kind='text',x=x,y=y,text=t,size=size,color=color))
def rect(x,y,w,h,fill='#ffffff',stroke='#ccd6df',width=2):scene.append(dict(kind='rect',x=x,y=y,w=w,h=h,fill=fill,color=stroke,width=width))
def poly(points,fill='#ffffff',stroke=INK,width=3):scene.append(dict(kind='poly',points=points,fill=fill,color=stroke,width=width))
def dot(x,y,color=INK):scene.append(dict(kind='dot',x=x,y=y,color=color))
def resistor_h(x1,x2,y,label,value,color=INK):
 line(x1,y,x1+15,y,color);rect(x1+15,y-13,x2-x1-30,26,stroke=color,width=3);line(x2-15,y,x2,y,color)
 text((x1+x2)/2-35,y-87,label,21);text((x1+x2)/2-45,y-55,value,23)
def resistor_v(x,y1,y2,label,value):
 line(x,y1,x,y1+10);rect(x-13,y1+10,26,y2-y1-20,stroke=INK,width=3);line(x,y2-10,x,y2)
 text(x-155,(y1+y2)/2-15,label,21);text(x-155,(y1+y2)/2+15,value,23)
def ground(x,y):
 line(x,y-15,x,y);line(x-22,y,x+22,y);line(x-14,y+9,x+14,y+9);line(x-6,y+18,x+6,y+18);text(x-27,y+27,'GND',19,GREEN)
def diode_up(x,y,label):
 # Current path: bottom anode -> top cathode. Bar/stripe is at top.
 line(x,y-50,x,y-15);line(x,y+15,x,y+50)
 poly([[x-16,y+15],[x+16,y+15],[x,y-15]],fill='#ffffff')
 line(x-20,y-19,x+20,y-19,INK,4)
 text(x+32,y-24,label+'  1N4148',22);text(x+32,y+7,'stripe at TOP (K)',18,GRAY)
def cap(x,y,kind,label,value):
 line(x,y-50,x,y-7);line(x-23,y-7,x+23,y-7);line(x-23,y+7,x+23,y+7);line(x,y+7,x,y+50)
 text(x+38,y-27,label,21);text(x+38,y+1,value,23)
 if kind=='polar':text(x-43,y-37,'+',25)

rect(0,0,2000,1900,fill='#ffffff',stroke='#ffffff')
text(55,32,'MCP6022 + 1N4148: two-channel measurement buffer',38)
text(58,88,'WHITE -> CH1 / PC0     RED -> CH3 / PC2     All GND labels join; all 3V3 labels join.',24,GRAY)
for offset,name,inputcolor,pinplus,pinminus,pinout,node,rbase,cname,d1,d2,pc in [
 (0,'WHITE',BLUE,3,2,1,'A',1,'C3','D1','D2','PC0 / CH1'),
 (590,'RED',RED,5,6,7,'B',6,'C4','D3','D4','PC2 / CH3')]:
 y=lambda v:v+offset
 rect(35,y(145),1930,560,fill='#fbfcfe')
 text(65,y(161),name+' CHANNEL',27,inputcolor)
 text(65,y(195),'High-resistance divider -> clamps -> unity-gain buffer -> ADC',20,GRAY)
 line(90,y(350),230,y(350),inputcolor);dot(90,y(350),inputcolor);text(65,y(292),name+'_IN',24,inputcolor)
 resistor_h(230,350,y(350),'R'+str(rbase),'1 MΩ',inputcolor);line(350,y(350),390,y(350),inputcolor)
 resistor_h(390,510,y(350),'R'+str(rbase+1),'33 kΩ',inputcolor);line(510,y(350),550,y(350),inputcolor)
 resistor_h(550,670,y(350),'R'+str(rbase+2),'1 kΩ',inputcolor);line(670,y(350),1130,y(350),inputcolor)
 dot(740,y(350),inputcolor);text(610,y(370),'NODE '+node,20,inputcolor)
 line(740,y(350),740,y(430));resistor_v(740,y(430),y(520),'R'+str(rbase+3),'220 kΩ');line(740,y(520),740,y(595));ground(740,y(595))
 # Clamp branch is joined only at y350.
 dot(900,y(350),inputcolor);line(900,y(350),900,y(310));diode_up(900,y(260),d2);line(900,y(210),900,y(202))
 text(850,y(175),'3V3',22,GREEN)
 line(900,y(350),900,y(410));diode_up(900,y(460),d1);line(900,y(510),900,y(595));ground(900,y(595))
 poly([[1130,y(290)],[1130,y(470)],[1280,y(380)]],fill='#edf5fb',stroke=inputcolor)
 text(1145,y(325),'+',25);text(1145,y(400),'−',25)
 text(1060,y(323),'pin '+str(pinplus),19);text(1110,y(477),'pin '+str(pinminus),19)
 text(1115,y(233),'U1'+('A' if node=='A' else 'B')+'  MCP6022',24)
 line(1090,y(420),1130,y(420));line(1090,y(420),1090,y(550));line(1090,y(550),1320,y(550));line(1320,y(550),1320,y(380))
 line(1280,y(380),1390,y(380),inputcolor);dot(1320,y(380),inputcolor);text(1282,y(337),'pin '+str(pinout),20)
 text(1100,y(561),'Direct OUT-to-minus feedback',18,GRAY)
 resistor_h(1390,1510,y(380),'R'+str(rbase+4),'100 Ω',inputcolor);line(1510,y(380),1870,y(380),inputcolor)
 dot(1630,y(380),inputcolor);line(1630,y(380),1630,y(430));cap(1630,y(480),'ceramic',cname,'1 nF');line(1630,y(530),1630,y(595));ground(1630,y(595))
 dot(1870,y(380),inputcolor);text(1720,y(328),pc,27,inputcolor);text(1720,y(410),'STM32 ADC pin',20,GRAY)
 text(1450,y(660),'Capacitor is AFTER 100 Ω',19,GRAY)

# Power network and a physical pinout inset.
rect(35,1330,1150,355,fill='#f7fbf8')
text(60,1348,'POWER: STM32 3.3 V and GND',27,GREEN)
text(60,1388,'Do not power the chip from intercom RED.',22,RED)
line(100,1460,1030,1460,GREEN);line(100,1620,1030,1620,GREEN)
text(60,1428,'3V3',21,GREEN);text(60,1633,'GND',21,GREEN)
for x,ctype,label,value in [(310,'ceramic','C1','100 nF'),(610,'polar','C2','1 µF')]:
 dot(x,1460,GREEN);line(x,1460,x,1490,GREEN);cap(x,1540,ctype,label,value);line(x,1590,x,1620,GREEN);dot(x,1620,GREEN)
text(985,1428,'U1 pin 8',22);text(985,1633,'U1 pin 4',22)
text(70,1650,'C1 closest to chip. C2 stripe / negative lead goes to GND.',20,GRAY)
rect(1210,1330,755,355,fill='#fbfcfe')
text(1235,1348,'CHIP TOP VIEW — pin-1 dot upper left',24)
rect(1450,1412,280,220,fill='#edf5fb',stroke=INK,width=3)
dot(1470,1435);text(1495,1500,'MCP6022',25)
left=['1  OUT A','2  − A','3  + A','4  GND'];right=['8  3V3','7  OUT B','6  − B','5  + B']
for i in range(4):
 yy=1440+i*53;line(1420,yy,1450,yy);line(1730,yy,1760,yy)
 text(1250,yy-14,left[i],21);text(1770,yy-14,right[i],21)
text(1240,1650,'Adapter underside is mirrored. Check pad numbering.',19,GRAY)
text(55,1720,'INSTALLATION: remove original 47 kΩ / 10 kΩ divider pairs on PC0 and PC2; leave PC1 unchanged.',24,RED)
text(55,1762,'BLACK -> common GND. YELLOW unused. Inputs 0–15 V only. Power off before soldering or rewiring.',23)
text(55,1804,'Test off-intercom first: 3.3 V input -> 0.579 V output; 13.6 V input -> 2.386 V output. Nominal scale ×5.7.',23)
text(55,1846,'Bench prototype: not isolated or surge-qualified. Keep buffer powered while intercom inputs are attached.',22,GRAY)

svg=['<svg xmlns="http://www.w3.org/2000/svg" width="2000" height="1900" viewBox="0 0 2000 1900">']
for s in scene:
 k=s['kind'];col=s.get('color',INK)
 if k=='line':svg.append(f'<line x1="{s["x1"]}" y1="{s["y1"]}" x2="{s["x2"]}" y2="{s["y2"]}" stroke="{col}" stroke-width="{s["width"]}"/>')
 elif k=='text':svg.append(f'<text x="{s["x"]}" y="{s["y"]+s["size"]}" font-family="Segoe UI, sans-serif" font-size="{s["size"]}" fill="{col}">{html.escape(s["text"])}</text>')
 elif k=='rect':svg.append(f'<rect x="{s["x"]}" y="{s["y"]}" width="{s["w"]}" height="{s["h"]}" fill="{s["fill"]}" stroke="{col}" stroke-width="{s["width"]}"/>')
 elif k=='dot':svg.append(f'<circle cx="{s["x"]}" cy="{s["y"]}" r="5" fill="{col}"/>')
 elif k=='poly':svg.append(f'<polygon points="'+ ' '.join(f'{x},{y}' for x,y in s['points'])+f'" fill="{s["fill"]}" stroke="{col}" stroke-width="{s["width"]}"/>')
svg.append('</svg>')
(out/'mcp6022_1n4148_schematic.svg').write_text('\n'.join(svg),encoding='utf-8')
(out/'schematic_scene.json').write_text(json.dumps(scene,ensure_ascii=False),encoding='utf-8')
print('Saved SVG and drawing scene')
