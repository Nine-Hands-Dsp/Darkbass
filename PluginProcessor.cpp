#include "PluginProcessor.h"
#include "NineHandsFolder.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Presets d'usine (0 a 10 comme sur la pedale ; inverseurs : 0 = haut, 1 = milieu, 2 = bas)
//   positions haut / milieu / bas comme sur la pedale : GRUNT boost / cut / flat, LO FREQ 500 / 1k / 250,
//   HI FREQ 1,5k / 3k / 750, ATTACK flat / boost / cut
//==============================================================================
static const DarkbassPreset factoryPresets[] =
{
    //  nom                      master blend level drive bass  lomid himid treb  grunt lo hi atk drive
    { "Modern Growl",            4.78f, 6.0f, 5.0f, 6.0f, 6.0f, 4.5f, 6.0f, 6.0f,  2,    0, 1, 1,  1 },
    { "Clean DI",                4.87f, 0.0f, 5.0f, 3.0f, 5.5f, 5.0f, 5.5f, 5.5f,  2,    0, 0, 0,  0 },
    { "Clank & Grind",           5.72f, 5.5f, 5.5f, 4.0f, 5.0f, 4.0f, 6.5f, 6.5f,  2,    0, 1, 1,  1 },
    { "Tight Djent",             4.96f, 7.0f, 5.0f, 7.5f, 5.5f, 3.5f, 6.0f, 6.5f,  1,    0, 0, 1,  1 },
    { "Doom Wall",               3.59f, 8.0f, 5.0f, 9.0f, 6.5f, 5.5f, 5.0f, 4.0f,  0,    2, 2, 2,  1 },
    { "Upright Warmth",          5.16f, 2.0f, 5.0f, 2.0f, 6.0f, 6.0f, 3.5f, 3.5f,  0,    2, 2, 2,  1 },
    { "Slap Bright",             4.68f, 1.0f, 5.0f, 3.0f, 6.5f, 3.5f, 6.5f, 7.0f,  2,    0, 1, 1,  1 },
    { "Scooped Metal",           3.59f, 6.5f, 5.0f, 7.0f, 7.0f, 3.0f, 4.5f, 7.0f,  0,    0, 0, 1,  1 },
    { "Mid Punch Rock",          6.25f, 4.5f, 5.0f, 5.0f, 5.5f, 6.5f, 6.0f, 5.0f,  2,    1, 0, 0,  1 },
    { "All Noon",                5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f,  1,    1, 1, 1,  1 },
};

int DarkbassAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }
const DarkbassPreset& DarkbassAudioProcessor::getFactoryPreset (int i) { return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)]; }

const char* DarkbassAudioProcessor::knobId (int i)
{
    static const char* ids[numKnobs] = { "master", "blend", "level", "drive", "bass", "lomids", "himids", "treble" };
    return ids[jlimit (0, numKnobs - 1, i)];
}

const char* DarkbassAudioProcessor::knobName (int i)
{
    static const char* n[numKnobs] = { "master", "blend", "level", "drive", "bass", "lo mids", "hi mids", "treble" };
    return n[jlimit (0, numKnobs - 1, i)];
}

const char* DarkbassAudioProcessor::toggleId (int i)
{
    static const char* ids[numToggles] = { "grunt", "loFreq", "hiFreq", "attack" };
    return ids[jlimit (0, numToggles - 1, i)];
}

StringArray DarkbassAudioProcessor::toggleChoices (int i)
{
    switch (i)
    {
        // ordre des positions comme sur la vraie pedale : haut / milieu / bas
        case 0:  return { "Boost", "Cut", "Flat" };
        case 1:  return { "500 Hz", "1 kHz", "250 Hz" };
        case 2:  return { "1.5 kHz", "3 kHz", "750 Hz" };
        default: return { "Flat", "Boost", "Cut" };
    }
}

