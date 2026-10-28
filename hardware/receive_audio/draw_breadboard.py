"""Breadboard layout for docs/RECEIVE_AUDIO_INTERFACE.md (draft 1).
Writes receive_audio_breadboard.svg and breadboard_scene.json (for render_breadboard.ps1).
Usage: python -B hardware/receive_audio/draw_breadboard.py
"""
import json
from pathlib import Path

OUT = Path(__file__).parent
P = 34                      # hole pitch, px
X0 = 340                    # x of column 1
COLS = 30
W, H = X0 + COLS * P + 300, 1040
ROW_Y = {"VCC": 120, "GNDT": 154, "a": 220, "b": 254, "c": 288, "d": 322, "e": 356,
         "f": 432, "g": 466, "h": 500, "i": 534, "j": 568, "GNDB": 634, "NC": 668}
scene = []


def x(col): return X0 + (col - 1) * P
def hole(row, col): return x(col), ROW_Y[row]
def line(a, b, color, width=3): scene.append(dict(kind="line", x1=a[0], y1=a[1], x2=b[0], y2=b[1], color=color, width=width))
def rect(x0, y0, w, h, fill, color="#333333", width=1.5, r=0): scene.append(dict(kind="rect", x=x0, y=y0, w=w, h=h, fill=fill, color=color, width=width, r=r))
def circle(c, r, fill, color="#333333", width=1.5): scene.append(dict(kind="circle", x=c[0], y=c[1], r=r, fill=fill, color=color, width=width))
def text(s, p, size=15, color="#111111", anchor="middle", bold=False): scene.append(dict(kind="text", text=s, x=p[0], y=p[1], size=size, color=color, anchor=anchor, bold=bold))


def body(a, b, length, thick, fill, label, label_off=(0, -16), stripe=None):
    """Axial part from a to b, body centred; optional stripe drawn at the b end (cathode)."""
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    line(a, b, "#7a7a7a", 2.5)
    horiz = abs(b[0] - a[0]) >= abs(b[1] - a[1])
    w, h = (length, thick) if horiz else (thick, length)
    rect(mx - w / 2, my - h / 2, w, h, fill, r=5)
    if stripe:
        if horiz:
            rect(mx + w / 2 - 7 if b[0] > a[0] else mx - w / 2 + 2, my - h / 2, 5, h, stripe, color=stripe, width=0)
        else:
            rect(mx - w / 2, my + h / 2 - 7 if b[1] > a[1] else my - h / 2 + 2, w, 5, stripe, color=stripe, width=0)
    text(label, (mx + label_off[0], my + label_off[1]), 14, bold=True)


def resistor(a, b, label, **k): body(a, b, 46, 14, "#d8b98a", label, **k)
def diode(anode, cathode, label, **k): body(anode, cathode, 34, 12, "#e9e9e9", label, stripe="#111111", **k)


def ceramic(a, b, label, dx=0, dy=-18):
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    line(a, b, "#7a7a7a", 2.5); circle((mx, my), 11, "#f2c94c"); text(label, (mx + dx, my + dy), 14, bold=True)


def electrolytic(plus, minus, label, dx=0, dy=-24):
    mx, my = (plus[0] + minus[0]) / 2, (plus[1] + minus[1]) / 2
    line(plus, minus, "#7a7a7a", 2.5); circle((mx, my), 15, "#2f5aa8")
    px, py = mx + (plus[0] - mx) * 0.45, my + (plus[1] - my) * 0.45   # '+' marker on the plus side
    text("+", (px, py + 5), 15, "#ffffff", bold=True)
    text(label, (mx + dx, my + dy), 14, bold=True)


def jumper(a, b, color, via=None):
    pts = [a] + (via or []) + [b]
    for p, q in zip(pts, pts[1:]): line(p, q, color, 5)
    for p in (a, b): circle(p, 5, color, color)


# Title and board ----------------------------------------------------------------------------
text("White-line receive audio interface — breadboard layout (draft 1, NOT yet tested)", (W / 2, 38), 22, bold=True)
text("Matches docs/RECEIVE_AUDIO_INTERFACE.md. Top view, MCP6022 notch/pin 1 to the LEFT. Every hole in one numbered column a–e is connected; f–j likewise.",
     (W / 2, 66), 14, "#444444")
