#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "PepelDsp.h"

class PepelSpaceProcessor : public juce::AudioProcessor
{
public:
    // BusesProperties is protected in AudioProcessor, so it is built inside a static member.
    static BusesProperties makeBuses()
    {
        return BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    PepelSpaceProcessor();
    ~PepelSpaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PEPEL SPACE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- data for the UI (written on the audio thread, read on the message thread) ----
    static constexpr int scopeColumns = 380;     // 380 columns * 2 px = 760 px
    static constexpr int scopeStride  = 128;     // samples per column
    static constexpr int gonioSize    = 512;
    std::atomic<float> outputLevel { 0.0f }, peakL { 0.0f }, peakR { 0.0f }, correlation { 0.0f };
    std::atomic<float> scopeMin[scopeColumns], scopeMax[scopeColumns];
    std::atomic<int>   scopeWrite { 0 };
    std::atomic<float> gonioL[gonioSize], gonioR[gonioSize];
    std::atomic<int>   gonioWrite { 0 };

private:
    pepel::Engine engine;
    std::atomic<float> *pIn, *pOut, *pMix, *pBypass, *pWidth, *pMonoBass, *pSlope, *pMid, *pSide, *pPan,
                       *pDepth, *pHeight, *pDelL, *pDelR, *pPhL, *pPhR, *pSwap, *pMonoChk;
    float curMin = 0.0f, curMax = 0.0f; int curCount = 0, gonioDecim = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelSpaceProcessor)
};
