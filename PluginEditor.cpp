#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    // repere layout::frameW x frameH (fond 2x), positions generees par tools/make_art.py dans Layout.h
    const Rectangle<float> presetRect = layout::presetRect;

    const float rotStart = MathConstants<float>::pi * (7.0f / 6.0f);    // -150 degres
    const float rotEnd   = MathConstants<float>::pi * (17.0f / 6.0f);   // +150 degres

    const Colour inkWhite (0xffecece8);
    const Colour accent (0xff3f7fff);       // bleu des LED

    // valeurs suivies pour la ligne d'etat : 8 potards, 4 inverseurs, cab, active, driveOn
    const char* const valueIds[] = { "master", "blend", "level", "drive", "bass", "lomids", "himids", "treble",
                                     "grunt", "loFreq", "hiFreq", "attack", "cab", "active", "driveOn" };
    constexpr int numValueIds = 15;
}

//==============================================================================
// POTARDS : repere blanc arrondi, comme sur la photo (de 0,42 a 0,90 du rayon, largeur 0,058 du rayon)
//==============================================================================
bool DarkKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
}

void DarkKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                            float pos, float startAngle, float endAngle, Slider&)
{
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) jmin (w, h) * 0.5f;
    const float a = startAngle + pos * (endAngle - startAngle);
    const Point<float> dir (std::sin (a), -std::cos (a));
    const Point<float> p0 = c + dir * (0.42f * R), p1 = c + dir * (0.90f * R);
    const float lw = jmax (1.4f, 0.058f * R);
    Path line; line.startNewSubPath (p0); line.lineTo (p1);
    // ombre tres fine (le repere est peint, a peine en relief), lumiere en haut a gauche
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (line, PathStrokeType (lw * 1.15f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (R * 0.010f, R * 0.014f));
    const float lightFacing = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.25f);
    g.setColour (inkWhite.withMultipliedBrightness (0.94f + 0.06f * lightFacing));
    g.strokePath (line, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
// INVERSEUR 3 POSITIONS
//==============================================================================
Toggle3::Toggle3 (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const int np = jlimit (0, 2, roundToInt (v));
                      if (np != pos)
                      {
                          if (std::abs (np - pos) == 2)                      // haut <-> bas : passe par le milieu ~55 ms
                          {
                              shownFrame = 1;
                              transitionUntil = Time::getMillisecondCounterHiRes() + 55.0;
                          }
                          else shownFrame = np;
                          pos = np;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = pos;
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Toggle3::tick (double now)
{
    if (shownFrame != pos && now >= transitionUntil) { shownFrame = pos; repaint(); }
}

void Toggle3::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, shownFrame * fs, fs, fs);
}

bool Toggle3::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return std::abs ((float) x - c.x) < getWidth() * 0.26f && std::abs ((float) y - c.y) < getHeight() * 0.40f;
}

void Toggle3::moveTo (int idx)
{
    idx = jlimit (0, 2, idx);
    if (idx == pos) return;
    attachment.setValueAsCompleteGesture ((float) idx);
    if (onUserChange) onUserChange();
}

void Toggle3::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    // moitie haute : monter d'un cran ; moitie basse : descendre d'un cran
    moveTo (e.position.y < getHeight() * 0.5f ? pos - 1 : pos + 1);
}

void Toggle3::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) <= 0.0f) return;
    moveTo (w.deltaY > 0 ? pos - 1 : pos + 1);
}

//==============================================================================
// FOOTSWITCH
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p, std::atomic<float>* holdMomentary)
    : hold (holdMomentary), attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    // zone d'appui : le bouton et l'ecrou (le sprite contient aussi l'ombre)
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * (layout::footR * 1.35f / layout::fsS);
}

void FootSwitchArea::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (ledLevel > 0.003f && reflection.isValid())
    {
        g.setOpacity (ledLevel);
        g.drawImage (reflection, b);
        g.setOpacity (1.0f);
    }
    if (pressed)
    {
        // bouton assombri tant qu'on appuie (comme Echoflex)
        const float r = b.getWidth() * (layout::footR / layout::fsS) * 0.98f;
        const auto c = b.getCentre();
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    pressed = true;
    downTime = Time::getMillisecondCounterHiRes();
    turnedOnByPress = ! value;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
    if (onUserChange) onUserChange();
}

