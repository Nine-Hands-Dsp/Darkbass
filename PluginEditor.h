#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potards (photo fournie par l'utilisateur, dans le fond : reflets fixes) ; seul le repere blanc peint tourne.
//==============================================================================
class DarkKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class DarkKnob : public juce::Slider
{
public:
    bool hitTest (int x, int y) override;
};

//==============================================================================
// Inverseur 3 positions (photos d'inverseur NINE HANDS | DSP) : haut / milieu / bas ;
// passage haut <-> bas par l'image du milieu ~55 ms. Clic moitie haute = monter, moitie basse = descendre.
//==============================================================================
class Toggle3 : public juce::Component
{
public:
    explicit Toggle3 (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool hitTest (int x, int y) override;
    void tick (double nowMs);
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;
private:
    void moveTo (int idx);
    juce::ParameterAttachment attachment;
    int pos = 1, shownFrame = 1;
    double transitionUntil = 0.0;
};

//==============================================================================
// Footswitch (photo fournie par l'utilisateur, dans le fond) : fonctionnement d'Echoflex : bascule a l'appui,
// bouton assombri tant qu'on appuie, appui maintenu (> 0,45 s) quand la section etait coupee = momentane.
// Reflet bleu de la LED voisine sur le flanc de l'ecrou, en fondu avec elle.
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    FootSwitchArea (juce::RangedAudioParameter& p, std::atomic<float>* holdMomentary);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick, onUserChange;
    juce::Image reflection;
    float ledLevel = 0.0f;
private:
    bool value = false, pressed = false, turnedOnByPress = false;
    double downTime = 0.0;
    std::atomic<float>* hold = nullptr;
    juce::ParameterAttachment attachment;
};

struct RightClickForwarder : public juce::MouseListener
{
    std::function<void (juce::Component*)> callback;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && callback) callback (e.eventComponent); }
};

//==============================================================================
class DarkbassAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit DarkbassAudioProcessorEditor (DarkbassAudioProcessor&);
    ~DarkbassAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // repere de reference (fond 2x = 2480 x 1856) ; tailles de fenetre comme Ring Think (format double)
    static constexpr int baseW = 1240, baseH = 928;
    static constexpr int defaultH = 495;             // 661 x 495 : ouverture par defaut = taille minimale
    static constexpr int refH = 743;                 // « 100 % » = 993 x 743 (comme Ring Think)
    static constexpr int maxH = 1123;

    void setScaledHeight (int h);
    static constexpr int numSizes = 3;          // 60 % (defaut d'origine) / 75 % / 100 %
    static int sizeHeight (int idx);
    void showStatus();

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void visibilityChanged() override;
    void askRepaint (juce::Rectangle<int> r);
    void showContextMenu (int learnIdx = -1);
    void addMidiMenu (juce::PopupMenu& menu, int learnIdx);
    void handleMidiMenu (int r);
    void showPresetMenu();
    void stepPreset (int delta);
    void chooseCabIR();
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void drawLed (juce::Graphics&, int which);
    void drawStatus (juce::Graphics&);
    void drawPresetBar (juce::Graphics&);

    DarkbassAudioProcessor& proc;
    DarkKnobLookAndFeel lnf;
    RightClickForwarder rightClickFwd;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Image background, bgScaled, glow[2], glowS[2], ledOn;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    juce::Typeface::Ptr labelFace;

    DarkKnob knobs[DarkbassAudioProcessor::numKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[DarkbassAudioProcessor::numKnobs];
    std::unique_ptr<Toggle3> toggles[DarkbassAudioProcessor::numToggles];
    FootSwitchArea bypassFs, driveFs;

    float ledLevel[2] { 0.0f, 0.0f }, drawnLed[2] { -1.0f, -1.0f };
    int lastChanged = -1;
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    double statusUntil = 0.0;
    juce::String lastLine1, lastPreset;
    float lastValues[16];
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0;
    int hoverZone = 0, drawnHover = -1;

    juce::String learnLine;
    double learnLineUntil = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkbassAudioProcessorEditor)
};
