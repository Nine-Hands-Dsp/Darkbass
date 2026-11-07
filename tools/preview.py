"""Apercu de la facade (design/preview_full.png) : fond + inverseurs + LED (make_art.py) + reperes des potards
et barre de presets, tels que le plugin les dessine. Usage : python3 tools/preview.py"""
import os, math, re
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SS = 4                                               # sur-echantillonnage du dessin
im = Image.open(os.path.join(ROOT, 'design', 'preview.png')).convert('RGBA')
W, H = im.size
ov = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
d = ImageDraw.Draw(ov)
lay = open(os.path.join(ROOT, 'Layout.h')).read()
pts = re.findall(r'\{ ([\d.]+)f, ([\d.]+)f \}', lay.split('knobPos[8] =')[1].split(';')[0])
R = float(re.search(r'knobR = ([\d.]+)f', lay).group(1))
vals = [6.5, 5.0, 6.0, 4.0, 5.5, 4.5, 6.5, 5.5]      # positions d'exemple (0..10)
for (x, y), v in zip(pts, vals):
    x, y = float(x), float(y)
    a = math.radians(-150 + 30 * v)
    ux, uy = math.sin(a), -math.cos(a)
    r0, r1, w = 0.42 * R, 0.90 * R, 0.058 * R       # repere blanc comme sur la photo du potard
    p0 = ((x + ux * r0) * SS, (y + uy * r0) * SS); p1 = ((x + ux * r1) * SS, (y + uy * r1) * SS)
    d.line([p0, p1], fill=(236, 236, 232, 255), width=int(w * SS))
    for p in (p0, p1):
        d.ellipse((p[0] - w * SS / 2, p[1] - w * SS / 2, p[0] + w * SS / 2, p[1] + w * SS / 2), fill=(236, 236, 232, 255))
pr = [float(v) for v in re.search(r'presetRect \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f', lay).groups()]
x, y, w, h = pr
d.rounded_rectangle((x * SS, y * SS, (x + w) * SS, (y + h) * SS), radius=h * SS / 2, fill=(10, 10, 12, 220), outline=(70, 130, 255, 140), width=SS)
f = ImageFont.truetype(os.path.join(ROOT, 'assets', 'fonts', 'Poppins-Medium.ttf'), 20 * SS)
d.text(((x + w / 2) * SS, (y + h / 2) * SS), 'P1  Modern Growl', font=f, fill=(235, 235, 232, 240), anchor='mm')
for cx, s in ((x + 20, -1), (x + w - 20, 1)):
    d.polygon([((cx - 6 * s) * SS, (y + h / 2 - 7) * SS), ((cx - 6 * s) * SS, (y + h / 2 + 7) * SS), ((cx + 7 * s) * SS, (y + h / 2) * SS)], fill=(235, 235, 232, 170))
ov = ov.resize((W, H), Image.LANCZOS)
Image.alpha_composite(im, ov).convert('RGB').save(os.path.join(ROOT, 'design', 'preview_full.png'))
print('ok')