void FootSwitchArea::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const bool momentary = hold != nullptr && hold->load() > 0.5f;
    if (pressed && momentary && turnedOnByPress && Time::getMillisecondCounterHiRes() - downTime > 450.0 && value)
    {
        attachment.setValueAsCompleteGesture (0.0f);
        if (onUserChange) onUserChange();
    }
    pressed = false;
    repaint();
}

//==============================================================================
// EDITEUR
//==============================================================================
DarkbassAudioProcessorEditor::DarkbassAudioProcessorEditor (DarkbassAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      bypassFs (*p.apvts.getParameter ("active"),  nullptr),
      driveFs  (*p.apvts.getParameter ("driveOn"), nullptr)
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    glow[0]    = ImageCache::getFromMemory (BinaryData::glow_l_png, BinaryData::glow_l_pngSize);
    glow[1]    = ImageCache::getFromMemory (BinaryData::glow_r_png, BinaryData::glow_r_pngSize);
    ledOn      = ImageCache::getFromMemory (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    const auto refl  = ImageCache::getFromMemory (BinaryData::fs_refl_png, BinaryData::fs_refl_pngSize);
    const auto strip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    bypassFs.reflection = driveFs.reflection = refl;
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    lnf.f = { labelFace, labelFace, Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize) };
    glowPc = NineHandsGlow::percent (JucePlugin_Name);

    // index des commandes = learnDef du processeur (0-7 potards, 8-11 inverseurs, 12 cab, 13 bypass, 14 drive)
    rightClickFwd.callback = [this] (Component* c)
    {
        int idx = -1;
        for (int i = 0; i < DarkbassAudioProcessor::numKnobs; ++i)
            if (c == &knobs[i]) idx = i;
        showContextMenu (idx);
    };

    for (int i = 0; i < DarkbassAudioProcessor::numKnobs; ++i)
    {
        auto& s = knobs[i];
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (rotStart, rotEnd, true);
        s.setMouseDragSensitivity (220);
        s.setDoubleClickReturnValue (true, 5.0);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        s.onDragStart = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (s);
        knobAtt[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, DarkbassAudioProcessor::knobId (i), s);
    }

    for (int t = 0; t < DarkbassAudioProcessor::numToggles; ++t)
    {
        toggles[t] = std::make_unique<Toggle3> (*proc.apvts.getParameter (DarkbassAudioProcessor::toggleId (t)));
        toggles[t]->strip = strip;
        toggles[t]->onRightClick = [this, t] { showContextMenu (8 + t); };
        toggles[t]->onUserChange = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (*toggles[t]);
    }

    bypassFs.onRightClick = [this] { showContextMenu (13); };
    driveFs.onRightClick  = [this] { showContextMenu (14); };
    for (auto* f : { &bypassFs, &driveFs })
    {
        f->onUserChange = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (*f);
    }

    for (int i = 0; i < numValueIds; ++i) lastValues[i] = proc.raw (valueIds[i]);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();
    ledLevel[0] = proc.raw ("active") > 0.5f ? 1.0f : 0.0f;
    ledLevel[1] = (proc.raw ("active") > 0.5f && proc.raw ("driveOn") > 0.5f) ? 1.0f : 0.0f;

    // taille par defaut = taille minimale (comme Ring Think), proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    hostLock.setLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    // taille des nouvelles fenetres : celle choisie dans le menu « Default size (new windows) »
    setScaledHeight (sizeHeight (NineHandsWindow::openIndex (JucePlugin_Name, proc.uiSizeIdx.load(), numSizes)));   // taille du projet, sinon taille par defaut
    rebuildCaches();
    startTimerHz (30);
}

DarkbassAudioProcessorEditor::~DarkbassAudioProcessorEditor()
{
    stopTimer();
    for (auto& s : knobs) { s.removeMouseListener (&rightClickFwd); s.setLookAndFeel (nullptr); }
}

//==============================================================================
int DarkbassAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, roundToInt (refH * 0.75f), (int) refH };
    return h[jlimit (0, numSizes - 1, idx)];
}

