#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Attr = juce::AudioParameterFloatAttributes;

juce::String fmt (const char* f, double v) { return juce::String::formatted (f, v); }
auto dbText   = [] (float v, int) { return fmt ("%+.1f dB", v); };
auto pctText  = [] (float v, int) { return fmt ("%.0f%%", v * 100.0); };
auto msText   = [] (float v, int) { return fmt ("%.1f ms", v); };
}

juce::AudioProcessorValueTreeState::ParameterLayout PepelSpaceProcessor::createLayout()
{
    using namespace juce;
    using NR = NormalisableRange<float>;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto F = [&] (const char* id, const char* name, NR r, float def, std::function<String (float, int)> txt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                            Attr().withStringFromValueFunction (std::move (txt))));
    };
    auto B = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    F ("inGain",  "Input Gain",  NR (-24.0f, 24.0f, 0.1f), 0.0f, dbText);
    F ("outGain", "Output Gain", NR (-24.0f, 24.0f, 0.1f), 0.0f, dbText);
    F ("mix",     "Dry/Wet",     NR (0.0f, 1.0f, 0.01f),   1.0f, pctText);
    B ("bypass",  "Bypass", false);

    F ("width",    "Width",     NR (0.0f, 2.0f, 0.01f), 1.0f, pctText);
    F ("monoBass", "Mono Bass", NR (20.0f, 300.0f, 1.0f, 0.5f), 20.0f,
       [] (float v, int) { return v <= 20.5f ? String ("OFF") : fmt ("%.0f Hz", v); });
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "slope", 1 }, "Mono Bass Slope",
                                                         StringArray { "6 dB/oct", "12 dB/oct" }, 0));
    F ("midGain",  "Mid Gain",  NR (-24.0f, 12.0f, 0.1f), 0.0f, dbText);
    F ("sideGain", "Side Gain", NR (-24.0f, 12.0f, 0.1f), 0.0f, dbText);

    F ("pan",    "Pan",    NR (-1.0f, 1.0f, 0.01f), 0.0f,
       [] (float v, int) { const int a = (int) std::lround (std::abs (v) * 100.0f);
                           return a == 0 ? String ("C") : String (v < 0 ? "L " : "R ") + String (a); });
    F ("depth",  "Depth",  NR (0.0f, 1.0f, 0.01f),  0.0f, pctText);
    F ("height", "Height", NR (-1.0f, 1.0f, 0.01f), 0.0f,
       [] (float v, int) { return fmt ("%+.0f%%", v * 100.0); });

    F ("delayL", "Delay L", NR (0.0f, 30.0f, 0.1f), 0.0f, msText);
    F ("delayR", "Delay R", NR (0.0f, 30.0f, 0.1f), 0.0f, msText);

    B ("phaseL", "Phase Invert L", false);
    B ("phaseR", "Phase Invert R", false);
    B ("swap",   "Channel Swap",   false);
    B ("monoCheck", "Mono Check",  false);
    return { p.begin(), p.end() };
}

PepelSpaceProcessor::PepelSpaceProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PEPELSPACE", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pIn = raw ("inGain"); pOut = raw ("outGain"); pMix = raw ("mix"); pBypass = raw ("bypass");
    pWidth = raw ("width"); pMonoBass = raw ("monoBass"); pSlope = raw ("slope");
    pMid = raw ("midGain"); pSide = raw ("sideGain"); pPan = raw ("pan"); pDepth = raw ("depth");
    pHeight = raw ("height"); pDelL = raw ("delayL"); pDelR = raw ("delayR");
    pPhL = raw ("phaseL"); pPhR = raw ("phaseR"); pSwap = raw ("swap"); pMonoChk = raw ("monoCheck");
    for (auto& a : scopeMin) a.store (0.0f);
    for (auto& a : scopeMax) a.store (0.0f);
    for (auto& a : gonioL) a.store (0.0f);
    for (auto& a : gonioR) a.store (0.0f);
}

void PepelSpaceProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    curMin = curMax = 0.0f; curCount = 0; gonioDecim = 0;
}

bool PepelSpaceProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PepelSpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0) return;

    pepel::Params p;
    p.inGainDb = pIn->load();  p.outGainDb = pOut->load();  p.mix = pMix->load();
    p.width = pWidth->load();  p.monoBassHz = pMonoBass->load();
    p.midDb = pMid->load();    p.sideDb = pSide->load();
    p.pan = pPan->load();      p.depth = pDepth->load();    p.height = pHeight->load();
    p.delayLms = pDelL->load(); p.delayRms = pDelR->load();
    p.bypass = pBypass->load() > 0.5f;  p.phaseL = pPhL->load() > 0.5f;  p.phaseR = pPhR->load() > 0.5f;
    p.swap = pSwap->load() > 0.5f;      p.monoCheck = pMonoChk->load() > 0.5f;
    p.slope12 = pSlope->load() > 0.5f;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    engine.process (L, R, n, p);

    outputLevel.store (engine.level(), std::memory_order_relaxed);
    peakL.store (engine.peakL(), std::memory_order_relaxed);
    peakR.store (engine.peakR(), std::memory_order_relaxed);
    correlation.store (engine.correlation(), std::memory_order_relaxed);

    // oscilloscope: one min/max column per 128 samples, lock-free ring
    int w = scopeWrite.load (std::memory_order_relaxed);
    int gw = gonioWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.5f * (L[i] + R[i]);
        if (curCount == 0) curMin = curMax = v;
        curMin = std::min (curMin, v);
        curMax = std::max (curMax, v);
        if (++curCount >= scopeStride)
        {
            const int idx = w % scopeColumns;
            scopeMin[idx].store (curMin, std::memory_order_relaxed);
            scopeMax[idx].store (curMax, std::memory_order_relaxed);
            w = (w + 1) % scopeColumns;
            scopeWrite.store (w, std::memory_order_release);
            curCount = 0;
        }
        if (++gonioDecim >= 4)
        {
            gonioDecim = 0;
            gonioL[gw].store (L[i], std::memory_order_relaxed);
            gonioR[gw].store (R[i], std::memory_order_relaxed);
            gw = (gw + 1) % gonioSize;
        }
    }
    gonioWrite.store (gw, std::memory_order_release);
}

juce::AudioProcessorEditor* PepelSpaceProcessor::createEditor() { return new PepelSpaceEditor (*this); }

void PepelSpaceProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PepelSpaceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PepelSpaceProcessor(); }