const DarkbassAudioProcessor::LearnDef& DarkbassAudioProcessor::learnDef (int i)
{
    static const LearnDef defs[numLearn] =
    {
        { "master",  "Master",            learnContinuous },
        { "blend",   "Blend",             learnContinuous },
        { "level",   "Level",             learnContinuous },
        { "drive",   "Drive",             learnContinuous },
        { "bass",    "Bass",              learnContinuous },
        { "lomids",  "Lo Mids",           learnContinuous },
        { "himids",  "Hi Mids",           learnContinuous },
        { "treble",  "Treble",            learnContinuous },
        { "grunt",   "Grunt",             learnSelector },
        { "loFreq",  "Lo Freq",           learnSelector },
        { "hiFreq",  "Hi Freq",           learnSelector },
        { "attack",  "Attack",            learnSelector },
        { "cab",     "Cab Sim",           learnToggle },
        { "active",  "Bypass footswitch", learnFootswitch },
        { "driveOn", "Drive footswitch",  learnFootswitch },
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

void DarkbassAudioProcessor::resetMidiMap()
{
    for (int i = 0; i < numLearn; ++i) ccMap[i].store (-1);
}

//==============================================================================
DarkbassAudioProcessor::DarkbassAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DARKBASS", createLayout())
{
    for (int i = 0; i < numRaw; ++i) rawPtr[i] = apvts.getRawParameterValue (rawIds[i]);
    for (int i = 0; i < numQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    resetMidiMap();
    startTimerHz (60);
}

AudioProcessorValueTreeState::ParameterLayout DarkbassAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];
    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 10.0f)) + " %"; }));
    };
    p.push_back (knob ("master", "Master", d.master));
    p.push_back (knob ("blend",  "Blend",  d.blend));
    p.push_back (knob ("level",  "Level (drive)", d.level));
    p.push_back (knob ("drive",  "Drive",  d.drive));
    p.push_back (knob ("bass",   "Bass",   d.bass));
    p.push_back (knob ("lomids", "Lo Mids", d.lomids));
    p.push_back (knob ("himids", "Hi Mids", d.himids));
    p.push_back (knob ("treble", "Treble", d.treble));
    const int tdef[numToggles] = { d.grunt, d.loFreq, d.hiFreq, d.attack };
    const char* tname[numToggles] = { "Grunt", "Lo Mids Freq", "Hi Mids Freq", "Attack" };
    for (int i = 0; i < numToggles; ++i)
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { toggleId (i), 1 }, tname[i], toggleChoices (i), tdef[i]));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "cab", 1 },     "Cab Sim",           false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 },  "Bypass Footswitch", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "driveOn", 1 }, "Drive Footswitch",  d.driveOn != 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "holdMomentary", 1 }, "Hold = Momentary", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "midiFsMode", 1 }, "MIDI Footswitch Mode",
                                                         StringArray { "Latching (127 / 0)", "Momentary (toggle)" }, 0));
    return { p.begin(), p.end() };
}

bool DarkbassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == out;
}

void DarkbassAudioProcessor::setParamValue (const String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// Anti-gel : jamais d'appel a l'hote depuis le thread audio ; le MIDI met la valeur en file,
// ce timer (thread de messages, 60 Hz) la transmet au DAW et a la fenetre.
void DarkbassAudioProcessor::setParamFromAudio (int qi, float plain)
{
    pendingValue[qi].store (plain, std::memory_order_relaxed);
    pendingSet[qi].store (true, std::memory_order_release);
}

void DarkbassAudioProcessor::timerCallback()
{
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (rawIds[i]))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));
}

//==============================================================================
void DarkbassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    if (cabIRPath.isNotEmpty()) engine.loadUserIR (File (cabIRPath));
    for (auto& r : ramps) { r.active = false; r.left = 0; }
    setLatencySamples (engine.getLatency());
}

bool DarkbassAudioProcessor::loadCabIR (const File& f)
{
    if (! engine.loadUserIR (f)) return false;
    cabIRPath = f.getFullPathName();
    return true;
}

void DarkbassAudioProcessor::useBuiltInCabIR()
{
    cabIRPath.clear();
    engine.useBuiltInIR();
}

//==============================================================================
// MIDI
//==============================================================================
float DarkbassAudioProcessor::mapCC (int k, float raw01)
{
    // course de la pedale d'expression apprise automatiquement (0 et 100 % exacts), zones mortes aux butees
    auto& r = ramps[k];
    if (raw01 < r.lo) r.lo = raw01;
    if (raw01 > r.hi) r.hi = raw01;
    float v = raw01;
    if (r.hi - r.lo > 0.4f) v = (raw01 - r.lo) / (r.hi - r.lo);
    constexpr float dz = 0.02f;
    return jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
}

