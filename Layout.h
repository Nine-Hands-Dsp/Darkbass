// genere par tools/make_art.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    // ordre : master, blend, level, drive, bass, lomids, himids, treble
    const juce::Point<float> knobPos[8] = { { 240.60f, 184.00f }, { 493.17f, 184.00f }, { 746.83f, 184.00f }, { 999.40f, 184.00f }, { 240.60f, 380.00f }, { 493.17f, 380.00f }, { 746.83f, 380.00f }, { 999.40f, 380.00f } };
    const float knobR = 56.00f, knobTopR = 50.40f, eqR = 56.00f, eqTopR = 50.40f;
    // inverseurs 3 positions : grunt, lo freq, hi freq, attack
    const juce::Point<float> togglePos[4] = { { 240.60f, 576.00f }, { 493.17f, 576.00f }, { 746.83f, 576.00f }, { 999.40f, 576.00f } };
    const float toggleS = 140.00f;
    const juce::Point<float> ledL { 240.60f, 670.00f }, ledR { 999.40f, 670.00f }; const float ledS = 35.75f;
    const juce::Rectangle<float> glowL { 120.50f, 550.00f, 240.00f, 240.00f }, glowR { 879.50f, 550.00f, 240.00f, 240.00f };
    const juce::Point<float> fsL { 240.60f, 790.00f }, fsR { 999.40f, 790.00f }; const float footR = 37.88f, fsS = 190.00f;   // bouton / sprite
    const juce::Rectangle<float> statusRect { 388.00f, 668.00f, 464.00f, 66.00f }, presetRect { 436.00f, 768.00f, 368.00f, 40.00f };
    const float statusFont = 30.00f, presetFont = 24.00f, uiScale = 1.08f;
    const juce::Rectangle<float> boxRect { 78.00f, 56.00f, 1084.00f, 816.00f };   // boitier (notice)
}