void DarkbassAudioProcessorEditor::setScaledHeight (int h)
{
    h = jlimit ((int) defaultH, (int) maxH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
    hostLock.enforce();
}

Rectangle<int> DarkbassAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float DarkbassAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void DarkbassAudioProcessorEditor::showStatus() { statusUntil = Time::getMillisecondCounterHiRes() + 2500.0; }

// caches : fond et calques lumineux a la taille de la fenetre (reconstruits par le timer seulement)
void DarkbassAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;
    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);
    const float s = (float) h / (float) baseH;
    const Rectangle<float> gr[2] = { layout::glowL, layout::glowR };
    for (int i = 0; i < 2; ++i)
    {
        const int gpx = roundToInt (gr[i].getWidth() * s * sc);
        glowS[i] = (gpx >= glow[i].getWidth()) ? glow[i] : glow[i].rescaled (gpx, gpx, Graphics::highResamplingQuality);
    }
    repaint();
}

void DarkbassAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici ; les limites du constrainer suffisent
    for (int i = 0; i < DarkbassAudioProcessor::numKnobs; ++i)
    {
        const auto c = layout::knobPos[i]; const float r = layout::knobR;
        knobs[i].setBounds (scaled ({ c.x - r, c.y - r, r * 2.0f, r * 2.0f }));
    }
    const float ts = layout::toggleS * 0.5f;
    for (int t = 0; t < DarkbassAudioProcessor::numToggles; ++t)
    {
        const auto c = layout::togglePos[t];
        toggles[t]->setBounds (scaled ({ c.x - ts, c.y - ts, ts * 2.0f, ts * 2.0f }));
    }
    const float fr = layout::fsS * 0.5f;
    bypassFs.setBounds (scaled ({ layout::fsL.x - fr, layout::fsL.y - fr, fr * 2.0f, fr * 2.0f }));
    driveFs.setBounds  (scaled ({ layout::fsR.x - fr, layout::fsR.y - fr, fr * 2.0f, fr * 2.0f }));
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (NINE HANDS | DSP) : rendu logiciel force sous Windows, aucun calcul lourd dans paint(),
// caches reconstruits par le timer, chien de garde de redessin ; cote audio, aucun appel a l'hote.
//==============================================================================
void DarkbassAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void DarkbassAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); hostLock.track (*this); }
void DarkbassAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void DarkbassAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String DarkbassAudioProcessorEditor::buildStatusLine() const
{
    const int i = lastChanged;
    if (i < 0) return {};
    const float v = proc.raw (valueIds[i]);
    if (i < 8)  return String (DarkbassAudioProcessor::knobName (i)) + "  " + String (roundToInt (v * 10.0f)) + " %";
    if (i < 12)
    {
        const char* names[4] = { "grunt", "lo mids", "hi mids", "attack" };
        return String (names[i - 8]) + "  " + DarkbassAudioProcessor::toggleChoices (i - 8)[jlimit (0, 2, roundToInt (v))].toLowerCase();
    }
    if (i == 12) return v > 0.5f ? "cab sim on" : "cab sim off";
    if (i == 13) return v > 0.5f ? "darkbass on" : "bypass";
    return v > 0.5f ? "drive on" : "drive off";
}

String DarkbassAudioProcessorEditor::buildPresetLine() const
{
    const int cp = proc.currentPreset.load();
    String s;
    if (cp >= 0 && cp < DarkbassAudioProcessor::getNumFactoryPresets())
        s << "P" << (cp + 1) << "  " << DarkbassAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100) s << "User " << (cp - 100);
    else               s << "Edited";
    return s;
}

