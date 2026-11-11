#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verificador del panel Anthracite v2.8 (24 HP, 360 x 380 px).

Comprueba, sin dependencias externas salvo Pillow para la parte de render:

  1. Estructura del SVG  (res/Saturator.svg): viewBox, fondo, bloques, arcos,
     etiquetas, anillos, placa, barras, bordes, paleta y ausencia de cadenas
     heredadas de 18 HP.
  2. Widget (src/SaturatorWidget.cpp): box.size, 23/11/2 hijos con ID y
     coordenadas exactas, 4 tornillos por formula y ausencia de literales v2.7.
  3. Render (opcional, Edge headless + Pillow): 44 centros de tinta a <= 0,5 px
     y 4 huecos criticos.

Uso:
    python specs/verify_v2.8_panel.py
    python specs/verify_v2.8_panel.py --svg <ruta> --widget <ruta>
    python specs/verify_v2.8_panel.py --no-render
    python specs/verify_v2.8_panel.py --dump          # imprime la tabla medida

Codigo de salida: 0 = todo OK, 1 = algun fallo, 2 = error de uso/entorno.
"""

import argparse
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

# ---------------------------------------------------------------------------
# Tablas de referencia (unicas fuentes de verdad de la spec v2.8)
# ---------------------------------------------------------------------------

W, H = 360.0, 380.0
TOL = 0.5

# (id, enum, tipo, x, y) en el ORDEN EXACTO de los addParam del widget.
PARAMS = [
    (0, "LOW_DRIVE", "Knob48", 60.0, 84.0),
    (1, "MID_DRIVE", "Knob48", 180.0, 84.0),
    (2, "HIGH_DRIVE", "Knob48", 300.0, 84.0),
    (3, "LOW_PAN", "Knob20", 30.0, 127.4),
    (4, "LOW_VOL", "Knob20", 60.0, 127.4),
    (5, "MID_PAN", "Knob20", 150.0, 127.4),
    (6, "MID_VOL", "Knob20", 180.0, 127.4),
    (7, "HIGH_PAN", "Knob20", 270.0, 127.4),
    (8, "HIGH_VOL", "Knob20", 300.0, 127.4),
    (9, "OUTPUT_VOL", "Knob48", 120.0, 201.3),
    (10, "MIX_PARAM", "Knob48", 240.0, 201.3),
    (11, "LOW_PAN_CV", "Knob20", 30.0, 299.6),
    (12, "LOW_VOL_CV", "Knob20", 90.0, 299.6),
    (13, "MID_PAN_CV", "Knob20", 150.0, 299.6),
    (14, "MID_VOL_CV", "Knob20", 210.0, 299.6),
    (15, "HIGH_PAN_CV", "Knob20", 270.0, 299.6),
    (16, "HIGH_VOL_CV", "Knob20", 330.0, 299.6),
    (17, "LOW_DRIVE_CV", "Knob20", 60.0, 299.6),
    (18, "MID_DRIVE_CV", "Knob20", 180.0, 299.6),
    (19, "HIGH_DRIVE_CV", "Knob20", 300.0, 299.6),
    (20, "LOW_MAKEUP", "Knob20", 90.0, 127.4),
    (21, "MID_MAKEUP", "Knob20", 210.0, 127.4),
    (22, "HIGH_MAKEUP", "Knob20", 330.0, 127.4),
]

INPUTS = [
    (0, "IN_L", "CustomInputPort", 45.0, 352.6),
    (1, "IN_R", "CustomInputPort", 90.0, 352.6),
    (2, "LOW_PAN_INPUT", "CustomInputPort", 30.0, 273.0),
    (3, "LOW_VOL_INPUT", "CustomInputPort", 90.0, 273.0),
    (4, "MID_PAN_INPUT", "CustomInputPort", 150.0, 273.0),
    (5, "MID_VOL_INPUT", "CustomInputPort", 210.0, 273.0),
    (6, "HIGH_PAN_INPUT", "CustomInputPort", 270.0, 273.0),
    (7, "HIGH_VOL_INPUT", "CustomInputPort", 330.0, 273.0),
    (8, "LOW_DRIVE_INPUT", "CustomInputPort", 60.0, 273.0),
    (9, "MID_DRIVE_INPUT", "CustomInputPort", 180.0, 273.0),
    (10, "HIGH_DRIVE_INPUT", "CustomInputPort", 300.0, 273.0),
]

OUTPUTS = [
    (0, "OUT_L", "CustomOutputPort", 270.0, 352.6),
    (1, "OUT_R", "CustomOutputPort", 315.0, 352.6),
]

CV_X = [30, 60, 90, 150, 180, 210, 270, 300, 330]
BANDS = [60, 180, 300]

PANVOL_TX = ["-22.5", "7.5", "37.5", "97.5", "127.5", "157.5", "217.5", "247.5", "277.5"]
BLOCK_D = (
    "M105,138.072L105,162.758C105,169.571 99.469,175.102 92.657,175.102"
    "L27.343,175.102C20.531,175.102 15,169.571 15,162.758L15,138.072"
    "C15,131.259 20.531,125.729 27.343,125.729L92.657,125.729"
    "C99.469,125.729 105,131.259 105,138.072Z"
)

MAIN_LABELS = [
    "matrix(1,0,0,1,-31.522128,18.992039)",   # PAN Low
    "matrix(1,0,0,1,-1.045128,18.992039)",    # VOL Low
    "matrix(1,0,0,1,88.477872,18.992039)",    # PAN Mid
    "matrix(1,0,0,1,118.954872,18.992039)",   # VOL Mid
    "matrix(1,0,0,1,208.477872,18.992039)",   # PAN High
    "matrix(1,0,0,1,238.954872,18.992039)",   # VOL High
]

BAND_LABELS = [
    "matrix(0.981482,0,0,1,-45.870229,-3.049203)",
    "matrix(0.981482,0,0,1,71.907527,-3.049203)",
    "matrix(0.981482,0,0,1,189.685284,-3.049203)",
]

CV_LABELS = [
    "matrix(1,0,0,1,-14.314782,6.177912)",   # PAN Low
    "matrix(1,0,0,1,6.437345,3.142594)",     # VOL Low
    "matrix(1,0,0,1,15.38674,-5.657139)",    # LOW
    "matrix(1,0,0,1,105.651368,6.267912)",   # PAN Mid
    "matrix(1,0,0,1,126.466495,3.232594)",   # VOL Mid
    "matrix(1,0,0,1,135.44874,-5.657139)",   # MID
    "matrix(1,0,0,1,255.68824,-5.657139)",   # HIGH
    "matrix(1,0,0,1,225.651368,6.177912)",   # PAN High
    "matrix(1,0,0,1,246.279495,3.142594)",   # VOL High
]

AUDIO_LABELS = [
    "matrix(1,0,0,1,-3.226634,12.388046)",     # L in
    "matrix(1,0,0,1,221.773366,12.388046)",    # L out
    "matrix(1,0,0,1,212.089418,267.310764)",   # R out
    "matrix(1,0,0,1,183.203374,94.567715)",    # OUTPUTS
    "matrix(1,0,0,1,-37.125173,94.567715)",    # INPUTS
    "matrix(1,0,0,1,24.214591,172.743049)",    # R in (anidado)
]

DRIVE_CV_E = ["-46.733", "73.267", "193.267"]

LEGACY = [
    'viewBox="0 0 270 380"',
    'width="270" height="380"',
    "M243,138.072",
    "M243,95L27,95L243,95Z",
    "matrix(1,0,0,1,30,19.999998)",
    "matrix(0.880755,0.508504,-0.508504,0.880755,98.16341,49.030914)",
    "matrix(0.880755,0.508504,-0.508504,0.880755,30.66249,49.030914)",
    "matrix(0.864446,0.508504,-0.499088,0.880755,63.212793,-54.318337)",
    "matrix(0.864446,0.508504,-0.499088,0.880755,-3.036378,-54.318337)",
    "matrix(0.864446,0.508504,-0.499088,0.880755,129.463678,-54.318337)",
    "matrix(1,0,0,1,6.780931,-83.691414)",
    "matrix(0.03,0,0,0.03,134.432983,354.005474)",
    "matrix(0.908989,0,0,0.847596,15.35881,55.233855)",
    "matrix(1.187191,0,0,0.269076,-37.906164,250.442542)",
    "matrix(0.379673,0,0,1,169.599622,0.161)",
    'width="264.444"',
    "matrix(1,0,0,1,69.765338,-16.468154)",
    "matrix(1,0,0,1,-8.180083,-16.468154)",
    "matrix(1,0,0,1,-9.022128,18.992039)",
    "matrix(1,0,0,1,21.454872,5.38204)",
    "matrix(1,0,0,1,58.477872,19.430291)",
    "matrix(1,0,0,1,88.954872,5.820292)",
    "matrix(1,0,0,1,125.977872,18.992039)",
    "matrix(1,0,0,1,156.454872,5.38204)",
    "matrix(0.981482,0,0,1,-38.509124,-3.049203)",
    "matrix(0.981482,0,0,1,27.740903,-3.049203)",
    "matrix(0.981482,0,0,1,93.990931,-3.049203)",
    "matrix(1,0,0,1,8.310218,6.177912)",
    "matrix(1,0,0,1,-1.124655,3.142594)",
    "matrix(1,0,0,1,22.88674,-5.657139)",
    "matrix(1,0,0,1,75.589368,6.267912)",
    "matrix(1,0,0,1,66.154495,3.232594)",
    "matrix(1,0,0,1,90.38674,-5.657139)",
    "matrix(1,0,0,1,158.18824,-5.657139)",
    "matrix(1,0,0,1,143.089368,6.177912)",
    "matrix(1,0,0,1,133.654495,3.142594)",
    "matrix(1,0,0,1,-0.976634,12.388046)",
    "matrix(1,0,0,1,134.023366,12.388046)",
    "matrix(1,0,0,1,119.839418,267.310764)",
    "matrix(1,0,0,1,93.203374,94.567715)",
    "matrix(1,0,0,1,-37.250173,94.567715)",
    "matrix(1,0,0,1,22.089591,172.743049)",
]

# (nombre, clase de tinta, region, centro esperado)
RENDER_TARGETS = [
    ("titulo", "light", (100, 6, 260, 30), (180.0, 17.88)),
    ("etq Drive L", "light", (40, 32, 80, 56), (60.0, 45.4375)),
    ("etq Drive M", "light", (160, 32, 200, 56), (180.0, 45.4375)),
    ("etq Drive H", "light", (280, 32, 320, 56), (300.0, 45.4375)),
    ("arco Drive L", "green", (28, 54, 95, 101), (60.0, 77.625)),
    ("arco Drive M", "green", (148, 54, 215, 101), (180.0, 77.625)),
    ("arco Drive H", "green", (268, 54, 335, 101), (300.0, 77.625)),
    ("bloque L", "green", (10, 105, 112, 168), (60.0, 136.4375)),
    ("bloque M", "green", (130, 105, 232, 168), (180.0, 136.4375)),
    ("bloque H", "green", (250, 105, 352, 168), (300.0, 136.4375)),
    ("PAN L", "dark", (18, 138, 45, 152), (30.0, 144.25)),
    ("VOL L", "dark", (48, 138, 75, 152), (60.0, 144.25)),
    ("PAN M", "dark", (138, 138, 165, 152), (150.0, 144.25)),
    ("PAN H", "dark", (258, 138, 285, 152), (270.0, 144.25)),
    ("OUTPUT", "light", (96, 160, 144, 175), (120.0, 169.125)),
    ("MIX", "light", (226, 160, 254, 175), (240.0, 169.125)),
    ("arco OUT", "green", (88, 171, 152, 218), (120.0, 195.0)),
    ("arco MIX", "green", (208, 171, 272, 218), (240.0, 195.0)),
    ("linea 1", "green", (5, 223, 355, 232), (180.0, 227.625)),
    ("cvname L", "white", (44, 230, 76, 246), (60.0, 240.188)),
    ("cvname M", "white", (164, 230, 196, 246), (180.0, 240.125)),
    ("cvname H", "white", (282, 230, 318, 246), (300.0, 240.188)),
    ("sub PAN L", "white", (18, 246, 42, 258), (30.0, 252.0)),
    ("sub DRIVE L", "white", (44, 246, 76, 258), (60.0, 252.0)),
    ("sub VOL L", "white", (78, 246, 102, 258), (89.938, 251.938)),
    ("sub DRIVE M", "white", (164, 246, 196, 258), (180.0, 252.0)),
    ("sub DRIVE H", "white", (284, 246, 316, 258), (300.0, 252.0)),
    ("anillo jack L", "green", (16, 257, 44, 289), (30.0, 273.0)),
    ("anillo jack M", "green", (46, 257, 74, 289), (60.0, 273.0)),
    ("anillo jack H", "green", (256, 257, 284, 289), (270.0, 273.0)),
    ("anillo at L", "light", (16, 285, 44, 314), (30.0, 299.625)),
    ("anillo at M", "light", (76, 285, 104, 314), (90.0, 299.625)),
    ("anillo at H", "light", (256, 285, 284, 314), (270.0, 299.625)),
    ("INPUTS", "light", (44, 316, 90, 330), (67.5, 323.1875)),
    ("OUTPUTS", "light", (264, 316, 320, 330), (292.5, 323.1875)),
    ("L in", "light", (38, 328, 52, 340), (45.0, 334.625)),
    ("R in", "light", (82, 328, 96, 340), (90.0, 334.625)),
    ("L out", "light", (262, 328, 276, 340), (270.0, 334.625)),
    ("R out", "light", (308, 328, 322, 340), (315.0, 334.625)),
    ("placa", "green", (250, 334, 336, 372), (292.5, 352.625)),
    ("logo", "green", (150, 342, 212, 366), (180.0, 354.0)),
    ("barra verde", "green", (56, 344, 80, 358), (67.5, 352.625)),
    ("barra oscura", "bar24", (282, 344, 304, 358), (292.5, 352.625)),
    ("linea 2", "green", (5, 311, 355, 320), (180.0, 316.0)),
]

ALLOWED_COLORS = {
    "rgb(14,14,14)",
    "rgb(18,161,21)",
    "rgb(211,211,211)",
    "white",
    "rgb(24,24,24)",
    "rgb(40,40,40)",
    "rgb(61,61,61)",
    "none",
}


# ---------------------------------------------------------------------------
# Infraestructura de comprobacion
# ---------------------------------------------------------------------------

class Report:
    def __init__(self):
        self.failures = []
        self.count = 0

    def check(self, cond, label, detail=""):
        self.count += 1
        if not cond:
            self.failures.append((label, detail))
            print("FAIL  %s%s" % (label, ("  [" + detail + "]") if detail else ""))
        return bool(cond)

    def summary(self):
        bad = len(self.failures)
        print("\n%s  %d comprobaciones, %d fallos" %
              ("FALLO" if bad else "OK", self.count, bad))
        return 1 if bad else 0


def read_text(path):
    return pathlib.Path(path).read_text(encoding="utf-8")


# ---------------------------------------------------------------------------
# 1. SVG
# ---------------------------------------------------------------------------

def check_svg(svg, rep, verbose=True):
    p = "svg:"

    # -- lienzo ------------------------------------------------------------
    rep.check('viewBox="0 0 360 380"' in svg, p + "viewBox 360x380")
    rep.check('<rect x="0" y="0" width="360" height="380"' in svg, p + "fondo 360x380")
    rep.check('fill:rgb(14,14,14)' in svg, p + "fondo rgb(14,14,14)")

    # -- XML bien formado --------------------------------------------------
    try:
        import xml.etree.ElementTree as ET
        ET.fromstring(svg)
        rep.check(True, p + "XML bien formado")
    except Exception as exc:  # pragma: no cover
        rep.check(False, p + "XML bien formado", str(exc))

    # -- bloques -----------------------------------------------------------
    rep.check(svg.count(BLOCK_D) == 3, p + "3 bloques con el path nuevo",
              "count=%d" % svg.count(BLOCK_D))
    for tx in (0, 120, 240):
        rep.check('transform="matrix(1,0,0,1,%d,-14)"' % tx in svg,
                  p + "bloque tx=%d" % tx)

    # -- arcos PanVol ------------------------------------------------------
    n_panvol = sum(svg.count('transform="matrix(1,0,0,1,%s,-13)"' % tx)
                   for tx in PANVOL_TX)
    rep.check(n_panvol == 9, p + "9 grupos PanVol en y=-13", "count=%d" % n_panvol)
    rep.check("matrix(1,0,0,1,30,19.999998)" not in svg,
              p + "sin arcos fantasma (hijo +30/+20)")

    # -- arcos Drive / OUT / MIX ------------------------------------------
    for e in ("107.379461", "-10.397493", "225.158128"):
        rep.check(("matrix(0.864446,0.508504,-0.499088,0.880755,%s,-54.318337)" % e) in svg,
                  p + "arco Drive e=%s" % e)
    rep.check("matrix(0.880755,0.508504,-0.508504,0.880755,169.36341,49.030914)" in svg,
              p + "arco MIX")
    rep.check("matrix(0.880755,0.508504,-0.508504,0.880755,49.36249,49.030914)" in svg,
              p + "arco OUTPUT")

    # -- lineas, titulo, logo, bordes, placa, barras ----------------------
    rep.check(svg.count("M345,95L15,95L345,95Z") == 2, p + "2 lineas 15-345")
    rep.check("matrix(1,0,0,1,51.970931,-83.691414)" in svg, p + "titulo centrado")
    rep.check(svg.count("matrix(0.03,0,0,0.03,179.932983,354.005474)") == 2,
              p + "logo x2 en 179.932983")
    rep.check("matrix(0.379673,0,0,1,259.599622,0.161)" in svg, p + "borde derecho")
    rep.check(svg.count('width="352.592"') == 2, p + "ancho de bordes 352.592")
    rep.check("matrix(0.969785,0,0,0.847596,92.842,55.233855)" in svg, p + "placa")
    rep.check("matrix(1.187191,0,0,0.269076,52.093836,250.442542)" in svg,
              p + "barra oscura")
    rep.check("matrix(1.187191,0,0,0.269076,-172.906164,250.442542)" in svg,
              p + "barra verde sin cambio")

    # -- etiquetas ---------------------------------------------------------
    for m in MAIN_LABELS:
        rep.check(('transform="%s"' % m) in svg, p + "etq principal " + m.split(",")[-2])
    for m in BAND_LABELS:
        rep.check(('transform="%s"' % m) in svg, p + "etq banda " + m.split(",")[-2])
    for m in CV_LABELS:
        rep.check(('transform="%s"' % m) in svg, p + "etq CV " + m.split(",")[-2])
    for m in AUDIO_LABELS:
        rep.check(('transform="%s"' % m) in svg, p + "etq audio " + m.split(",")[-2])

    # -- copias DRIVE ------------------------------------------------------
    for e in DRIVE_CV_E:
        head = 'transform="matrix(1,0,0,1,%s,184.631)"' % e
        rep.check(head in svg, p + "copia DRIVE e=%s" % e)

    # -- anillos -----------------------------------------------------------
    block_j = re.search(r'<g id="cv_bg_jacks">(.*?)</g>', svg, re.S)
    block_k = re.search(r'<g id="cv_bg_knobs">(.*?)</g>', svg, re.S)
    rep.check(block_j is not None and block_k is not None, p + "grupos cv_bg_*")
    if block_j and block_k:
        cj = re.findall(r'<circle cx="([-\d.]+)" cy="273" r="12.5"', block_j.group(1))
        ck = re.findall(r'<circle cx="([-\d.]+)" cy="299.6" r="11.5"', block_k.group(1))
        rep.check([float(v) for v in cj] == [float(v) for v in CV_X],
                  p + "9 anillos de jack", str(cj))
        rep.check([float(v) for v in ck] == [float(v) for v in CV_X],
                  p + "9 anillos de atenuador", str(ck))

    # -- texto y paleta ----------------------------------------------------
    rep.check(svg.count("<text") == 7, p + "7 nodos <text>",
              "count=%d" % svg.count("<text"))
    colors = set(re.findall(r"(?:fill|stroke):([^;\"]+)", svg))
    extra = colors - ALLOWED_COLORS
    rep.check(not extra, p + "paleta congelada", ", ".join(sorted(extra)))

    return svg


# ---------------------------------------------------------------------------
# 2. Widget C++
# ---------------------------------------------------------------------------

CALL_RE = re.compile(
    r"add(Param|Input|Output)\(\s*create(?:Param|Input|Output)Centered<(\w+)>"
    r"\(\s*Vec\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)\s*,\s*module\s*,\s*Saturator::(\w+)\s*\)\s*\);"
)


def check_widget(path, rep):
    src = read_text(path)
    p = "cpp:"

    code = re.sub(r"//[^\n]*", "", src)
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.S)

    rep.check("box.size = Vec(360, 380);" in code, p + "box.size = Vec(360, 380)")
    rep.check("box.size = Vec(270, 380);" not in code, p + "sin box.size v2.7")

    calls = CALL_RE.findall(code)
    rep.check(len(calls) == 36, p + "36 llamadas add*Centered", "count=%d" % len(calls))

    seen_p, seen_i, seen_o = [], [], []
    parse_ok = True
    for kind, typ, xs, ys, name in calls:
        try:
            x, y = float(xs), float(ys)
        except ValueError:
            parse_ok = False
            continue
        if kind == "Param":
            seen_p.append((typ, x, y, name))
        elif kind == "Input":
            seen_i.append((typ, x, y, name))
        else:
            seen_o.append((typ, x, y, name))
    rep.check(parse_ok, p + "coordenadas numericas")

    def cmp(tag, expected, found):
        # El orden de los add* del fichero NO es el orden del enum: se compara
        # por nombre (ID congelado), no por posicion.
        ok = len(expected) == len(found)
        rep.check(ok, p + tag + " cardinalidad",
                  "esperado %d, hallado %d" % (len(expected), len(found)))
        if not ok:
            return
        byname = {}
        for f in found:
            byname.setdefault(f[3], []).append(f)
        dup = sorted(k for k, v in byname.items() if len(v) > 1)
        rep.check(not dup, p + tag + " sin duplicados", ", ".join(dup))
        extra = sorted(set(byname) - set(e[1] for e in expected))
        rep.check(not extra, p + tag + " sin IDs extra", ", ".join(extra))
        for (eid, ename, etype, ex, ey) in expected:
            f = byname.get(ename)
            if f is None:
                rep.check(False, p + "%s[%d] %s" % (tag, eid, ename), "ausente")
                continue
            ftype, fx, fy, _ = f[0]
            good = (ftype == etype and abs(fx - ex) <= 0.001 and abs(fy - ey) <= 0.001)
            rep.check(good, p + "%s[%d] %s" % (tag, eid, ename),
                      "hallado %s (%.3f, %.3f)" % (ftype, fx, fy))

    cmp("param", PARAMS, seen_p)
    cmp("input", INPUTS, seen_i)
    cmp("output", OUTPUTS, seen_o)

    # tornillos: 4, por formula viva
    flat = re.sub(r"\s+", " ", code)
    rep.check(code.count("createWidget<TornilloNegro>") == 4, p + "4 tornillos")
    for expr in ("Vec(RACK_GRID_WIDTH, 0)",
                 "Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)",
                 "Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)",
                 "Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)"):
        pat = re.escape(expr)
        pat = "Vec\\(\\s*" + pat[len("Vec\\("):]  # tolera salto de linea tras "Vec("
        rep.check(re.search(pat, flat) is not None, p + "tornillo " + expr)

    # ningun literal v2.7 sobrevive
    legacy = ["Vec(67.5, 84)", "Vec(135, 84)", "Vec(202.5, 84)",
              "Vec(82.5, 147.4)", "Vec(52.5, 155.0)", "Vec(67.5, 325)",
              "Vec(67.5, 240)", "Vec(47.3, 352.6)", "Vec(182.3, 352.6)"]
    left = [lit for lit in legacy if lit in code]
    rep.check(not left, p + "sin literales v2.7", ", ".join(left))

    # rejilla
    off = [(n, x) for (_i, n, _t, x, _y) in PARAMS + INPUTS + OUTPUTS
           if abs((x / 7.5) - round(x / 7.5)) > 1e-9]
    rep.check(not off, p + "X multiplos de 7,5", str(off))


# ---------------------------------------------------------------------------
# 3. Render (Edge headless + Pillow)
# ---------------------------------------------------------------------------

def find_edge():
    cands = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        "/usr/bin/microsoft-edge",
        "/usr/bin/google-chrome",
        "/usr/bin/chromium",
    ]
    for c in cands:
        if pathlib.Path(c).exists():
            return c
    return None


def render(svg, scale, edge, tag="v28"):
    from PIL import Image
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="anth_ver_"))
    body = re.sub(r"<\?xml.*?\?>", "", svg, flags=re.S)
    body = re.sub(r"<!DOCTYPE.*?>", "", body, flags=re.S)
    html = ('<!doctype html><meta charset="utf-8"><style>'
            'html,body{margin:0;padding:0;background:#000}'
            '#p{width:%dpx;height:%dpx}</style><div id="p">%s</div>'
            % (W * scale, H * scale, body))
    page = tmp / (tag + ".html")
    png = tmp / (tag + ".png")
    page.write_text(html, encoding="utf-8")
    for _ in range(3):
        if png.exists():
            png.unlink()
        subprocess.run([edge, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--force-device-scale-factor=1",
                        "--window-size=%d,%d" % (W * scale, H * scale),
                        "--virtual-time-budget=4000",
                        "--run-all-compositor-stages-before-draw",
                        "--screenshot=" + str(png.resolve()),
                        "file:///" + str(page.resolve()).replace("\\", "/")],
                       check=True, capture_output=True)
        im = Image.open(png).convert("RGB")
        if im.size == (int(W * scale), int(H * scale)):
            px = im.load()
            if px[int(30 * scale), int(130 * scale)] == (18, 161, 21):
                return im
    raise RuntimeError("render vacio o incompleto")


def build_masks(im):
    from PIL import Image, ImageChops

    def lt(c, v):
        return c.point(lambda p: 255 if p < v else 0)

    def gt(c, v):
        return c.point(lambda p: 255 if p > v else 0)

    def AND(*ms):
        m = ms[0]
        for x in ms[1:]:
            m = ImageChops.darker(m, x)
        return m

    r, g, b = im.split()
    return {
        "green": AND(lt(r, 100), gt(g, 100), lt(b, 100)),
        "light": AND(gt(r, 140), gt(g, 140), gt(b, 140)),
        "white": AND(gt(r, 240), gt(g, 240), gt(b, 240)),
        "dark": AND(lt(r, 60), lt(g, 60), lt(b, 60)),
        "bar24": AND(gt(r, 17), lt(r, 33), gt(g, 17), lt(g, 33), gt(b, 17), lt(b, 33)),
    }


def bbox(mask, region, scale):
    x0, y0, x1, y1 = [int(v * scale) for v in region]
    bb = mask.crop((x0, y0, x1, y1)).getbbox()
    if not bb:
        return None
    return ((x0 + bb[0]) / scale, (y0 + bb[1]) / scale,
            (x0 + bb[2]) / scale, (y0 + bb[3]) / scale)


def check_render(svg, rep, edge, scale=8, dump=False):
    p = "render:"
    im = render(svg, scale, edge)
    masks = build_masks(im)
    boxes = {}

    for name, kind, region, (tx, ty) in RENDER_TARGETS:
        bb = bbox(masks[kind], region, scale)
        if bb is None:
            rep.check(False, p + name, "sin tinta")
            continue
        cx, cy = (bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2
        boxes[name] = bb
        dx, dy = cx - tx, cy - ty
        if dump:
            print("%-14s c=(%8.3f,%8.3f)  d=(%+.3f,%+.3f)  %6.2fx%6.2f"
                  % (name, cx, cy, dx, dy, bb[2] - bb[0], bb[3] - bb[1]))
        rep.check(abs(dx) <= TOL and abs(dy) <= TOL, p + name,
                  "d=(%+.3f,%+.3f)" % (dx, dy))

    # -- bordes (sondas de pixel) -----------------------------------------
    px = im.load()

    def probe(x, y):
        return px[int(x * scale), int(y * scale)]

    rep.check(probe(180, 0.75) == (61, 61, 61), p + "borde sup rgb(61,61,61)", str(probe(180, 0.75)))
    rep.check(probe(180, 3.00) == (14, 14, 14), p + "borde sup acaba en 1,54", str(probe(180, 3.00)))
    rep.check(probe(0.75, 190) == (61, 61, 61), p + "borde izq rgb(61,61,61)", str(probe(0.75, 190)))
    rep.check(probe(359.0, 190) == (40, 40, 40), p + "borde der rgb(40,40,40)", str(probe(359.0, 190)))
    rep.check(probe(180, 379.0) == (40, 40, 40), p + "borde inf rgb(40,40,40)", str(probe(180, 379.0)))

    # -- huecos criticos ---------------------------------------------------
    if "L in" in boxes and "placa" in boxes:
        gap = boxes["placa"][1] - boxes["L in"][3]
        rep.check(gap >= 1.0, p + "hueco L/R -> placa", "%.3f px" % gap)
    if "placa" in boxes:
        gap = W - 1.5 - boxes["placa"][2]  # borde derecho 358,5
        rep.check(gap >= 1.0, p + "hueco placa -> borde derecho", "%.3f px" % gap)
    if "anillo jack L" in boxes and "anillo at L" in boxes:
        gap = boxes["anillo at L"][1] - boxes["anillo jack L"][3]
        rep.check(gap >= 1.2, p + "hueco anillo jack -> atenuador", "%.3f px" % gap)
    if "anillo at L" in boxes and "linea 2" in boxes:
        gap = boxes["linea 2"][1] - boxes["anillo at L"][3]
        rep.check(gap >= 3.0, p + "hueco atenuador -> linea 2", "%.3f px" % gap)
    if "bloque L" in boxes and "OUTPUT" in boxes:
        gap = boxes["OUTPUT"][1] - boxes["bloque L"][3]
        rep.check(gap >= 4.0, p + "hueco bloque -> OUTPUT", "%.3f px" % gap)

    # -- puertos de audio dentro de la placa (geometria) -------------------
    plate = (256.5, 328.5)
    for cx in (270.0, 315.0):
        rep.check(cx - 12 >= plate[0] + 1.0 and cx + 12 <= plate[1] - 1.0,
                  p + "puerto %.0f dentro de la placa" % cx)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="Verificador del panel Anthracite v2.8")
    ap.add_argument("--svg", default=str(ROOT / "res" / "Saturator.svg"))
    ap.add_argument("--widget", default=str(ROOT / "src" / "SaturatorWidget.cpp"))
    ap.add_argument("--no-render", action="store_true", help="omite la parte de render")
    ap.add_argument("--edge", default=None, help="ruta al ejecutable de Edge/Chrome")
    ap.add_argument("--scale", type=int, default=8)
    ap.add_argument("--dump", action="store_true", help="imprime la tabla medida")
    args = ap.parse_args(argv)

    rep = Report()

    svg_path = pathlib.Path(args.svg)
    cpp_path = pathlib.Path(args.widget)
    if not svg_path.exists():
        print("No existe %s" % svg_path)
        return 2
    if not cpp_path.exists():
        print("No existe %s" % cpp_path)
        return 2

    print("SVG    : %s" % svg_path)
    print("Widget : %s" % cpp_path)
    print("-" * 72)

    svg = read_text(svg_path)
    check_svg(svg, rep)
    check_widget(cpp_path, rep)

    if args.no_render:
        print("\n(render omitido)")
    else:
        edge = args.edge or find_edge()
        if not edge:
            print("\n[AVISO] no se encontro Edge/Chrome: parte de render omitida")
        else:
            try:
                check_render(svg, rep, edge, scale=args.scale, dump=args.dump)
            except Exception as exc:
                rep.check(False, "render", "%s: %s" % (type(exc).__name__, exc))

    return rep.summary()


if __name__ == "__main__":
    sys.exit(main())
