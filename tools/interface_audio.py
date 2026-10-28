"""Exports CH3 (interface U1 pin 1) of an OSC v2 recording as listening WAVs.
Usage: python -B tools/interface_audio.py file.OSC out_dir gain_per_volt [clip_start clip_end ...]
Filtered: first-order HP 150 Hz + LP 3 kHz (same as tools/intercom_audio.py). Large excursions
(>0.8 V from baseline, i.e. control steps) and known gaps are muted with 2 ms fades."""
import json
import math
import struct
import sys
import wave
from pathlib import Path

sys.path.insert(0, "tools")
import osc_recording as osc  # noqa: E402

R = 10000
src, out, gain = Path(sys.argv[1]), Path(sys.argv[2]), float(sys.argv[3])
clips = [(float(a), float(b)) for a, b in zip(sys.argv[4::2], sys.argv[5::2])]
p, gaps = [], []
with src.open("rb") as f:
    h = osc.header_v2(f.read(512))
    while (b := f.read(osc.SIZE)) and len(b) == osc.SIZE:
        r = osc.record_v2(b)
        if r["type"] == "DATA":
            n = r["first"] - len(p)
            if n > 0:
                gaps.append([len(p) / R, (len(p) + n) / R]); p.extend([math.nan] * n)
            p.extend(osc.volts(t[2], h["channels"][2]) for t in struct.iter_unpack("<HHH", r["payload"]))
fin = sorted(v for v in p if math.isfinite(v))
base = fin[len(fin) // 2]

mute = bytearray(len(p))
for i, v in enumerate(p):
    if not math.isfinite(v):
        mute[max(0, i - 50):i + 51] = b"\1" * (min(len(p), i + 51) - max(0, i - 50))
    elif abs(v - base) > 1.5:                         # control step: mute 30 ms before, 120 ms after
        lo, hi = max(0, i - 300), min(len(p), i + 1200)
        mute[lo:hi] = b"\1" * (hi - lo)

hp_a = 1 / (1 + 2 * math.pi * 150 / R)
lp_a = 1 - math.exp(-2 * math.pi * 3000 / R)
y, prev, hp, lp = [], None, 0.0, 0.0
for v in p:
    if not math.isfinite(v):
        y.append(0.0); prev = None; continue
    if prev is None:
        prev, hp, lp = v, 0.0, 0.0
    hp = hp_a * (hp + v - prev); prev = v
    lp += lp_a * (hp - lp)
    y.append(lp)
# 2 ms fades at mute boundaries
env = [0.0 if m else 1.0 for m in mute]
fade = 20
for i in range(1, len(env)):
    if env[i] and not env[i - 1]:
        for k in range(fade):
            if i + k < len(env) and env[i + k]: env[i + k] = min(env[i + k], k / fade)
    if not env[i] and env[i - 1]:
        for k in range(1, fade + 1):
            if i - k >= 0 and env[i - k]: env[i - k] = min(env[i - k], k / fade)


def write(name, a, b):
    lo, hi = int(a * R), int(b * R)
    clipped = 0; data = bytearray()
    for i in range(lo, hi):
        s = y[i] * gain * env[i]
        clipped += abs(s) > 1
        data += struct.pack("<h", round(max(-1, min(1, s)) * 32767))
    with wave.open(str(out / name), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R); w.writeframes(bytes(data))
    return dict(file=name, start_s=a, end_s=b, clipped_samples=clipped)


out.mkdir(parents=True, exist_ok=True)
files = [write(src.stem + "_pin1_full.wav", 0, len(p) / R)]
for a, b in clips:
    files.append(write(f"{src.stem}_pin1_{a:g}-{b:g}s.wav", a, b))
meta = dict(source=str(src), channel="CH3 = receive interface U1 pin 1", baseline_V=base, gain_per_volt=gain,
            filter="first-order HP 150 Hz + LP 3 kHz", muted="known gaps (+-5 ms) and control steps >1.5 V (30 ms before, 120 ms after)",
            gaps_s=gaps, limitation="scope resolution 4.59 mV/step, 10 kS/s; hiss is mostly the scope's own noise floor", files=files)
(out / (src.stem + "_pin1_audio.json")).write_text(json.dumps(meta, indent=2))
print(json.dumps(files, indent=2))