//==============================================================================
void DarkbassAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }
    if (now - lastGlowReadMs > 3000.0)       // clic droit > Glow intensity : commun a toutes les fenetres (jamais dans paint)
    {
        lastGlowReadMs = now;
        const int pc = NineHandsGlow::percent (JucePlugin_Name);
        if (pc != glowPc) setGlowPercent (pc);
    }
    hostLock.track (*this);

    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0; lastPeer = nullptr;
        useSoftwareRenderer(); repaint();
    }

    for (auto& t : toggles) t->tick (now);

    int changed = -1, numChanged = 0;
    for (int i = 0; i < numValueIds; ++i)
    {
        const float v = proc.raw (valueIds[i]);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pcount = proc.presetCounter.load();
    if (pcount != lastPresetCounter) { lastPresetCounter = pcount; numChanged = 0; }
    if (numChanged == 1) { lastChanged = changed; showStatus(); learnLineUntil = 0.0; }
    for (int i = 0; i < DarkbassAudioProcessor::numKnobs; ++i)
        if (knobs[i].isMouseButtonDown()) { lastChanged = i; showStatus(); learnLineUntil = 0.0; }
    // MIDI Learn : attente puis confirmation 2 s dans la ligne d'etat
    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnLine = String (DarkbassAudioProcessor::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load());
        learnLineUntil = now + 2000.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
    }
    else if (lt >= 0)
    {
        learnLine = String (CharPointer_UTF8 ("MIDI LEARN \xc2\xb7 ")) + String (DarkbassAudioProcessor::learnDef (lt).name);
        learnLineUntil = now + 200.0;
        statusUntil = jmax (statusUntil, now + 200.0);
    }
    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const float target = now < statusUntil ? 1.0f : 0.0f;
    // barre de presets et ligne d'etat dans la meme pastille : fondu croise ~120 ms a l'apparition, ~300 ms au retour
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.62f : 0.28f);
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    // LED bleues : BYPASS (gauche, pedale active), DRIVE (droite) ; fondu 0,45 a l'allumage / 0,28 a l'extinction
    const bool active = proc.raw ("active") > 0.5f;
    const bool on[2] = { active, active && proc.raw ("driveOn") > 0.5f };
    const Rectangle<float> gr[2] = { layout::glowL, layout::glowR };
    FootSwitchArea* fs[2] = { &bypassFs, &driveFs };
    for (int i = 0; i < 2; ++i)
    {
        auto& l = ledLevel[i];
        l += ((on[i] ? 1.0f : 0.0f) - l) * (on[i] ? 0.45f : 0.28f);
        if (l < 0.003f) l = 0.0f;
        if (l > 0.997f) l = 1.0f;
        if (std::abs (l - drawnLed[i]) > 0.004f)
        {
            drawnLed[i] = l;
            fs[i]->ledLevel = l;
            askRepaint (scaled (gr[i]));
            fs[i]->repaint();
        }
    }

    const auto l1 = (learnLineUntil > 0.0 && now < learnLineUntil + 800.0 ? learnLine : buildStatusLine()), pl = buildPresetLine();
    const bool textChanged = l1 != lastLine1;
    lastLine1 = l1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
}

//==============================================================================
void DarkbassAudioProcessorEditor::drawLed (Graphics& g, int which)
{
    const float level = ledLevel[which];
    if (level <= 0.003f) return;
    // facade eclairee par la LED (calque alpha precalcule : seule la lumiere ajoutee)
    const auto gr = which == 0 ? layout::glowL : layout::glowR;
    if (glowS[which].isValid()) { g.setOpacity (level * glowAmt()); g.drawImage (glowS[which], gr); }   // halo : clic droit > Glow intensity
    // lentille allumee (photo de LED recoloree en bleu)
    const auto c = which == 0 ? layout::ledL : layout::ledR;
    const float hs = layout::ledS * 0.5f;
    if (ledOn.isValid()) { g.setOpacity (level); g.drawImage (ledOn, { c.x - hs, c.y - hs, hs * 2.0f, hs * 2.0f }); }
    g.setOpacity (1.0f);
    // tres leger halo optique autour de la lentille
    const float hr = 22.0f;
    ColourGradient halo (Colour (0xff5a9bff).withAlpha (0.33f * level * glowAmt()), c.x, c.y, Colour (0xff5a9bff).withAlpha (0.0f), c.x + hr, c.y, true);
    halo.addColour (0.4, Colour (0xff5a9bff).withAlpha (0.088f * level * glowAmt()));
    g.setGradientFill (halo);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);
}

