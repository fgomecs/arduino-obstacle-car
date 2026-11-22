import sys
from svglib import SVG

s = SVG(1500, 1180)
s.title("Obstacle-avoiding car v3 · wiring with WiFi bridge",
        "Arduino Uno R3 + Adafruit Motor Shield v1 (L293D) · 4 DC motors · SG90 servo · HC-SR04 ultrasonic sensor")

# ---------------- board (Uno with shield stacked on top) ----------------
X0, Y0, BW, BH = 520, 330, 460, 330
s.rect(X0 + 10, Y0 + 10, BW, BH, fill="#00878f", stroke="#005a5f", sw=2, rx=12)          # Uno peeking out
s.rect(X0, Y0, BW, BH, fill="#1b4f8a", stroke="#0e2f55", sw=3, rx=12)                    # shield
s.rect(X0 + 150, Y0 + 120, 80, 50, fill="#111", stroke="#000", rx=3)                     # L293D chips
s.rect(X0 + 250, Y0 + 120, 80, 50, fill="#111", stroke="#000", rx=3)
s.text(X0 + 190, Y0 + 150, "L293D", 11, "#ddd", "middle")
s.text(X0 + 290, Y0 + 150, "L293D", 11, "#ddd", "middle")
s.text(X0 + BW / 2, Y0 + 205, "Adafruit Motor Shield v1", 16, "#fff", "middle", "bold")
s.text(X0 + BW / 2, Y0 + 226, "stacked on Arduino Uno R3", 13, "#cfe0f5", "middle")
s.rect(X0 + 330, Y0 + 260, 50, 22, fill="#222", stroke="#000", rx=2)
s.text(X0 + 355, Y0 + 252, "PWR jumper", 11, "#fff", "middle")
s.rect(X0 + 40, Y0 - 22, 44, 30, fill="#cfcfcf", stroke="#555", rx=3)                   # USB-B on Uno
s.text(X0 + 62, Y0 - 30, "USB → PC (upload + Serial 9600)", 11, "#333", "middle")

def edge_pin(x, y, label, side):
    s.pin(x, y)
    if side == "left":
        s.text(x + 14, y + 4, label, 12, "#fff")
    elif side == "right":
        s.text(x - 14, y + 4, label, 12, "#fff", "end")
    elif side == "top":
        s.text(x, y + 24, label, 11, "#fff", "middle")
    else:
        s.text(x, y - 14, label, 11, "#fff", "middle")

# motor terminals
for lab, y in (("M1", 380), ("M1", 405), ("M2", 585), ("M2", 610)):
    edge_pin(X0, y, lab, "left")
for lab, y in (("M3", 380), ("M3", 405), ("M4", 585), ("M4", 610)):
    edge_pin(X0 + BW, y, lab, "right")
# servo header (top edge): GND, +5V, signal (D10)
for lab, x in (("−", 880), ("+5V", 905), ("S", 930)):
    edge_pin(x, Y0, lab, "top")
s.text(905, Y0 + 44, "SERVO_1 (D10)", 11, "#cfe0f5", "middle")
# analog/power row (bottom edge)
for lab, x in (("5V", 780), ("GND", 810), ("A0", 840), ("A1", 870), ("A2", 900), ("A3", 930)):
    edge_pin(x, Y0 + BH, lab, "bottom")
# external power terminal (bottom edge)
for lab, x in (("+", 580), ("−", 610)):
    edge_pin(x, Y0 + BH, lab, "bottom")
s.text(595, Y0 + BH - 30, "EXT_PWR", 11, "#cfe0f5", "middle")

# ---------------- motors ----------------
def motor(x, y, name, sub):
    s.rect(x, y, 160, 90, fill="#f2c200", stroke="#8a6d00", sw=2, rx=8)
    s.circle(x + 80, y + 45, 18, fill="#e6e6e6", stroke="#666")
    s.circle(x + 80, y + 45, 5, fill="#666", stroke="#333", sw=1)
    s.text(x + 80, y - 10, name, 14, "#222", "middle", "bold")
    s.text(x + 80, y + 108, sub, 12, "#555", "middle")

