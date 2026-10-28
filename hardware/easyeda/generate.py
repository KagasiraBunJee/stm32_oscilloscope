"""Generate the A0 engineering draft. Standard-library Python only.

No autorouting, ERC, DRC, or analog simulation is performed by this script.
Coordinates in the component table are millimetres from the top-left corner.
"""
import collections
import html
import itertools
import json
from pathlib import Path
import uuid
import zipfile

OUT = Path(__file__).resolve().parent / 'output'
parts = []
counts = collections.Counter()


def add(prefix, value, footprint, pins, x, y, spec=''):
    counts[prefix] += 1
    ref = f'{prefix}{counts[prefix]}'
    parts.append(dict(ref=ref, value=value, footprint=footprint,
                      pins={str(i): n for i, n in enumerate(pins, 1)},
                      x_mm=x, y_mm=y, specification=spec,
                      uuid=uuid.uuid5(uuid.NAMESPACE_URL, 'osc-afe-a0/' + ref).hex))
    return ref


def resistor(value, a, b, x, y, precision=False):
    return add('R', value, 'R0805', [a, b], x, y,
               '0.1%, <=25 ppm/K, >=0.125 W, >=50 V' if precision
               else '1%, >=0.125 W, >=50 V')


def capacitor(value, a, x, y):
    return add('C', value, 'C0805', [a, 'GND'], x, y,
               'C0G, 5%, >=25 V' if value == '1nF' else 'X7R, 10%, >=16 V')


