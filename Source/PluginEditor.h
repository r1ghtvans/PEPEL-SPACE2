#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

class PepelSpaceEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PepelSpaceEditor (PepelSpaceProcessor&);
    ~PepelSpaceEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // For the Snapshot tool: freezes animation at a given page / frame / glitch state.
    void prepareSnapshot (int page, int frames, bool glitch);

private:
    void timerCallback() override;
    void setPage (int p);
    bool glitchActive() const;

    void rebuildBackground();   // cached static scene
    void rebuildOverlays();     // cached scanlines + vignette

    void paintScene (juce::Graphics&);
    void paintHeader (juce::Graphics&);
    void paintPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent);
    void paintPanels (juce::Graphics&);
    void paintField (juce::Graphics&, juce::Rectangle<int> body);
    void paintMap (juce::Graphics&, juce::Rectangle<int> body);
    void paintAnalysis (juce::Graphics&, juce::Rectangle<int> panel);
    void paintScope (juce::Graphics&);

    PixelLookAndFeel lnf;    // must stay the first member; reset in the destructor
    PepelSpaceProcessor& proc;

    ParamRow rIn, rOut, rMix, rWidth, rMonoBass, rMid, rSide, rPan, rDepth, rHeight, rDelL, rDelR;
    ChoiceRow cSlope;
    PixelButton bBypass, tabSpace, tabPos;
    WindowPad padPhL, padPhR, padSwap, padMono;

    juce::Image bg, scanlines, vignette;
    int frame = 0, page = 0;
    bool forceGlitch = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelSpaceEditor)
};
