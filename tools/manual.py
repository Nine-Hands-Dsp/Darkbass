# DARKBASS (NINE HANDS | DSP) - notice PDF bilingue (anglais puis francais), illustree avec le rendu reel du plugin.
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
VERSION = "1.0.0"
import tempfile
TMP = tempfile.mkdtemp()
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#3F7FFF"); ACC_L = colors.HexColor("#7FA9FF")
DARK = colors.HexColor("#141518"); GREY = colors.HexColor("#63666D"); LIGHT = colors.HexColor("#E8EEFB")

# ---------- geometrie (Layout.h, repere 1240 x 928) ----------
LAY = open(os.path.join(ROOT, "Layout.h")).read()
def pt(name): return [float(v) for v in re.search(name + r" \{ ([\d.]+)f, ([\d.]+)f", LAY).groups()]
def pts(name): return [(float(a), float(b)) for a, b in re.findall(r"\{ ([\d.]+)f, ([\d.]+)f \}", LAY.split(name + "[")[1].split(";")[0])]
KP, TP = pts("knobPos"), pts("togglePos")
LEDL, LEDR, FSL, FSR = pt("ledL"), pt("ledR"), pt("fsL"), pt("fsR")
PX0, PY0, PW, PH = [float(v) for v in re.search(r'boxRect \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f', LAY).groups()]
PR, JACK_Y = 40.0, PY0 + 0.40 * PH

ASSETS = os.path.join(ROOT, "tools", "manual_assets")
NAMES = ["darkbass_annotated.png", "darkbass_toggles.png", "darkbass_leds.png", "darkbass_bar.png", "darkbass_cover_cutout.png"]
if SHOTS:
    src = Image.open(os.path.join(SHOTS, "darkbass_100.png")).convert("RGB")
    k = src.height / 928.0
    marks = [(i + 1, *KP[i]) for i in range(8)] + [(9 + i, *TP[i]) for i in range(4)] + \
            [(13, *LEDL), (14, *LEDR), (15, *FSL), (16, *FSR), (17, 620, 702), (18, 620, 788), (19, PX0 + PW + 22, JACK_Y + 78), (20, PX0 - 22, JACK_Y + 78)]
    off = {**{i: (-82, 0) for i in range(1, 9)}, **{i: (-62, 0) for i in range(9, 13)}, 13: (-48, 0), 14: (-48, 0), 15: (-88, 0), 16: (88, 0),
           17: (-262, -4), 18: (-218, 0), 19: (0, 0), 20: (0, 0)}
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
        kk = im.height / 928.0
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
    bar = crop_u("darkbass_status.png", (372, 655, 868, 818))
    bar_img = os.path.join(TMP, NAMES[3]); bar.save(bar_img)

    # ---------- couverture : pedale detouree (boitier + jacks), reflet au sol, halo bleu tres doux ----------
    shot = Image.open(os.path.join(SHOTS, "darkbass_100.png")).convert("RGB")
    kk = shot.height / 928.0
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
    ("Modern Growl",   5.15, 6.0, 5.0, 6.0, 6.0, 4.5, 6.0, 6.0, 1, 1, 0, 0, 1),
    ("Clean DI",       5.25, 0.0, 5.0, 3.0, 5.5, 5.0, 5.5, 5.5, 1, 1, 1, 1, 0),
    ("Clank & Grind",  5.65, 5.5, 5.5, 4.0, 5.0, 4.0, 6.5, 6.5, 1, 1, 0, 0, 1),
    ("Tight Djent",    6.8,  7.0, 5.0, 7.5, 5.5, 3.5, 6.0, 6.5, 2, 1, 1, 0, 1),
    ("Doom Wall",      3.65, 8.0, 5.0, 9.0, 6.5, 5.5, 5.0, 4.0, 0, 2, 2, 2, 1),
    ("Upright Warmth", 4.1,  2.0, 5.0, 2.0, 6.0, 6.0, 3.5, 3.5, 0, 2, 2, 2, 1),
    ("Slap Bright",    4.85, 1.0, 5.0, 3.0, 6.5, 3.5, 6.5, 7.0, 1, 1, 0, 0, 1),
    ("Scooped Metal",  3.45, 6.5, 5.0, 7.0, 7.0, 3.0, 4.5, 7.0, 0, 1, 1, 0, 1),
    ("Mid Punch Rock", 5.6,  4.5, 5.0, 5.0, 5.5, 6.5, 6.0, 5.0, 1, 0, 1, 1, 1),
    ("All Noon",       5.0,  5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 1, 1, 1, 1, 1)]
