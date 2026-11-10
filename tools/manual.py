# DARKBASS (NINE HANDS | DSP) - notice PDF en francais, illustree avec le rendu reel du plugin.
# Usage : python3 tools/manual.py [<dossier des captures (tools/render)>] <fichier PDF de sortie>
#   avec captures : recompose les images et les copie dans tools/manual_assets/ ;
#   sans captures : reprend les images composees de tools/manual_assets/.
import os, sys, re, shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, CondPageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1] if len(sys.argv) > 2 else None
OUT = sys.argv[-1]
VERSION = "1.1.4"
import tempfile
TMP = tempfile.mkdtemp()
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"), ("Title", "BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#3F7FFF"); ACC_L = colors.HexColor("#7FA9FF")
DARK = colors.HexColor("#141518"); GREY = colors.HexColor("#63666D"); LIGHT = colors.HexColor("#E8EEFB")

# ---------- geometrie (Layout.h, repere Layout.h) ----------
LAY = open(os.path.join(ROOT, "Layout.h")).read()
def pt(name): return [float(v) for v in re.search(name + r" \{ ([\d.]+)f, ([\d.]+)f", LAY).groups()]
def pts(name): return [(float(a), float(b)) for a, b in re.findall(r"\{ ([\d.]+)f, ([\d.]+)f \}", LAY.split(name + "[")[1].split(";")[0])]
KP, TP = pts("knobPos"), pts("togglePos")
LEDL, LEDR, FSL, FSR = pt("ledL"), pt("ledR"), pt("fsL"), pt("fsR")
PX0, PY0, PW, PH = [float(v) for v in re.search(r'boxRect \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f', LAY).groups()]
PR = 40.0
FH = float(re.search(r'frameH = (\d+)', LAY).group(1))
JACK_Y = float(re.search(r'jackY = ([\d.]+)f', LAY).group(1))
def rect(name): return [float(v) for v in re.search(name + r" \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f", LAY).groups()]
SR, PRR = rect("statusRect"), rect("presetRect")