rect(X0 - 50, 92, COLS * P + 66, 600, "#f4f1ea", "#b8b2a6", 2, r=10)
rect(X0 - 40, (ROW_Y["e"] + ROW_Y["f"]) / 2 - 10, COLS * P + 46, 20, "#e2ddd2", "#e2ddd2", 0)  # centre channel
for row, col, off in (("VCC", "#d33", -17), ("GNDT", "#2a5bd7", 17), ("GNDB", "#2a5bd7", -17), ("NC", "#bbbbbb", 17)):
    line((X0 - 30, ROW_Y[row] + off), (x(COLS) + 16, ROW_Y[row] + off), col, 2)
for row, y in ROW_Y.items():
    for c in range(1, COLS + 1):
        circle((x(c), y), 4, "#4a4a4a", "#4a4a4a", 0)
    lab = {"VCC": "VCC rail (filtered ≈4.9 V)", "GNDT": "GND rail", "GNDB": "GND rail", "NC": "(unused rail)"}.get(row, row)
    text(lab, (X0 - 46, y - 8 if row.startswith("GND") else y + 5), 13 if len(lab) > 1 else 15, "#555555", anchor="end")
for c in (1, 5, 10, 15, 20, 25, 30):
    text(str(c), (x(c), ROW_Y["a"] - 26), 13, "#777777")
    text(str(c), (x(c), ROW_Y["j"] + 30), 13, "#777777")

# MCP6022 on SOIC-to-DIP adapter (cols 10-13) -----------------------------------------------
rect(x(10) - 18, ROW_Y["e"] - 14, 3 * P + 36, ROW_Y["f"] - ROW_Y["e"] + 28, "#1d1d1d", "#000000", 2, r=4)
circle((x(10) - 6, (ROW_Y["e"] + ROW_Y["f"]) / 2), 6, "#555555", "#555555", 0)
text("U1 MCP6022", (x(11.5), (ROW_Y["e"] + ROW_Y["f"]) / 2 + 5), 14, "#ffffff", bold=True)
for i, c in enumerate(range(10, 14)):
    text(str(8 - i), (x(c), ROW_Y["e"] + 2), 11, "#ffd54a", bold=True)
    text(str(1 + i), (x(c), ROW_Y["f"] + 10), 11, "#ffd54a", bold=True)

# Supply ---------------------------------------------------------------------------------------
resistor(hole("a", 3), (x(3), ROW_Y["VCC"]), "R8 100Ω", label_off=(44, 0))
electrolytic((x(6), ROW_Y["VCC"]), (x(6), ROW_Y["GNDT"]), "C4 100µF", dx=48, dy=5)
jumper(hole("a", 10), (x(10), ROW_Y["VCC"]), "#d33")
ceramic(hole("b", 10), (x(9), ROW_Y["GNDT"]), "C5 100nF", dx=-48, dy=6)

# VMID divider (col 16) and unused half U1B ------------------------------------------------
resistor(hole("a", 16), (x(16), ROW_Y["VCC"]), "R6 100k", label_off=(-42, 0))
resistor(hole("b", 16), (x(17), ROW_Y["GNDT"]), "R7 100k", label_off=(46, 2))
electrolytic(hole("d", 16), (x(15), ROW_Y["GNDT"]), "C3 10µF", dx=-46, dy=0)
text("VMID", (x(16) + 2, ROW_Y["e"] + 26), 13, "#b35c00", bold=True)
jumper(hole("d", 11), hole("d", 12), "#2e9e44")                                  # pin 7 -> pin 6
jumper(hole("c", 13), hole("c", 16), "#e08a00")                                  # pin 5 -> VMID

# Input network, node X at col 21 -------------------------------------------------------------
diode(hole("a", 21), (x(21), ROW_Y["VCC"]), "D1", label_off=(-20, 0))          # stripe at VCC
diode((x(22), ROW_Y["GNDT"]), hole("b", 21), "D2", label_off=(26, -4))         # stripe at node X
resistor(hole("c", 21), hole("c", 16), "R2 1M", label_off=(0, -14))
ceramic(hole("d", 21), hole("d", 24), "C1 10nF ≥50V", dy=24)
resistor(hole("e", 21), hole("f", 21), "R3 10k", label_off=(40, 0))
text("node X", (x(21) + 12, ROW_Y["c"] + 5), 13, "#b00020", "start", True)
resistor(hole("b", 24), hole("b", 28), "R1 100k", label_off=(0, -14))
jumper(hole("g", 21), hole("g", 12), "#7b3fb5")                                 # R3 -> pin 3 (+INA)