// Une seule pastille : au repos la barre de presets ; des qu'un reglage bouge, la ligne d'etat prend sa place
// (fondu croise, meme police, meme taille), puis s'efface ~2,5 s apres le dernier mouvement.
void DarkbassAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f) return;
    g.setFont (Font (FontOptions (labelFace).withHeight (layout::presetFont)));
    g.setColour (inkWhite.withAlpha (a));
    g.drawFittedText (lastLine1.toUpperCase(), presetRect.reduced (16.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
}

void DarkbassAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    const float pa = 1.0f - statusAlpha;                       // contenu de la barre de presets (fondu croise)
    const int hz = statusAlpha > 0.05f ? 0 : hoverZone;        // pas de survol tant que la ligne d'etat est affichee
    g.setColour (Colour (0xff0a0a0c).withAlpha (0.86f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (hz != 0 ? 0.80f : 0.50f));
    g.drawRoundedRectangle (r.reduced (0.6f), r.getHeight() * 0.5f, 1.2f);
    if (pa <= 0.0f) return;
    const float aw = 36.0f;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p; const auto c = ar.getCentre(); const float sz = 6.5f;
        if (left) p.addTriangle (c.x + sz * 0.6f, c.y - sz, c.x + sz * 0.6f, c.y + sz, c.x - sz * 0.8f, c.y);
        else      p.addTriangle (c.x - sz * 0.6f, c.y - sz, c.x - sz * 0.6f, c.y + sz, c.x + sz * 0.8f, c.y);
        g.setColour (inkWhite.withAlpha ((hot ? 1.0f : 0.62f) * pa));
        g.fillPath (p);
    };
    arrow (r.withWidth (aw), true, hz == 1);
    arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hz == 3);
    g.setFont (Font (FontOptions (labelFace).withHeight (layout::presetFont)));
    g.setColour ((midiFlash > 0 ? Colour (0xff8fb8ff) : inkWhite).withAlpha ((hz == 2 ? 1.0f : 0.90f) * pa));
    g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void DarkbassAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    drawLed (g, 0);
    drawLed (g, 1);
    drawPresetBar (g);
    drawStatus (g);
}

//==============================================================================
static int presetZoneAt (Point<float> p)
{
    if (! presetRect.contains (p)) return 0;
    if (p.x < presetRect.getX() + 36.0f) return 1;
    if (p.x > presetRect.getRight() - 36.0f) return 3;
    return 2;
}

void DarkbassAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    const float s = (float) getHeight() / (float) baseH;
    hoverZone = presetZoneAt (e.position / s);
    setMouseCursor (hoverZone != 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void DarkbassAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void DarkbassAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    const float s = (float) getHeight() / (float) baseH;
    const int zone = presetZoneAt (e.position / s);
    if (zone != 0 && statusAlpha > 0.05f)
    {
        // la ligne d'etat ne bloque pas les clics : un clic fait revenir la barre de presets
        statusUntil = 0.0; learnLineUntil = 0.0;
        return;
    }
    switch (zone)
    {
        case 1: stepPreset (-1); break;
        case 2: showPresetMenu(); break;
        case 3: stepPreset (1); break;
        default: break;
    }
}

void DarkbassAudioProcessorEditor::stepPreset (int delta)
{
    const int n = DarkbassAudioProcessor::getNumFactoryPresets();
    int cp = proc.currentPreset.load();
    if (cp < 0 || cp >= n) cp = delta > 0 ? -1 : 0;
    proc.applyFactoryPreset ((cp + delta + n) % n);
}

void DarkbassAudioProcessorEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < DarkbassAudioProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + DarkbassAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    m.addSeparator();
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);
    m.setLookAndFeel (&lnf);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled (presetRect))),
        [safe = Component::SafePointer<DarkbassAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if      (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
        });
}

void DarkbassAudioProcessorEditor::chooseCabIR()
{
    chooser = std::make_unique<FileChooser> ("Load a cabinet impulse response (WAV / AIFF)",
                                             File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [safe = Component::SafePointer<DarkbassAudioProcessorEditor> (this)] (const FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto f = fc.getResult();
            if (f.existsAsFile() && safe->proc.loadCabIR (f))
            {
                safe->proc.setParamValue ("cab", 1.0f);
                safe->learnLine = "IR  " + f.getFileNameWithoutExtension();
                safe->learnLineUntil = Time::getMillisecondCounterHiRes() + 2000.0;
                safe->statusUntil = safe->learnLineUntil;
            }
        });
}


void DarkbassAudioProcessorEditor::handleMidiMenu (int r)
{
    if (r >= 1000 && r < 1000 + DarkbassAudioProcessor::numLearn)
        proc.learnTarget.store (proc.learnTarget.load() == r - 1000 ? -1 : r - 1000);   // re-cliquer annule
    else if (r >= 1100 && r < 1100 + DarkbassAudioProcessor::numLearn) proc.ccMap[r - 1100].store (-1);
    else if (r == 1200) proc.learnTarget.store (-1);
    else if (r == 1201) { proc.learnTarget.store (-1); proc.resetMidiMap(); }
}

void DarkbassAudioProcessorEditor::setGlowPercent (int pc)
{
    glowPc = NineHandsGlow::sanitize (pc);
    drawnLed[0] = drawnLed[1] = -1.0f;      // le timer redessine les LED
}