void DarkbassAudioProcessor::handleMidi (MidiBuffer& midi)
{
    const bool latching = rawAt (16) < 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const juce::int64 when = sampleClock + meta.samplePosition;
        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            bool known = true;
            auto knobFromCC = [&] (int k, float raw01)
            {
                // rampe lineaire vers la nouvelle valeur sur l'intervalle mesure entre deux CC (puis lissage 2 poles dans le DSP)
                auto& r = ramps[k];
                const float target = mapCC (k, raw01) * 10.0f;
                if (! r.active) r.value = rawAt (k);
                const double interval = (double) (when - r.lastTime);
                r.left = (int) jlimit (0.002 * sr, 0.040 * sr, interval);
                r.lastTime = when;
                r.target = target;
                r.step = (r.target - r.value) / (float) r.left;
                r.active = true;
                setParamFromAudio (k, target);
                currentPreset = -1;
            };

            // MIDI Learn : le premier CC recu est affecte a la commande visee (il n'agit pas sur le son)
            const int lt = learnTarget.load();
            if (lt >= 0 && lt < numLearn && cc < 120)
            {
                for (int j = 0; j < numLearn; ++j)
                    if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);
                ccMap[lt].store (cc);
                if (lt < numKnobs) { ramps[lt].msb = -1; ramps[lt].lo = 1.0f; ramps[lt].hi = 0.0f; }
                prevFsCC[lt] = v;
                lastLearned.store (lt);
                learnTarget.store (-1);
                lastMidiCC = cc; lastMidiValue = v; ++midiCounter;
                continue;
            }

            int ctl = -1;
            for (int j = 0; j < numLearn; ++j)
                if (ccMap[j].load (std::memory_order_relaxed) == cc) { ctl = j; break; }
            int lsbOf = -1;
            if (ctl < 0)
                for (int k = 0; k < numKnobs; ++k)
                {
                    const int msbCC = ccMap[k].load (std::memory_order_relaxed);
                    if (msbCC >= 0 && msbCC < 32 && cc == msbCC + 32) { lsbOf = k; break; }
                }

            if (ctl >= 0 && ctl < numKnobs)
            {
                ramps[ctl].msb = v;
                knobFromCC (ctl, (float) v / 127.0f);
            }
            else if (lsbOf >= 0)
            {
                // CC 14 bits (CC n + CC n+32) : resolution fine si la pedale l'envoie
                if (ramps[lsbOf].msb >= 0) knobFromCC (lsbOf, (float) (ramps[lsbOf].msb * 128 + v) / 16383.0f);
            }
            else if (ctl >= 8 && ctl < 12)
            {
                // inverseur 3 positions : CC 86-127 = haut, 43-85 = milieu, 0-42 = bas
                setParamFromAudio (ctl, (float) (v >= 86 ? 0 : (v >= 43 ? 1 : 2)));
                currentPreset = -1;
            }
            else if (ctl == 12) { setParamFromAudio (12, v >= 64 ? 1.0f : 0.0f); }
            else if (ctl == 13 || ctl == 14)
            {
                int& prev = prevFsCC[ctl];
                const float curV = pendingSet[ctl].load() ? pendingValue[ctl].load() : rawAt (ctl);
                if (latching)                  setParamFromAudio (ctl, v >= 64 ? 1.0f : 0.0f);
                else if (v >= 64 && prev < 64) setParamFromAudio (ctl, curV > 0.5f ? 0.0f : 1.0f);
                prev = v;
            }
            else known = false;
            if (known) { lastMidiCC = cc; lastMidiValue = v; ++midiCounter; }
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) { pendingPreset = pc; triggerAsyncUpdate(); }
            lastMidiCC = -2; ++midiCounter;
        }
    }
    midi.clear();
}

void DarkbassAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) applyFactoryPreset (p);
}

