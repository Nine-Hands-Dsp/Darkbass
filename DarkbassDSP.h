#pragma once
// DARKBASS (NINE HANDS | DSP) - moteur audio v1.1, d'apres la Darkglass Microtubes B7K Ultra v2.
//
// Sources (nov. 2026) : manuel / fiche Darkglass (commandes, EQ, footswitch dedie a l'overdrive, clair a gain unitaire)
// et deux analyses de circuit open source de la B7K Ultra (schema du clone « Black Mirror VII », valeurs des composants,
// mesures sur captures) : github.com/tehguitarist/Obsidian-B7000 (.claude/rules/circuit.md, src/dsp/FitParams.h).
//
//   IN (1 Mohm) --> buffer --+-- voie claire (gain unitaire) -------------------------------------------------------+
//                            |                                                                                     |
//                            +-- C2/R4 (passe-haut 145 Hz) --> etage JFET J201 (gain ~x12 a x17, saturation douce   |
//                                asymetrique) --> reseau ATTACK / echelle d'aigus (creux ~320 Hz) --> C7 (183 Hz)   |
//                                --> DRIVE : ampli op non inverseur x4 a x78 (potard C, ecretage sur les rails)      |
//                                --> GRUNT : condensateur d'entree de l'inverseur (3,7 n / +47 n / +220 n)           |
//                                --> UN inverseur CMOS CD4049UBE en contre-reaction (R16 6k8, R18 330k // C14 220p,   |
//                                    gain de boucle ouverte fini ~25), saturation douce asymetrique                   |
//                                --> C15 --> T ponte (creux ~717 Hz) --> Sallen-Key 10,7 kHz --> Sallen-Key 3,3 kHz   |
//                                --> LEVEL (potard A) ------------------------------------------------------------- BLEND (potard B, fondu lineaire)
//   --> C21 (passe-haut 12 Hz) --> EQ : BASS / TREBLE Baxandall (+-12 dB a 100 Hz / 5 kHz), LO MIDS / HI MIDS en cloche
//   --> [CAB SIM] --> MASTER
//
// Partie saturee entierement sur-echantillonnee x4 (filtres en double precision). La voie claire et le bypass sont
// retardes de la latence du sur-echantillonnage. Footswitch DRIVE coupe : voie claire seule dans l'EQ.
// Tensions : 1,0 en numerique = 2 V crete (niveau instrument), pour que les seuils de saturation soient ceux du circuit.
// Aucune allocation, aucun verrou dans process().
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace darkbass
{
    struct Params
    {
        float master = 5, blend = 5, level = 5, drive = 5, bass = 5, lomids = 5, himids = 5, treble = 5;   // 0..10
        int grunt = 2, loFreq = 1, hiFreq = 1, attack = 0;     // positions : 0 = haut, 1 = milieu, 2 = bas
        bool active = true, driveOn = true, cab = false;
    };

    // ------------------------------------------------------------------ biquad (double), forme transposee II
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        inline double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        {
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
        // filtre analogique (n0 + n1 s + n2 s^2) / (d0 + d1 s + d2 s^2), transformee bilineaire pre-deformee a fw (Hz)
        void analog (double sr, double n0, double n1, double n2, double d0, double d1, double d2, double fw)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, 0.45 * sr, fw);
            const double K = w / std::tan (w / (2.0 * sr));
            const double K2 = K * K;
            set (n0 + n1 * K + n2 * K2, 2.0 * (n0 - n2 * K2), n0 - n1 * K + n2 * K2,
                 d0 + d1 * K + d2 * K2, 2.0 * (d0 - d2 * K2), d0 - d1 * K + d2 * K2);
        }
        // 1er ordre analogique (n0 + n1 s) / (d0 + d1 s)
        void analog1 (double sr, double n0, double n1, double d0, double d1, double fw) { analog (sr, n0, n1, 0, d0, d1, 0, fw); }
        void highPass1 (double sr, double f) { const double w = 2 * juce::MathConstants<double>::pi * f; analog1 (sr, 0, 1, w, 1, f); }
        void lowPass1 (double sr, double f)  { const double w = 2 * juce::MathConstants<double>::pi * f; analog1 (sr, w, 0, w, 1, f); }
        // passe-bas Sallen-Key gain unitaire (f0, Q)
        void lowPassQ (double sr, double f, double q)
        {
            const double w = 2 * juce::MathConstants<double>::pi * f;
            analog (sr, w * w, 0, 0, w * w, w / q, 1, f);
        }
        void peak (double sr, double f, double q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
        }
        void highShelf (double sr, double f, double q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w), al = std::sin (w) / (2 * q), sA = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * cw + sA), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sA),
                 (A + 1) - (A - 1) * cw + sA, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sA);
        }
        // plateaux du 1er ordre facon Baxandall : gain g (lineaire) sous / au-dessus de fc
        void lowShelf1 (double sr, double fc, double g)
        {
            const double w = 2 * juce::MathConstants<double>::pi * fc;
            if (g >= 1.0) analog1 (sr, g * w, 1, w, 1, fc);        // (s + g w) / (s + w)
            else          analog1 (sr, w, 1, w / g, 1, fc);        // inverse exacte (coupe symetrique)
        }
        void highShelf1 (double sr, double fc, double g)
        {
            const double w = 2 * juce::MathConstants<double>::pi * fc;
            if (g >= 1.0) analog1 (sr, w, g, w, 1, fc);            // (g s + w) / (s + w)
            else          analog1 (sr, w, 1, w, 1.0 / g, fc);
        }
    };

    // lissage a 2 poles (pedale d'expression sans marches, potards et parametres derives)
    struct Smooth2
    {
        float a = 0.01f, s1 = 0, s2 = 0;
        void prepare (double sr, double ms) { a = (float) (1.0 - std::exp (-1.0 / (0.001 * ms * sr))); }
        void snap (float v) { s1 = s2 = v; }
        inline float next (float target) { s1 += a * (target - s1); s2 += a * (s1 - s2); return s2; }
    };

    // courbe des potards d'EQ : +-12 dB a la frequence nominale, meme pente des deux cotes de midi
    inline float eqDb (float knob) { const float x = juce::jlimit (-1.0f, 1.0f, (knob - 5.0f) / 5.0f); return 9.0f * x + 3.0f * x * std::abs (x); }
    // MASTER : 0 = silence, midi = 0 dB, 10 = +12 dB (loi continue)
    inline float gainLaw (float knob) { const float x = juce::jmax (0.0f, knob) / 5.0f; return x * x; }
    // LEVEL : potard 100k courbe A, loi releve sur captures (fraction de la resistance selon la rotation)
    inline float levelTaper (float knob)
    {
        static const float rx[5] = { 0.0f, 0.206030f, 0.543750f, 0.775388f, 1.0f };
        static const float ry[5] = { 0.0f, 0.026166f, 0.223470f, 0.528328f, 1.0f };
        const float x = juce::jlimit (0.0f, 1.0f, knob / 10.0f);
        int i = 0; while (i < 3 && x > rx[i + 1]) ++i;
        return ry[i] + (ry[i + 1] - ry[i]) * (x - rx[i]) / (rx[i + 1] - rx[i]);
    }
    // DRIVE : ampli op non inverseur, gain = 1 + 330k / (3k3 + 1k + R), R = 100k (1 - x)^1,98 (potard C)
    inline float driveGain (float knob)
    {
        const float x = juce::jlimit (0.0f, 1.0f, knob / 10.0f);
        return 1.0f + 330.0e3f / (4.3e3f + 100.0e3f * std::pow (1.0f - x, 1.98f));
    }
    // saturation douce asymetrique (seuils positif / negatif en volts)
    inline double softClip (double v, double pos, double neg)
    {
        return v >= 0.0 ? pos * std::tanh (v / pos) : neg * std::tanh (v / neg);
    }

    class Engine
    {
    public:
        static constexpr int kChunk = 512;
        static constexpr int kOS = 2;                 // 2 etages = x4
        static constexpr double kVolts = 2.0;         // 1,0 numerique = 2 V crete

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            sr = sampleRate;
            osr = sr * (1 << kOS);
            os = std::make_unique<juce::dsp::Oversampling<float>> (2, kOS, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os->initProcessing ((size_t) kChunk);
            latency = (int) std::lround (os->getLatencyInSamples());
            for (int c = 0; c < 2; ++c)
            {
                // voie saturee (cadence x4)
                hpC2[c].highPass1 (osr, 145.0);                          // C2 1n / (R4 100k + R5 1M)
                // J201 : gain x12,5 dans le grave, x16,7 au-dessus (C3 shunte la degeneration R6), zero 219 Hz / pole 292 Hz
                {
                    const double w1 = 2 * juce::MathConstants<double>::pi * 219.0, w2 = w1 * 1.333;
                    jfetShelf[c].analog1 (osr, 12.5 * w2, 12.5 * w2 / w1, w2, 1.0, 250.0);
                }
                jfetDC[c].highPass1 (osr, 8.0);
                hpC7[c].highPass1 (osr, 183.0);                          // C7 (valeur ajustee sur captures) / R11 // R13
                driveLP[c].lowPass1 (osr, 10300.0);                      // C10 47p // R15 330k
                c15[c].highPass1 (osr, 30.0);                            // C15 (ajuste sur captures)
                // sortie (cadence de base)
                bridgedT[c].analog (sr, kBtN0, kBtN1, kBtN2, 1.0, kBtD1, kBtD2, 717.0);   // T ponte R22 100k, R23 33k, C16 680p, C17 22n
                sk1[c].lowPassQ (sr, juce::jmin (10700.0, 0.45 * sr), 0.463);           // R24 10k, R25 22k, C18 / C27 1n
                sk2[c].lowPassQ (sr, 3337.0, 0.691);                                    // R26 22k, R27 47k, C19 2n2, C20 1n
                c21[c].highPass1 (sr, 12.2);                                            // C21 100n (passe-haut apres BLEND)
                inDC[c].highPass1 (sr, 5.0);
                for (auto* b : { &hpC2[c], &jfetShelf[c], &jfetDC[c], &ladderNotch[c], &attackTilt[c], &hpC7[c], &driveLP[c],
                                 &clipLin[c], &c15[c], &bridgedT[c], &sk1[c], &sk2[c], &c21[c], &inDC[c] }) b->reset();
                for (auto& b : eq[c]) b.reset();
                std::fill (std::begin (delay[c]), std::end (delay[c]), 0.0f);
            }
            delayPos = 0;
            for (auto* s : { &sMaster, &sBlend, &sLevel, &sDrive, &sBass, &sLo, &sHi, &sTreb }) s->prepare (sr, 18.0);
            for (auto* s : { &sActive, &sDriveOn, &sCab }) s->prepare (sr, 9.0);
            for (auto* s : { &sLoF, &sHiF }) s->prepare (sr, 25.0);
            first = true;
            cachedGrunt = cachedAttack = -1;
            odBuf.setSize (2, kChunk); cleanBuf.setSize (2, kChunk); dryBuf.setSize (2, kChunk); tmpBuf.setSize (2, kChunk);

            juce::dsp::ProcessSpec spec { sr, (juce::uint32) kChunk, 2 };
            conv.prepare (spec);
            makeBuiltInIR();
            if (! userIRLoaded) conv.loadImpulseResponse (juce::AudioBuffer<float> (builtInIR), sr, juce::dsp::Convolution::Stereo::no,
                                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
            conv.reset();
        }

        int getLatency() const { return latency; }

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
        // ATTACK et GRUNT : reseaux recalcules seulement quand l'inverseur change
        void updateSwitches (int grunt, int attack)
        {
            if (grunt == cachedGrunt && attack == cachedAttack) return;
            cachedGrunt = grunt; cachedAttack = attack;
            // ATTACK (mesure sur unite reelle) : gain large bande dans l'ecretage +8,65 / 0 / -2,39 dB (boost / flat / cut),
            // creux d'annulation de l'echelle d'aigus a 316 / 322 / 328 Hz ; Darkglass : plus ou moins d'aigus satures
            const double gainDb[3] = { 8.65, 0.0, -2.39 }, notchF[3] = { 316.4, 322.0, 328.1 }, tiltDb[3] = { 3.0, 0.0, -4.0 };
            attackGain = (float) std::pow (10.0, gainDb[attack] / 20.0);
            // GRUNT : condensateur en serie avec R16 a l'entree de l'inverseur (moins -> plus de graves satures)
            //   position haute BOOST = C11 + C13 220n, milieu FLAT = C11 + C12 47n, basse CUT = C11 seul
            const double cg[3] = { 3.69e-9 + 220.0e-9, 3.69e-9 + 47.0e-9, 17.0e-9 };   // CUT : valeur effective (schema 4,7 n) pour retrouver l'ecart mesure CUT / FLAT (~ -5,4 dB)
            // inverseur CMOS en contre-reaction, gain de boucle ouverte fini A0 :
            //   H = Zf / Zin = s Cg R18 / ((1 + s R18 C14)(1 + s R16 Cg)) ;  G = A0 H / ((A0 + 1) + H)  (signe ignore)
            constexpr double A0 = 24.871, R16 = 6.8e3, R18 = 330.0e3, C14 = 220.0e-12;
            const double C = cg[grunt];
            const double n1 = A0 * C * R18;
            const double d0 = A0 + 1.0, d1 = (A0 + 1.0) * (R18 * C14 + R16 * C) + C * R18, d2 = (A0 + 1.0) * R18 * C14 * R16 * C;
            for (int c = 0; c < 2; ++c)
            {
                ladderNotch[c].peak (osr, notchF[attack], 1.4, -10.0);
                attackTilt[c].highShelf (osr, 2500.0, 0.7, tiltDb[attack]);
                clipLin[c].analog (osr, 0.0, n1, 0.0, d0, d1, d2, 1000.0);
            }
        }

        void updateEq (float bassDb, float loDb, float hiDb, float trebDb, float loF, float hiF)
        {
            // Baxandall : plateau du 1er ordre, extremes +-16 dB, +-12 dB a 100 Hz / 5 kHz (coins 78,7 Hz / 6,35 kHz)
            const double gb = std::pow (10.0, (bassDb * 16.0 / 12.0) / 20.0), gt = std::pow (10.0, (trebDb * 16.0 / 12.0) / 20.0);
            for (int c = 0; c < 2; ++c)
            {
                eq[c][0].lowShelf1 (sr, 78.7, gb);
                eq[c][1].peak (sr, loF, 0.9, loDb);
                eq[c][2].peak (sr, hiF, 0.9, hiDb);
                eq[c][3].highShelf1 (sr, juce::jmin (6350.0, 0.42 * sr), gt);
            }
        }

        void processChunk (float* const* io, int numCh, int n, const Params& p)
        {
            // positions comme sur la vraie pedale (haut / milieu / bas) -> fonction (0 boost, 1 flat, 2 cut)
            //   GRUNT  : BOOST / CUT / FLAT      ATTACK : FLAT / BOOST / CUT
            //   LO FREQ : 500 / 1k / 250 Hz      HI FREQ : 1,5k / 3k / 750 Hz   (milieu = valeurs de la B7K d'origine)
            static constexpr int gruntFn[3] = { 0, 2, 1 }, attackFn[3] = { 1, 0, 2 };
            updateSwitches (gruntFn[juce::jlimit (0, 2, p.grunt)], attackFn[juce::jlimit (0, 2, p.attack)]);
            const float loTarget = std::log2 (std::array<float, 3> { 500.0f, 1000.0f, 250.0f }[(size_t) juce::jlimit (0, 2, p.loFreq)]);
            const float hiTarget = std::log2 (std::array<float, 3> { 1500.0f, 3000.0f, 750.0f }[(size_t) juce::jlimit (0, 2, p.hiFreq)]);
            if (first)
            {
                sMaster.snap (p.master); sBlend.snap (p.blend); sLevel.snap (p.level); sDrive.snap (p.drive);
                sBass.snap (p.bass); sLo.snap (p.lomids); sHi.snap (p.himids); sTreb.snap (p.treble);
                sActive.snap (p.active ? 1.f : 0.f); sDriveOn.snap (p.driveOn ? 1.f : 0.f); sCab.snap (p.cab ? 1.f : 0.f);
                sLoF.snap (loTarget); sHiF.snap (hiTarget);
                first = false;
            }

            // 1. entree en volts ; voie claire et sec retardes de la latence du sur-echantillonnage
            for (int c = 0; c < 2; ++c)
            {
                const float* in = io[juce::jmin (c, numCh - 1)];
                float* od = odBuf.getWritePointer (c);
                float* cl = cleanBuf.getWritePointer (c);
                float* dry = dryBuf.getWritePointer (c);
                auto& dl = delay[c];
                for (int i = 0; i < n; ++i)
                {
                    const float x = in[i];
                    const int w = (delayPos + i) & (kDelay - 1);
                    dl[(size_t) w] = x;
                    const float xd = dl[(size_t) ((w - latency) & (kDelay - 1))];
                    dry[i] = xd;
                    cl[i] = (float) inDC[c].process (xd);
                    od[i] = x * (float) kVolts;
                }
            }
            delayPos = (delayPos + n) & (kDelay - 1);

            // gain DRIVE (lisse), interpole sur la cadence x4
            float dg[kChunk];
            for (int i = 0; i < n; ++i) dg[i] = driveGain (sDrive.next (p.drive));

            // 2. voie saturee x4
            {
                juce::dsp::AudioBlock<float> blk (odBuf.getArrayOfWritePointers(), 2, (size_t) n);
                auto up = os->processSamplesUp (blk);
                const int un = (int) up.getNumSamples();
                const int f = 1 << kOS;
                const double ag = attackGain * kLadder;
                for (int c = 0; c < 2; ++c)
                {
                    float* u = up.getChannelPointer ((size_t) c);
                    for (int i = 0; i < un; ++i)
                    {
                        double v = hpC2[c].process ((double) u[i]);
                        v = jfetShelf[c].process (v);
                        v = jfetDC[c].process (softClip (v, 2.0111, 0.65743));     // J201 : plafonds asymetriques (harmoniques paires)
                        v = ladderNotch[c].process (v * ag);
                        v = attackTilt[c].process (v);
                        v = hpC7[c].process (v);
                        v = driveLP[c].process (v * (double) dg[juce::jmin (n - 1, i / f)]);
                        v = softClip (v, 2.7, 2.9);                                // rails de l'ampli op (alim 9 V)
                        v = clipLin[c].process (v);                                // GRUNT + inverseur CMOS (lineaire)
                        v = softClip (v, 2.98, 2.66);                              // inverseur CMOS : seuil 2,66 V sur rail 5,64 V
                        u[i] = (float) c15[c].process (v);
                    }
                }
                os->processSamplesDown (blk);
            }

            // 3. filtres de sortie, LEVEL, BLEND (fondu lineaire du potard), footswitch DRIVE
            float* mix[2] = { cleanBuf.getWritePointer (0), cleanBuf.getWritePointer (1) };
            for (int i = 0; i < n; ++i)
            {
                const float b = juce::jlimit (0.0f, 1.0f, sBlend.next (p.blend) / 10.0f);
                const float on = sDriveOn.next (p.driveOn ? 1.0f : 0.0f);
                const float lv = levelTaper (sLevel.next (p.level)) * (float) (kOdTrim / kVolts);
                const float cg = 1.0f - b * on;
                const float og = b * on * lv;
                for (int c = 0; c < 2; ++c)
                {
                    double y = odBuf.getSample (c, i);
                    y = bridgedT[c].process (y);
                    y = sk1[c].process (y);
                    y = sk2[c].process (y);
                    mix[c][i] = mix[c][i] * cg + (float) y * og;
                }
            }

            // 4. C21 + EQ 4 bandes (coefficients recalcules tous les 16 echantillons, valeurs lissees)
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
                        double y = c21[c].process (mix[c][k]);
                        for (auto& bq : eq[c]) y = bq.process (y);
                        mix[c][k] = (float) y;
                    }
            }

            // 5. simulation de baffle (IR), en fondu
            {
                const float cabStart = sCab.s2;
                float cabEnd = cabStart;
                for (int i = 0; i < n; ++i) cabEnd = sCab.next (p.cab ? 1.0f : 0.0f);
                if (cabStart > 1.0e-4f || cabEnd > 1.0e-4f)
                {
                    for (int c = 0; c < 2; ++c) tmpBuf.copyFrom (c, 0, cleanBuf, c, 0, n);
                    juce::dsp::AudioBlock<float> blk (cleanBuf.getArrayOfWritePointers(), 2, (size_t) n);
                    conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < n; ++i)
                        {
                            const float t = cabStart + (cabEnd - cabStart) * ((float) (i + 1) / (float) n);
                            mix[c][i] = tmpBuf.getSample (c, i) * (1.0f - t) + mix[c][i] * t * kCabMakeup;
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
                    const float y = dryBuf.getSample (c, i) + (wet - dryBuf.getSample (c, i)) * a;
                    io[c][i] = std::isfinite (y) ? y : 0.0f;
                }
            }
        }

        // T ponte (R22 100k, R23 33k, C16 680p, C17 22n), charge haute impedance, normalise par R23 :
        //   H(s) = (1 + s tau (R22+R23)/R23 + s^2 tau C17 R22) / (1 + s (C17 R22 + tau (R22+R23)/R23) + s^2 tau C17 R22), tau = R23 C16
        static constexpr double kBtTau = 33.0e3 * 680.0e-12;
        static constexpr double kBtN0 = 1.0;
        static constexpr double kBtN1 = kBtTau * 133.0e3 / 33.0e3;
        static constexpr double kBtN2 = kBtTau * 22.0e-9 * 100.0e3;
        static constexpr double kBtD1 = 22.0e-9 * 100.0e3 + kBtN1;
        static constexpr double kBtD2 = kBtN2;
        static constexpr double kLadder = 0.0225;      // echelle d'aigus + R7 / R8 : clair -> IC2_A ~ -8,5 dB au maximum (avec le gain du JFET)
        static constexpr double kOdTrim = 2.95;        // calibrage : LEVEL et DRIVE a midi ~ meme volume que la voie claire (basse a -18 dBFS)
        static constexpr float kCabMakeup = 2.55f;     // IR normalisee : meme volume avec ou sans CAB SIM
        static constexpr int kDelay = 256;

        void makeBuiltInIR()
        {
            // baffle basse 4 x 10 + tweeter coupe (IR synthetique, 2048 echantillons)
            const int len = 2048;
            builtInIR.setSize (1, len);
            builtInIR.clear();
            float* d = builtInIR.getWritePointer (0);
            d[0] = 1.0f;
            Biquad f[6];
            const double w48 = 2 * juce::MathConstants<double>::pi * 48.0;
            f[0].analog (sr, 0, 0, 1, w48 * w48, w48 / 1.1, 1, 48.0);
            f[1].peak (sr, 75.0, 1.2, 3.0);
            f[2].peak (sr, 420.0, 1.0, -4.0);
            f[3].peak (sr, 1800.0, 1.4, 3.0);
            f[4].lowPassQ (sr, juce::jmin (4300.0, 0.4 * sr), 0.8);
            f[5].lowPassQ (sr, juce::jmin (6500.0, 0.42 * sr), 0.6);
            for (int i = 0; i < len; ++i)
            {
                double y = d[i];
                for (auto& b : f) y = b.process (y);
                d[i] = (float) y;
            }
            const int r = (int) (0.0011 * sr);
            for (int i = len - 1; i >= r; --i) d[i] += 0.18f * d[i - r];
            for (int i = len - 256; i < len; ++i) d[i] *= (float) (len - i) / 256.0f;
        }

        double sr = 48000.0, osr = 192000.0;
        int latency = 0;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        Biquad hpC2[2], jfetShelf[2], jfetDC[2], ladderNotch[2], attackTilt[2], hpC7[2], driveLP[2], clipLin[2], c15[2];
        Biquad bridgedT[2], sk1[2], sk2[2], c21[2], inDC[2];
        Biquad eq[2][4];
        float attackGain = 1.0f;
        float delay[2][kDelay] {};
        int delayPos = 0;
        Smooth2 sMaster, sBlend, sLevel, sDrive, sBass, sLo, sHi, sTreb, sActive, sDriveOn, sCab, sLoF, sHiF;
        bool first = true, convIdle = true;
        int cachedGrunt = -1, cachedAttack = -1;
        juce::AudioBuffer<float> odBuf, cleanBuf, dryBuf, tmpBuf, builtInIR;
        juce::dsp::Convolution conv;
        std::atomic<bool> userIRLoaded { false };
    };
}
