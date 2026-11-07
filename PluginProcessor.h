#pragma once
#include <cstring>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "DarkbassDSP.h"

//==============================================================================
// DARKBASS (NINE HANDS | DSP) - preampli basse + saturation au format pedale double,
// d'apres la Darkglass Microtubes B7K Ultra v2. Voir README.md et DarkbassDSP.h.
//==============================================================================

struct DarkbassPreset
{
    const char* name;
    float master, blend, level, drive, bass, lomids, himids, treble;   // 0..10 comme sur la pedale
    int   grunt, loFreq, hiFreq, attack;                               // 0 = haut, 1 = milieu, 2 = bas
    int   driveOn;
};

class DarkbassAudioProcessor : public juce::AudioProcessor,
                               private juce::AsyncUpdater,
                               private juce::Timer
{
public:
    DarkbassAudioProcessor();
    ~DarkbassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Darkbass"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    static int getNumFactoryPresets();
    static const DarkbassPreset& getFactoryPreset (int index);
    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override { applyFactoryPreset (index); }
    const juce::String getProgramName (int index) override { return getFactoryPreset (index).name; }
    void changeProgramName (int, const juce::String&) override {}
    void applyFactoryPreset (int index);

    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float plain);

    // anti-gel : pointeurs des parametres mis en cache (aucune recherche par chaine dans processBlock)
    static constexpr int numRaw = 17;
    static constexpr const char* rawIds[numRaw] = { "master", "blend", "level", "drive", "bass", "lomids", "himids", "treble",
                                                     "grunt", "loFreq", "hiFreq", "attack", "cab", "active", "driveOn",
                                                     "holdMomentary", "midiFsMode" };
    std::atomic<float>* rawPtr[numRaw] {};
    float raw (const char* id) const
    {
        for (int i = 0; i < numRaw; ++i)
            if (std::strcmp (id, rawIds[i]) == 0) return rawPtr[i]->load (std::memory_order_relaxed);
        return apvts.getRawParameterValue (id)->load();
    }
    float rawAt (int i) const { return rawPtr[i]->load (std::memory_order_relaxed); }

    // informations pour la fenetre
    std::atomic<int>   currentPreset { 0 }, presetCounter { 0 };
    std::atomic<int>   lastMidiCC { -1 }, lastMidiValue { 0 }, midiCounter { 0 };
    std::atomic<float> outLevel { 0.0f };

    // simulation de baffle : IR integree ou fichier de l'utilisateur (thread de messages)
    bool loadCabIR (const juce::File& f);
    void useBuiltInCabIR();
    juce::String cabIRName() const { return cabIRPath.isEmpty() ? juce::String ("built-in 4x10") : juce::File (cabIRPath).getFileNameWithoutExtension(); }
    juce::String cabIRPath;

    // 8 potards (ordre du Layout) ; 4 inverseurs 3 positions
    static constexpr int numKnobs = 8;
    static const char* knobId (int i);
    static const char* knobName (int i);
    static constexpr int numToggles = 4;
    static const char* toggleId (int i);
    static juce::StringArray toggleChoices (int i);

    // MIDI Learn : toutes les commandes de la facade (meme ordre que la file anti-gel), aucun CC assigne par defaut
    enum LearnType { learnContinuous, learnToggle, learnSelector, learnFootswitch };
    struct LearnDef { const char* id; const char* name; int type; };
    static constexpr int numLearn = 15;
    static const LearnDef& learnDef (int i);
    std::atomic<int> ccMap[numLearn];            // numero de CC de chaque commande, -1 = non assigne
    std::atomic<int> learnTarget { -1 };         // commande en attente d'un CC (-1 = pas de learn)
    std::atomic<int> lastLearned { -1 };         // derniere commande apprise (lue et remise a -1 par la fenetre)
    void resetMidiMap();

private:
    void handleMidi (juce::MidiBuffer& midi);
    void setParamFromAudio (int queueIndex, float plain);
    void timerCallback() override;
    static constexpr int numQueued = numLearn;   // 8 potards + 4 inverseurs + cab + active + driveOn
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    void handleAsyncUpdate() override;
    float mapCC (int knob, float raw01);

    darkbass::Engine engine;
    double sr = 48000.0;

    struct ExprRamp
    {
        bool  active = false;
        float value = 0.0f, target = 0.0f, step = 0.0f;
        int   left = 0;
        juce::int64 lastTime = 0;
        int   msb = -1;
        float lo = 1.0f, hi = 0.0f;
    } ramps[numKnobs];
    juce::int64 sampleClock = 0;
    int prevFsCC[numLearn] {};
    std::atomic<int> pendingPreset { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkbassAudioProcessor)
};