PopupMenu DarkbassAudioProcessorEditor::buildContextMenu (int learnIdx)
{
    using namespace NineHandsMenu;
    PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String ("not assigned");
    };
    const int lt = proc.learnTarget.load();
    if (learnIdx >= 0)
    {
        // menu d'une commande (tronc commun) : en-tete, MIDI Learn, Clear MIDI, Cancel MIDI learn
        const int i = learnIdx;
        const String name = DarkbassAudioProcessor::learnDef (i).name;
        addHeader (menu, lnf.f, (learnIdx < 8 ? icoKnob : (learnIdx < 13 ? icoSwitch : icoPedal)), accent, name, "MIDI: " + ccText (learnIdx));
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name, true, lt == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        menu.addItem (1200, "Cancel MIDI learn", lt >= 0);
        return menu;
    }

    // menu de la pedale (tronc commun) : presets, reglages propres, MIDI, Plugin size, Glow intensity
    addHeader (menu, lnf.f, icoPedal, accent, "Darkbass", "Bass preamp  v" + String (JucePlugin_VersionString));
    PopupMenu factory;
    for (int i = 0; i < DarkbassAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + DarkbassAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {{
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }}
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();
    PopupMenu cab;
    cab.addItem (60, "Cab sim on", true, proc.raw ("cab") > 0.5f);
    cab.addSeparator();
    cab.addItem (61, "Built-in IR (bass 4x10)", true, proc.cabIRPath.isEmpty());
    cab.addItem (62, "Load IR file...");
    if (proc.cabIRPath.isNotEmpty()) cab.addItem (63, "Current: " + proc.cabIRName(), false, true);
    menu.addSubMenu ("Cab sim", cab);
    menu.addSeparator();

    PopupMenu midi;
    midi.addSectionHeader ("MIDI Learn");
    for (int i = 0; i < DarkbassAudioProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (DarkbassAudioProcessor::learnDef (i).name) + "  (" + ccText (i) + ")", true, lt == i);
    midi.addSeparator();
    midi.addItem (1200, "Cancel MIDI learn", lt >= 0);
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    NineHandsWindow::addMidiFootswitchMode (midi, proc.raw ("midiFsMode") < 0.5f);
    midi.addSeparator();
    NineHandsWindow::addProgramChangeInfo (midi);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();

    const char* names[numSizes] = { "60 %", "75 %", "100 %" };
    const int heights[numSizes] = { sizeHeight (0), sizeHeight (1), sizeHeight (2) };
    menu.addSubMenu ("Plugin size", NineHandsWindow::sizeMenu (JucePlugin_Name, numSizes, names, heights, getHeight()));
    menu.addSubMenu ("Glow intensity", NineHandsWindow::glowMenu (glowPc));
    return menu;
}

void DarkbassAudioProcessorEditor::showContextMenu (int learnIdx)
{
    auto menu = buildContextMenu (learnIdx);
    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),       // menu clic droit : toujours la ou l'on clique
        [safe = Component::SafePointer<DarkbassAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            int inst = pr.uiSizeIdx.load();
            const int sz = NineHandsWindow::handleSize (r, JucePlugin_Name, numSizes, inst);
            if (sz >= 0) { pr.uiSizeIdx = inst; ed.setScaledHeight (ed.sizeHeight (sz)); return; }
            if (const int pc = NineHandsWindow::handleGlow (r, JucePlugin_Name))
            {
                ed.setGlowPercent (pc);
                ed.learnLine = "Glow  " + String (pc) + " %";        // ligne d'etat, brievement
                ed.learnLineUntil = Time::getMillisecondCounterHiRes() + 1500.0;
                ed.statusUntil = jmax (ed.statusUntil, ed.learnLineUntil);
                return;
            }
            if (false) {}
            else if (r == NineHandsWindow::idLatch)     pr.setParamValue ("midiFsMode", 0.0f);
            else if (r == NineHandsWindow::idMomentary) pr.setParamValue ("midiFsMode", 1.0f);
            else if (r == 60) pr.setParamValue ("cab", pr.raw ("cab") > 0.5f ? 0.0f : 1.0f);
            else if (r == 61) pr.useBuiltInCabIR();
            else if (r == 62) { ed.chooseCabIR(); return; }
            else if (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            else if (r >= 1000) { ed.handleMidiMenu (r); return; }
            
        });
}
