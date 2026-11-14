#pragma once
// PEPEL SPACE - visual theme: post-soviet night urbanism + 90s VHS.
// Everything is drawn by hand; no stock JUCE widget look is used.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <memory>
#include "BinaryData.h"

//==============================================================================
namespace pal
{
inline const juce::Colour night       { 0xff04050e };
inline const juce::Colour navy        { 0xff090d26 };   // panel fill
inline const juce::Colour navy2       { 0xff111845 };   // empty segments, buttons
inline const juce::Colour frame       { 0xff2b3a82 };
inline const juce::Colour steel       { 0xff8e9ac8 };   // chains, wire
inline const juce::Colour steelDim    { 0xff4a5588 };
inline const juce::Colour lamp        { 0xffcfe0ff };   // street lamp light
inline const juce::Colour lampSoft    { 0xff9fb8ff };
inline const juce::Colour deepBlue    { 0xff2e44a8 };
inline const juce::Colour red         { 0xffe0182d };   // accent only
inline const juce::Colour redDim      { 0xff6b0f1f };
inline const juce::Colour text        { 0xffc9d6ff };
inline const juce::Colour textDim     { 0xff6f7db8 };
inline const juce::Colour white       { 0xfff2f6ff };
inline const juce::Colour glitchRed   { 0xffff2a3c };
inline const juce::Colour glitchBlue  { 0xff3c5cff };
}

//==============================================================================
// Fonts. The ONLY way to get a font anywhere in the project. Swap the .ttf files in Assets/.
namespace pixelfont
{
inline juce::Font make (int slot, float height)
{
    static const juce::Typeface::Ptr faces[3] = {
        juce::Typeface::createSystemTypefaceFor (BinaryData::FontTitle_ttf, (size_t) BinaryData::FontTitle_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::FontText_ttf,  (size_t) BinaryData::FontText_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::FontKanji_ttf, (size_t) BinaryData::FontKanji_ttfSize) };
    // point height = em-square size in px, so the numbers used across the UI mean the same for every font
    return juce::Font (juce::FontOptions (faces[slot]).withPointHeight (height));
}
inline juce::Font title (float h) { return make (0, h); }   // headings, panel names (Cyrillic needed)
inline juce::Font vhs   (float h) { return make (1, h); }   // main text and values
inline juce::Font kanji (float h) { return make (2, h); }   // hieroglyphs
}

//==============================================================================
// Small drawing primitives
namespace deco
{
using Pt = juce::Point<float>;

inline Pt bezier (Pt a, Pt b, Pt c, Pt d, float t)
{
    const float u = 1.0f - t;
    return { u*u*u*a.x + 3*u*u*t*b.x + 3*u*t*t*c.x + t*t*t*d.x,
             u*u*u*a.y + 3*u*u*t*b.y + 3*u*t*t*c.y + t*t*t*d.y };
}

struct Sample { Pt p; float angle; float arc; };

// Walks along a cubic Bezier with a constant arc-length step.
template <typename Fn>
inline void walkCurve (Pt a, Pt b, Pt c, Pt d, float step, Fn&& fn)
{
    constexpr int N = 400;
    Pt prev = a; float acc = 0.0f, next = 0.0f;
    for (int i = 1; i <= N; ++i)
    {
        const Pt q = bezier (a, b, c, d, (float) i / N);
        const float seg = prev.getDistanceFrom (q);
        while (acc + seg >= next && seg > 0.0f)
        {
            const float f = (next - acc) / seg;
            const Pt p = prev + (q - prev) * f;
            fn (Sample { p, std::atan2 (q.y - prev.y, q.x - prev.x), next });
            next += step;
        }
        acc += seg; prev = q;
    }
}

// Thin cross with a soft glow (blue and red "sparkle" stars, accents).
inline void drawStar (juce::Graphics& g, float cx, float cy, int r, juce::Colour c, float glow = 1.0f)
{
    if (glow > 0.0f)
    {
        const float gr = (float) r * 2.2f;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.38f * glow), cx, cy, c.withAlpha (0.0f), cx + gr, cy, true));
        g.fillEllipse (cx - gr, cy - gr, gr * 2, gr * 2);
    }
    const int x = (int) std::round (cx), y = (int) std::round (cy);
    g.setColour (c);
    g.fillRect (x - r, y, 2 * r + 1, 1);
    g.fillRect (x, y - r, 1, 2 * r + 1);
    g.setColour (c.withAlpha (0.55f));
    g.fillRect (x - 1, y - 1, 1, 1); g.fillRect (x + 1, y - 1, 1, 1);
    g.fillRect (x - 1, y + 1, 1, 1); g.fillRect (x + 1, y + 1, 1, 1);
    g.setColour (c.interpolatedWith (pal::white, 0.7f));
    g.fillRect (x, y, 1, 1);
}

