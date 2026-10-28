"""Fixed noise-profile spectral reduction for listening comparisons.
Original WAV is preserved. No speech recognition, sample reconstruction, AGC,
or claim of improved intelligibility. Standard-library radix-2 FFT, Hann OLA.
"""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct
import wave

N=512
HOP=128
BITS=N.bit_length()-1
REVERSE=[int(f'{i:0{BITS}b}'[::-1],2) for i in range(N)]
TWIDDLES={size:[cmath.exp(-2j*math.pi*k/size) for k in range(size//2)] for size in (2,4,8,16,32,64,128,256,512)}
WINDOW=[math.sin(math.pi*(i+.5)/N) for i in range(N)]
def fft(values,inverse=False):
    a=[complex(values[i]) for i in REVERSE]
    size=2
    while size<=N:
        half=size//2;roots=TWIDDLES[size]
        for base in range(0,N,size):
            for k,w in enumerate(roots):
                if inverse:w=w.conjugate()
                u=a[base+k];v=w*a[base+k+half]
                a[base+k]=u+v;a[base+k+half]=u-v
        size*=2
    return [v/N for v in a] if inverse else a

def rms(values):return math.sqrt(sum(x*x for x in values)/len(values))
def write_wav(path,values,rate):
    clipped=sum(abs(v)>1 for v in values)
    payload=struct.pack('<'+'h'*len(values),*(round(max(-1,min(1,v))*32767) for v in values))
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(payload)
    return clipped

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('wav',type=Path)
    p.add_argument('--noise-start',type=float,default=20)
    p.add_argument('--noise-end',type=float,default=40)
    args=p.parse_args()
    test=[math.sin(i*.123)+.3*math.cos(i*.57) for i in range(N)]
    error=max(abs(a-b.real) for a,b in zip(test,fft(fft(test),True)))
    assert error<1e-10,error
    with wave.open(str(args.wav),'rb') as f:
        assert f.getnchannels()==1 and f.getsampwidth()==2
        rate=f.getframerate();frames=f.getnframes();raw=f.readframes(frames)
    values=[v[0]/32768 for v in struct.iter_unpack('<h',raw)]
    noise_frames=[[] for _ in range(N//2+1)]
    for start in range(round(args.noise_start*rate),round(args.noise_end*rate)-N+1,HOP):
        spectrum=fft([values[start+i]*WINDOW[i] for i in range(N)])
        for k in range(N//2+1):noise_frames[k].append(abs(spectrum[k])**2)
    assert noise_frames[0],'Empty noise profile'
    noise=[]
    for powers in noise_frames:
        powers.sort();trim=len(powers)//10
        noise.append(statistics.mean(powers[trim:len(powers)-trim or None]))
    configs=[('mild',1.0,.25),('stronger',1.5,.12)]
    outputs=[[0.]*(frames+2*N) for _ in configs]
    previous=[[1.]*(N//2+1) for _ in configs]
    weights=[0.]*(frames+2*N)
    padded=[0.]*N+values+[0.]*N
    for start in range(0,len(padded)-N+1,HOP):
        spectrum=fft([padded[start+i]*WINDOW[i] for i in range(N)])
        power=[abs(x)**2 for x in spectrum[:N//2+1]]
        for i,w in enumerate(WINDOW):weights[start+i]+=w*w
        for ci,(name,amount,floor) in enumerate(configs):
            gains=[]
            for k,P in enumerate(power):
                target=max(floor,math.sqrt(max(0.,1-amount*noise[k]/(P+1e-25))))
                freq=k*rate/N
                band=min(1.,max(0.,(freq-80)/80),max(0.,(3800-freq)/600))
                target*=band
                # Fast recovery for onsets; slower attenuation to avoid pumping.
                mix=.7 if target>previous[ci][k] else .2
                value=previous[ci][k]+mix*(target-previous[ci][k])
                previous[ci][k]=value;gains.append(value)
            # Small spectral smoothing reduces isolated musical-noise bins.
            smooth=[sum(gains[max(0,k-1):min(len(gains),k+2)])/len(gains[max(0,k-1):min(len(gains),k+2)]) for k in range(len(gains))]
            filtered=[v*smooth[k if k<=N//2 else N-k] for k,v in enumerate(spectrum)]
            time=fft(filtered,True)
            for i,v in enumerate(time):outputs[ci][start+i]+=v.real*WINDOW[i]
    report=dict(input=str(args.wav),input_sha256=hashlib.sha256(args.wav.read_bytes()).hexdigest(),rate=rate,frames=frames,
        noise_profile_s=[args.noise_start,args.noise_end],fft_size=N,hop=HOP,fft_roundtrip_max_error=error,
        method='Fixed-profile spectral power subtraction with magnitude gain floor, temporal and frequency smoothing, and soft band limit. No normalization or gain boost.',
        limitations='Noise profile is based on operator-reported quiet interval. Stationary noise reduction can suppress quiet speech or create artifacts. No words are reconstructed; changing baseline interference may remain.',versions=[])
    for ci,(name,amount,floor) in enumerate(configs):
        result=[outputs[ci][N+i]/weights[N+i] for i in range(frames)]
        dest=args.wav.with_name('R00000006_white_denoise_'+name+'.wav')
        clipped=write_wav(dest,result,rate)
        for lo,hi in [(42,80),(50,70),(60,67),(67,77)]:
            write_wav(args.wav.with_name(f'R00000006_denoise_{name}_{lo}-{hi}s.wav'),result[lo*rate:hi*rate],rate)
        intervals=[]
        for lo,hi in [(20,40),(42,49),(50,60),(60,67),(67,77)]:
            before=rms(values[lo*rate:hi*rate]);after=rms(result[lo*rate:hi*rate])
            intervals.append(dict(start_s=lo,end_s=hi,before_rms=before,after_rms=after,level_change_db=20*math.log10(after/before)))
        report['versions'].append(dict(name=name,file=str(dest),clipped_samples=clipped,noise_amount=amount,magnitude_floor=floor,intervals=intervals))
    args.wav.with_name('R00000006_denoise_report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