# Output and follower --------------------------------------------------------------------------
jumper(hole("g", 10), hole("g", 11), "#2e9e44")                                  # pin 1 -> pin 2
jumper(hole("h", 13), (x(13), ROW_Y["GNDB"]), "#2a5bd7")                         # pin 4 -> GND
electrolytic(hole("i", 10), hole("i", 8), "C2 10µF", dx=0, dy=30)
resistor(hole("h", 8), hole("h", 5), "R4 100Ω", label_off=(0, -14))
resistor(hole("i", 5), (x(5), ROW_Y["GNDB"]), "R5 100k", label_off=(-40, 0))

# Ground bridge between the two GND rails -----------------------------------------------------
jumper((x(1), ROW_Y["GNDT"]), (x(1), ROW_Y["GNDB"]), "#2a5bd7", via=[(x(1) - 26, ROW_Y["GNDT"]), (x(1) - 26, ROW_Y["GNDB"])])

# Off-board connections ------------------------------------------------------------------------
# +5 V and its GND (top-left)
line(hole("d", 3), (x(3) - 120, ROW_Y["d"]), "#d33", 5); circle(hole("d", 3), 6, "#d33", "#d33")
line((x(2), ROW_Y["GNDT"]), (X0 - 124, ROW_Y["GNDT"]), "#222222", 5); circle((x(2), ROW_Y["GNDT"]), 6, "#222", "#222")
text("+5 V in", (x(3) - 126, ROW_Y["d"] + 5), 15, "#d33", "end", True)
text("(STM32 board 5 V on the bench;", (x(3) - 126, ROW_Y["d"] + 25), 12, "#444", "end")
text("Pi 5 V later)", (x(3) - 126, ROW_Y["d"] + 41), 12, "#444", "end")
text("5 V source GND", (X0 - 130, ROW_Y["GNDT"] + 5), 15, "#222", "end", True)

# Intercom (right side)
line(hole("d", 28), (x(COLS) + 70, ROW_Y["d"]), "#cccccc", 6); line(hole("d", 28), (x(COLS) + 70, ROW_Y["d"]), "#ffffff", 3)
circle(hole("d", 28), 6, "#999999", "#666666")
line((x(28), ROW_Y["GNDB"]), (x(COLS) + 70, ROW_Y["GNDB"]), "#111111", 6); circle((x(28), ROW_Y["GNDB"]), 6, "#111", "#111")
text("WHITE (ML-20HD)", (x(COLS) + 78, ROW_Y["d"] + 5), 15, "#111", "start", True)
text("audio/control, 0–13.5 V", (x(COLS) + 78, ROW_Y["d"] + 23), 12, "#444", "start")
text("BLACK (ML-20HD GND)", (x(COLS) + 78, ROW_Y["GNDB"] + 5), 15, "#111", "start", True)
text("NEVER connect RED", (x(COLS) + 78, ROW_Y["GNDB"] - 60), 15, "#c00000", "start", True)
text("(13.6 V panel supply)", (x(COLS) + 78, ROW_Y["GNDB"] - 42), 12, "#c00000", "start")

# EasyCAP (bottom-left)
line(hole("g", 5), (X0 - 124, ROW_Y["g"]), "#f5f5f5", 7); line(hole("g", 5), (X0 - 124, ROW_Y["g"]), "#bdbdbd", 2)
circle(hole("g", 5), 6, "#999999", "#666666")
line((x(3), ROW_Y["GNDB"]), (X0 - 124, ROW_Y["GNDB"]), "#111111", 5); circle((x(3), ROW_Y["GNDB"]), 6, "#111", "#111")
text("EasyCAP WHITE RCA", (X0 - 130, ROW_Y["g"] - 4), 15, "#111", "end", True)
text("centre pin (left / even samples)", (X0 - 130, ROW_Y["g"] + 14), 12, "#444", "end")
text("EasyCAP WHITE RCA", (X0 - 130, ROW_Y["GNDB"] - 4), 15, "#111", "end", True)
text("outer sleeve", (X0 - 130, ROW_Y["GNDB"] + 14), 12, "#444", "end")