// Chain along a Bezier; links alternate flat and edge-on.
inline void drawChain (juce::Graphics& g, Pt a, Pt b, Pt c, Pt d, juce::Colour col = pal::steel)
{
    int i = 0;
    walkCurve (a, b, c, d, 5.5f, [&] (const Sample& s)
    {
        juce::Graphics::ScopedSaveState st (g);
        g.addTransform (juce::AffineTransform::rotation (s.angle, s.p.x, s.p.y));
        if ((i++ & 1) == 0)
        {
            g.setColour (col);
            g.drawRoundedRectangle (s.p.x - 4.5f, s.p.y - 2.75f, 9.0f, 5.5f, 2.2f, 1.3f);
        }
        else
        {
            g.setColour (col.interpolatedWith (pal::steelDim, 0.35f));
            g.fillRect (s.p.x - 4.0f, s.p.y - 1.0f, 8.0f, 2.0f);
            g.setColour (col.withAlpha (0.8f));
            g.fillRect (s.p.x - 4.0f, s.p.y - 1.0f, 8.0f, 1.0f);
        }
    });
}

// Two twisted strands with cross-shaped barbs about every 26 px.
inline void drawBarbedWire (juce::Graphics& g, Pt a, Pt b, Pt c, Pt d, juce::Colour col = pal::steel)
{
    walkCurve (a, b, c, d, 1.0f, [&] (const Sample& s)
    {
        const float o = 1.6f * std::sin (s.arc * 0.55f);
        const float nx = -std::sin (s.angle), ny = std::cos (s.angle);
        g.setColour (col);
        g.fillRect (std::round (s.p.x + nx * o), std::round (s.p.y + ny * o), 1.0f, 1.0f);
        g.setColour (pal::steelDim);
        g.fillRect (std::round (s.p.x - nx * o), std::round (s.p.y - ny * o), 1.0f, 1.0f);
    });
    walkCurve (a, b, c, d, 26.0f, [&] (const Sample& s)
    {
        if (s.arc < 1.0f) return;
        const int x = (int) std::round (s.p.x), y = (int) std::round (s.p.y);
        g.setColour (col);
        g.fillRect (x - 3, y, 7, 1); g.fillRect (x, y - 3, 1, 7);
        g.setColour (pal::lampSoft.withAlpha (0.9f));
        g.fillRect (x, y, 1, 1);
    });
}

// Sprite from an array of strings: '#' = filled pixel.
inline void drawSprite (juce::Graphics& g, const char* const* rows, int numRows, int x, int y, int scale, juce::Colour c)
{
    g.setColour (c);
    for (int r = 0; r < numRows; ++r)
        for (int i = 0; rows[r][i] != 0; ++i)
            if (rows[r][i] == '#') g.fillRect (x + i * scale, y + r * scale, scale, scale);
}

// Double frame + corner marks in the accent colour.
inline void drawPanelBorders (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent)
{
    g.setColour (pal::frame);         g.drawRect (r, 1);
    g.setColour (pal::frame.withAlpha (0.5f)); g.drawRect (r.reduced (3), 1);
    g.setColour (accent);
    const int x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom();
    g.fillRect (x0, y0, 10, 2);      g.fillRect (x0, y0, 2, 10);
    g.fillRect (x1 - 10, y0, 10, 2); g.fillRect (x1 - 2, y0, 2, 10);
    g.fillRect (x0, y1 - 2, 10, 2);  g.fillRect (x0, y1 - 10, 2, 10);
    g.fillRect (x1 - 10, y1 - 2, 10, 2); g.fillRect (x1 - 2, y1 - 10, 2, 10);
}
inline void drawPanelFrame (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent)
{
    g.setColour (pal::navy); g.fillRect (r);
    drawPanelBorders (g, r, accent);
}