motor(140, 330, "Motor 1 → M1", "left front")
motor(140, 555, "Motor 2 → M2", "left rear")
motor(1200, 330, "Motor 3 → M3", "right front")
motor(1200, 555, "Motor 4 → M4", "right rear")

s.wire([(300, 360), (400, 360), (400, 380), (X0, 380)], "red")
s.wire([(300, 395), (380, 395), (380, 405), (X0, 405)], "black")
s.wire([(300, 585), (X0, 585)], "red")
s.wire([(300, 620), (380, 620), (380, 610), (X0, 610)], "black")
s.wire([(1200, 360), (1100, 360), (1100, 380), (X0 + BW, 380)], "red")
s.wire([(1200, 395), (1120, 395), (1120, 405), (X0 + BW, 405)], "black")
s.wire([(1200, 585), (X0 + BW, 585)], "red")
s.wire([(1200, 620), (1120, 620), (1120, 610), (X0 + BW, 610)], "black")

# ---------------- HC-SR04 + servo ----------------
s.rect(560, 105, 210, 75, fill="#2b6cb0", stroke="#173d66", sw=2, rx=6)
s.circle(600, 133, 22, fill="#d9d9d9", stroke="#777")
s.circle(730, 133, 22, fill="#d9d9d9", stroke="#777")
s.text(665, 98, "HC-SR04 ultrasonic", 14, "#222", "middle", "bold")
for lab, x in (("VCC", 610), ("Trig", 645), ("Echo", 680), ("GND", 715)):
    s.pin(x, 180)
    s.text(x, 166, lab, 9, "#fff", "middle")

s.rect(810, 150, 130, 62, fill="#1f6fd1", stroke="#0e3d78", sw=2, rx=6)
s.rect(845, 128, 60, 22, fill="#eee", stroke="#666", rx=10)
s.text(875, 186, "SG90 servo", 13, "#fff", "middle", "bold")
s.text(960, 140, "sensor is mounted on", 11, "#555")
s.text(960, 155, "the servo horn (pans", 11, "#555")
s.text(960, 170, "36° / 90° / 144°)", 11, "#555")

s.wire([(880, 212), (880, Y0)], "brown", 4)
s.wire([(905, 212), (905, Y0)], "red", 4)
s.wire([(930, 212), (930, Y0)], "orange", 4)

# HC-SR04 → bottom row, routed around the left side of the board
s.wire([(610, 180), (610, 250), (470, 250), (470, 705), (780, 705), (780, Y0 + BH)], "red", 4,
       "VCC → 5V", (520, 705))
s.wire([(645, 180), (645, 262), (455, 262), (455, 720), (840, 720), (840, Y0 + BH)], "yellow", 4,
       "Trig → A0", (640, 720))
s.wire([(680, 180), (680, 274), (440, 274), (440, 735), (870, 735), (870, Y0 + BH)], "green", 4,
       "Echo → A1", (760, 735))
s.wire([(715, 180), (715, 286), (425, 286), (425, 750), (810, 750), (810, Y0 + BH)], "black", 4,
       "GND → GND", (520, 750))

# ---------------- battery ----------------
s.rect(470, 830, 230, 70, fill="#333", stroke="#000", sw=2, rx=8)
s.text(585, 860, "Battery pack", 14, "#fff", "middle", "bold")
s.text(585, 880, "2× 18650 (7.4 V) recommended", 11, "#ddd", "middle")
s.wire([(560, 830), (560, 812)], "red", 5)
s.wire([(580, 778), (580, Y0 + BH)], "red", 5)
s.rect(538, 778, 62, 34, fill="#ddd", stroke="#555", rx=4)
s.text(569, 800, "switch", 11, "#333", "middle")
s.wire([(610, 830), (610, Y0 + BH)], "black", 5)