for ch in range(1, 4):
    y = 15 + (ch - 1) * 22
    direct, high = f'PA{ch}', f'PC{ch-1}'
    di, hi, summ, buf = [f'CH{ch}_{s}' for s in ('DIRECT', 'HIGH', 'SUM', 'BUF')]
    add('J', f'CH{ch} DIRECT 0-3.3V', 'HDR1X2', [di, 'GND'], 7, y - 5)
    add('J', f'CH{ch} HIGH +/-15V', 'HDR1X2', [hi, 'GND'], 7, y + 5)
    resistor('100R', di, direct, 15, y - 5)
    capacitor('1nF', direct, 21, y - 5)
    add('D', 'BAT54S', 'SOT23', ['GND', '3V3_A', direct], 27, y - 5,
        'Nexperia BAT54S, series pair; pin 3 midpoint')
    last = hi
    for i in range(9):
        nxt = summ if i == 8 else f'CH{ch}_ATT{i+1}'
        resistor('100k', last, nxt, 15 + (i % 5) * 6, y + 3 + (i // 5) * 5, True)
        last = nxt
    resistor('100k', summ, 'VBIAS', 42, y + 8, True)
    add('D', 'BAS70-04', 'SOT23', ['GND', '3V3_A', summ], 43, y + 2,
        'Series-pair SOT23; verify chosen manufacturer pin 3 midpoint')
    resistor('33R', buf, high, 57, y - 4)
    capacitor('1nF', high, 63, y - 4)
    add('D', 'BAT54S', 'SOT23', ['GND', '3V3_A', high], 61, y + 3,
        'Nexperia BAT54S, series pair; pin 3 midpoint')

add('J', 'POWER 3.3V', 'HDR1X2', ['3V3_IN', 'GND'], 8, 79)
add('J', 'STM32 OUTPUTS', 'HDR2X4',
    ['PA1', 'PC0', 'PA2', 'PC1', 'PA3', 'PC2', 'GND', 'GND'], 60, 73.92)
add('FB', 'Ferrite 600R@100MHz', 'R0805', ['3V3_IN', '3V3_A'], 15, 79,
    '>=100 mA, DCR <=0.5 ohm; verify rail drop during test')
capacitor('10uF', '3V3_A', 21, 79)
capacitor('100nF', '3V3_A', 50, 38)
resistor('40k', '3V3_A', 'REF_MID', 28, 78, True)
resistor('40k', 'REF_MID', 'REF_RAW', 34, 78, True)
resistor('100k', 'REF_RAW', 'GND', 40, 78, True)
capacitor('100nF', 'REF_RAW', 46, 78)
add('U', 'TLV9064IDR', 'SOIC14',
    ['VBIAS', 'VBIAS', 'REF_RAW', '3V3_A', 'CH1_SUM', 'CH1_BUF', 'CH1_BUF',
     'CH2_BUF', 'CH2_BUF', 'CH2_SUM', 'GND', 'CH3_SUM', 'CH3_BUF', 'CH3_BUF'],
    51, 46, 'TI TLV9064IDR, SOIC-14 D package; 3.3 V quad buffer')


def pads(p):
    """Return pin, dx, dy, width, height, drill (mm). Draft land patterns."""
    fp = p['footprint']
    if fp in ('R0805', 'C0805'):
        return [(1, -1, 0, 1.2, 1.4, 0), (2, 1, 0, 1.2, 1.4, 0)]
    if fp == 'SOT23':
        return [(1, -.95, 1.05, .65, 1.1, 0), (2, .95, 1.05, .65, 1.1, 0),
                (3, 0, -1.05, .65, 1.1, 0)]
    if fp == 'SOIC14':
        return [(i + 1, -2.7, (i - 3) * 1.27, 1.8, .6, 0) for i in range(7)] + [
            (14 - i, 2.7, (i - 3) * 1.27, 1.8, .6, 0) for i in range(7)]
    if fp == 'HDR1X2':
        return [(i + 1, 0, i * 2.54, 1.8, 1.8, 1) for i in range(2)]
    if fp == 'HDR2X4':
        return [(i + 1, (i % 2) * 2.54, (i // 2) * 2.54, 1.8, 1.8, 1)
                for i in range(8)]
    raise ValueError(fp)


serial = itertools.count(1)


def gid():
    return 'gge' + str(next(serial))


def unit(mm):
    return round(mm / .254, 5)


def ref_path(ref, x, y):
    """Small stroke font: EasyEDA PCB TEXT stores rendered paths explicitly."""
    # Use explicit coordinate pairs for clarity in the font data.
    glyphs = {
        'R': [[(0,1),(0,0),(.6,0),(.6,.5),(0,.5)],[(0,.5),(.6,1)]],
        'C': [[(.6,0),(0,0),(0,1),(.6,1)]],
        'D': [[(0,0),(0,1),(.6,1),(.6,0),(0,0)]],
        'U': [[(0,0),(0,1),(.6,1),(.6,0)]],
        'J': [[(.6,0),(.6,1),(0,1),(0,.7)]],
        'F': [[(0,1),(0,0),(.6,0)],[(0,.5),(.5,.5)]],
        'B': [[(0,0),(0,1),(.6,1),(.6,0),(0,0)],[(0,.5),(.6,.5)]],
    }
    segments = [[(0,0),(.6,0)],[(.6,0),(.6,.5)],[(.6,.5),(.6,1)],
                [(0,1),(.6,1)],[(0,.5),(0,1)],[(0,0),(0,.5)],[(0,.5),(.6,.5)]]
    for digit, indices in enumerate(['012345','12','01643','01236','5126','05236','054326','012','0123456','012356']):
        glyphs[str(digit)] = [segments[int(k)] for k in indices]
    paths = []
    for i, char in enumerate(ref):
        for stroke in glyphs[char]:
            paths.append(' '.join(f'{"M" if j == 0 else "L"} {unit(x+i*.9+dx)} {unit(y+dy)}'
                                  for j, (dx, dy) in enumerate(stroke)))
    return ' '.join(paths)


def text(kind, x, y, value):
    return f'T~{kind}~{x}~{y}~0~#000080~Arial~~~~~comment~{value}~1~start~{gid()}~0~'


def schematic():
    shapes = [text('L', 60, 40, 'OSC AFE A0 - ENGINEERING DRAFT - NOT FOR FABRICATION'),
              text('L', 60, 60, 'Numbered functional symbols; equal net labels connect. Common ground, NOT isolated.')]
    for i, p in enumerate(parts):
        x, y = 160 + (i % 5) * 320, 130 + (i // 5) * 230
        np = len(p['pins'])
        height = max(40, np * 12)
        attrs = f"package`{p['footprint']}`nameAlias`Value`Value`{p['value']}`spicePre`{p['ref'].rstrip('0123456789')}`"
        sub = [text('P', x - 40, y - 25, p['ref']), text('N', x - 40, y - 10, p['value']),
               f'PL~{x-40} {y} {x+40} {y} {x+40} {y+height} {x-40} {y+height} {x-40} {y}~#800000~1~0~none~{gid()}~0']
        for number, net in p['pins'].items():
            py, px = y + 10 + (int(number) - 1) * 12, x - 60
            sub.append(f'P~show~0~{number}~{px}~{py}~180~{gid()}~0^^{px}~{py}^^M {px} {py} h 20~#880000^^1~{x-35}~{py+3}~0~{number}~start~~~#0000FF^^1~{px+10}~{py-3}~0~{number}~middle~~~#0000FF^^0~{x-43}~{py}^^0~M {x-40} {py-3} L {x-37} {py} L {x-40} {py+3}')
            shapes += [f'W~{px-70} {py} {px} {py}~#008800~1~0~none~{gid()}~0',
                       f'N~{px-70}~{py}~0~#0000FF~{net}~{gid()}~start~{px-68}~{py-2}~Arial~7pt~0']
        p['schematic_id'] = gid()
        shapes.append(f"LIB~{x}~{y}~{attrs}~~0~{p['schematic_id']}~{p['uuid']}~{p['uuid']}~0~~yes~yes#@$" + '#@$'.join(sub))
    return dict(head=dict(docType='1', editorVersion='6.5.51', c_para={}, x='0', y='0'),
                canvas='CA~2400~4200~#FFFFFF~yes~#CCCCCC~10~2400~4200~line~10~pixel~5~0~0',
                shape=shapes)


def pcb():
    shapes = []
    outline = ' '.join(str(unit(v)) for v in (0, 0, 70, 0, 70, 90, 0, 90, 0, 0))
    shapes.append(f'TRACK~{unit(.15)}~10~~{outline}~{gid()}~0')
    for x, y in ((3.5, 3.5), (66.5, 3.5), (3.5, 86.5), (66.5, 86.5)):
        shapes.append(f'HOLE~{unit(x)}~{unit(y)}~{unit(3.2)}~{gid()}~0')
    for p in parts:
        x, y = p['x_mm'], p['y_mm']
        sub = [f"TEXT~P~{unit(x-2)}~{unit(y-2.6)}~{unit(.15)}~0~~3~~{unit(1)}~{p['ref']}~{ref_path(p['ref'], x-2, y-2.6)}~~{gid()}~0"]
        for pin, dx, dy, w, h, drill in pads(p):
            shape = 'ELLIPSE' if drill and pin != 1 else 'RECT'
            points = '' if shape == 'ELLIPSE' else ' '.join(str(unit(v)) for v in
                (x+dx-w/2,y+dy-h/2,x+dx+w/2,y+dy-h/2,x+dx+w/2,y+dy+h/2,x+dx-w/2,y+dy+h/2))
            sub.append(f"PAD~{shape}~{unit(x+dx)}~{unit(y+dy)}~{unit(w)}~{unit(h)}~{11 if drill else 1}~{p['pins'][str(pin)]}~{pin}~{unit(drill/2)}~{points}~0~{gid()}~0~~Y~0")
        # Standard -> Pro importer uses the LIB instance ID to link twins.
        shapes.append(f"LIB~{unit(x)}~{unit(y)}~package`{p['footprint']}`~0~~{p['schematic_id']}~1~{p['uuid']}~0~0~#@$" + '#@$'.join(sub))
    names = ['TopLayer', 'BottomLayer', 'TopSilkLayer', 'BottomSilkLayer', 'TopPasteMaskLayer',
             'BottomPasteMaskLayer', 'TopSolderMaskLayer', 'BottomSolderMaskLayer',
             'Ratlines', 'BoardOutline', 'Multi-Layer', 'Document']
    return dict(head=dict(docType='3', editorVersion='6.5.51', c_para={}, x='0', y='0'),
                canvas='CA~2400~2400~#000000~yes~#FFFFFF~10~1200~1200~line~1~mil~1~45~visible~0.5~0~0',
                shape=shapes, layers=[f'{i}~{n}~{c}~true~true~true' for i, (n, c) in enumerate(zip(names,
                    ['#FF0000', '#0000FF', '#FFFF00', '#FFFF00'] + ['#AAAAAA'] * 8), 1)],
                preference={'hideFootprints': '', 'hideNets': ''},
                DRCRULE={'Default': {'trackWidth': unit(.25), 'clearance': unit(.25),
                                    'viaHoleDiameter': unit(.3), 'viaHoleD': unit(.6)}})


def preview():
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="700" height="980" viewBox="-2 -6 74 103">',
         '<rect x="0" y="0" width="70" height="90" fill="#16382e" stroke="black" stroke-width=".2"/>',
         '<g fill="white" font-family="sans-serif" font-size="1.6">',
         '<text x="0" y="-3">A0: 70 x 90 mm — PLACEMENT ONLY / UNROUTED</text>']
    for x, y in ((3.5, 3.5), (66.5, 3.5), (3.5, 86.5), (66.5, 86.5)):
        s.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="white" stroke="#bbb"/>')
    for p in parts:
        x, y = p['x_mm'], p['y_mm']
        s.append(f'<text x="{x-2}" y="{y-2.5}">{p["ref"]}</text>')
        for pin, dx, dy, w, h, drill in pads(p):
            s.append(f'<rect x="{x+dx-w/2}" y="{y+dy-h/2}" width="{w}" height="{h}" fill="#dcb66d"><title>{html.escape(p["ref"]+":"+str(pin)+" "+p["pins"][str(pin)])}</title></rect>')
            if drill:
                s.append(f'<circle cx="{x+dx}" cy="{y+dy}" r="{drill/2}" fill="#222"/>')
    s.extend(['<text x="0" y="94" fill="black">Draft footprints. No copper routes. Not a fabrication preview.</text>', '</g></svg>'])
    return '\n'.join(s)


def validate():
    refs = [p['ref'] for p in parts]
    assert len(refs) == len(set(refs))
    copper = []
    for p in parts:
        assert set(p['pins']) == {str(v[0]) for v in pads(p)}
        for pin, dx, dy, w, h, drill in pads(p):
            x, y = p['x_mm'] + dx, p['y_mm'] + dy
            assert 0 < x-w/2 < x+w/2 < 70 and 0 < y-h/2 < y+h/2 < 90
            copper.append((p['ref'], pin, x, y, w, h))
    for a, b in itertools.combinations(copper, 2):
        assert abs(a[2]-b[2]) >= (a[4]+b[4])/2+.2 or abs(a[3]-b[3]) >= (a[5]+b[5])/2+.2, (a, b)
    nets = collections.defaultdict(list)
    for p in parts:
        for pin, net in p['pins'].items():
            nets[net].append(f'{p["ref"]}.{pin}')
    assert all(len(v) >= 2 for v in nets.values()), 'Dangling net'
    return dict(nets)


def main():
    nets = validate()
    OUT.mkdir(exist_ok=True)
    sch, board = schematic(), pcb()
    documents = {'analog_front_end_schematic.json': sch, 'analog_front_end_pcb.json': board,
                 'components.json': parts, 'connectivity.json': nets}
    for name, content in documents.items():
        (OUT / name).write_text(json.dumps(content, indent=2) + '\n', encoding='utf-8')
    with zipfile.ZipFile(OUT / 'analog_front_end_import.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for name in list(documents)[:2]:
            z.write(OUT / name, name)
    (OUT / 'placement.svg').write_text(preview(), encoding='utf-8')
    grouped = collections.defaultdict(list)
    for p in parts:
        grouped[(p['value'], p['footprint'], p['specification'])].append(p['ref'])
    bom = ['# A0 draft BOM', '', 'Not a purchase/release list. No supplier availability verified.', '',
           '| Qty | References | Value / part | Package | Requirement |', '|---:|---|---|---|---|']
    for (value, fp, spec), refs in grouped.items():
        bom.append(f'| {len(refs)} | {", ".join(refs)} | {value} | {fp} | {spec} |')
    bom += ['', 'J1/J3/J5: direct CH1/2/3. J2/J4/J6: conditioned CH1/2/3. Pin 1 signal, pin 2 GND.',
            '', 'J7: pin 1 +3.3 V, pin 2 GND. J8: 1 PA1, 2 PC0, 3 PA2, 4 PC1, 5 PA3, 6 PC2, 7/8 GND.',
            '', 'Headers: 2.54 mm pitch, 1 mm finished holes; select physical part before ordering.',
            '', 'Off-board: six signal/ground cables or panel connectors; four M3 screws, insulated spacers, nuts. Length depends on the revised enclosure.',
            '', 'Capacitor effective capacitance, diode pinouts, all land patterns, rail injection, power-off behavior, bandwidth and DRC remain release checks.']
    (OUT / 'BOM.md').write_text('\n'.join(bom) + '\n', encoding='utf-8')
    report = {'status': 'ENGINEERING DRAFT / UNROUTED / NOT FOR FABRICATION',
              'components': len(parts), 'nets': len(nets), 'pads': sum(len(p['pins']) for p in parts),
              'checks_passed': ['unique references', 'symbol-pin / footprint-pad coverage',
                                'pads within outline', 'pad bounding-box clearance >= 0.2mm', 'no single-pin nets'],
              'not_checked': ['EasyEDA Pro import', 'ERC', 'DRC', 'routing', 'body courtyard clearance',
                              'manufacturer land patterns', 'analog simulation', 'bench testing', 'enclosure fit']}
    (OUT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