inline void dashedLineH (juce::Graphics& g, int x, int y, int w, int dash, int gap, juce::Colour c, int thick = 1)
{
    g.setColour (c);
    for (int i = 0; i < w; i += dash + gap) g.fillRect (x + i, y, std::min (dash, w - i), thick);
}
inline void dashedLineV (juce::Graphics& g, int x, int y, int h, int dash, int gap, juce::Colour c, int thick = 1)
{
    g.setColour (c);
    for (int i = 0; i < h; i += dash + gap) g.fillRect (x, y + i, thick, std::min (dash, h - i));
}
}

//==============================================================================
// Segmented slider look (no thumb): 6x14 segments, 2 px gap.
class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr int segW = 6, segH = 14, segGap = 2;

    PixelLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, pal::night);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 0; }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float, float, float,
                           const juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const int n = std::max (1, (width + segGap) / (segW + segGap));
        const float prop = (float) juce::jlimit (0.0, 1.0, s.valueToProportionOfLength (s.getValue()));
        const int filled = (int) std::lround (prop * (float) n);
        const int x0 = x + (width - (n * (segW + segGap) - segGap)) / 2;
        const int y0 = y + (height - segH) / 2;

        for (int i = 0; i < n; ++i)
        {
            const int sx = x0 + i * (segW + segGap);
            if (i < filled)
            {
                const auto c = pal::deepBlue.interpolatedWith (pal::lamp, n > 1 ? (float) i / (float) (n - 1) : 1.0f);
                g.setColour (c);                         g.fillRect (sx, y0, segW, segH);
                g.setColour (c.interpolatedWith (pal::white, 0.55f)); g.fillRect (sx, y0, segW, 2);
            }
            else
            {
                g.setColour (pal::navy2);                g.fillRect (sx, y0, segW, segH);
            }
        }
        // end-of-fill marker: white block with a 1 px red frame
        const int mi = juce::jmax (0, filled - 1);
        const int mx = x0 + mi * (segW + segGap);
        g.setColour (pal::red);   g.fillRect (mx - 1, y0 - 1, segW + 2, segH + 2);
        g.setColour (pal::white); g.fillRect (mx, y0, segW, segH);
    }
};

//==============================================================================
// Parameter row: name | segmented slider | value
class ParamRow : public juce::Component
{
public:
    static constexpr int rowH = 30, nameW = 100, valueW = 80;

    ParamRow (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label)
        : param (apvts.getParameter (paramId)), name (label), attachment (apvts, paramId, slider)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setScrollWheelEnabled (true);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void resized() override { slider.setBounds (nameW, 0, getWidth() - nameW - valueW, getHeight()); }

    void paint (juce::Graphics& g) override
    {
        g.setFont (pixelfont::vhs (24.0f));
        g.setColour (pal::text);
        g.drawText (name, 0, 0, nameW, getHeight(), juce::Justification::centredLeft, false);
        g.setColour (pal::textDim);
        g.drawText (param->getCurrentValueAsText(), getWidth() - valueW, 0, valueW, getHeight(),
                    juce::Justification::centredRight, false);
    }

private:
    juce::RangedAudioParameter* param;
    juce::String name;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// Hard-shadow button. Parameter-bound (bool) or free (onClick).
class PixelButton : public juce::Component
{
public:
    PixelButton (const juce::String& t, juce::Colour accentColour) : text (t), accent (accentColour) {}

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { setOn (v > 0.5f); }, nullptr);
        attachment->sendInitialUpdate();
    }
    void setOn (bool b) { if (on != b) { on = b; repaint(); } }
    bool isOn() const { return on; }
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        const int off = pressed ? 2 : 0;
        const auto body = juce::Rectangle<int> (off, off, getWidth() - 3, getHeight() - 3);
        g.setColour (pal::night);  g.fillRect (body.translated (3 - off, 3 - off));   // hard shadow
        g.setColour (on ? accent : pal::navy2); g.fillRect (body);
        g.setColour (accent);      g.drawRect (body, 2);
        g.setColour (on ? pal::night : pal::text);
        g.setFont (pixelfont::vhs (22.0f));
        g.drawText (text, body, juce::Justification::centred, false);
    }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        pressed = false; repaint();
        if (! getLocalBounds().contains (e.getPosition())) return;
        if (attachment) attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
        if (onClick) onClick();
    }

