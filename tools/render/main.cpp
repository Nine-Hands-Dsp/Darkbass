// Outil hors plugin : tests audio de DARKBASS + captures PNG de la vraie fenetre (meme code que le plugin).
#include "BinaryData.h"
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
            // basse en cordes filees : dent de scie limitee (harmoniques 1..48 en 1/n, pente -6 dB/oct) avec attaque
            double sw = 0.0;
            for (int h = 1; h <= 48 && h * f0 < 0.45 * sr; ++h) sw += std::sin (h * ph + 0.37 * h) / h;
            const float x = 0.17f * env * (float) sw;
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
            double sw = 0.0;
            for (int h = 1; h <= 48 && h * 55.0 < 21600.0; ++h) sw += std::sin (h * ph + 0.37 * h) / h;
            const double x = 0.17 * env * sw;
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
    set (proc, "drive", 5); pump (3);
    for (int g = 0; g < 3; ++g) { set (proc, "grunt", (float) g); pump (3); std::printf ("sature seul grunt pos %d (haut/mil/bas): %.1f dB\n", g, runRms (proc, 48000.0)); }
    set (proc, "grunt", 1);
    for (int a = 0; a < 3; ++a) { set (proc, "attack", (float) a); pump (3); std::printf ("sature seul attack pos %d (haut/mil/bas): %.1f dB\n", a, runRms (proc, 48000.0)); }
    set (proc, "attack", 1);
    // reponse en frequence de la voie saturee, petit signal (drive 0) : sinus purs
    {
        set (proc, "drive", 0); pump (3);
        for (double f : { 50.0, 100.0, 200.0, 320.0, 500.0, 717.0, 1000.0, 2000.0, 3000.0, 5000.0 })
        {
            double ph = 0, s2 = 0; long cnt = 0;
            for (int blk = 0; blk < 120; ++blk)
            {
                AudioBuffer<float> b (2, 512); MidiBuffer m;
                for (int i = 0; i < 512; ++i) { const float x = 0.01f * (float) std::sin (ph); ph += 2 * MathConstants<double>::pi * f / 48000.0; b.setSample (0, i, x); b.setSample (1, i, x); }
                proc.processBlock (b, m);
                if (blk > 40) for (int i = 0; i < 512; ++i) { s2 += b.getSample (0, i) * b.getSample (0, i); ++cnt; }
            }
            std::printf ("  OD petit signal %6.0f Hz : %6.1f dB (re entree)\n", f, 10 * std::log10 (s2 / cnt / 0.00005));
        }
        set (proc, "drive", 5); pump (3);
    }
    set (proc, "blend", 5); pump (3);
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
    // apercu du menu clic droit (meme LookAndFeel que l editeur)
    {
        DarkKnobLookAndFeel l;
        l.reg = l.med = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
        l.bold = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
        struct It { String t; bool sep, sub, tick, act, hi; };
        std::vector<It> items { { "Learn: DRIVE (none)", 0,0,0,1,0 }, { "Clear MIDI for DRIVE", 0,0,0,1,0 }, { "", 1,0,0,0,0 },
            { "Plugin size", 0,1,0,1,1 }, { "", 1,0,0,0,0 }, { "Factory presets", 0,1,0,1,0 }, { "Load user preset", 0,1,0,1,0 },
            { "Save user preset", 0,1,0,1,0 }, { "", 1,0,0,0,0 }, { "Cab sim", 0,1,0,1,0 }, { "Settings", 0,1,0,1,0 },
            { "MIDI map", 0,1,0,1,0 }, { "", 1,0,0,0,0 }, { "Hold footswitch = momentary", 0,0,1,1,0 },
            { "Darkbass v" + String (JucePlugin_VersionString) + " - NINE HANDS | DSP", 0,0,0,0,0 } };
        int W = 0, H = 14;
        for (auto& it : items) { int w, h; l.getIdealPopupMenuItemSize (it.t, it.sep, 0, w, h); W = jmax (W, w); H += h; }
        W += 14;
        Image img (Image::ARGB, W * 2, H * 2, true);
        { Graphics g (img); g.addTransform (AffineTransform::scale (2.0f));
          l.drawPopupMenuBackground (g, W, H);
          int y = 7;
          for (auto& it : items) { int w, h; l.getIdealPopupMenuItemSize (it.t, it.sep, 0, w, h);
              l.drawPopupMenuItem (g, { 7, y, W - 14, h }, it.sep, it.act, it.hi, it.tick, it.sub, it.t, {}, nullptr, nullptr); y += h; } }
        File f (out + "/darkbass_menu.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
