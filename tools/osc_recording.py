"""Streaming OSC v1/v2 validator/exporter (standard library only).

python tools/osc_recording.py R00000001_*.OSC --csv session.csv
An interrupted valid prefix is exported with a final INTERRUPTED marker.
No interpolation across gaps, no synthetic samples, no whole-file buffering for v2.
"""
import argparse
import csv
import json
import struct
import zlib
from pathlib import Path

SIZE = 3584
TYPES = {1: 'DATA', 2: 'GAP', 3: 'ERROR', 4: 'PAUSE', 5: 'RESUME',
         6: 'SEGMENT_END', 7: 'SESSION_END'}


def u16(b, n): return struct.unpack_from('<H', b, n)[0]
def u32(b, n): return struct.unpack_from('<I', b, n)[0]
def u64(b, n): return struct.unpack_from('<Q', b, n)[0]
def i32(b, n): return struct.unpack_from('<i', b, n)[0]
def crc(b): return zlib.crc32(b) & 0xffffffff


def header_v2(b):
    if (len(b) != 512 or b[:4] != b'OSC2' or u16(b, 4) != 2 or
            u16(b, 6) != 512 or u32(b, 28) != 10000 or u32(b, 32) != 3 or
            u32(b, 36) != SIZE or u32(b, 508) != crc(b[:508])):
        raise ValueError('invalid v2 header/CRC')
    channels = []
    for c in range(3):
        p = 64+c*32
        if b[p:p+4] != bytes([ord('C'), c, 10+c, 1]):
            raise ValueError('invalid physical channel mapping')
        channels.append(dict(pin=f'PC{c}', adc=10+c, gain=i32(b,p+4), offset=i32(b,p+8)))
    return dict(session=u32(b,8), segment=u32(b,12), rate=u32(b,28),
                utc=u64(b,40), rtc_valid=bool(u32(b,20)&1), start_us=u64(b,48),
                sequence=u64(b,56), first=u64(b,160), channels=channels)


def record_v2(b):
    if (len(b) != SIZE or b[:4] != b'BLK2' or u32(b,8) != SIZE or
            u32(b,60) != crc(b[:60])):
        raise ValueError('invalid record header/CRC')
    kind, flags, count, length = u16(b,4), u16(b,6), u32(b,12), u32(b,36)
    if kind not in TYPES or length > 3072 or (kind == 1 and
            (not 0 < count <= 512 or length != count*6)) or (kind != 1 and count):
        raise ValueError('invalid record structure')
    payload=b[64:64+length]
    if (u32(b,56) != crc(payload) or b[-16:-12] != b'END2' or
            u64(b,SIZE-12) != u64(b,16) or u32(b,SIZE-4) != crc(b[:-4])):
        raise ValueError('invalid payload/trailer CRC')
    if kind == 1 and any(x[0]>4095 for x in struct.iter_unpack('<H',payload)):
        raise ValueError('non-12-bit ADC data')
    if kind == 2 and ((flags&1 and length != 0) or (not flags&1 and length != 16)):
        raise ValueError('invalid gap')
    if kind in (4,7) and length != 112: raise ValueError('invalid summary size')
    return dict(type=TYPES[kind], flags=flags, count=count, sequence=u64(b,16),
                first=u64(b,24), epoch=u32(b,32), time_us=u64(b,40),
                session=u32(b,48), segment=u32(b,52), payload=payload)


