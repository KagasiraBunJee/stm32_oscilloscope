"""Decode OSC1 files into validated JSON, CSV, and SVG. Usage: python tools/read_osc.py recordings/rec1"""
import csv
import html
import json
from pathlib import Path
import statistics
import struct
import sys
import zlib

folder = Path(sys.argv[1])
output = folder / 'analysis'
output.mkdir(exist_ok=True)
for path in sorted(folder.glob('*.OSC')):
    data = path.read_bytes()
    if len(data) < 256:
        raise ValueError(f'{path}: truncated header')
    magic, version, header_size, total = struct.unpack_from('<IHHI', data)
    if (magic, version, header_size, total) != (0x3143534F, 1, 256, len(data)):
        raise ValueError(f'{path}: invalid OSC1 header')
    size, payload_crc, header_crc = struct.unpack_from('<III', data, 168)
    header = bytearray(data[:256])
    struct.pack_into('<I', header, 176, 0)
    if zlib.crc32(header) != header_crc or zlib.crc32(data[256:]) != payload_crc:
        raise ValueError(f'{path}: CRC failure')
    rate, count, trigger, level, channels, layout, mode, edge, stop, _ = struct.unpack_from('<IHHHBBBBBB', data, 32)
    if not rate or count < 2 or not 1 <= channels <= 3 or layout != 1 or size != count*channels*2 or size != len(data)-256:
        raise ValueError(f'{path}: invalid payload metadata')
    raw = struct.unpack_from(f'<{count*channels}H', data, 256)
    if any(x > 4095 for x in raw):
        raise ValueError(f'{path}: invalid ADC sample')
    result = dict(file=path.name, crc_valid=True, sample_rate_hz=rate, sample_count=count,
        duration_s=count/rate, flags=struct.unpack_from('<I', data, 24)[0],
        firmware_version=struct.unpack_from('<I', data, 28)[0], trigger_index=trigger,
        trigger_level_raw=level, trigger_mode=mode, trigger_edge=edge, channels=[])
    series = []
    for c in range(channels):
        logical, route, adc, _, gain, offset, *measurements = struct.unpack_from('<BBBBiiiiiIIIII', data, 48+c*40)
        samples = raw[c::channels]
        def convert(x):
            product = x*gain
            return ((abs(product)//65536)*(-1 if product < 0 else 1)+offset)/1e6
        volts = [convert(x) for x in samples]
        series.append((samples, volts))
        result['channels'].append(dict(logical_channel=logical, route=route, adc_channel=adc,
            gain_uv_per_lsb_q16=gain, offset_uv=offset, min_v=min(volts), max_v=max(volts),
            mean_v=statistics.mean(volts), stdev_v=statistics.pstdev(volts),
            min_raw=min(samples), max_raw=max(samples),
            samples_at_adc_rails=sum(x in (0,4095) for x in samples)))
    with (output/(path.stem+'.csv')).open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['time_s']+[label for c in result['channels'] for label in
            (f"CH{c['logical_channel']+1}_raw", f"CH{c['logical_channel']+1}_V")])
        for i in range(count):
            writer.writerow([i/rate]+[x for samples,volts in series for x in (samples[i],volts[i])])
    (output/(path.stem+'.json')).write_text(json.dumps(result,indent=2)+'\n', encoding='utf-8')
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="{340*channels}">',
        '<rect width="100%" height="100%" fill="white"/>', '<g font-family="sans-serif" font-size="14" fill="#182330">']
    for c,(_,volts) in enumerate(series):
        top = 52+c*340
        pad = max((max(volts)-min(volts))*.1,.02)
        lo,hi = min(volts)-pad,max(volts)+pad
        svg.append(f'<text x="75" y="{top-25}">{html.escape(path.name)} / CH{result["channels"][c]["logical_channel"]+1} / {rate} samples/s / saved window only</text>')
        for k in range(6):
            y = top+230*k/5
            svg.append(f'<path d="M75 {y} H1060" stroke="#e0e5ea"/><text x="8" y="{y+5}">{hi-(hi-lo)*k/5:.3f} V</text>')
        for k in range(5):
            x = 75+985*k/4
            svg.append(f'<text x="{x-18}" y="{top+255}">{(count-1)/rate*1000*k/4:.1f}</text>')
        points = ' '.join(f'{75+985*i/(count-1):.2f},{top+230*(hi-v)/(hi-lo):.2f}' for i,v in enumerate(volts))
        svg.append(f'<polyline points="{points}" fill="none" stroke="#176ab5" stroke-width="1"/>')
        svg.append(f'<text x="440" y="{top+280}">Time from first saved sample (ms)</text>')
    svg.append('</g></svg>')
    (output/(path.stem+'.svg')).write_text('\n'.join(svg), encoding='utf-8')
    print(json.dumps(result))
