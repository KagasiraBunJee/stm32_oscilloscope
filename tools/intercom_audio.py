"""Inspect white-wire AC variations without modifying the original recording.
Usage: python -B tools/intercom_audio.py recordings/rec3/R00000008_00000000.OSC
Exports relative-level listening previews; these do not establish speech presence.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics
import struct
import wave
from array import array
import osc_recording as osc

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('file',type=Path)
parser.add_argument('--gain',type=float,default=1.0,help='Fixed WAV amplitude per volt; lower avoids clipping')
parser.add_argument('--gap-margin-ms',type=float,default=5.0,help='Small silence guard around missing samples; control pulses retain 250 ms margins')
args=parser.parse_args()
report=osc.validate([args.file])
if not report['completed']:
    raise ValueError('Require a validated completed recording for this analysis')
if args.gap_margin_ms<0:parser.error('gap margin must be nonnegative')
rate=10000
values=array('d'); gaps=[]
with args.file.open('rb') as source:
    header=osc.header_v2(source.read(512))
    while block:=source.read(osc.SIZE):
        record=osc.record_v2(block)
        if record['type']=='DATA':
            if len(values)!=record['first']:
                raise ValueError('Unexpected sample index')
            values.extend(osc.volts(row[0],header['channels'][0]) for row in struct.iter_unpack('<HHH',record['payload']))
        elif record['type']=='GAP':
            if record['flags']&1:raise ValueError('Unknown gap requires separate epochs')
            n=osc.u64(record['payload'],8)
            gaps.append([record['first']/rate,(record['first']+n)/rate])
            values.extend([math.nan]*n)

# First-order high-pass at 150 Hz then first-order low-pass at 3 kHz.
# This is a listening aid, not a calibrated steep band-pass or anti-alias filter.
hp=1/(1+2*math.pi*150/rate)
lp=1-math.exp(-2*math.pi*3000/rate)
filtered=array('d');previous=None;high=low=0.;settle=0
for x in values:
    if not math.isfinite(x):
        filtered.append(math.nan);previous=None;continue
    if previous is None:
        previous=x;high=low=0.;settle=50
    high=hp*(high+x-previous);previous=x
    low+=lp*(high-low)
    filtered.append(math.nan if settle else low)
    settle=max(0,settle-1)

out=args.file.parent/'analysis';out.mkdir(exist_ok=True)
rows=[]
for start in range(0,len(values),1000):
    chunk=values[start:start+1000];ac=filtered[start:start+1000]
    valid=len(chunk)==1000 and all(math.isfinite(x) for x in chunk) and all(math.isfinite(x) for x in ac)
    if valid:
        mean=statistics.mean(chunk)
        rows.append(dict(time_s=start/rate,mean_V=mean,ac_rms_mV=1000*math.sqrt(statistics.mean(x*x for x in ac)),
                         raw_stdev_mV=1000*statistics.pstdev(chunk),raw_pp_mV=1000*(max(chunk)-min(chunk))))
with (out/(args.file.stem+'_audio_levels.csv')).open('w',newline='') as target:
    writer=csv.DictWriter(target,fieldnames=['time_s','mean_V','ac_rms_mV','raw_stdev_mV','raw_pp_mV'])
    writer.writeheader();writer.writerows(rows)

# Large control excursions get a 250 ms guard. Gaps get only a short
# guard so missing blocks do not unnecessarily erase nearby recorded speech.
masked=bytearray(len(values));intervals=[]
for i,x in enumerate(values):
    if math.isfinite(x) and not 1.0<=x<=8.0:
        lo=max(0,i-2500);hi=min(len(values),i+2501)
        if intervals and lo<=intervals[-1][1]:intervals[-1][1]=max(intervals[-1][1],hi)
        else:intervals.append([lo,hi])
guard=max(50,round(args.gap_margin_ms*rate/1000))
intervals.extend([max(0,round(lo*rate)-guard),min(len(values),round(hi*rate)+guard)] for lo,hi in gaps)
merged=[]
for lo,hi in sorted(intervals):
    if merged and lo<=merged[-1][1]:merged[-1][1]=max(merged[-1][1],hi)
    else:merged.append([lo,hi])
intervals=merged
for lo,hi in intervals:masked[lo:hi]=b'\1'*(hi-lo)
# A 2 ms fade on available samples bordering a mute avoids hard cut clicks.
fade_weights={};fade=20
for lo,hi in intervals:
    for n in range(1,fade+1):
        for index in (lo-n,hi+n-1):
            if 0<=index<len(values):fade_weights[index]=min(fade_weights.get(index,1.),n/fade)
listening_gain=args.gain # Fixed across file; no automatic gain adjustment.
with wave.open(str(out/(args.file.stem+'_white_AC_preview.wav')),'wb') as target:
    target.setnchannels(1);target.setsampwidth(2);target.setframerate(rate)
    payload=bytearray();clipped=0
    for i,x in enumerate(filtered):
        unlimited=0 if masked[i] or not math.isfinite(x) else x*listening_gain*fade_weights.get(i,1.)
        clipped+=abs(unlimited)>1
        amplitude=max(-1,min(1,unlimited))
        payload.extend(struct.pack('<h',round(amplitude*32767)))
    target.writeframes(payload)
summary=dict(validation=report,gaps_s=gaps,lsb_mV=header['channels'][0]['gain']/65536/1000,
    filtering='first-order HP 150Hz + first-order LP 3000Hz; no removal of pre-existing aliasing',
    gap_margin_ms=guard*1000/rate,filter_settle_ms=5,edge_fade_ms=2,preview_gain=listening_gain,preview_clipped_samples=clipped,preview_silence_intervals_s=[[lo/rate,hi/rate] for lo,hi in intervals],
    limitations='Speech not identified. ADC noise, interference, and audio all contribute. Windows are not confirmed silent/speech labels.',windows=[])
for lo,hi in [(0.5,3),(5,10),(14,19),(20,25),(25,30),(30,35),(46,50),(55,60),(67,72)]:
    selected=[row for row in rows if lo<=row['time_s']<hi and 1.0<row['mean_V']<8.0]
    if selected:
        summary['windows'].append(dict(start_s=lo,end_s=hi,median_ac_rms_mV=statistics.median(row['ac_rms_mV'] for row in selected),
            max_ac_rms_mV=max(row['ac_rms_mV'] for row in selected)))
(out/(args.file.stem+'_audio_summary.json')).write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