# ---------------- NodeMCU ESP8266 WiFi bridge ----------------
NX, NY = 1040, 840
s.rect(NX, NY, 300, 110, fill="#151515", stroke="#000", sw=2, rx=8)
s.rect(NX + 175, NY + 22, 105, 70, fill="#bdbdbd", stroke="#555", rx=4)
s.text(NX + 227, NY + 62, "ESP-12", 12, "#333", "middle", "bold")
s.text(NX + 85, NY + 62, "NodeMCU", 15, "#fff", "middle", "bold")
s.text(NX + 85, NY + 80, "ESP8266 · WiFi", 11, "#bbb", "middle")
s.rect(NX - 18, NY + 40, 22, 30, fill="#ccc", stroke="#555", rx=3)
s.text(NX + 150, NY + 135, "NodeMCU (top pins shown)", 12, "#333", "middle")
NP = {"Vin": NX + 40, "GND": NX + 70, "D5": NX + 100, "D6": NX + 130, "GND2": NX + 160}
for lab, x in NP.items():
    s.pin(x, NY)
    s.text(x, NY + 24, "GND" if lab == "GND2" else lab, 10, "#fff", "middle")

BY = Y0 + BH
s.wire([(780, 705), (780, 800), (NP["Vin"], 800), (NP["Vin"], NY)], "red", 4, "5V → Vin", (900, 800))
s.circle(780, 705, 5, fill="#d62828", stroke="#000", sw=1)
s.wire([(810, 750), (810, 785), (NP["GND"], 785), (NP["GND"], NY)], "black", 4)
s.circle(810, 750, 5, fill="#222", stroke="#000", sw=1)
# Uno A3 (TX, 5 V) -> 1k -> node -> D5 ; node -> 2k -> GND  (5 V -> 3.3 V)
s.wire([(930, BY), (930, 680), (NP["D5"], 680), (NP["D5"], 715)], "purple", 4, "A3 TX → D5", (1010, 680))
s.wire([(NP["D5"], 755), (NP["D5"], NY)], "purple", 4)
s.rect(NP["D5"] - 8, 715, 16, 40, fill="#e8d4a8", stroke="#7a5a1e", sw=1.5, rx=4)
s.text(NP["D5"] - 14, 740, "1kΩ", 11, "#333", "end")
s.wire([(NP["D5"], 770), (NP["GND2"] + 40, 770)], "gray", 3, dots=False)
s.rect(NP["D5"] + 20, 763, 40, 14, fill="#e8d4a8", stroke="#7a5a1e", sw=1.5, rx=4)
s.text(NP["D5"] + 40, 795, "2kΩ", 11, "#333", "middle")
s.circle(NP["D5"], 770, 5, fill="#8a3ffc", stroke="#000", sw=1)
s.wire([(NP["GND2"] + 40, 770), (NP["GND2"] + 40, 820), (NP["GND2"], 820), (NP["GND2"], NY)], "black", 3)
s.wire([(900, BY), (900, 695), (1215, 695), (1215, 815), (NP["D6"], 815), (NP["D6"], NY)], "cyan", 4,
       "A2 RX ← D6", (1300, 695))

s.legend_box(40, 1000, 1420, [
    "• Motors M1/M2 = left, M3/M4 = right. Sensor Trig A0, Echo A1 (NewPing). Servo on SERVO_1 = D10.",
    "• WiFi bridge: Uno A2 (RX) ← NodeMCU D6 (3.3 V is fine). Uno A3 (TX, 5 V) → 1kΩ → D5, with 2kΩ from D5 to GND (keeps D5 at ~3.3 V).",
    "• NodeMCU Vin ← Uno 5V, GND ↔ GND. Upload to the NodeMCU over its own USB; it can stay wired to the Uno.",
    "• USB alone can NOT run the motors. PWR jumper ON: battery powers motors + Uno (+ NodeMCU).",
    "• No 1k/2k? Any pair at a 1:2 ratio works (e.g. 10k + 20k, or 2.2k + 4.7k). A wheel spins backwards? Swap its 2 wires.",
], "Notes")

s.save(sys.argv[1])
