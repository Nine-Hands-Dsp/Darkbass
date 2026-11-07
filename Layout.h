// genere par tools/make_art.py - ne pas modifier a la main (repere 1188 x 876, fond 2x)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    constexpr int frameW = 1188, frameH = 876;   // 30 unites de fond autour de la pedale (bout des jacks sur les cotes)
    // ordre : master, blend, level, drive, bass, lomids, himids, treble
    const juce::Point<float> knobPos[8] = { { 214.60f, 158.00f }, { 467.17f, 158.00f }, { 720.83f, 158.00f }, { 973.40f, 158.00f }, { 214.60f, 354.00f }, { 467.17f, 354.00f }, { 720.83f, 354.00f }, { 973.40f, 354.00f } };
    const float knobR = 56.00f, knobTopR = 50.40f, eqR = 56.00f, eqTopR = 50.40f;
    // inverseurs 3 positions : grunt, lo freq, hi freq, attack
    const juce::Point<float> togglePos[4] = { { 214.60f, 550.00f }, { 467.17f, 550.00f }, { 720.83f, 550.00f }, { 973.40f, 550.00f } };
    const float toggleS = 140.00f;
    const juce::Point<float> ledL { 214.60f, 644.00f }, ledR { 973.40f, 644.00f }; const float ledS = 35.75f;
    const juce::Rectangle<float> glowL { 94.50f, 524.00f, 240.00f, 240.00f }, glowR { 853.50f, 524.00f, 240.00f, 240.00f };
    const juce::Point<float> fsL { 214.60f, 764.00f }, fsR { 973.40f, 764.00f }; const float footR = 37.88f, fsS = 190.00f;   // bouton / sprite
    const juce::Rectangle<float> statusRect { 396.00f, 652.50f, 396.00f, 83.50f }, presetRect { 410.00f, 742.00f, 368.00f, 40.00f };
    const float statusFont = 30.00f, presetFont = 24.00f, uiScale = 1.08f;
    const juce::Rectangle<float> boxRect { 52.00f, 30.00f, 1084.00f, 816.00f };   // boitier (notice)
    const float jackY = 356.40f, jackOuterL = 30.00f, jackOuterR = 1158.00f;   // jacks (notice)
}
