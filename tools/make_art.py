"""
DARKBASS (NINE HANDS | DSP) - visuel du plugin, entierement synthetise : pedale double noir uni.

Disposition des commandes reprise de la Darkglass Microtubes B7K Ultra v2 (preampli + saturation basse, format double) :
  rangee 1 : MASTER  BLEND  LEVEL  DRIVE
  rangee 2 : BASS  LO MIDS  HI MIDS  TREBLE
  rangee 3 : inverseurs 3 positions GRUNT, frequence LO MIDS, frequence HI MIDS, ATTACK
  bas      : LED + footswitch BYPASS (gauche), LED + footswitch DRIVE (droite)
Habillage original : boitier alu peint noir satine uni, serigraphie blanche (Poppins), logo NINE HANDS | DSP blanc en bas.
Une seule lumiere pour toute la pedale : en haut a gauche (ombres portees vers le bas a droite).

Repere du plugin : 1240 x 928 unites (comme Ring Think / Peachy), images en 2x (2480 x 1856).
Sorties (racine du depot) :
  background.jpg          fond complet (boitier, serigraphie, potards fixes, footswitchs, LED eteintes, jacks, logo)
  toggle_strip.png        inverseur 3 positions : 3 images carrees empilees (haut, milieu, bas)
  glow_l.png / glow_r.png facade eclairee par chaque LED bleue (canal alpha = seule la lumiere ajoutee)
  led_on.png              lentille allumee (coeur presque blanc, corps bleu sature, reflet fixe)
  fs_refl_l.png / fs_refl_r.png  reflet bleu de la LED voisine sur le flanc de l'ecrou du footswitch (calque alpha)
  Layout.h                reperes des commandes
Usage : python3 tools/make_art.py
"""
import os, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
AS = 2                                   # pixels par unite (images 2x)
UW, UH = 1240, 928
W, H = UW * AS, UH * AS

L = np.array([-0.46, -0.62, 0.64]); L /= np.linalg.norm(L)        # vers la lumiere (x droite, y bas, z vers l'oeil)
V = np.array([0.0, 0.0, 1.0]); HV = (L + V) / np.linalg.norm(L + V)

# ------------------------------------------------------------------ geometrie (unites)
PX0, PY0, PW, PH, PR = 78.0, 56.0, 1084.0, 816.0, 40.0          # boitier (proportions de la B7K Ultra : 125 x 96 mm)
COLS = [PX0 + PW * u for u in (0.150, 0.383, 0.617, 0.850)]
ROW1, ROW2, ROW3 = 184.0, 380.0, 576.0
KNOB_R, EQ_R = 56.0, 56.0
KNOBS = {   # id : (x, y, R, label)
    'master': (COLS[0], ROW1, KNOB_R, 'MASTER'),
    'blend':  (COLS[1], ROW1, KNOB_R, 'BLEND'),
    'level':  (COLS[2], ROW1, KNOB_R, 'LEVEL'),
    'drive':  (COLS[3], ROW1, KNOB_R, 'DRIVE'),
    'bass':   (COLS[0], ROW2, EQ_R, 'BASS'),
    'lomids': (COLS[1], ROW2, EQ_R, 'LO MIDS'),
    'himids': (COLS[2], ROW2, EQ_R, 'HI MIDS'),
    'treble': (COLS[3], ROW2, EQ_R, 'TREBLE'),
}
ORDER = ['master', 'blend', 'level', 'drive', 'bass', 'lomids', 'himids', 'treble']
# inverseurs 3 positions : (x, y, nom, (haut, milieu, bas))
TOGGLES = [
    (COLS[0], ROW3, 'GRUNT',   ('BOOST', 'FLAT', 'CUT')),
    (COLS[1], ROW3, 'LO FREQ', ('1 kHz', '500', '250')),
    (COLS[2], ROW3, 'HI FREQ', ('3 kHz', '1.5 k', '750')),
    (COLS[3], ROW3, 'ATTACK',  ('BOOST', 'FLAT', 'CUT')),
]
LED_L, LED_R = (COLS[0], 670.0), (COLS[3], 670.0)
FS_L, FS_R = (COLS[0], 790.0), (COLS[3], 790.0)
JACK_Y = PY0 + 0.40 * PH