def volts(raw, channel):
    product=raw*channel['gain']
    uv=(abs(product)//65536)*(-1 if product<0 else 1)+channel['offset']
    return uv/1e6


def validate(paths, writer=None):
    """Recover one session in segment order. Stop at first bad record/segment."""
    report=dict(completed=False, interrupted=True, recorded_sets=0, known_dropped=0,
                unknown_gaps=0, records=0, segments=0, valid_bytes=0)
    expected_seq=0; expected_index=0; identity=None; metadata=None; last_epoch=None
    paused=False
    for number,path in enumerate(sorted(map(Path, paths))):
        if report['completed']: raise ValueError('extra segment after SESSION_END')
        with path.open('rb') as f:
            h=header_v2(f.read(512))
            if h['segment'] != number or h['sequence'] != expected_seq or h['first'] != expected_index:
                report['error']='missing/out-of-order segment';break
            if identity is None: identity=h['session']; metadata=h
            if h['session'] != identity or h['channels'] != metadata['channels'] or h['rate'] != metadata['rate']:
                report['error']='session metadata mismatch';break
            report['segments']+=1;report['valid_bytes']+=512
            segment_end=False
            while True:
                b=f.read(SIZE)
                try:
                    r=record_v2(b)
                    if (r['sequence'] != expected_seq or r['session'] != identity or
                            r['segment'] != number): raise ValueError('record sequence/identity mismatch')
                    if r['first'] != expected_index: raise ValueError('unmarked sample interval')
                    if r['type']=='DATA':
                        if last_epoch is not None and r['epoch']!=last_epoch:
                            raise ValueError('unmarked acquisition epoch change')
                    if r['type']=='GAP' and not r['flags']&1 and u64(r['payload'],0)!=expected_index:
                        raise ValueError('invalid missing interval')
                    if r['type']=='SESSION_END':
                        p=r['payload']
                        if (u64(p,0)!=report['recorded_sets'] or u64(p,8)!=report['known_dropped'] or
                                u64(p,16)!=report['unknown_gaps'] or u64(p,56)!=expected_seq+1 or
                                u32(p,64)!=number+1): raise ValueError('summary totals mismatch')
                except ValueError as e:
                    report['error']=str(e);break
                expected_seq+=1;report['records']+=1;report['valid_bytes']+=SIZE
                kind=r['type']
                if kind=='DATA':
                    if paused: report['error']='data while paused';break
                    last_epoch=r['epoch']
                    report['recorded_sets']+=r['count']
                    if writer:
                        for j,values in enumerate(struct.iter_unpack('<HHH',r['payload'])):
                            writer.writerow(['DATA',r['sequence'],r['epoch'],r['first']+j,
                                (r['time_us']-metadata['start_us'])/1e6+j/h['rate'],
                                *values,*[volts(v,c) for v,c in zip(values,h['channels'])],''])
                    expected_index+=r['count']
                else:
                    detail={}
                    if kind=='GAP':
                        if r['flags']&1: report['unknown_gaps']+=1;last_epoch=None;detail['length']='unknown'
                        else:
                            lost=u64(r['payload'],8);expected_index+=lost
                            report['known_dropped']+=lost;detail['missing_sets']=lost
                    if kind=='PAUSE': paused=True
                    if kind=='RESUME': paused=False;last_epoch=None;detail['pause_us']=u64(r['payload'],0)
                    if kind=='SESSION_END':
                        report.update(completed=True,interrupted=False,stop_reason=u32(r['payload'],68),
                                      elapsed_us=u64(r['payload'],40),pauses=u64(r['payload'],24),
                                      errors=u64(r['payload'],32))
                    if writer: writer.writerow([kind,r['sequence'],r['epoch'],r['first'],
                        (r['time_us']-metadata['start_us'])/1e6,'','','','','','',json.dumps(detail)])
                if kind in ('SESSION_END','SEGMENT_END'): segment_end=True;break
            if not segment_end: break
    if not report['completed'] and writer:
        writer.writerow(['INTERRUPTED',expected_seq,'',expected_index,'','','','','','','',report.get('error','missing final segment')])
    report['session']=identity
    return report


def validate_v1(path, writer=None):
    data=Path(path).read_bytes()
    if len(data)<256 or data[:4]!=b'OSC1' or u16(data,4)!=1 or u16(data,6)!=256:
        raise ValueError('invalid v1 header')
    h=bytearray(data[:256]);saved=u32(h,176);struct.pack_into('<I',h,176,0)
    rate,count,channels=u32(h,32),u16(h,36),h[42]
    if (not rate or not 1<=channels<=3 or h[43]!=1 or u32(h,8)!=len(data) or
            u32(h,168)!=count*channels*2 or len(data)!=256+count*channels*2 or
            saved!=crc(h) or u32(h,172)!=crc(data[256:])): raise ValueError('invalid v1 CRC/layout')
    values=struct.unpack('<'+'H'*(count*channels),data[256:])
    if any(v>4095 for v in values): raise ValueError('invalid ADC data')
    if writer:
        for i in range(count):
            raw=list(values[i*channels:(i+1)*channels]);v=[]
            for c,x in enumerate(raw):v.append(volts(x,dict(gain=i32(h,52+c*40),offset=i32(h,56+c*40))))
            writer.writerow(['DATA',0,0,i,i/rate,*raw,*(['']*(3-channels)),*v,*(['']*(3-channels)),''])
    return dict(version=1,completed=True,recorded_sets=count,channels=channels)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('segments',nargs='+',type=Path);p.add_argument('--csv',type=Path)
    args=p.parse_args()
    out=args.csv.open('w',newline='',encoding='utf-8') if args.csv else None
    try:
        writer=csv.writer(out) if out else None
        if writer:writer.writerow(['type','record','epoch','sample_index','time_s','PC0_raw','PC1_raw','PC2_raw','PC0_V','PC1_V','PC2_V','detail'])
        with args.segments[0].open('rb') as f: magic=f.read(4)
        if magic==b'OSC1':
            if len(args.segments)!=1:p.error('v1 captures are independent; supply one file')
            report=validate_v1(args.segments[0],writer)
        else: report=validate(args.segments,writer)
        print(json.dumps(report,indent=2))
    finally:
        if out:out.close()


if __name__=='__main__':main()
