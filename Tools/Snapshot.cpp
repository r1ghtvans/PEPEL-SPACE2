// Snapshot - renders the real editor to PNG (createComponentSnapshot), no DAW / window needed.
//   Snapshot <output-dir>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void feed (PepelSpaceProcessor& proc, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; juce::Random rnd (42);
    double ph = 0.0, ph2 = 0.0; const double fs = 48000.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float env = 0.55f + 0.4f * (float) std::sin (6.283185 * (b * 512 + i) / fs * 0.9);
            const float bass = (float) std::sin (ph) * 0.5f, lead = (float) std::sin (ph2) * 0.3f + (rnd.nextFloat() - 0.5f) * 0.06f;
            buf.setSample (0, i, env * (bass + lead));
            buf.setSample (1, i, env * (bass + 0.6f * (float) std::sin (ph2 * 1.5) * 0.3f + (rnd.nextFloat() - 0.5f) * 0.06f));
            ph += 6.283185 * 82.0 / fs; ph2 += 6.283185 * 440.0 / fs;
        }
        proc.processBlock (buf, midi);
    }
}

static void save (juce::Component& c, const juce::File& dir, const juce::String& name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    auto file = dir.getChildFile (name);
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    PepelSpaceProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set ("width", 1.35f); set ("monoBass", 110.0f); set ("slope", 1.0f); set ("midGain", 1.5f); set ("sideGain", -3.0f);
    set ("inGain", 3.0f); set ("outGain", -6.0f); set ("mix", 0.85f);
    set ("pan", -0.25f); set ("depth", 0.35f); set ("height", 0.4f); set ("delayL", 6.5f); set ("delayR", 12.0f);
    set ("phaseR", 1.0f);
    feed (proc, 260);

    std::unique_ptr<PepelSpaceEditor> ed (static_cast<PepelSpaceEditor*> (proc.createEditor()));
    ed->prepareSnapshot (0, 37, false);  save (*ed, dir, "page1_space.png");
    ed->prepareSnapshot (1, 83, false);  save (*ed, dir, "page2_position.png");
    ed->prepareSnapshot (0, 150, true);  save (*ed, dir, "page1_glitch.png");

    // anti-phase state: feed an inverted right channel
    set ("phaseR", 1.0f); set ("phaseL", 0.0f); set ("width", 1.0f); set ("pan", 0.0f); set ("delayL", 0.0f); set ("delayR", 0.0f); set ("mix", 1.0f);
    set ("monoBass", 20.0f); set ("depth", 0.0f); set ("height", 0.0f); set ("midGain", 0.0f); set ("sideGain", 0.0f); set ("outGain", 0.0f); set ("inGain", 0.0f);
    {
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer m; double ph = 0;
        for (int b = 0; b < 200; ++b) { for (int i = 0; i < 512; ++i) { const float v = 0.8f * (float) std::sin (ph); buf.setSample (0, i, v); buf.setSample (1, i, v * 0.95f); ph += 6.283185 * 220.0 / 48000.0; } proc.processBlock (buf, m); }
    }
    ed->prepareSnapshot (1, 211, false); save (*ed, dir, "page2_antiphase.png");
    ed.reset();
    return 0;
}
