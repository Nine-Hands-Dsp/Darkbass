// Outil hors plugin : tests audio de DARKBASS + captures PNG de la vraie fenetre (meme code que le plugin).
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static void pump (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20); }

// riff de basse synthetique (fondamentale + harmoniques, attaques), ~ -18 dBFS RMS
static double runRms (DarkbassAudioProcessor& proc, double sr, double f0 = 55.0, bool checkNaN = true, double* peakOut = nullptr)
{
    double sum = 0, ph = 0, peak = 0; long cnt = 0; bool bad = false;
    for (int blk = 0; blk < 240; ++blk)
    {
        AudioBuffer<float> b (2, 512); MidiBuffer m;
        for (int i = 0; i < 512; ++i)
        {
            const double t = (blk * 512 + i) / sr;
            const float env = (float) (0.35 + 0.65 * std::exp (-std::fmod (t, 0.5) * 6.0));
            const float x = 0.17f * env * (float) (std::sin (ph) + 0.5 * std::sin (2 * ph + 0.3) + 0.25 * std::sin (3 * ph + 1.1) + 0.12 * std::sin (5 * ph));
            ph += 2 * MathConstants<double>::pi * f0 / sr;
            b.setSample (0, i, x); b.setSample (1, i, x);
        }
        proc.processBlock (b, m);
        for (int i = 0; i < 512; ++i)
        {
            const float y = b.getSample (0, i);
            if (! std::isfinite (y)) bad = true;
            if (blk > 60) { sum += y * y; ++cnt; peak = jmax (peak, (double) std::abs (y)); }
        }
    }
    if (checkNaN && bad) std::printf ("  !! valeur non finie\n");
    if (peakOut) *peakOut = peak;
    return 10 * std::log10 (sum / cnt + 1e-20);
}