//==============================================================================
void DarkbassAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    handleMidi (midi);
    if (n == 0) return;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    darkbass::Params p;
    float* knobs[numKnobs] = { &p.master, &p.blend, &p.level, &p.drive, &p.bass, &p.lomids, &p.himids, &p.treble };
    for (int k = 0; k < numKnobs; ++k)
    {
        auto& r = ramps[k];
        if (r.active)
        {
            const int adv = jmin (r.left, n);
            r.value += r.step * (float) adv; r.left -= adv;
            if (r.left <= 0) { r.value = r.target; r.active = false; }
            *knobs[k] = r.value;
        }
        else *knobs[k] = rawAt (k);
    }
    p.grunt   = jlimit (0, 2, roundToInt (rawAt (8)));
    p.loFreq  = jlimit (0, 2, roundToInt (rawAt (9)));
    p.hiFreq  = jlimit (0, 2, roundToInt (rawAt (10)));
    p.attack  = jlimit (0, 2, roundToInt (rawAt (11)));
    p.cab     = rawAt (12) > 0.5f;
    p.active  = rawAt (13) > 0.5f;
    p.driveOn = rawAt (14) > 0.5f;

    const int nCh = jmin (2, buffer.getNumChannels());
    float* io[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, nCh, n, p);

    float pk = 0.0f;
    for (int c = 0; c < nCh; ++c) pk = jmax (pk, buffer.getMagnitude (c, 0, n));
    outLevel = pk;
    sampleClock += n;
}

//==============================================================================
// Presets
//==============================================================================
int DarkbassAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void DarkbassAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    const float kv[numKnobs] = { p.master, p.blend, p.level, p.drive, p.bass, p.lomids, p.himids, p.treble };
    for (int k = 0; k < numKnobs; ++k) setParamValue (knobId (k), kv[k]);
    const int tv[numToggles] = { p.grunt, p.loFreq, p.hiFreq, p.attack };
    for (int t = 0; t < numToggles; ++t) setParamValue (toggleId (t), (float) tv[t]);
    setParamValue ("driveOn", (float) p.driveOn);
    currentPreset = jlimit (0, getNumFactoryPresets() - 1, index);
    ++presetCounter;
}

File DarkbassAudioProcessor::getUserPresetFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile (nhd::folderName()).getChildFile ("Darkbass").getChildFile ("UserPresets.xml");
}

static const char* presetIds[] = { "master", "blend", "level", "drive", "bass", "lomids", "himids", "treble",
                                   "grunt", "loFreq", "hiFreq", "attack", "driveOn", "cab" };

bool DarkbassAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return false;
    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<XmlElement> root;
    if (file.existsAsFile()) root = XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets")) root = std::make_unique<XmlElement> ("UserPresets");
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot) { root->removeChildElement (e, true); break; }
    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot", slot);
    for (auto* id : presetIds) e->setAttribute (id, (double) raw (id));
    const bool ok = root->writeTo (file);
    if (ok) { currentPreset = 100 + slot; ++presetCounter; }
    return ok;
}

bool DarkbassAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    auto root = XmlDocument::parse (file);
    if (root == nullptr) return false;
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot)
        {
            for (auto* id : presetIds)
                if (e->hasAttribute (id)) setParamValue (id, (float) e->getDoubleAttribute (id));
            currentPreset = 100 + slot; ++presetCounter;
            return true;
        }
    return false;
}

bool DarkbassAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    if (auto root = XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot) return true;
    return false;
}

//==============================================================================
void DarkbassAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    state.setProperty ("cabIR", cabIRPath, nullptr);
    for (int i = 0; i < numLearn; ++i)
        state.setProperty (String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void DarkbassAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            for (int i = 0; i < numLearn; ++i)
                ccMap[i].store (jlimit (-1, 127, (int) state.getProperty (String ("cc_") + learnDef (i).id, -1)));
            const String ir = state.getProperty ("cabIR", "").toString();
            if (ir.isNotEmpty() && File (ir).existsAsFile()) loadCabIR (File (ir));
            else if (cabIRPath.isNotEmpty()) useBuiltInCabIR();
            apvts.replaceState (state);
            ++presetCounter;
        }
}

AudioProcessorEditor* DarkbassAudioProcessor::createEditor() { return new DarkbassAudioProcessorEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DarkbassAudioProcessor(); }