# ------------------------------------------------------------------ outils
YY, XX = np.mgrid[0:H, 0:W].astype(np.float32)
UX, UY = (XX + 0.5) / AS, (YY + 0.5) / AS

def rrect_sd(x, y, x0, y0, w, h, r):
    """distance signee (unites, negative a l'interieur) a un rectangle arrondi"""
    cx, cy = x0 + w / 2, y0 + h / 2
    qx = np.abs(x - cx) - (w / 2 - r); qy = np.abs(y - cy) - (h / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def fbm(shape, scales, amps, seed=0):
    r = np.random.default_rng(seed); out = np.zeros(shape, np.float32)
    for s, a in zip(scales, amps):
        n = r.standard_normal((max(2, shape[0] // s + 2), max(2, shape[1] // s + 2))).astype(np.float32)
        out += a * cv2.resize(n, (shape[1], shape[0]), interpolation=cv2.INTER_CUBIC)
    return out

def over(dst, rgba, x0, y0):
    """compose un sprite RGBA (float) sur dst a la position pixel (x0, y0)"""
    h, w = rgba.shape[:2]
    X0, Y0 = int(round(x0)), int(round(y0))
    sx0, sy0 = max(0, -X0), max(0, -Y0)
    ex, ey = min(w, W - X0), min(h, H - Y0)
    a = rgba[sy0:ey, sx0:ex, 3:4] / 255.0
    reg = dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex]
    dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = reg * (1 - a) + rgba[sy0:ey, sx0:ex, :3] * a

# ------------------------------------------------------------------ 1. surface autour de la pedale
img = np.zeros((H, W, 3), np.float32)
base = 11.0 + 6.0 * np.exp(-(((UX - 300) / 900) ** 2 + ((UY - 150) / 700) ** 2))
img[:] = (base + fbm((H, W), [3, 40], [1.0, 0.9], 1))[..., None] * np.array([1.0, 0.99, 0.98])

sd = rrect_sd(UX, UY, PX0, PY0, PW, PH, PR)
body = np.clip(0.5 - sd * AS, 0, 1)

sh_m = cv2.GaussianBlur((rrect_sd(UX - 16, UY - 22, PX0, PY0, PW, PH, PR) < 0).astype(np.float32), (0, 0), 22 * AS)
ct_m = cv2.GaussianBlur((rrect_sd(UX - 3, UY - 4, PX0, PY0, PW, PH, PR) < 0).astype(np.float32), (0, 0), 4 * AS)
img *= (1 - 0.72 * sh_m - 0.35 * ct_m)[..., None]

# ------------------------------------------------------------------ 2. boitier : alu peint noir satine uni, arete arrondie
EDGE = 9.0
d_in = np.clip(-sd, 0, None)
hgt = np.where(d_in < EDGE, np.sqrt(np.clip(EDGE ** 2 - (EDGE - d_in) ** 2, 0, None)), EDGE)
gy, gx = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 1.0), 1.0 / AS)
nrm = np.dstack([-gx, -gy, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
ndl = np.clip(nrm @ L, 0, 1); ndh = np.clip(nrm @ HV, 0, 1)
paint_black = np.array([17.0, 17.0, 19.0])
encl = paint_black * (0.55 + 0.75 * ndl)[..., None] + (64 * ndh ** 40 + 15 * ndh ** 6)[..., None]
# peinture poudre fine : grain serre + legeres variations, reflet satine large en haut a gauche
grain = fbm((H, W), [1, 2], [1.3, 0.8], 3) + 0.6 * fbm((H, W), [90, 260], [0.6, 0.8], 4)
sheen = 9.0 * np.exp(-(((UX - 360) / 520) ** 2 + ((UY - 190) / 360) ** 2))
encl += (grain + sheen)[..., None] * np.array([1.0, 1.0, 1.05])
img = img * (1 - body[..., None]) + encl * body[..., None]

# ------------------------------------------------------------------ 3. serigraphie blanche (masques sur-echantillonnes 2x)
SS = 2
K = AS * SS
KW, KH = W * SS, H * SS
FONT_MED = os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Medium.ttf')
FONT_BOLD = os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Bold.ttf')
FONT_LIGHT = os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Light.ttf')
ink_im = Image.new('L', (KW, KH), 0)
dr = ImageDraw.Draw(ink_im)

def spaced(x, y, txt, font, track, anchor='m', fill=255):
    """texte centre (anchor m), aligne a gauche (l) ou a droite (r) avec interlettrage (unites)"""
    widths = [font.getlength(ch) for ch in txt]
    total = sum(widths) + track * K * (len(txt) - 1)
    if anchor == 'm': cx = x * K - total / 2
    elif anchor == 'l': cx = x * K
    else: cx = x * K - total
    for ch, w in zip(txt, widths):
        dr.text((cx, y * K), ch, font=font, fill=fill, anchor='lm')
        cx += w + track * K

f_lab = ImageFont.truetype(FONT_MED, int(25 * K))
f_eq = ImageFont.truetype(FONT_MED, int(22 * K))
f_pos = ImageFont.truetype(FONT_LIGHT, int(15 * K))
f_tog = ImageFont.truetype(FONT_MED, int(18 * K))
for kid in ORDER:
    x, y, R, txt = KNOBS[kid]
    spaced(x, y - R - 37, txt, f_lab if R == KNOB_R else f_eq, 4.0)
    # graduations : 11 points de 0 a 10 sur 300 degres autour du potard
    for i in range(11):
        a = math.radians(-150 + 30 * i)
        rr = R + 11
        px, py = x + rr * math.sin(a), y - rr * math.cos(a)
        d = 2.3 if i in (0, 5, 10) else 1.5
        dr.ellipse(((px - d) * K, (py - d) * K, (px + d) * K, (py + d) * K), fill=255)
for (x, y, name, pos) in TOGGLES:
    spaced(x, y - 72, name, f_tog, 3.0)
    for j, t in enumerate(pos):
        spaced(x + 44, y - 32 + 32 * j, t, f_pos, 1.2, anchor='l')
spaced(LED_L[0], LED_L[1] + 34, 'BYPASS', f_tog, 3.0)
spaced(LED_R[0], LED_R[1] + 34, 'DRIVE', f_tog, 3.0)
def spaced_rot(cx, cy, txt, font, track):
    """texte tourne de 90 degres dans le sens anti-horaire (se lit de bas en haut), centre en (cx, cy)"""
    widths = [font.getlength(ch) for ch in txt]
    total = sum(widths) + track * K * (len(txt) - 1)
    hpx = int(font.size * 1.6)
    t = Image.new('L', (int(total) + 8, hpx), 0); td = ImageDraw.Draw(t)
    x = 4
    for ch, w in zip(txt, widths):
        td.text((x, hpx / 2), ch, font=font, fill=255, anchor='lm'); x += w + track * K
    t = t.rotate(90, expand=True)
    ink_im.paste(255, (int(cx * K - t.width / 2), int(cy * K - t.height / 2)), t)
# IN / OUT : tournes de 90 degres (anti-horaire), le long des tranches, a la hauteur des jacks
spaced_rot(PX0 + 30, JACK_Y, 'OUT', f_tog, 2.0)
spaced_rot(PX0 + PW - 30, JACK_Y, 'IN', f_tog, 2.0)
# nom du plugin, au centre entre les footswitchs
# logo DARKBASS fourni par l'utilisateur (assets/darkbass_logo_blanc.svg : nom, filet, « BASS PREAMP / OVERDRIVE »),
# meme largeur que la barre de presets juste en dessous et centre sur elle ; serigraphie blanche comme le reste
PRESET_BAR = (436.0, 768.0, 368.0, 40.0)                 # x, y, largeur, hauteur (repere avant recadrage)
LOGO_GAP = 14.0
import subprocess, io
_lw = int(round(PRESET_BAR[2] * K))
_png = subprocess.run(['rsvg-convert', '-w', str(_lw), os.path.join(ROOT, 'assets', 'darkbass_logo_blanc.svg')], capture_output=True, check=True).stdout
_dl = Image.open(io.BytesIO(_png)).convert('RGBA')
TITLE_W, TITLE_H = PRESET_BAR[2], PRESET_BAR[2] * _dl.height / _dl.width
TITLE_X0, TITLE_Y0 = PRESET_BAR[0], PRESET_BAR[1] - LOGO_GAP - TITLE_H
ink_im.paste(255, (int(round(TITLE_X0 * K)), int(round(TITLE_Y0 * K))), _dl.split()[3])

ink = cv2.resize(np.asarray(ink_im, np.float32) / 255.0, (W, H), interpolation=cv2.INTER_AREA)
# serigraphie : micro-manques de l'encre, tres legers
wear = cv2.GaussianBlur((fbm((H, W), [1, 3], [1.0, 0.6], 21) > 2.5).astype(np.float32), (0, 0), 0.5)
ink = ink * (1 - wear * 0.45) * body
light = 1.0 + 0.04 * np.clip(1 - (UX - PX0) / PW * 0.6 - (UY - PY0) / PH * 0.6, -1, 1)
ink_col = np.array([226.0, 226.0, 222.0]) * light[..., None]
img = img * (1 - ink[..., None]) + ink_col * ink[..., None]

# ------------------------------------------------------------------ 4. logo NINE HANDS | DSP (blanc), tout en bas au centre
logo = np.asarray(Image.open(os.path.join(ROOT, 'assets', 'nine_hands_logo_blanc.png')).convert('RGBA')).astype(np.float32)
LOGO_W = 230.0
lw, lh = int(LOGO_W * AS), int(round(LOGO_W * AS * logo.shape[0] / logo.shape[1]))
lrgb = cv2.resize(logo[..., :3], (lw, lh), interpolation=cv2.INTER_AREA)
la = cv2.resize(logo[..., 3], (lw, lh), interpolation=cv2.INTER_AREA) / 255.0
LOGO_C = (620.0, 836.0)
lx0, ly0 = int(LOGO_C[0] * AS - lw / 2), int(LOGO_C[1] * AS - lh / 2)
reg = img[ly0:ly0 + lh, lx0:lx0 + lw]
lcol = lrgb * 0.9 * light[ly0:ly0 + lh, lx0:lx0 + lw, None]       # meme blanc casse que la serigraphie, barre grise d'origine
img[ly0:ly0 + lh, lx0:lx0 + lw] = reg * (1 - la[..., None]) + lcol * la[..., None]
# le grain de la peinture reste visible a travers l'encre
img += (fbm((H, W), [1], [0.9], 22) * body)[..., None]

# ------------------------------------------------------------------ 5. commandes en relief (champ de hauteurs + lumiere)
SSAA = 3
PXU = AS * SSAA

class Canvas:
    def __init__(self, w_u, h_u):
        self.W, self.H = round(w_u * PXU), round(h_u * PXU)
        yy, xx = np.mgrid[0:self.H, 0:self.W].astype(np.float64)
        self.x = (xx + 0.5) / PXU - w_u / 2
        self.y = (yy + 0.5) / PXU - h_u / 2
        self.h = np.zeros((self.H, self.W)); self.mat = np.zeros((self.H, self.W), np.int32)
    def put(self, hgt, mask, mid):
        m = mask & (hgt > self.h - 1e-9); self.h[m] = hgt[m]; self.mat[m] = mid

def normals(c):
    gy, gx = np.gradient(c.h, 1.0 / PXU)
    n = np.dstack([-gx, -gy, np.ones_like(c.h)]); return n / np.linalg.norm(n, axis=2, keepdims=True)

def soft_shadow(c, samples=10, spread=0.10, max_u=40.0):
    vis = np.zeros_like(c.h); r = np.random.default_rng(1)
    step = 1.0 / PXU * 2; nsteps = int(max_u / step)
    for s in range(samples):
        d = L + r.normal(0, spread, 3) * np.array([1, 1, 0.5]); d /= np.linalg.norm(d)
        hz = math.hypot(d[0], d[1]); tanE = d[2] / hz; dx, dy = d[0] / hz, d[1] / hz
        occl = np.zeros_like(c.h, bool)
        for k in range(1, nsteps):
            t = k * step
            sx, sy = int(round(dx * t * PXU)), int(round(dy * t * PXU))
            if abs(sx) >= c.W or abs(sy) >= c.H: break
            sh = np.full_like(c.h, -1.0)
            ys0, ys1 = max(0, -sy), min(c.H, c.H - sy); xs0, xs1 = max(0, -sx), min(c.W, c.W - sx)
            sh[ys0:ys1, xs0:xs1] = c.h[ys0 + sy:ys1 + sy, xs0 + sx:xs1 + sx]
            occl |= sh > c.h + t * tanE + 0.05
        vis += ~occl
    return vis / samples

def env_chrome(n, gloss=1.0):
    nz = n[..., 2]
    r = np.dstack([2 * nz * n[..., 0], 2 * nz * n[..., 1], 2 * nz * nz - 1])
    k = np.clip(r @ L, -1, 1)
    sky = 0.5 - 0.5 * r[..., 1]
    base_ = 30 + 118 * np.clip(sky, 0, 1) ** 1.6
    box = 215 * np.clip((k - 0.55) / 0.45, 0, 1) ** (1.5 / gloss)
    rim = 30 * np.clip(-r[..., 0] * 0.6 - r[..., 1] * 0.3, 0, 1)
    return base_ + box + rim

def shade(n, vis, tint, kind, rough=0.35):
    ndl = np.clip(n @ L, 0, 1) * (0.25 + 0.75 * vis)
    ndh = np.clip(n @ HV, 0, 1)
    shin = 12 + 220 * (1 - rough) ** 2
    spec = ndh ** shin * (0.35 + 0.65 * vis)
    if kind == 'chrome':
        e = env_chrome(n)
        col = (e * (0.55 + 0.45 * vis) + 255 * spec * 0.9)[..., None] * np.asarray(tint)
    elif kind == 'black':
        col = (32 * ndl + 8)[..., None] * np.asarray(tint) + (105 * spec)[..., None]
    elif kind == 'lens':
        col = (18 * ndl + 8)[..., None] * np.asarray(tint) + (250 * spec)[..., None]
    return col

def finish(c, rgb, shadow_on_panel, size_px):
    obj = cv2.GaussianBlur((c.mat > 0).astype(np.float64), (0, 0), 0.35 * SSAA)
    alpha = np.clip(obj + shadow_on_panel * (1 - obj), 0, 1)
    rgba = np.dstack([rgb * obj[..., None], alpha * 255])
    small = cv2.resize(rgba, size_px, interpolation=cv2.INTER_AREA)
    a = small[..., 3:4] / 255.0
    col = np.where(a > 1e-4, small[..., :3] / np.maximum(a, 1e-4), 0)
    return np.dstack([np.clip(col, 0, 255), np.clip(small[..., 3], 0, 255)]).astype(np.float32)

def panel_shadow(c, vis, amount=0.62):
    sh = (1 - vis) * amount
    occ = cv2.GaussianBlur((c.mat > 0).astype(np.float64), (0, 0), 1.6 * PXU) * 0.35
    return np.clip(sh + occ, 0, 0.85) * (c.mat == 0)

def knob_sprite(R):
    """potard noir cannele, biseau chrome poli au sommet ; le repere blanc est dessine par le plugin (seul lui tourne)"""
    S_ = R * 2.9
    c = Canvas(S_, S_)
    r = np.hypot(c.x, c.y); th = np.arctan2(c.y, c.x)
    Rc = R
    Rk = Rc - 0.022 * R * (0.5 + 0.5 * np.cos(th * 40))           # cannelures
    Hc = 0.40 * R
    side = r < Rk
    rin = Rc - 0.17 * R
    top = np.where(r < rin, Hc + 0.02 * R * (1 - (r / rin) ** 2),
                   Hc - 0.11 * R * np.clip((r - rin) / (Rc - rin), 0, None) ** 1.6)
    c.put(top, side, 1)
    c.mat[side & (r >= rin - 0.01 * R) & (r < Rk - 0.035 * R)] = 2       # biseau poli
    vis = soft_shadow(c, samples=12, spread=0.11, max_u=R * 0.9)
    n = normals(c)
    blk = shade(n, vis, (1.0, 1.0, 1.04), 'black', rough=0.45)
    chr_ = shade(n, vis, (0.97, 0.98, 1.0), 'chrome')
    m = c.mat[..., None]
    rgb = np.where(m == 2, chr_, blk)
    sh = panel_shadow(c, vis, amount=0.66)
    return finish(c, rgb, sh, (round(S_ * AS), round(S_ * AS))), S_, rin

LED_RING = 9.5
def led_sprite(R=LED_RING, lit=False):
    """bague chromee + lentille bleue (eteinte : dome translucide sombre ; allumee : coeur presque blanc)"""
    S_ = R * 3.4
    c = Canvas(S_, S_)
    r = np.hypot(c.x, c.y)
    ring = (r < R)
    c.put(np.where(ring, 3.0 + 1.6 * np.sqrt(np.clip(1 - ((r - 0.80 * R) / (0.2 * R)) ** 2, 0, 1)), 0), ring & (r > 0.6 * R), 2)
    lens = r < 0.62 * R
    c.put(2.2 + 2.4 * np.sqrt(np.clip(1 - (r / (0.62 * R)) ** 2, 0, 1)), lens, 3)
    gap = (r >= 0.60 * R) & (r < 0.66 * R)
    c.put(np.full_like(r, 2.0), gap, 4)
    vis = soft_shadow(c, samples=10, spread=0.12, max_u=10)
    n = normals(c)
    chr_ = shade(n, vis, (0.97, 0.97, 1.0), 'chrome')
    if not lit:
        lensc = shade(n, vis, (0.40, 0.62, 2.4), 'lens', rough=0.05)
    else:
        q = np.clip(r / (0.62 * R), 0, 1)[..., None]
        core = np.array([235.0, 245.0, 255.0]); body_ = np.array([40.0, 120.0, 255.0]); edge = np.array([10.0, 45.0, 170.0])
        lensc = np.where(q < 0.45, core + (body_ - core) * (q / 0.45), body_ + (edge - body_) * ((q - 0.45) / 0.55))
        spec = np.clip(n @ HV, 0, 1)[..., None] ** 120 * 255
        lensc = lensc + spec
    gapc = np.full(chr_.shape, 14.0)
    m = c.mat[..., None]
    rgb = np.where(m == 2, chr_, np.where(m == 3, lensc, gapc))
    if lit:   # sprite allume : seule la lentille (la bague est dans le fond)
        obj = cv2.GaussianBlur((c.mat == 3).astype(np.float64), (0, 0), 0.35 * SSAA)
        rgba = np.dstack([rgb * obj[..., None], obj * 255])
        small = cv2.resize(rgba, (round(S_ * AS), round(S_ * AS)), interpolation=cv2.INTER_AREA)
        a = small[..., 3:4] / 255.0
        col = np.where(a > 1e-4, small[..., :3] / np.maximum(a, 1e-4), 0)
        return np.dstack([np.clip(col, 0, 255), np.clip(small[..., 3], 0, 255)]).astype(np.float32), S_
    return finish(c, rgb, panel_shadow(c, vis, 0.55), (round(S_ * AS), round(S_ * AS))), S_

print('rendu potards...')
import sys
sys.path.insert(0, HERE)
import photo_controls as PC
# potards : photo fournie par l'utilisateur, repere efface (le plugin dessine le repere qui tourne)
KN, KS = PC.knob_sprite(KNOB_R, AS); KRIN = KNOB_R * 0.90
EQK, EQS, EQRIN = KN, KS, KRIN
# LED : photo fournie par l'utilisateur, lentille seule (sans bague metal), recoloree en bleu
LED_LENS = 14.3
LD, LDON, LS = PC.led_sprites(LED_LENS, AS)
LED_RING = LED_LENS * 1.07
for kid in ORDER:
    x, y, R, _ = KNOBS[kid]
    spr, S_ = (KN, KS) if R == KNOB_R else (EQK, EQS)
    over(img, spr, (x - S_ / 2) * AS, (y - S_ / 2) * AS)
for (x, y) in (LED_L, LED_R):
    over(img, LD, (x - LS / 2) * AS, (y - LS / 2) * AS)
Image.fromarray(np.clip(LDON, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'led_on.png'))

# ------------------------------------------------------------------ 6. footswitchs : celui d'ECHOFLEX (photo reelle)
EF_PTS = [(1.7, -56.7), (51.7, -26.7), (51.7, 31.7), (5.0, 58.3), (-48.3, 30.0), (-49.3, -26.7)]
EF_CENTROID = (1.5, 1.5); EF_FLATS = 100.5
# footswitch : photo fournie par l'utilisateur (ecrou hexagonal + bouton en metal tourne)
FS_FLATS = 100.0
FSPR, FSREFL, FS_W, FS_BTN = PC.footswitch_sprite(FS_FLATS, AS)

for (x, y) in (FS_L, FS_R):
    over(img, FSPR, (x - FS_W / 2) * AS, (y - FS_W / 2) * AS)
Image.fromarray(FSREFL, 'RGBA').save(os.path.join(ROOT, 'fs_refl.png'))

# ------------------------------------------------------------------ 7. jacks du kit sur les tranches (OUT a gauche, IN a droite)
JACK_LEN = 108.8                                     # longueur de l'ecrou le long de la tranche (unites)
for side in (-1, 1):
    j, jw_u, jh_u = PC.jack_sprite(JACK_LEN, AS, side)
    jh, jw = j.shape[:2]
    x0 = (PX0 * AS - jw + 2) if side < 0 else ((PX0 + PW) * AS - 2)
    if side < 0: JACK_OUTER_L = x0 / AS
    else:        JACK_OUTER_R = (x0 + jw) / AS
    y0 = JACK_Y * AS - jh / 2
    # ombre portee du jack sur la surface (vers le bas a droite) + ombre de contact contre le boitier
    sh = np.zeros((H, W), np.float32)
    sh[int(y0):int(y0) + jh, int(x0):int(x0) + jw] = j[..., 3] / 255.0
    sh = (cv2.GaussianBlur(cv2.warpAffine(sh, np.float32([[1, 0, 10], [0, 1, 14]]), (W, H)), (0, 0), 8) * 0.6
          + cv2.GaussianBlur(cv2.warpAffine(sh, np.float32([[1, 0, 2], [0, 1, 3]]), (W, H)), (0, 0), 2) * 0.35) * (1 - body)
    img *= (1 - np.clip(sh, 0, 0.8))[..., None]
    over(img, j, x0, y0)

# cadrage final de l'image : 30 unites de fond autour de la pedale (depuis le bout des jacks sur les cotes)
MARGIN = 30.0
CX0, CX1 = int(math.floor(JACK_OUTER_L - MARGIN)), int(math.ceil(JACK_OUTER_R + MARGIN))
CY0, CY1 = int(math.floor(PY0 - MARGIN)), int(math.ceil(PY0 + PH + MARGIN))
FW, FH = CX1 - CX0, CY1 - CY0
print('cadre', FW, 'x', FH, 'decalage', CX0, CY0)
def crop2x(a): return a[CY0 * AS:CY1 * AS, CX0 * AS:CX1 * AS]

# ------------------------------------------------------------------ 8. inverseurs 3 positions : photos d'inverseur NINE HANDS | DSP (tools/toggle_photo.py)
#     haut / milieu (compose des deux photos, levier vu dans l'axe) / bas ; ombre portee vers le bas a droite
import sys
sys.path.insert(0, HERE)
import toggle_photo as TP
TOG_WASHER = 30.0                                    # rayon de la rondelle (unites)
TOG_S = 140.0                                        # sprite carre (unites)
tframes = TP.frames(TOG_WASHER, TOG_S, AS)
Image.fromarray(np.concatenate(tframes, 0), 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'))
PREVIEW_TOG = tframes[1].astype(np.float32)

# ------------------------------------------------------------------ 9. sauvegarde du fond
bg8 = np.clip(img, 0, 255).astype(np.uint8)
Image.fromarray(crop2x(bg8)).save(os.path.join(ROOT, 'background.jpg'), quality=94, subsampling=0)

# ------------------------------------------------------------------ 10. facade eclairee par chaque LED bleue (calque alpha : seule la lumiere ajoutee)
GR = 120.0
def glow(c, fn):
    x0p, y0p = round((c[0] - GR) * AS), round((c[1] - GR) * AS)
    size = round(2 * GR * AS)
    reg = img[y0p:y0p + size, x0p:x0p + size].astype(np.float64)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    d = np.hypot((xx + 0.5) / AS - GR, (yy + 0.5) / AS - GR)
    h1, h2 = 8.0, 30.0
    E = 1.1 * h1 ** 3 / (d ** 2 + h1 ** 2) ** 1.5 + 0.176 * h2 ** 3 / (d ** 2 + h2 ** 2) ** 1.5
    E *= np.clip((GR - d) / 30.0, 0, 1)
    E[d < LED_RING] *= 0.0                               # la LED elle-meme est dessinee par le plugin
    col = np.array([0.22, 0.52, 1.0])
    lum = reg.mean(2, keepdims=True)
    lit = reg + (reg * 2.6 + 12.0 * np.clip(lum / 40.0, 0.3, 1.6)) * E[..., None] * col
    add = lit - reg
    a = np.clip(np.abs(add).max(-1) / 12.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
    Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, fn))
    return (x0p / AS, y0p / AS, size / AS, size / AS)
GL = glow(LED_L, 'glow_l.png'); GRr = glow(LED_R, 'glow_r.png')

# ------------------------------------------------------------------ 11. Layout.h (coordonnees dans le cadre recadre FW x FH)
def f(v): return f"{v:.2f}f"
def X(v): return f(v - CX0)
def Y(v): return f(v - CY0)
def PT(p): return "{ %s, %s }" % (X(p[0]), Y(p[1]))
def RC(r): return "{ %s, %s, %s, %s }" % (X(r[0]), Y(r[1]), f(r[2]), f(r[3]))
with open(os.path.join(ROOT, 'Layout.h'), 'w') as fo:
    fo.write(f"// genere par tools/make_art.py - ne pas modifier a la main (repere {FW} x {FH}, fond 2x)\n#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{{\n")
    fo.write(f"    constexpr int frameW = {FW}, frameH = {FH};   // 30 unites de fond autour de la pedale (bout des jacks sur les cotes)\n")
    fo.write("    // ordre : " + ", ".join(ORDER) + "\n")
    fo.write("    const juce::Point<float> knobPos[8] = { " + ", ".join(PT(KNOBS[k][:2]) for k in ORDER) + " };\n")
    fo.write(f"    const float knobR = {f(KNOB_R)}, knobTopR = {f(KRIN)}, eqR = {f(EQ_R)}, eqTopR = {f(EQRIN)};\n")
    fo.write("    // inverseurs 3 positions : grunt, lo freq, hi freq, attack\n")
    fo.write("    const juce::Point<float> togglePos[4] = { " + ", ".join(PT(t[:2]) for t in TOGGLES) + " };\n")
    fo.write(f"    const float toggleS = {f(TOG_S)};\n")
    fo.write(f"    const juce::Point<float> ledL {PT(LED_L)}, ledR {PT(LED_R)}; const float ledS = {f(LS)};\n")
    fo.write(f"    const juce::Rectangle<float> glowL {RC(GL)}, glowR {RC(GRr)};\n")
    fo.write(f"    const juce::Point<float> fsL {PT(FS_L)}, fsR {PT(FS_R)}; const float footR = {f(FS_BTN)}, fsS = {f(FS_W)};   // bouton / sprite\n")
    fo.write(f"    const juce::Rectangle<float> statusRect {RC((TITLE_X0 - 14, TITLE_Y0 - 10, TITLE_W + 28, TITLE_H + 18))}, presetRect {RC(PRESET_BAR)};\n")
    fo.write(f"    const float statusFont = {f(30.0)}, presetFont = {f(24.0)}, uiScale = 1.08f;\n")
    fo.write(f"    const juce::Rectangle<float> boxRect {RC((PX0, PY0, PW, PH))};   // boitier (notice)\n")
    fo.write(f"    const float jackY = {Y(JACK_Y)}, jackOuterL = {X(JACK_OUTER_L)}, jackOuterR = {X(JACK_OUTER_R)};   // jacks (notice)\n")
    fo.write("}\n")

# ------------------------------------------------------------------ 12. apercu (design/preview.png) : fond + leviers au milieu + LED allumees
prev = img.copy()
for ti, (x, y, *_ ) in enumerate(TOGGLES):
    over(prev, tframes[(0, 1, 2, 1)[ti]].astype(np.float32), (x - TOG_S / 2) * AS, (y - TOG_S / 2) * AS)
for (c, g) in ((LED_L, 'glow_l.png'), (LED_R, 'glow_r.png')):
    gi = np.asarray(Image.open(os.path.join(ROOT, g))).astype(np.float32)
    over(prev, gi, (c[0] - GR) * AS, (c[1] - GR) * AS)
    over(prev, LDON, (c[0] - LS / 2) * AS, (c[1] - LS / 2) * AS)
    over(prev, FSREFL.astype(np.float32), ((FS_L if c == LED_L else FS_R)[0] - FS_W / 2) * AS, ((FS_L if c == LED_L else FS_R)[1] - FS_W / 2) * AS)
os.makedirs(os.path.join(ROOT, 'design'), exist_ok=True)
Image.fromarray(np.clip(crop2x(prev), 0, 255).astype(np.uint8)).resize((FW, FH), Image.LANCZOS).save(os.path.join(ROOT, 'design', 'preview.png'))
print('ok')