ASSETS = os.path.join(ROOT, "tools", "manual_assets")
NAMES = ["darkbass_annotated.png", "darkbass_toggles.png", "darkbass_leds.png", "darkbass_bar.png", "darkbass_cover_cutout.png"]
if SHOTS:
    src = Image.open(os.path.join(SHOTS, "darkbass_100.png")).convert("RGB")
    k = src.height / FH
    marks = [(i + 1, *KP[i]) for i in range(8)] + [(9 + i, *TP[i]) for i in range(4)] + \
            [(13, *LEDL), (14, *LEDR), (15, *FSL), (16, *FSR), (17, PRR[0] + PRR[2] / 2, PRR[1] + PRR[3] / 2), (18, PX0 + PW + 22, JACK_Y + 78), (19, PX0 - 22, JACK_Y + 78)]
    off = {**{i: (-82, 0) for i in range(1, 9)}, **{i: (-62, 0) for i in range(9, 13)}, 13: (-48, 0), 14: (-48, 0), 15: (-88, 0), 16: (88, 0),
           17: (-218, 0), 18: (0, 0), 19: (0, 0)}
    ann = src.copy(); dr = ImageDraw.Draw(ann)
    fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), int(24 * k))
    for n, x, y in marks:
        ox, oy = off[n]
        cx, cy, r = (x + ox) * k, (y + oy) * k, 19 * k
        dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(63, 127, 255), width=int(4 * k))
        dr.text((cx, cy + 1), str(n), font=fnt, fill=(20, 21, 24), anchor="mm")
    annotated = os.path.join(TMP, NAMES[0]); ann.save(annotated)

    def crop_u(path, box):
        im = Image.open(os.path.join(SHOTS, path)).convert("RGB")
        kk = im.height / FH
        return im.crop(tuple(int(v * kk) for v in box))
    gap = 18
    def strip(ims, name):
        s = Image.new("RGB", (sum(m.width for m in ims) + gap * (len(ims) - 1), ims[0].height), (255, 255, 255))
        x = 0
        for m in ims: s.paste(m, (x, 0)); x += m.width + gap
        p = os.path.join(TMP, name); s.save(p); return p
    # inverseurs (rangee complete) ; LED + footswitchs off / on ; ligne d'etat + presets
    tog = crop_u("darkbass_100.png", (PX0 + 40, TP[0][1] - 92, PX0 + PW - 40, TP[0][1] + 52))
    tog_img = os.path.join(TMP, NAMES[1]); tog.save(tog_img)
    lb = (LEDL[0] - 110, LEDL[1] - 40, LEDL[0] + 110, FSL[1] + 70)
    led_img = strip([crop_u("darkbass_bypass.png", lb), crop_u("darkbass_100.png", lb)], NAMES[2])
    # meme pastille : barre de presets au repos, ligne d'etat pendant les reglages
    pb = (PRR[0] - 14, PRR[1] - 10, PRR[0] + PRR[2] + 14, PRR[1] + PRR[3] + 10)
    a, b = crop_u("darkbass_100.png", pb), crop_u("darkbass_status.png", pb)
    bar = Image.new("RGB", (a.width, a.height * 2 + 8), (255, 255, 255)); bar.paste(a, (0, 0)); bar.paste(b, (0, a.height + 8))
    bar_img = os.path.join(TMP, NAMES[3]); bar.save(bar_img)

    # ---------- couverture : pedale detouree (boitier + jacks), reflet au sol, halo bleu tres doux ----------
    shot = Image.open(os.path.join(SHOTS, "darkbass_100.png")).convert("RGB")
    kk = shot.height / FH
    body = Image.new("L", shot.size, 0)
    ImageDraw.Draw(body).rounded_rectangle((PX0 * kk, PY0 * kk, (PX0 + PW) * kk, (PY0 + PH) * kk), radius=PR * kk, fill=255)
    jm = Image.new("L", shot.size, 0); jd = ImageDraw.Draw(jm)
    jd.rectangle(((PX0 - 34) * kk, (JACK_Y - 62) * kk, PX0 * kk + 2, (JACK_Y + 62) * kk), fill=255)
    jd.rectangle(((PX0 + PW) * kk - 2, (JACK_Y - 62) * kk, (PX0 + PW + 34) * kk, (JACK_Y + 62) * kk), fill=255)
    arr = np.asarray(shot).astype(np.float32).mean(-1)
    m = np.maximum(np.asarray(body).astype(np.float32), np.asarray(jm).astype(np.float32) * (arr > 45))
    mk = Image.fromarray(m.astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    cut = Image.new("RGBA", shot.size, (0, 0, 0, 0)); cut.paste(shot, (0, 0), mk)
    cut = cut.crop(cut.getbbox())
    cw, ch = cut.size
    pad = int(cw * 0.14)
    canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
    yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
    gcx, gcy = canvas.width / 2, pad + ch * 0.45
    glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 38
    edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
    glow *= edge ** 2
    halo = np.dstack([glow * 0.25, glow * 0.50, glow * 1.0, np.full_like(glow, 255)]).astype(np.uint8)
    canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
    refl = cut.transpose(Image.FLIP_TOP_BOTTOM)          # reflet au sol (image de couverture, pas une commande)
    ra = np.array(refl).astype(np.float64)
    fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
    ra[..., 3] *= fade[:, None]
    canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
    canvas.alpha_composite(cut, (pad, pad))
    canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
    cover = os.path.join(TMP, NAMES[4]); canvas.convert("RGB").save(cover)
    os.makedirs(ASSETS, exist_ok=True)
    for n in NAMES: shutil.copy(os.path.join(TMP, n), os.path.join(ASSETS, n))
else:
    annotated, tog_img, led_img, bar_img, cover = [os.path.join(ASSETS, n) for n in NAMES]

LOGO = os.path.join(ROOT, "assets", "nine_hands_logo_blanc.png")

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=48, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#C4C8D0"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=ACC_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths, header=True):
    data = [[P(c, "cellb" if (header and r == 0) else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DCEB")),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------- donnees (identiques a PluginProcessor.cpp) ----------
FACTORY = [  # nom, master, blend, level, drive, bass, lomids, himids, treble, grunt, lo, hi, attack, driveOn
    ("Modern Growl",   4.78, 6.0, 5.0, 6.0, 6.0, 4.5, 6.0, 6.0, 2, 0, 1, 1, 1),
    ("Clean DI",       4.87, 0.0, 5.0, 3.0, 5.5, 5.0, 5.5, 5.5, 2, 0, 0, 0, 0),
    ("Clank & Grind",  5.72, 5.5, 5.5, 4.0, 5.0, 4.0, 6.5, 6.5, 2, 0, 1, 1, 1),
    ("Tight Djent",    4.96,  7.0, 5.0, 7.5, 5.5, 3.5, 6.0, 6.5, 1, 0, 0, 1, 1),
    ("Doom Wall",      3.59, 8.0, 5.0, 9.0, 6.5, 5.5, 5.0, 4.0, 0, 2, 2, 2, 1),
    ("Upright Warmth", 5.16,  2.0, 5.0, 2.0, 6.0, 6.0, 3.5, 3.5, 0, 2, 2, 2, 1),
    ("Slap Bright",    4.68, 1.0, 5.0, 3.0, 6.5, 3.5, 6.5, 7.0, 2, 0, 1, 1, 1),
    ("Scooped Metal",  3.59, 6.5, 5.0, 7.0, 7.0, 3.0, 4.5, 7.0, 0, 0, 0, 1, 1),
    ("Mid Punch Rock", 6.25,  4.5, 5.0, 5.0, 5.5, 6.5, 6.0, 5.0, 2, 1, 0, 0, 1),
    ("All Noon",       5.0,  5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 1, 1, 1, 1, 1)]
# positions haut / milieu / bas comme sur la pedale
GRU, ATT = ["boost", "cut", "flat"], ["flat", "boost", "cut"]
LOF, HIF = ["500 Hz", "1 kHz", "250 Hz"], ["1.5 kHz", "3 kHz", "750 Hz"]
DESC = {
 "fr": ["Le preset de départ : saturation mélangée à un clair solide, aigus à 3 kHz et attack boost. Growl moderne qui reste défini.",
        "Footswitch DRIVE coupé : le préampli seul, en DI clair, EQ 4 bandes presque à plat.",
        "Peu de drive, hi mids à 3 kHz et attack boost : le « clank » métallique d'un médiator sur une basse brillante.",
        "Beaucoup de drive avec grunt cut (graves serrés dans la saturation) et attack boost : riffs rapides et articulés en accordage bas.",
        "Drive élevé, grunt boost, lo mids à 250 Hz, attack cut : un mur saturé épais et sombre.",
        "Presque clair, un soupçon de drive, graves montés, aigus baissés : chaud, rond, proche d'une contrebasse.",
        "Presque clair, graves et aigus montés, 500 Hz creusés : slap brillant et percussif.",
        "500 Hz creusés, graves et aigus montés, grunt et attack boost : la basse metal moderne classique.",
        "Lo mids montés à 1 kHz : un son rock percutant qui passe au-dessus des guitares.",
        "Tous les potards à midi, tous les inverseurs au milieu : le point de référence."]}
def pc(v): return f"{round(v * 10)} %"

T = {
"fr": dict(
 lang="FRANÇAIS", sub="Préampli basse / overdrive",
 intro_t="1. Présentation",
 intro=["<b>Darkbass</b> est un préampli basse avec saturation au format VST3, en pédale double NINE HANDS | DSP. Il reproduit la "
        "Darkglass Microtubes B7K Ultra v2 : une voie claire à gain unitaire et une voie saturée séparée, bâtie autour d'un inverseur CMOS (le son "
        "« microtubes »), mélangées par BLEND, puis sculptées par un EQ 4 bandes à fréquences de médiums commutables.",
        "Deux footswitchs, comme sur l'original : BYPASS allume ou coupe toute la pédale, DRIVE active la saturation "
        "indépendamment, pour garder en permanence le préampli clair et son EQ. GRUNT et ATTACK règlent la quantité de graves "
        "et d'aigus qui entrent dans l'écrêtage, du son serré et moderne au son épais et sombre.",
        "Le visuel est un design original noir uni (sans lien avec Darkglass). La simulation de baffle de la v2 est disponible "
        "dans le menu clic droit (IR intégrée ou fichier personnel) ; elle est coupée par défaut car Darkbass est prévu pour "
        "être placé avant un simulateur d'ampli ou de baffle."],
 inst_t="2. Installation",
 inst=[("Windows", "Copier <b>Darkbass.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3\\</b>."),
       ("macOS", "Dézipper puis copier <b>Darkbass.vst3</b> dans <b>~/Library/Audio/Plug-Ins/VST3/</b>. Si macOS le bloque au "
                 "premier lancement, taper <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Darkbass.vst3</b> dans le Terminal."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom "
                  "<b>Darkbass (NINE HANDS | DSP)</b>.")],
 ov_t="3. Vue d'ensemble",
 ov_hdr=["N°", "Commande", "Rôle"],
 ov=[("MASTER", "Volume général"), ("BLEND", "Mélange clair / saturé"), ("LEVEL", "Volume de la voie saturée"), ("DRIVE", "Quantité de saturation"),
     ("BASS", "Plateau grave, 100 Hz"), ("LO MIDS", "Cloche bas-médiums"), ("HI MIDS", "Cloche haut-médiums"), ("TREBLE", "Plateau aigu, 5 kHz"),
     ("GRUNT", "Graves dans l'écrêtage : boost / cut / flat (haut / milieu / bas)"), ("LO FREQ", "Fréquence de LO MIDS : 500 Hz / 1 kHz / 250 Hz"),
     ("HI FREQ", "Fréquence de HI MIDS : 1,5 kHz / 3 kHz / 750 Hz"), ("ATTACK", "Signal et aigus dans l'écrêtage : flat / boost / cut"),
     ("LED BYPASS", "Pédale active"), ("LED DRIVE", "Saturation active"), ("Footswitch BYPASS", "Toute la pédale on / off"),
     ("Footswitch DRIVE", "Voie saturée on / off"), ("Barre de presets / ligne d'état", "Preset en cours (flèches : précédent / suivant ; centre : menu) ; pendant un réglage, valeur en direct"), ("IN", "Entrée (côté droit)"), ("OUT", "Sortie (côté gauche)")],
 ctl_t="4. Les commandes",
 ctl=[("MASTER", "Volume de sortie de toute la pédale. Midi = gain unitaire (0 dB), tout à gauche = silence, tout à droite = +12 dB. "
                 "Permet d'égaliser avec le niveau en bypass."),
      ("BLEND", "Fondu linéaire entre la voie claire (toujours à gain unitaire) et la voie saturée, comme le potentiomètre de la "
                "pédale : 0 = clair seul, 10 = saturé seul ; vers midi chaque voie est baissée d'environ 6 dB, le mélange peut donc "
                "sortir un peu moins fort que les deux extrêmes. Garder un peu de clair conserve l'attaque et les graves sous une "
                "forte saturation. Sans effet quand le footswitch DRIVE est coupé."),
      ("LEVEL", "Volume de la voie saturée seule (loi logarithmique, comme sur la pédale). À midi, la saturation sort à peu près "
                "au même volume que la voie claire avec DRIVE à midi. Elle monte nettement quand on tourne DRIVE, comme sur "
                "l'original."),
      ("DRIVE", "Gain de l'étage à ampli op placé avant l'écrêteur, de ×4 à ×78 (loi de potentiomètre logarithmique inverse). Le "
                "signal passe d'abord dans un étage JFET J201 (saturation douce, asymétrique), puis dans l'ampli op (qui écrête "
                "lui-même sur ses rails aux réglages élevés), puis dans un seul inverseur CMOS CD4049UBE en contre-réaction : la "
                "saturation « microtubes ». Le tout est sur-échantillonné x4. D'un léger grain à 0 jusqu'à une distorsion dense "
                "et compressée à 10."),
      ("BASS / TREBLE", "Filtres en plateau ±12 dB à 100 Hz et 5 kHz."),
      ("LO MIDS / HI MIDS", "Filtres en cloche ±12 dB. LO FREQ choisit 500 Hz, 1 kHz ou 250 Hz, HI FREQ choisit 1,5 kHz, 3 kHz ou "
                            "750 Hz (inverseur haut / milieu / bas, dans le même ordre que sur la pédale ; le milieu correspond à la B7K d'origine). Le changement de fréquence glisse en douceur, sans clic."),
      ("Courbe de l'EQ", "Les quatre potards d'EQ suivent la même courbe continue : aucune marche ni changement de pente à midi, "
                          "réglage fin autour de midi, plus rapide vers les butées. L'EQ agit sur le mélange, clair compris : il "
                          "règle aussi le préampli clair quand DRIVE est coupé."),
      ("GRUNT", "Trois niveaux de graves envoyés dans l'écrêteur, fixés par le condensateur placé devant l'inverseur CMOS (comme "
                "sur la pédale). BOOST : le plus de graves, grain lourd et épais. FLAT : niveau intermédiaire. CUT : le moins de "
                "graves, environ 5 dB sous FLAT, distorsion serrée et moderne qui laisse les graves propres à BLEND."),
      ("ATTACK", "Quantité de signal, et d'aigus, envoyée dans l'écrêtage. BOOST : environ +8,7 dB dans la saturation avec un peu "
                 "plus d'aigus, plus de clarté, de présence et de « clank ». FLAT : neutre. CUT : environ −2,4 dB et moins "
                 "d'aigus, distorsion plus chaude et plus douce."),
      ("Couleur de la saturation", "Après l'écrêteur, la voie saturée est filtrée comme sur le circuit d'origine : un réseau en T "
                                   "ponté creuse les médiums autour de 720 Hz, puis deux passe-bas (10,7 kHz et 3,3 kHz) arrondissent "
                                   "le grésillement. La saturation est volontairement légère dans le grave ; la voie claire ramène "
                                   "les graves via BLEND.")],
 tog_cap="Les quatre inverseurs 3 positions. Clic sur la moitié haute d'un inverseur pour le monter, sur la moitié basse pour le descendre (ou molette de la souris).",
 fs_t="5. Footswitchs, LED et MIDI",
 fs=["<b>BYPASS</b> (gauche) : allume ou coupe toute la pédale. Coupée = le signal d'entrée, compensé en retard, donc rien ne "
     "bouge dans le temps au moment de la bascule. LED gauche allumée = pédale active.",
     "<b>DRIVE</b> (droite) : active ou coupe la voie saturée. Coupée = préampli clair et EQ seuls. LED droite allumée = "
     "saturation active (et pédale active).",
     "Clic = on / off. Un appui maintenu plus de 0,45 s alors que la section était coupée la rend momentanée : elle se coupe au "
     "relâchement (désactivable : clic droit &gt; Settings).",
     "Les LED s'allument et s'éteignent en fondu ; la lentille bleue facettée éclaire la façade noire et l'écrou du footswitch."],
 led_cap="À gauche : bypass (LED éteintes). À droite : pédale et saturation actives.",
 midi_t="6. MIDI et pédales d'expression",
 midi_txt=["Aucun CC n'est assigné par défaut. Clic droit sur n'importe quelle commande (potard, inverseur, footswitch) &gt; "
           "<b>MIDI Learn</b>, puis bouger la pédale ou appuyer sur le switch du contrôleur : le premier CC reçu est assigné "
           "(affiché 2 s à la place du nom du preset). Les assignations sont sauvegardées avec le projet. Toutes les commandes sont "
           "aussi automatisables dans le DAW."],
 midi_hdr=["Commande", "Fonctionnement"],
 midi=[["Potards", "Rampe linéaire entre deux CC, puis lissage à 2 pôles : aucun escalier avec une pédale d'expression. "
                   "La course de la pédale est apprise automatiquement (0 et 100 % exacts même si elle n'envoie que 2 à 116), "
                   "zones mortes aux deux butées. CC 14 bits accepté (CC n + CC n+32 pour les CC 0 à 31)."],
       ["Inverseurs 3 positions", "CC 86 à 127 = haut, 43 à 85 = milieu, 0 à 42 = bas."],
       ["Footswitchs", "À accrochage (127 = on / 0 = off) ou momentanés (chaque appui bascule) : clic droit &gt; Settings."],
       ["Cab sim", "CC ≥ 64 = on."],
       ["Program Change", "1 à 10 = presets d'usine."]],
 win_t="7. La fenêtre",
 win=["La fenêtre s'ouvre à la taille « 60 % », qui est aussi la taille minimale : on peut l'agrandir en tirant le coin, jamais la "
      "réduire en dessous. Clic droit &gt; <b>Plugin size</b> : 60 %, 75 %, 100 %, et <b>Default size (new windows)</b> pour "
      "choisir la taille de toutes les nouvelles fenêtres de Darkbass. Le menu clic droit s'ouvre toujours à l'endroit du clic, "
      "sur une commande (MIDI Learn de cette commande) comme sur la façade.",
      "La <b>barre de presets</b> affiche le preset en cours : flèches pour précédent / suivant, centre pour le menu complet "
      "(presets d'usine, 9 emplacements utilisateur à charger et sauvegarder). Dès qu'un réglage bouge (souris, automation, MIDI, "
      "footswitch), la <b>ligne d'état</b> prend sa place dans la même pastille avec la valeur en direct (en %, ou la position "
      "de l'inverseur) et les messages du MIDI Learn ; 2,5 s après le dernier mouvement, elle s'efface en fondu et le preset "
      "revient. Un clic sur la pastille pendant un réglage fait revenir tout de suite la barre de presets."],
 bar_cap="La même pastille : barre de presets au repos (en haut), ligne d'état pendant un réglage (en bas).",
 cab_t="Simulation de baffle",
 cab="Clic droit &gt; <b>Cab sim</b> : on / off, <b>Built-in IR</b> (baffle basse 4x10 intégré) ou <b>Load IR file...</b> "
     "(WAV / AIFF, normalisé, le chemin est sauvegardé avec le projet). Le volume est compensé : activer la simulation ne "
     "fait pas de saut de volume.",
 pre_t="8. Presets",
 pre=["Dix presets d'usine, rappelés depuis la barre de presets, le menu clic droit, la liste des programmes du DAW ou un "
      "Program Change MIDI. Ils sont tous au même volume. Vos propres réglages se sauvegardent dans 9 emplacements utilisateur."],
 fac_hdr=["N°", "Preset", "Master / Blend / Level / Drive", "Bass / Lo / Hi / Treble", "Inverseurs", "Description"],
 sw=lambda g, lo, hi, a, d: f"grunt {GRU[g]}, {LOF[lo].replace('.', ',')}, {HIF[hi].replace('.', ',')}, attack {ATT[a]}" + ("" if d else ", <b>drive coupé</b>"),
 tips_t="9. Conseils d'utilisation",
 tips=["Dans Reaper, placer Darkbass sur la piste basse avant le simulateur d'ampli ou de baffle (par exemple Ampire, puis Impulse), "
       "cab sim coupé.",
       "Pour une basse accordée bas, partir de DRIVE à midi, BLEND vers 5 et GRUNT sur CUT : la voie claire garde la "
       "fondamentale, la saturation ajoute le growl par-dessus.",
       "Pour enregistrer en direct sans simulateur d'ampli, activer le cab sim.",
       "Le footswitch DRIVE (ou le MIDI) passe du préampli clair à la saturation sans changer l'EQ.",
       "Assigner une pédale d'expression à BLEND ou DRIVE avec le MIDI Learn pour des variations progressives en live."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Canaux", "Mono ou stéréo"],
       ["Latence", "61 échantillons (sur-échantillonnage x4 à phase linéaire), annoncée au DAW ; voies claire et bypass compensées"],
       ["Saturation", "Étage JFET J201, ampli op ×4 à ×78, un inverseur CMOS CD4049UBE (gain de boucle ouverte fini), sur-échantillonnage x4 ; creux de médiums en T ponté ~720 Hz, passe-bas 10,7 kHz + 3,3 kHz"],
       ["EQ", "Bass 100 Hz, Lo mids 250 / 500 / 1k, Hi mids 750 / 1,5k / 3k, Treble 5 kHz, ±12 dB chacun"],
       ["Cab sim", "IR basse 4x10 intégrée ou fichier WAV / AIFF (convolution sans latence)"],
       ["MIDI", "MIDI Learn sur les 15 commandes (sauvegardé avec le projet), aucun CC par défaut, 14 bits sur CC n+32, Program Change 1 à 10, tous canaux"],
       ["Fenêtre", "Plugin size 60 % (minimum), 75 %, 100 %, taille par défaut des nouvelles fenêtres au choix ; anti-gel v2 sous Windows (rendu logiciel, caches d'affichage refaits par un timer, chien de garde de redessin, aucun appel à l'hôte depuis le thread audio)"],
       ["Version", VERSION]]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p")); f.append(Spacer(1, 1)); f.append(PageBreak())     # couverture : dessinee par cover_bg
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())
    f.append(P(t["ov_t"], "h1"))
    f.append(img(annotated, W * 0.84))
    f.append(Spacer(1, 3 * mm))
    f.append(table([t["ov_hdr"]] + [[str(i + 1), a, b] for i, (a, b) in enumerate(t["ov"])], [10 * mm, 40 * mm, W - 50 * mm]))
    f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([Spacer(1, 3 * mm), img(tog_img, W), P(t["tog_cap"], "small")]))
    f.append(P(t["fs_t"], "h1")); f += bullets(t["fs"])
    f.append(KeepTogether([Spacer(1, 2 * mm), img(led_img, W * 0.46), P(t["led_cap"], "small")]))
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi_txt"]]
    f.append(Spacer(1, 2 * mm)); f.append(KeepTogether([table([t["midi_hdr"]] + t["midi"], [40 * mm, W - 40 * mm])]))
    f.append(P(t["win_t"], "h1")); f.append(P(t["win"][0]))
    wt = Table([[[P(t["win"][1])], [img(bar_img, 72 * mm), P(t["bar_cap"], "small")]]], colWidths=[W - 76 * mm, 76 * mm])
    wt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    f.append(wt)
    f.append(P(t["cab_t"], "h2")); f.append(P(t["cab"]))
    f.append(CondPageBreak(205 * mm))
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    rows = [t["fac_hdr"]]
    for i, (n, ma, bl, lv, dr_, ba, lo, hi, tr, g, lf, hf, a, d) in enumerate(FACTORY):
        rows.append([str(i + 1), "<b>" + n + "</b>", f"{pc(ma)} / {pc(bl)} / {pc(lv)} / {pc(dr_)}",
                     f"{pc(ba)} / {pc(lo)} / {pc(hi)} / {pc(tr)}", t["sw"](g, lf, hf, a, d), DESC[lang][i]])
    f.append(table(rows, [8 * mm, 25 * mm, 28 * mm, 28 * mm, 33 * mm, W - 122 * mm]))
    f.append(KeepTogether([P(t["tips_t"], "h1")] + bullets(t["tips"])))
    f.append(KeepTogether([P(t["tech_t"], "h1"), table(t["tech"], [30 * mm, W - 30 * mm], header=False)]))
    f.append(Spacer(1, 4 * mm))
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Darkbass (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()
# ---- Couverture « minimale » (modele 6, valide nov. 2026) : gabarit fixe commun a toutes les notices ----
# Mesures en mm depuis le HAUT de la page A4, sauf le logo et le bandeau (depuis le bas). Aucun autre element.
COVER_NAME = "DARKBASS"
PEDAL_H, PEDAL_MAXW, PEDAL_TOP = 82 * mm, 115 * mm, 84 * mm      # pedale (boitier + jacks, sans reflet) centree horizontalement
LANG_TOP = 18 * mm                                               # « FRANÇAIS » centre, Poppins Medium 8,5 pt, accent clair
NAME_TOP, NAME_PT = 194 * mm, 30                                 # nom : Barlow Condensed ExtraBold 30 pt, approche 0,5 x corps
SUB_TOP, SUB_PT = 208 * mm, 10                                   # mention courte : Poppins 10 pt, approche 1,5 pt, gris clair
LOGO_W, LOGO_BOTTOM = 38 * mm, 22 * mm                           # logo blanc centre en bas
BAND_H = 3 * mm                                                  # bandeau couleur d'accent en BAS de page
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, A4[0], BAND_H, stroke=0, fill=1)
    def line(text, top, font, size, cs, col):
        w = pdfmetrics.stringWidth(text, font, size) + cs * (len(text) - 1)
        t = c.beginText((A4[0] - w) / 2, A4[1] - top - size * 0.8); t.setFont(font, size); t.setCharSpace(cs)
        t.setFillColor(col); t.textOut(text); c.drawText(t)
    line(T["fr"]["lang"], LANG_TOP, "PopM", 8.5, 0, ACC_L)
    ci = Image.open(cover); iw, ih = ci.size
    cw = iw / 1.28; pad = int(cw * 0.14); ch = (ih - pad) / 1.2          # geometrie de darkbass_cover_cutout.png
    k = min(PEDAL_H / ch, PEDAL_MAXW / cw)
    dw, dh = iw * k, ih * k
    y0 = A4[1] - (PEDAL_TOP + (PEDAL_H - ch * k) / 2) + pad * k            # haut de l'image (pedale centree dans sa boite)
    c.drawImage(cover, (A4[0] - dw) / 2, y0 - dh, dw, dh)
    line(COVER_NAME, NAME_TOP - NAME_PT * 0.02, "Title", NAME_PT, NAME_PT * 0.5, colors.white)
    line(T["fr"]["sub"], SUB_TOP, "Pop", SUB_PT, 1.5, colors.HexColor("#BDC4B8"))
    lw, lh = Image.open(LOGO).size
    c.drawImage(LOGO, (A4[0] - LOGO_W) / 2, LOGO_BOTTOM, LOGO_W, LOGO_W * lh / lw, mask="auto")
    c.restoreState()
doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="DARKBASS - Notice v" + VERSION, author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
fr = section("fr")              # notice en francais uniquement (couverture noire FRANCAIS)
doc.build(fr[:-1])
print(OUT)