GA = {"en": ["boost", "flat", "cut"], "fr": ["boost", "flat", "cut"]}
LOF, HIF = ["1 kHz", "500 Hz", "250 Hz"], ["3 kHz", "1.5 kHz", "750 Hz"]
DESC = {
 "en": ["The default: drive blended over a solid clean, bright 3 kHz edge and attack boost. Modern growl that stays defined.",
        "Drive footswitch off: the preamp alone, as a clean DI with the 4-band EQ almost flat.",
        "Low drive, hi mids at 3 kHz and attack boost: the metallic clank and grind of a pick on a bright bass.",
        "High drive with grunt cut (tight lows into the clipping) and attack boost: fast, articulate riffs on low-tuned basses.",
        "Lots of drive, grunt boost, lows-mids at 250 Hz, attack cut: a thick, dark, saturated wall.",
        "Mostly clean, a hint of drive, lows up, highs down: warm, round, almost upright.",
        "Almost clean, lows and highs up, 500 Hz down: bright, percussive slap.",
        "Scooped 500 Hz, lows and highs up, grunt and attack boost: the classic modern metal bass.",
        "Lo mids up at 1 kHz: a punchy rock tone that cuts through guitars.",
        "Every knob at noon, every switch in the middle: the reference point."],
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
"en": dict(
 lang="ENGLISH", sub="Bass preamp / overdrive · double pedal format", ver="User manual · version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>Darkbass</b> is a VST3 bass preamp and overdrive in the NINE HANDS | DSP double pedal format. It recreates the "
        "Darkglass Microtubes B7K Ultra v2: a clean path at unity gain and a separate CMOS overdrive path (the « microtubes » "
        "sound), mixed with BLEND, then shaped by a 4-band EQ with switchable mid frequencies.",
        "Two footswitches, like the original: BYPASS turns the whole pedal on or off, DRIVE engages the overdrive section "
        "independently, so the clean preamp and its EQ can stay on all the time. GRUNT and ATTACK set how much low end and "
        "treble go into the clipping stage, from tight and modern to thick and dark.",
        "The visual is an original plain black design (Darkglass is not affiliated). The cabinet simulation of the v2 is "
        "available from the right-click menu (built-in IR or your own file); it is off by default because Darkbass is meant "
        "to sit before an amp or cab simulator."],
 inst_t="2. Installation",
 inst=[("Windows", "Copy <b>Darkbass.vst3</b> into <b>C:\\Program Files\\Common Files\\VST3\\</b>."),
       ("macOS", "Unzip and copy <b>Darkbass.vst3</b> into <b>~/Library/Audio/Plug-Ins/VST3/</b>. If macOS blocks it the first "
                 "time, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Darkbass.vst3</b> in Terminal."),
       ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. The plugin appears as "
                  "<b>Darkbass (NINE HANDS | DSP)</b>.")],
 ov_t="3. Interface overview",
 ov_hdr=["#", "Control", "Function"],
 ov=[("MASTER", "Overall output level"), ("BLEND", "Clean / overdrive mix"), ("LEVEL", "Overdrive path level"), ("DRIVE", "Amount of saturation"),
     ("BASS", "Low shelf, 100 Hz"), ("LO MIDS", "Low-mid bell"), ("HI MIDS", "High-mid bell"), ("TREBLE", "High shelf, 5 kHz"),
     ("GRUNT", "Lows into the clipping: boost / flat / cut"), ("LO FREQ", "LO MIDS frequency: 1 kHz / 500 / 250 Hz"),
     ("HI FREQ", "HI MIDS frequency: 3 kHz / 1.5 kHz / 750 Hz"), ("ATTACK", "Treble into the clipping: boost / flat / cut"),
     ("BYPASS LED", "Pedal on"), ("DRIVE LED", "Overdrive on"), ("BYPASS footswitch", "Whole pedal on / off"),
     ("DRIVE footswitch", "Overdrive section on / off"), ("Status line", "Live value of the control you move (appears 2.5 s over the name)"),
     ("Preset bar", "Arrows: previous / next factory preset ; centre: preset menu"), ("IN", "Input jack (right side)"), ("OUT", "Output jack (left side)")],
 ctl_t="4. Controls",
 ctl=[("MASTER", "Output level of the whole pedal. Noon = unity (0 dB), fully left = silence, fully right = +12 dB. "
                 "Use it to match the bypassed level."),
      ("BLEND", "Mix between the clean path (always at unity gain) and the overdrive path, at equal power. 0 = clean only, "
                "10 = overdrive only. Keeping some clean signal preserves the attack and the low end under heavy drive. "
                "Ignored when the DRIVE footswitch is off."),
      ("LEVEL", "Level of the overdrive path only. At noon, the overdrive is about as loud as the clean path with DRIVE at noon; "
                "it gets a little louder as DRIVE goes up, like on the pedal."),
      ("DRIVE", "Gain before the two CMOS « microtubes » stages (asymmetric, 4x oversampled): from a light grit at 0 to a dense, "
                "compressed distortion at 10."),
      ("BASS / TREBLE", "±12 dB shelving filters at 100 Hz and 5 kHz."),
      ("LO MIDS / HI MIDS", "±12 dB bell filters. LO FREQ selects 1 kHz, 500 Hz or 250 Hz, HI FREQ selects 3 kHz, 1.5 kHz or 750 Hz "
                            "(switch up / middle / down). Frequency changes glide smoothly, without clicks."),
      ("EQ curve", "All four EQ knobs follow the same continuous curve: no step or change of slope at noon, finer around noon "
                   "and faster towards the ends. The EQ acts on the mix, clean signal included, so it also shapes the clean "
                   "preamp when DRIVE is off."),
      ("GRUNT", "How much low end goes into the clipping. BOOST: +9 dB shelf at 140 Hz, a heavy, thick grind. FLAT: neutral. "
                "CUT: lows removed below about 190 Hz before clipping, a tight, modern distortion that leaves the clean lows to BLEND."),
      ("ATTACK", "How much treble goes into the clipping. BOOST: +8 dB above 2.4 kHz with extra presence, more clarity and clank. "
                 "FLAT: neutral. CUT: −9 dB, a warmer, smoother distortion.")],
 tog_cap="The four 3-position switches. Click the upper half of a switch to move it up, the lower half to move it down (or use the mouse wheel).",
 fs_t="5. Footswitches, LEDs and MIDI",
 fs=["<b>BYPASS</b> (left): turns the whole pedal on or off. Off = the input signal, delay-compensated, so nothing moves in "
     "time when you switch. Left LED lit = pedal on.",
     "<b>DRIVE</b> (right): turns the overdrive path on or off. Off = clean preamp and EQ only. Right LED lit = overdrive on "
     "(and the pedal on).",
     "Click = on / off. Holding a footswitch for more than 0.45 s when its section was off makes it momentary: it turns off "
     "when you release it (can be disabled: right-click &gt; Settings).",
     "LEDs fade in and out; the faceted blue lens lights the black panel and the footswitch nut around it."],
 led_cap="Left: bypassed (both LEDs off). Right: pedal and overdrive on.",
 midi_t="6. MIDI and expression pedals",
 midi_txt=["No CC is assigned by default. Right-click any control (knob, switch, footswitch) &gt; <b>MIDI Learn</b>, then move the "
           "pedal or press the switch on your controller: the first CC received is assigned (shown 2 s in the status line). "
           "Assignments are saved with the project. All controls are also automatable from the DAW."],
 midi_hdr=["Control", "Behaviour"],
 midi=[["Knobs", "Linear ramp between two CC messages, then 2-pole smoothing: no zipper noise with an expression pedal. "
                 "The pedal travel is learned automatically (exact 0 and 100 % even if it only sends 2 to 116), dead zones at both ends. "
                 "14-bit CC accepted (CC n + CC n+32 for CC 0-31)."],
       ["3-position switches", "CC 86-127 = up, 43-85 = middle, 0-42 = down."],
       ["Footswitches", "Latching (127 = on / 0 = off) or momentary (each press toggles): right-click &gt; Settings."],
       ["Cab sim", "CC ≥ 64 = on."],
       ["Program Change", "1 to 10 = factory presets."]],
 win_t="7. Window",
 win=["The window opens at the « 60 % » size, which is also the minimum: it can be enlarged by dragging the corner, never made "
      "smaller. Right-click &gt; <b>Plugin size</b>: 60 %, 75 %, 100 %, and <b>Default size (new windows)</b> to choose the size "
      "of every new Darkbass window.",
      "The <b>status line</b> shows the live value of the control you move (in %, or the switch position), and the MIDI Learn "
      "messages. The <b>preset bar</b> shows the current preset: arrows for previous / next, centre for the full menu "
      "(factory presets, 9 user slots to load and save)."],
 bar_cap="Status line (top) and preset bar.",
 cab_t="Cab sim",
 cab="Right-click &gt; <b>Cab sim</b>: on / off, <b>Built-in IR</b> (bass 4x10 cabinet) or <b>Load IR file...</b> (WAV / AIFF, "
     "normalised, the path is saved with the project). The level is matched so that switching the cab sim does not jump in volume.",
 pre_t="8. Presets",
 pre=["Ten factory presets, recalled from the preset bar, the right-click menu, the DAW program list or MIDI Program Change. "
      "Factory presets are levelled to the same loudness. Your own settings can be saved in 9 user slots."],
 fac_hdr=["#", "Preset", "Master / Blend / Level / Drive", "Bass / Lo / Hi / Treble", "Switches", "Description"],
 sw=lambda g, lo, hi, a, d: f"grunt {GA['en'][g]}, {LOF[lo]}, {HIF[hi]}, attack {GA['en'][a]}" + ("" if d else ", <b>drive off</b>"),
 tips_t="9. Tips",
 tips=["In Reaper, put Darkbass on the bass track before your amp or cab simulator (for example Two Notes Genome), cab sim off.",
       "Start with DRIVE at noon, BLEND around 5 and GRUNT on CUT for low-tuned basses: the clean path keeps the fundamental, "
       "the overdrive adds growl on top.",
       "For direct recording without an amp simulator, turn the cab sim on.",
       "Use the DRIVE footswitch (or MIDI) to switch between a clean preamp and the overdrive without changing the EQ.",
       "Map an expression pedal to BLEND or DRIVE with MIDI Learn for smooth live changes."],
 tech_t="10. Technical specifications",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal)"], ["Channels", "Mono or stereo"],
       ["Latency", "61 samples (4x linear-phase oversampling), reported to the DAW; clean and bypass paths are delay-compensated"],
       ["Overdrive", "Two asymmetric CMOS stages, 4x oversampled, gain +2 to +46 dB, GRUNT / ATTACK pre-filters"],
       ["EQ", "Bass 100 Hz, Lo mids 250 / 500 / 1k, Hi mids 750 / 1.5k / 3k, Treble 5 kHz, ±12 dB each"],
       ["Cab sim", "Built-in bass 4x10 IR or user WAV / AIFF (zero-latency convolution)"],
       ["MIDI", "MIDI Learn on all 15 controls (saved with the project), no CC by default, 14-bit on CC n+32, Program Change 1-10, all channels"],
       ["Window", "Plugin size 60 % (minimum), 75 %, 100 %, default size for new windows; anti-freeze v2 on Windows (software renderer, display caches rebuilt by a timer, repaint watchdog, no host call from the audio thread)"],
       ["Version", VERSION]]),
