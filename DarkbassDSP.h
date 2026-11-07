#pragma once
// DARKBASS (NINE HANDS | DSP) - moteur audio, d'apres la Darkglass Microtubes B7K Ultra v2 :
//
//   entree --+-- voie claire (gain unitaire, retardee de la latence du sur-echantillonnage) ------------+
//            |                                                                                          +-- BLEND --> EQ 4 bandes --> [CAB SIM] --> MASTER
//            +-- voie saturee : GRUNT (graves avant ecretage) + ATTACK (aigus avant ecretage) -> DRIVE   |
//                -> 2 etages CMOS facon « microtubes » (x4, asymetriques) -> filtre de sortie -> LEVEL --+
//
// Footswitch DRIVE coupe : la voie claire seule passe dans l'EQ (BLEND ignore), comme sur la pedale.
// Footswitch BYPASS coupe : signal d'entree intact (retarde de la meme latence, aucun saut de phase).
// EQ (sur le melange, clair compris) : BASS +-12 dB @ 100 Hz, LO MIDS +-12 dB @ 250 / 500 / 1k, HI MIDS +-12 dB @ 750 / 1,5k / 3k,
// TREBLE +-12 dB @ 5 kHz ; courbe continue de meme pente de part et d'autre de midi (aucune cassure).
// Aucune allocation, aucun verrou dans process() ; filtres maison (pas de juce::dsp::IIR qui alloue).
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace darkbass
{
    struct Params
    {
        float master = 5, blend = 5, level = 5, drive = 5, bass = 5, lomids = 5, himids = 5, treble = 5;   // 0..10
        int grunt = 1, loFreq = 1, hiFreq = 1, attack = 1;     // 0 = position haute, 1 = milieu, 2 = basse
        bool active = true, driveOn = true, cab = false;
    };

    // ------------------------------------------------------------------ biquad (RBJ), forme transposee II
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        inline float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        {
            b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
        }
        void lowShelf (double sr, double f, double q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w), al = std::sin (w) / (2 * q), sA = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * cw + sA), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sA),
                 (A + 1) + (A - 1) * cw + sA, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sA);
        }
        void highShelf (double sr, double f, double q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w), al = std::sin (w) / (2 * q), sA = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * cw + sA), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sA),
                 (A + 1) - (A - 1) * cw + sA, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sA);
        }
        void peak (double sr, double f, double q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
        }
        void lowPass (double sr, double f, double q)
        {
            const double w = 2 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
        }
        void highPass (double sr, double f, double q)
        {
            const double w = 2 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
        }
    };

    // passe-haut du 1er ordre (blocage du continu)
    struct DCBlock
    {
        float R = 0.999f, x1 = 0, y1 = 0;
        void prepare (double sr, double f) { R = (float) std::exp (-2 * juce::MathConstants<double>::pi * f / sr); x1 = y1 = 0; }
        inline float process (float x) { const float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
    };

    // lissage a 2 poles (pedale d'expression sans marches, potards et parametres derives)
    struct Smooth2
    {
        float a = 0.01f, s1 = 0, s2 = 0;
        void prepare (double sr, double ms) { a = (float) (1.0 - std::exp (-1.0 / (0.001 * ms * sr))); }
        void snap (float v) { s1 = s2 = v; }
        inline float next (float target) { s1 += a * (target - s1); s2 += a * (s1 - s2); return s2; }
    };

    // courbe des potards d'EQ : +-12 dB, meme pente des deux cotes de midi, s'accelere vers les butees
    inline float eqDb (float knob) { const float x = juce::jlimit (-1.0f, 1.0f, (knob - 5.0f) / 5.0f); return 9.0f * x + 3.0f * x * std::abs (x); }
    // MASTER / LEVEL : 0 = silence, midi = 0 dB, 10 = +12 dB (loi continue)
    inline float gainLaw (float knob) { const float x = juce::jmax (0.0f, knob) / 5.0f; return x * x; }

    // etage CMOS « microtubes » : saturation douce asymetrique, pente unitaire a l'origine
    inline float cmos (float x)
    {
        constexpr float b = 0.28f;
        static const float tb = std::tanh (b), k = 1.0f / (1.0f - tb * tb);
        return (std::tanh (x + b) - tb) * k;
    }

    class Engine
    {
    public:
        static constexpr int kChunk = 512;
        static constexpr int kOS = 2;      // 2 etages = x4

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            sr = sampleRate;
            os = std::make_unique<juce::dsp::Oversampling<float>> (2, kOS, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os->initProcessing ((size_t) kChunk);
            latency = (int) std::lround (os->getLatencyInSamples());
            const double osr = sr * (1 << kOS);
            for (int c = 0; c < 2; ++c)
            {
                inDC[c].prepare (sr, 18.0);
                preHP[c].reset(); gruntShelf[c].reset(); attackShelf[c].reset(); attackPeak[c].reset();
                stage1LP[c].reset(); postLP[c].reset(); postLP2[c].reset();
                mid1DC[c].prepare (osr, 12.0); outDC[c].prepare (sr, 15.0);
                for (auto& b : eq[c]) b.reset();
                std::fill (std::begin (delay[c]), std::end (delay[c]), 0.0f);
            }
            for (int c = 0; c < 2; ++c) stage1LP[c].lowPass (osr, 12000.0, 0.6);
            for (int c = 0; c < 2; ++c) { postLP[c].lowPass (sr, 6200.0, 0.62); postLP2[c].lowPass (sr, juce::jmin (11000.0, 0.45 * sr), 0.5); }
            delayPos = 0;
            for (auto* s : { &sMaster, &sBlend, &sLevel, &sDrive, &sBass, &sLo, &sHi, &sTreb }) s->prepare (sr, 18.0);
            for (auto* s : { &sActive, &sDriveOn, &sCab }) s->prepare (sr, 9.0);
            for (auto* s : { &sLoF, &sHiF }) s->prepare (sr, 25.0);
            first = true;
            cachedGrunt = cachedAttack = -1;
            driveBuf.setSize (2, kChunk); cleanBuf.setSize (2, kChunk); dryBuf.setSize (2, kChunk);

            juce::dsp::ProcessSpec spec { sr, (juce::uint32) kChunk, 2 };
            conv.prepare (spec);
            makeBuiltInIR();
            if (! userIRLoaded) conv.loadImpulseResponse (juce::AudioBuffer<float> (builtInIR), sr, juce::dsp::Convolution::Stereo::no,
                                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
            conv.reset();
        }

        int getLatency() const { return latency; }

        // IR utilisateur (thread de messages) ; la convolution la charge en arriere-plan sans couper le son
        bool loadUserIR (const juce::File& f)
        {
            if (! f.existsAsFile()) return false;
            conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                      juce::dsp::Convolution::Normalise::yes);
            userIRLoaded = true;
            return true;
        }
        void useBuiltInIR()
        {
            userIRLoaded = false;
            if (builtInIR.getNumSamples() > 0)
                conv.loadImpulseResponse (juce::AudioBuffer<float> (builtInIR), sr, juce::dsp::Convolution::Stereo::no,
                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
        }

        void process (float* const* io, int numCh, int n, const Params& p)
        {
            for (int done = 0; done < n; )
            {
                const int len = juce::jmin (kChunk, n - done);
                float* ch[2] = { io[0] + done, numCh > 1 ? io[1] + done : nullptr };
                processChunk (ch, numCh, len, p);
                done += len;
            }
        }

    private:
        void updatePreFilters (int grunt, int attack)
        {
            if (grunt == cachedGrunt && attack == cachedAttack) return;
            cachedGrunt = grunt; cachedAttack = attack;
            // GRUNT : quantite de graves qui entre dans l'ecretage (boost / flat / cut)
            const double hp[3] = { 35.0, 70.0, 190.0 }, sh[3] = { 9.0, 0.0, -3.0 };
            // ATTACK : quantite d'aigus qui entre dans l'ecretage (boost / flat / cut)
            const double hs[3] = { 8.0, 0.0, -9.0 }, pk[3] = { 3.5, 0.0, 0.0 };
            for (int c = 0; c < 2; ++c)
            {
                preHP[c].highPass (sr, hp[grunt], grunt == 2 ? 0.75 : 0.62);
                gruntShelf[c].lowShelf (sr, 140.0, 0.7, sh[grunt]);
                attackShelf[c].highShelf (sr, 2400.0, 0.7, hs[attack]);
                attackPeak[c].peak (sr, 4200.0, 1.1, pk[attack]);
            }
        }

        void updateEq (float bassDb, float loDb, float hiDb, float trebDb, float loF, float hiF)
        {
            for (int c = 0; c < 2; ++c)
            {
                eq[c][0].lowShelf (sr, 100.0, 0.707, bassDb);
                eq[c][1].peak (sr, loF, 0.9, loDb);
                eq[c][2].peak (sr, hiF, 0.9, hiDb);
                eq[c][3].highShelf (sr, juce::jmin (5000.0, 0.4 * sr), 0.707, trebDb);
            }
        }

        void processChunk (float* const* io, int numCh, int n, const Params& p)
        {
            updatePreFilters (juce::jlimit (0, 2, p.grunt), juce::jlimit (0, 2, p.attack));
            const float loTarget = std::log2 (std::array<float, 3> { 1000.0f, 500.0f, 250.0f }[(size_t) juce::jlimit (0, 2, p.loFreq)]);
            const float hiTarget = std::log2 (std::array<float, 3> { 3000.0f, 1500.0f, 750.0f }[(size_t) juce::jlimit (0, 2, p.hiFreq)]);
            if (first)
            {
                sMaster.snap (p.master); sBlend.snap (p.blend); sLevel.snap (p.level); sDrive.snap (p.drive);
                sBass.snap (p.bass); sLo.snap (p.lomids); sHi.snap (p.himids); sTreb.snap (p.treble);
                sActive.snap (p.active ? 1.f : 0.f); sDriveOn.snap (p.driveOn ? 1.f : 0.f); sCab.snap (p.cab ? 1.f : 0.f);
                sLoF.snap (loTarget); sHiF.snap (hiTarget);
                first = false;
            }

            // 1. entree : copie du signal sec (bypass), voie claire et voie saturee
            float driveG[kChunk], driveMk[kChunk];
            for (int c = 0; c < 2; ++c)
            {
                const float* in = io[juce::jmin (c, numCh - 1)];
                float* d = driveBuf.getWritePointer (c);
                float* cl = cleanBuf.getWritePointer (c);
                float* dry = dryBuf.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    const float x = in[i];
                    // ligne a retard commune (voie claire + bypass alignes sur la voie sur-echantillonnee)
                    auto& dl = delay[c];
                    const int w = (delayPos + i) & (kDelay - 1);
                    dl[(size_t) w] = x;
                    const float xd = dl[(size_t) ((w - latency) & (kDelay - 1))];
                    dry[i] = xd;
                    cl[i] = inDC[c].process (xd);
                    float y = preHP[c].process (x);
                    y = gruntShelf[c].process (y);
                    y = attackShelf[c].process (y);
                    d[i] = attackPeak[c].process (y);
                }
            }
            delayPos = (delayPos + n) & (kDelay - 1);

            // gain d'entree de la saturation (DRIVE), lisse par echantillon
            for (int i = 0; i < n; ++i)
            {
                const float dv = juce::jlimit (0.0f, 1.0f, sDrive.next (p.drive) / 10.0f);
                driveG[i] = std::pow (10.0f, (2.0f + 44.0f * std::pow (dv, 1.35f)) / 20.0f);
                // compensation de volume de la voie saturee (mesuree, tools/render) : LEVEL a midi ~ voie claire
                // a DRIVE midi ; le volume monte encore un peu avec le DRIVE (+6 dB de 0 a 10), comme sur la pedale
                static constexpr float adj[5] = { 6.3f, 1.5f, -5.5f, -10.2f, -10.3f };
                const float fi = dv * 4.0f; const int i0 = juce::jmin (3, (int) fi); const float fr = fi - (float) i0;
                driveMk[i] = std::pow (10.0f, (adj[i0] + (adj[i0 + 1] - adj[i0]) * fr) / 20.0f);
            }
            for (int c = 0; c < 2; ++c)
            {
                float* d = driveBuf.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] *= driveG[i];
            }

            // 2. saturation x4
            {
                juce::dsp::AudioBlock<float> blk (driveBuf.getArrayOfWritePointers(), 2, (size_t) n);
                auto up = os->processSamplesUp (blk);
                const int un = (int) up.getNumSamples();
                for (int c = 0; c < 2; ++c)
                {
                    float* u = up.getChannelPointer ((size_t) c);
                    for (int i = 0; i < un; ++i)
                    {
                        float y = cmos (u[i] * 0.55f);                 // 1er etage
                        y = stage1LP[c].process (y);
                        y = mid1DC[c].process (y);
                        y = cmos (y * 2.4f);                           // 2e etage (plus serre)
                        u[i] = y;
                    }
                }
                os->processSamplesDown (blk);
            }

            // 3. filtre de sortie, LEVEL, BLEND, footswitch DRIVE
            float* mix[2] = { cleanBuf.getWritePointer (0), cleanBuf.getWritePointer (1) };
            // (driveMk est calcule plus haut, echantillon par echantillon)
            for (int i = 0; i < n; ++i)
            {
                const float b = juce::jlimit (0.0f, 1.0f, sBlend.next (p.blend) / 10.0f);
                const float on = sDriveOn.next (p.driveOn ? 1.0f : 0.0f);
                const float lv = gainLaw (sLevel.next (p.level)) * kDriveMakeup;
                const float cg = 1.0f + (std::cos (b * juce::MathConstants<float>::halfPi) - 1.0f) * on;
                const float dg = std::sin (b * juce::MathConstants<float>::halfPi) * on * lv * driveMk[i];
                for (int c = 0; c < 2; ++c)
                {
                    float y = driveBuf.getSample (c, i);
                    y = postLP[c].process (y);
                    y = postLP2[c].process (y);
                    y = outDC[c].process (y);
                    mix[c][i] = mix[c][i] * cg + y * dg;
                }
            }

            // 4. EQ 4 bandes (coefficients recalcules tous les 16 echantillons, valeurs lissees)
            for (int i0 = 0; i0 < n; i0 += 16)
            {
                const int len = juce::jmin (16, n - i0);
                float bdb = 0, ldb = 0, hdb = 0, tdb = 0, lf = 0, hf = 0;
                for (int k = 0; k < len; ++k)
                {
                    bdb = sBass.next (p.bass); ldb = sLo.next (p.lomids); hdb = sHi.next (p.himids); tdb = sTreb.next (p.treble);
                    lf = sLoF.next (loTarget); hf = sHiF.next (hiTarget);
                }
                updateEq (eqDb (bdb), eqDb (ldb), eqDb (hdb), eqDb (tdb), std::exp2 (lf), std::exp2 (hf));
                for (int c = 0; c < 2; ++c)
                    for (int k = i0; k < i0 + len; ++k)
                    {
                        float y = mix[c][k];
                        for (auto& bq : eq[c]) y = bq.process (y);
                        mix[c][k] = y;
                    }
            }

            // 5. simulation de baffle (IR), en fondu
            float cabEnd = 0.0f;
            {
                float cabStart = sCab.s2;
                for (int i = 0; i < n; ++i) cabEnd = sCab.next (p.cab ? 1.0f : 0.0f);
                if (cabStart > 1.0e-4f || cabEnd > 1.0e-4f)
                {
                    // copie du signal sans IR dans driveBuf (libre a ce stade)
                    for (int c = 0; c < 2; ++c) driveBuf.copyFrom (c, 0, cleanBuf, c, 0, n);
                    juce::dsp::AudioBlock<float> blk (cleanBuf.getArrayOfWritePointers(), 2, (size_t) n);
                    conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < n; ++i)
                        {
                            const float t = cabStart + (cabEnd - cabStart) * ((float) (i + 1) / (float) n);
                            mix[c][i] = driveBuf.getSample (c, i) * (1.0f - t) + mix[c][i] * t * kCabMakeup;
                        }
                }
                else if (! convIdle) { conv.reset(); }
                convIdle = cabEnd <= 1.0e-4f;
            }

            // 6. MASTER, footswitch BYPASS (fondu), sortie
            for (int i = 0; i < n; ++i)
            {
                const float m = gainLaw (sMaster.next (p.master));
                const float a = sActive.next (p.active ? 1.0f : 0.0f);
                for (int c = 0; c < numCh; ++c)
                {
                    const float wet = mix[c][i] * m;
                    float y = dryBuf.getSample (c, i) + (wet - dryBuf.getSample (c, i)) * a;
                    io[c][i] = std::isfinite (y) ? y : 0.0f;
                }
            }
        }

        static constexpr float kDriveMakeup = 0.42f;   // gain fixe de la voie saturee (voir driveMk)
        static constexpr float kCabMakeup = 2.55f;    // IR normalisee : meme volume avec ou sans CAB SIM
        static constexpr int kDelay = 256;

        void makeBuiltInIR()
        {
            // baffle basse 4 x 10 + tweeter coupe (IR synthetique, 2048 echantillons) : resonance ~ 70 Hz, creux ~ 400 Hz,
            // presence ~ 1,8 kHz, coupure progressive au-dessus de 4 kHz
            const int len = 2048;
            builtInIR.setSize (1, len);
            builtInIR.clear();
            float* d = builtInIR.getWritePointer (0);
            d[0] = 1.0f;
            Biquad f[6];
            f[0].highPass (sr, 48.0, 1.1);
            f[1].peak (sr, 75.0, 1.2, 3.0);
            f[2].peak (sr, 420.0, 1.0, -4.0);
            f[3].peak (sr, 1800.0, 1.4, 3.0);
            f[4].lowPass (sr, juce::jmin (4300.0, 0.4 * sr), 0.8);
            f[5].lowPass (sr, juce::jmin (6500.0, 0.42 * sr), 0.6);
            for (int i = 0; i < len; ++i)
            {
                float y = d[i];
                for (auto& b : f) y = b.process (y);
                d[i] = y;
            }
            // tres courte premiere reflexion (caisse fermee)
            const int r = (int) (0.0011 * sr);
            for (int i = len - 1; i >= r; --i) d[i] += 0.18f * d[i - r];
            for (int i = len - 256; i < len; ++i) d[i] *= (float) (len - i) / 256.0f;
        }

        double sr = 48000.0;
        int latency = 0;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        DCBlock inDC[2], mid1DC[2], outDC[2];
        Biquad preHP[2], gruntShelf[2], attackShelf[2], attackPeak[2], stage1LP[2], postLP[2], postLP2[2];
        Biquad eq[2][4];
        float delay[2][kDelay] {};
        int delayPos = 0;
        Smooth2 sMaster, sBlend, sLevel, sDrive, sBass, sLo, sHi, sTreb, sActive, sDriveOn, sCab, sLoF, sHiF;
        bool first = true, convIdle = true;
        int cachedGrunt = -1, cachedAttack = -1;
        juce::AudioBuffer<float> driveBuf, cleanBuf, dryBuf, builtInIR;
        juce::dsp::Convolution conv;
        std::atomic<bool> userIRLoaded { false };
    };
}