# Notes ------------------------------------------------------------------------------------------
notes = [
    "Checklist:",
    "•  U1 pins: 1 OUTA, 2 −INA, 3 +INA, 4 GND (lower row); 8 VDD, 7 OUTB, 6 −INB, 5 +INB (upper row). U1B (pins 5–7) is unused: follower tied to VMID.",
    "•  Diodes: D1 stripe (cathode) goes into the VCC rail; D2 stripe goes into column 21 (node X), its plain end into the GND rail.",
    "•  Electrolytics: '+' leg of C4 to VCC rail, C3 to column 16 (VMID), C2 to column 10 (U1 pin 1). The minus stripe on the can is the other leg.",
    "•  Only R3 crosses the centre gap (21e → 21f). Check that the two GND rails are bridged (left edge) — many boards split rails in the middle.",
    "•  Bench first: power only, then the DMM checks in the document (VCC ≈ 4.8–4.9 V, VMID and node X ≈ VCC/2, output ≈ 0 V), then phone tone, then step test.",
    "•  Pin numbers assume the adapter's pin 1 is bottom-left (notch left). Verify against your adapter's markings before inserting.",
]
for i, s in enumerate(notes):
    text(s, (X0 - 290, 750 + i * 24), 14 if i else 15, "#222222", "start", bold=(i == 0))

# Legend
lx, ly = X0 - 290, 950
for i, (lab, col) in enumerate([("VCC / +5 V", "#d33"), ("GND", "#2a5bd7"), ("follower links", "#2e9e44"), ("to VMID", "#e08a00"), ("to +INA", "#7b3fb5")]):
    line((lx + i * 190, ly), (lx + i * 190 + 34, ly), col, 5); text(lab, (lx + i * 190 + 42, ly + 5), 13, "#333", "start")
for i, (lab, fill) in enumerate([("resistor", "#d8b98a"), ("ceramic cap", "#f2c94c"), ("electrolytic", "#2f5aa8"), ("1N4148 (stripe = cathode)", "#e9e9e9")]):
    rect(lx + i * 190, ly + 22, 30, 14, fill, r=4); text(lab, (lx + i * 190 + 42, ly + 34), 13, "#333", "start")


# Output ------------------------------------------------------------------------------------------
def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Segoe UI, Arial, sans-serif">',
       f'<rect width="{W}" height="{H}" fill="#ffffff"/>']
for s in scene:
    if s["kind"] == "line":
        svg.append(f'<line x1="{s["x1"]:.1f}" y1="{s["y1"]:.1f}" x2="{s["x2"]:.1f}" y2="{s["y2"]:.1f}" stroke="{s["color"]}" stroke-width="{s["width"]}" stroke-linecap="round"/>')
    elif s["kind"] == "rect":
        svg.append(f'<rect x="{s["x"]:.1f}" y="{s["y"]:.1f}" width="{s["w"]:.1f}" height="{s["h"]:.1f}" rx="{s["r"]}" fill="{s["fill"]}" stroke="{s["color"]}" stroke-width="{s["width"]}"/>')
    elif s["kind"] == "circle":
        svg.append(f'<circle cx="{s["x"]:.1f}" cy="{s["y"]:.1f}" r="{s["r"]}" fill="{s["fill"]}" stroke="{s["color"]}" stroke-width="{s["width"]}"/>')
    elif s["kind"] == "text":
        wgt = ' font-weight="bold"' if s["bold"] else ""
        svg.append(f'<text x="{s["x"]:.1f}" y="{s["y"]:.1f}" font-size="{s["size"]}" fill="{s["color"]}" text-anchor="{s["anchor"]}"{wgt}>{esc(s["text"])}</text>')
svg.append("</svg>")
(OUT / "receive_audio_breadboard.svg").write_text("\n".join(svg), encoding="utf-8")
(OUT / "breadboard_scene.json").write_text(json.dumps(dict(width=W, height=H, items=scene)), encoding="utf-8")
print(f"wrote {OUT / 'receive_audio_breadboard.svg'} ({len(scene)} items)")