"fr": dict(
 lang="FRANÇAIS", sub="Préampli basse / overdrive · format pédale double", ver="Notice d'utilisation · version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>Darkbass</b> est un préampli basse avec saturation au format VST3, en pédale double NINE HANDS | DSP. Il reproduit la "
        "Darkglass Microtubes B7K Ultra v2 : une voie claire à gain unitaire et une voie saturée CMOS séparée (le son "
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
     ("GRUNT", "Graves dans l'écrêtage : boost / flat / cut"), ("LO FREQ", "Fréquence de LO MIDS : 1 kHz / 500 / 250 Hz"),
     ("HI FREQ", "Fréquence de HI MIDS : 3 kHz / 1,5 kHz / 750 Hz"), ("ATTACK", "Aigus dans l'écrêtage : boost / flat / cut"),
     ("LED BYPASS", "Pédale active"), ("LED DRIVE", "Saturation active"), ("Footswitch BYPASS", "Toute la pédale on / off"),
     ("Footswitch DRIVE", "Voie saturée on / off"), ("Ligne d'état", "Valeur en direct de la commande manipulée (2,5 s par-dessus le nom)"),
     ("Barre de presets", "Flèches : preset d'usine précédent / suivant ; centre : menu des presets"), ("IN", "Entrée (côté droit)"), ("OUT", "Sortie (côté gauche)")],
 ctl_t="4. Les commandes",
 ctl=[("MASTER", "Volume de sortie de toute la pédale. Midi = gain unitaire (0 dB), tout à gauche = silence, tout à droite = +12 dB. "
                 "Permet d'égaliser avec le niveau en bypass."),
      ("BLEND", "Mélange entre la voie claire (toujours à gain unitaire) et la voie saturée, à puissance égale. 0 = clair seul, "
                "10 = saturé seul. Garder un peu de clair conserve l'attaque et les graves sous une forte saturation. "
                "Sans effet quand le footswitch DRIVE est coupé."),
      ("LEVEL", "Volume de la voie saturée seule. À midi, la saturation sort à peu près au même volume que la voie claire avec "
                "DRIVE à midi ; elle monte un peu quand on tourne DRIVE, comme sur la pédale."),
      ("DRIVE", "Gain avant les deux étages CMOS « microtubes » (asymétriques, sur-échantillonnés x4) : d'un léger grain à 0 "
                "jusqu'à une distorsion dense et compressée à 10."),
      ("BASS / TREBLE", "Filtres en plateau ±12 dB à 100 Hz et 5 kHz."),
      ("LO MIDS / HI MIDS", "Filtres en cloche ±12 dB. LO FREQ choisit 1 kHz, 500 Hz ou 250 Hz, HI FREQ choisit 3 kHz, 1,5 kHz ou "
                            "750 Hz (inverseur haut / milieu / bas). Le changement de fréquence glisse en douceur, sans clic."),
      ("Courbe de l'EQ", "Les quatre potards d'EQ suivent la même courbe continue : aucune marche ni changement de pente à midi, "
                          "réglage fin autour de midi, plus rapide vers les butées. L'EQ agit sur le mélange, clair compris : il "
                          "règle aussi le préampli clair quand DRIVE est coupé."),
      ("GRUNT", "Quantité de graves envoyée dans l'écrêtage. BOOST : plateau +9 dB à 140 Hz, grain lourd et épais. FLAT : neutre. "
                "CUT : graves retirés sous 190 Hz environ avant l'écrêtage, distorsion serrée et moderne qui laisse les graves "
                "propres à BLEND."),
      ("ATTACK", "Quantité d'aigus envoyée dans l'écrêtage. BOOST : +8 dB au-dessus de 2,4 kHz avec plus de présence, plus de "
                 "clarté et de « clank ». FLAT : neutre. CUT : −9 dB, distorsion plus chaude et plus douce.")],
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
           "(affiché 2 s dans la ligne d'état). Les assignations sont sauvegardées avec le projet. Toutes les commandes sont "
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
      "choisir la taille de toutes les nouvelles fenêtres de Darkbass.",
      "La <b>ligne d'état</b> affiche la valeur en direct de la commande manipulée (en %, ou la position de l'inverseur), et les "
      "messages du MIDI Learn. La <b>barre de presets</b> affiche le preset en cours : flèches pour précédent / suivant, centre "
      "pour le menu complet (presets d'usine, 9 emplacements utilisateur à charger et sauvegarder)."],
 bar_cap="Ligne d'état (en haut) et barre de presets.",
 cab_t="Simulation de baffle",
 cab="Clic droit &gt; <b>Cab sim</b> : on / off, <b>Built-in IR</b> (baffle basse 4x10 intégré) ou <b>Load IR file...</b> "
     "(WAV / AIFF, normalisé, le chemin est sauvegardé avec le projet). Le volume est compensé : activer la simulation ne "
     "fait pas de saut de volume.",
 pre_t="8. Presets",
 pre=["Dix presets d'usine, rappelés depuis la barre de presets, le menu clic droit, la liste des programmes du DAW ou un "
      "Program Change MIDI. Ils sont tous au même volume. Vos propres réglages se sauvegardent dans 9 emplacements utilisateur."],
 fac_hdr=["N°", "Preset", "Master / Blend / Level / Drive", "Bass / Lo / Hi / Treble", "Inverseurs", "Description"],
 sw=lambda g, lo, hi, a, d: f"grunt {GA['fr'][g]}, {LOF[lo].replace('.', ',')}, {HIF[hi].replace('.', ',')}, attack {GA['fr'][a]}" + ("" if d else ", <b>drive coupé</b>"),
 tips_t="9. Conseils d'utilisation",
 tips=["Dans Reaper, placer Darkbass sur la piste basse avant le simulateur d'ampli ou de baffle (par exemple Two Notes Genome), "
       "cab sim coupé.",
       "Pour une basse accordée bas, partir de DRIVE à midi, BLEND vers 5 et GRUNT sur CUT : la voie claire garde la "
       "fondamentale, la saturation ajoute le growl par-dessus.",
       "Pour enregistrer en direct sans simulateur d'ampli, activer le cab sim.",
       "Le footswitch DRIVE (ou le MIDI) passe du préampli clair à la saturation sans changer l'EQ.",
       "Assigner une pédale d'expression à BLEND ou DRIVE avec le MIDI Learn pour des variations progressives en live."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Canaux", "Mono ou stéréo"],
       ["Latence", "61 échantillons (sur-échantillonnage x4 à phase linéaire), annoncée au DAW ; voies claire et bypass compensées"],
       ["Saturation", "Deux étages CMOS asymétriques, sur-échantillonnage x4, gain +2 à +46 dB, préfiltres GRUNT / ATTACK"],
       ["EQ", "Bass 100 Hz, Lo mids 250 / 500 / 1k, Hi mids 750 / 1,5k / 3k, Treble 5 kHz, ±12 dB chacun"],
       ["Cab sim", "IR basse 4x10 intégrée ou fichier WAV / AIFF (convolution sans latence)"],
       ["MIDI", "MIDI Learn sur les 15 commandes (sauvegardé avec le projet), aucun CC par défaut, 14 bits sur CC n+32, Program Change 1 à 10, tous canaux"],
       ["Fenêtre", "Plugin size 60 % (minimum), 75 %, 100 %, taille par défaut des nouvelles fenêtres au choix ; anti-gel v2 sous Windows (rendu logiciel, caches d'affichage refaits par un timer, chien de garde de redessin, aucun appel à l'hôte depuis le thread audio)"],
       ["Version", VERSION]]),
}

W = A4[0] - 36 * mm
def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p"))
    f.append(Spacer(1, 2 * mm)); f.append(P(t["lang"], "coverlang")); f.append(Spacer(1, 4 * mm))
    f.append(img(cover, W * 0.80)); f.append(Spacer(1, 3 * mm))
    f.append(P("D A R K B A S S", "cover")); f.append(P(t["sub"], "coversub")); f.append(Spacer(1, 2 * mm))
    f.append(P(t["ver"], "coversub")); f.append(Spacer(1, 10 * mm))
    f.append(img(LOGO, 56 * mm))
    f.append(PageBreak())
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
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()
doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="DARKBASS - Manual / Notice v" + VERSION, author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
en = section("en"); fr = section("fr")
en[-1:] = [NextPageTemplate("cover"), PageBreak()]
doc.build(en + fr[:-1])
print(OUT)