static void set (DarkbassAudioProcessor& p, const char* id, float v) { p.setParamValue (id, v); }

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    DarkbassAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    std::printf ("latence a 48 kHz : %d echantillons\n", proc.getLatencySamples());

    // reference : signal d'entree
    {
        double ph = 0, sum = 0; long n = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const double t = i / 48000.0;
            const double env = 0.35 + 0.65 * std::exp (-std::fmod (t, 0.5) * 6.0);
            const double x = 0.17 * env * (std::sin (ph) + 0.5 * std::sin (2 * ph + 0.3) + 0.25 * std::sin (3 * ph + 1.1) + 0.12 * std::sin (5 * ph));
            ph += 2 * MathConstants<double>::pi * 55.0 / 48000.0; sum += x * x; ++n;
        }
        std::printf ("entree : %.1f dB RMS\n", 10 * std::log10 (sum / n));
    }

    // calibrage : clair seul / sature seul (DRIVE et LEVEL a midi) / bypass
    proc.applyFactoryPreset (9); pump (3);   // All Noon
    set (proc, "driveOn", 0); pump (3);
    std::printf ("clair (drive off, EQ a plat)        : %.1f dB\n", runRms (proc, 48000.0));
    set (proc, "driveOn", 1); set (proc, "blend", 10); pump (3);
    for (float d : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
    {
        set (proc, "drive", d); pump (3);
        double pk; const double r = runRms (proc, 48000.0, 55.0, true, &pk);
        std::printf ("sature seul drive %4.1f level 5      : %.1f dB  (crete %.2f)\n", d, r, pk);
    }
    set (proc, "drive", 5); set (proc, "blend", 5); pump (3);
    std::printf ("blend 5                              : %.1f dB\n", runRms (proc, 48000.0));
    set (proc, "active", 0); pump (3);
    std::printf ("bypass                               : %.1f dB\n", runRms (proc, 48000.0));
    set (proc, "active", 1); pump (3);
    std::printf ("cab sim off (blend 5)                : %.1f dB\n", runRms (proc, 48000.0));
    set (proc, "cab", 1); pump (3);
    runRms (proc, 48000.0); Thread::sleep (300); pump (10);
    std::printf ("cab sim on  (blend 5)                : %.1f dB\n", runRms (proc, 48000.0));
    set (proc, "cab", 0); pump (3);

    // alignement clair / bypass : drive off, EQ a plat, master midi -> sortie = entree retardee de la latence
    {
        set (proc, "driveOn", 0); pump (3);
        proc.prepareToPlay (48000.0, 512);
        const int L = proc.getLatencySamples();
        AudioBuffer<float> b (2, 4096); MidiBuffer m; b.clear();
        for (int i = 0; i < 4096; ++i) { const float x = (float) std::sin (2 * MathConstants<double>::pi * 3000.0 * i / 48000.0) * 0.3f; b.setSample (0, i, x); b.setSample (1, i, x); }
        AudioBuffer<float> in (b);
        for (int k = 0; k < 4; ++k) { AudioBuffer<float> tmp (in); proc.processBlock (tmp, m); }   // regime etabli
        proc.processBlock (b, m);
        double err = 0, ref = 0;
        for (int i = 1000; i < 4000; ++i) { const double d = b.getSample (0, i) - in.getSample (0, i - L); err += d * d; ref += (double) in.getSample (0, i) * in.getSample (0, i); }
        std::printf ("ecart clair / entree retardee (3 kHz) : %.1f dB\n", 10 * std::log10 (err / ref + 1e-20));
        set (proc, "driveOn", 1); pump (3);
    }

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        proc.prepareToPlay (sr, 512);
        std::printf ("-- %.0f Hz (latence %d)\n", sr, proc.getLatencySamples());
        for (int pi = 0; pi < DarkbassAudioProcessor::getNumFactoryPresets(); ++pi)
        {
            proc.applyFactoryPreset (pi); pump (2);
            double pk; const double r = runRms (proc, sr, 41.2, true, &pk);
            std::printf ("preset %2d %-18s rms %6.1f dB  crete %.2f\n", pi + 1, DarkbassAudioProcessor::getFactoryPreset (pi).name, r, pk);
        }
    }

    // MIDI : pedale qui n'envoie que 2..116 -> 0 et 100 % exacts apres apprentissage de la course
    {
        proc.prepareToPlay (48000.0, 256);
        proc.ccMap[3].store (11);
        auto sendCC = [&] (int v)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m; m.addEvent (MidiMessage::controllerEvent (1, 11, v), 0);
            proc.processBlock (b, m);
            for (int k = 0; k < 20; ++k) { MidiBuffer e; b.clear(); proc.processBlock (b, e); }
            pump (3);
            return proc.raw ("drive");
        };
        const float a = sendCC (2), c = sendCC (116), mid = sendCC (59);
        std::printf ("pedale 2..116 -> drive %.2f / %.2f, mi-course %.2f\n", a, c, mid);
        proc.ccMap[8].store (12);
        AudioBuffer<float> b (2, 64); MidiBuffer m; m.addEvent (MidiMessage::controllerEvent (1, 12, 127), 0);
        proc.processBlock (b, m); pump (3);
        std::printf ("CC 127 -> grunt %d (0 = haut)\n", roundToInt (proc.raw ("grunt")));
        proc.resetMidiMap();
    }

    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* ped = static_cast<DarkbassAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto shot = [&] (const String& name, int preset, bool active, bool drive, bool status, int height)
    {
        ped->setScaledHeight (height);
        proc.applyFactoryPreset (preset);
        proc.setParamValue ("active", active ? 1.f : 0.f);
        proc.setParamValue ("driveOn", drive ? 1.f : 0.f);
        AudioBuffer<float> b (2, 256); MidiBuffer m;
        for (int i = 0; i < 10; ++i) { b.clear(); proc.processBlock (b, m); }
        pump (30);
        if (status) proc.setParamValue ("drive", 7.3f); else pump (150);
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    shot ("darkbass_default", 0, true,  true,  false, DarkbassAudioProcessorEditor::defaultH);
    shot ("darkbass_100",     4, true,  true,  false, DarkbassAudioProcessorEditor::refH);
    shot ("darkbass_status",  3, true,  false, true,  DarkbassAudioProcessorEditor::refH);
    shot ("darkbass_bypass",  1, false, true,  false, DarkbassAudioProcessorEditor::refH);
    ed.reset();
    return 0;
}