private:
    juce::String text; juce::Colour accent; bool on = false, pressed = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Choice row: left third = previous, otherwise next; mouse wheel scrolls.
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label,
               juce::Colour accentColour = pal::red)
        : name (label), accent (accentColour), param (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = (int) std::lround (v); repaint(); }, nullptr);
        attachment->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (pixelfont::vhs (24.0f));
        g.setColour (pal::text);
        g.drawText (name, 0, 0, ParamRow::nameW, getHeight(), juce::Justification::centredLeft, false);

        const auto box = getBoxBounds();
        g.setColour (pal::navy2); g.fillRect (box);
        g.setColour (accent);     g.drawRect (box, 2);
        const int cy = box.getCentreY();
        for (int i = 0; i < 4; ++i)   // pixel arrows
        {
            g.fillRect (box.getX() + 8 + i, cy - 3 + i, 1, 7 - 2 * i);
            g.fillRect (box.getRight() - 9 - i, cy - 3 + i, 1, 7 - 2 * i);
        }
        g.setColour (pal::text);
        g.drawText (param->choices[juce::jlimit (0, param->choices.size() - 1, index)], box, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto box = getBoxBounds();
        if (! box.contains (e.getPosition())) return;
        step (e.x < box.getX() + box.getWidth() / 3 ? -1 : 1);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) step (w.deltaY > 0 ? -1 : 1);
    }

private:
    juce::Rectangle<int> getBoxBounds() const { return { ParamRow::nameW, 3, getWidth() - ParamRow::nameW, getHeight() - 6 }; }
    void step (int dir)
    {
        const int n = param->choices.size();
        attachment->setValueAsCompleteGesture ((float) ((index + dir + n) % n));
    }
    juce::String name; juce::Colour accent; juce::AudioParameterChoice* param; int index = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Window-pad: a toggle drawn as a panel-block window (wall, glass, muntins, frame, sill).
// off = dark glass, hover = blue, just toggled = lamp-light flash, on (selected) = red.
class WindowPad : public juce::Component
{
public:
    WindowPad (const juce::String& label) : text (label) {}

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { setOn (v > 0.5f); }, nullptr);
        attachment->sendInitialUpdate();
        flash = 0;   // no lamp flash for the initial state
    }
    void setOn (bool b) { if (on != b) { on = b; flash = 2; repaint(); } }
    bool isOn() const { return on; }
    void tickFlash() { if (flash > 0) { --flash; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto w = getLocalBounds().withTrimmedBottom (22);
        g.setColour (pal::navy2.darker (0.35f)); g.fillRect (w);                   // wall
        g.setColour (pal::night.withAlpha (0.55f));
        for (int y = 4; y < w.getHeight(); y += 8) g.fillRect (w.getX(), y, w.getWidth(), 1);   // panel seams
        const auto outer = w.reduced (8, 7);
        g.setColour (pal::steelDim);  g.fillRect (outer);                          // frame
        const auto glass = outer.reduced (3);
        juce::Colour gc = pal::night;
        if (flash > 0)       gc = pal::lamp;
        else if (on)         gc = pal::red;
        else if (hover)      gc = pal::deepBlue;
        g.setColour (gc);             g.fillRect (glass);
        if (on && flash == 0) { g.setColour (pal::redDim); g.fillRect (glass.withTop (glass.getCentreY() + 4)); }
        if (! on && flash == 0)
        {
            g.setColour (pal::navy2.withAlpha (0.8f)); g.fillRect (glass.withHeight (6));
        }
        g.setColour (pal::steelDim);                                                // muntins
        g.fillRect (glass.getCentreX() - 1, glass.getY(), 2, glass.getHeight());
        g.fillRect (glass.getX(), glass.getCentreY() - 1, glass.getWidth(), 2);
        g.setColour (pal::steel);     g.fillRect (outer.getX() - 3, outer.getBottom(), outer.getWidth() + 6, 3);   // sill
        if (on || flash > 0)
        {
            g.setGradientFill (juce::ColourGradient ((flash > 0 ? pal::lamp : pal::red).withAlpha (0.28f), (float) glass.getCentreX(), (float) glass.getCentreY(),
                                                     juce::Colours::transparentBlack, (float) glass.getCentreX() + 46.0f, (float) glass.getCentreY(), true));
            g.fillEllipse ((float) glass.getCentreX() - 46.0f, (float) glass.getCentreY() - 46.0f, 92.0f, 92.0f);
        }
        g.setFont (pixelfont::vhs (22.0f));
        g.setColour (on ? pal::white : pal::textDim);
        g.drawText (text, 0, getHeight() - 22, getWidth(), 22, juce::Justification::centred, false);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && attachment)
            attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }

private:
    juce::String text; bool on = false, hover = false; int flash = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
