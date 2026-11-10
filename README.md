# Darkbass (NINE HANDS | DSP) — VST3

Préampli basse + saturation au **format pédale double**, d'après la Darkglass Microtubes B7K Ultra v2 (avec entrée Aux).
Disposition des commandes reprise de la pédale d'origine, habillage noir uni original, logo NINE HANDS | DSP blanc en bas.
Usage personnel. Windows 11 et macOS (binaire universel).

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/Darkbass/releases/latest/download/Darkbass-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/Darkbass/releases/latest/download/Darkbass-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Darkbass/releases/latest/download/Darkbass-Notice.pdf"><img alt="Télécharger la notice (PDF)" src="https://img.shields.io/badge/%F0%9F%93%96%20Notice-PDF-919496?style=for-the-badge"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Darkbass/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/Darkbass?label=derni%C3%A8re%20version&color=919496"></a>
</p>

## Installation via GitHub

1. Le push sur `main` lance la compilation (onglet **Actions**) : Windows (`windows-2022`) et macOS.
2. Télécharger l'artefact :
   - **Windows** : copier `Darkbass.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `Darkbass.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
     (premier lancement : `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Darkbass.vst3` si macOS le bloque)
3. Relancer l'analyse des plugins. Le plugin apparaît comme **Darkbass (NINE HANDS | DSP)**.

## Moteur audio (v1.1)

Modèle du circuit de la B7K Ultra d'après le manuel Darkglass et l'analyse de circuit open source
[Obsidian-B7000](https://github.com/tehguitarist/Obsidian-B7000) (valeurs des composants et mesures sur captures) :
buffer → voie claire unitaire ; voie saturée : passe-haut C2, JFET J201, réseau ATTACK / échelle d'aigus, C7, ampli op DRIVE
×4–×78 (écrêtage sur rails), condensateur GRUNT, un inverseur CD4049UBE en contre-réaction (gain fini ~25), C15, T ponté
(creux ~717 Hz), Sallen-Key 10,7 kHz + 3,3 kHz, LEVEL (courbe A) → BLEND (fondu linéaire) → C21 → EQ (Baxandall + cloches) → MASTER.

## Commandes

| Commande | Rôle |
|---|---|
| MASTER | Volume général (midi = 0 dB, 0 = coupé, 10 = +12 dB) |
| BLEND | Fondu linéaire voie claire / voie saturée, comme le potentiomètre de la pédale (0 = clair seul, 10 = saturé seul) |
| LEVEL | Volume de la voie saturée, loi logarithmique (midi ≈ volume de la voie claire avec DRIVE à midi) |
| DRIVE | Gain de l'ampli op avant l'écrêteur, ×4 à ×78 ; chaîne : JFET J201 → ampli op → un inverseur CMOS CD4049UBE (x4) |
| BASS | ±12 dB, plateau à 100 Hz |
| LO MIDS + LO FREQ | ±12 dB, cloche à 500 Hz / 1 kHz / 250 Hz (inverseur haut / milieu / bas, ordre de la pédale) |
| HI MIDS + HI FREQ | ±12 dB, cloche à 1,5 kHz / 3 kHz / 750 Hz (inverseur haut / milieu / bas, ordre de la pédale) |
| TREBLE | ±12 dB, plateau à 5 kHz |
| GRUNT (boost / cut / flat, haut → bas) | Trois niveaux de graves dans l'écrêteur (condensateur d'entrée de l'inverseur) : le plus / moyen / le moins |
| ATTACK (flat / boost / cut, haut → bas) | Signal et aigus dans la saturation : neutre / +8,7 dB + aigus / −2,4 dB − aigus |
| Footswitch BYPASS + LED gauche | Pédale entière on / off (signal sec aligné, aucun saut de phase) |
| Footswitch DRIVE + LED droite | Voie saturée on / off ; coupée = voie claire seule dans l'EQ (BLEND ignoré) |

L'EQ agit sur le mélange, clair compris, comme sur la pédale d'origine. Courbe des potards d'EQ continue, même pente de
part et d'autre de midi (aucune cassure), qui s'accélère vers les butées.

Barre de presets et ligne d'état dans la même pastille : le preset en cours au repos ; dès qu'un réglage bouge, la valeur
en direct prend sa place, puis le preset revient en fondu 2,5 s après le dernier mouvement.

Clic droit (menu au design d'Octopus, ouvert à l'endroit du clic) : taille de la fenêtre (60 / 75 / 100 %, taille par défaut des nouvelles fenêtres), presets d'usine et
utilisateur (9 emplacements), **Cab sim** (IR de baffle basse 4x10 intégrée ou fichier WAV / AIFF de l'utilisateur ;
désactivé par défaut, Darkbass étant placé avant Ampire / Impulse), réglages des footswitchs, MIDI Learn.

## MIDI

- Aucun CC assigné par défaut : tout passe par le **MIDI Learn** (clic droit sur une commande).
- Potards : rampe linéaire entre deux CC puis lissage 2 pôles dans le DSP ; course de la pédale d'expression apprise
  automatiquement (0 et 100 % exacts même si elle n'envoie que 2-116) ; CC 14 bits (CC n + n+32) accepté.
- Inverseurs : CC 86-127 = haut, 43-85 = milieu, 0-42 = bas (positions dans l'ordre de la vraie pédale). Cab sim : CC ≥ 64 = on.
- Footswitchs : à accrochage (127 / 0) ou momentanés (chaque appui bascule), au choix.
- Program Change 1-10 : presets d'usine.

## Anti-gel v2

JUCE 8.0.15, rendu logiciel forcé sous Windows, aucun calcul lourd dans `paint()` (caches reconstruits par le timer),
chien de garde de redessin, jamais de `setSize` dans `resized()`, aucun appel à l'hôte depuis le thread audio (file
vidée à 60 Hz sur le thread de messages), pointeurs de paramètres mis en cache, aucune allocation ni verrou dans
`processBlock`.

## Fichiers

- `DarkbassDSP.h` : moteur audio. `PluginProcessor.*` : paramètres, presets, MIDI. `PluginEditor.*` : fenêtre.
- `tools/make_art.py` : visuel complet (`background.jpg`, `toggle_strip.png`, `glow_*.png`, `led_on.png`,
  `fs_refl.png`, `Layout.h`). `tools/preview.py` : aperçu de la façade.
- `tools/toggle_photo.py`, `tools/photo_controls.py` : inverseurs, potards, footswitchs, LED et jacks d'après les photos
  de l'utilisateur (`assets/kit/`).
- `tools/render/` : compile le plugin sous Linux, tests audio (niveaux, presets, alignement, MIDI) et captures PNG.
- `docs/` : notice en français (`Darkbass_Notice_FR_v1.1.4.pdf`).
