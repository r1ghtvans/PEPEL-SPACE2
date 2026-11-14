#include "PluginEditor.h"

namespace lay
{
constexpr int W = 800, H = 690, headerH = 118;
constexpr int colX[2] = { 20, 410 }, colW = 370;
constexpr int row1Y = 130, row1H = 204, row2Y = 342, row2H = 152;
constexpr int anY = 502, anH = 106;
constexpr int scopeY = 616, scopeH = 46, sigY = 664, checkY = 682;
}

//==============================================================================
namespace scene
{
struct Block { int x, w, h; };

// tall panel blocks (windows are animated), low far houses (other tint), lamp, tower
const Block panelki[]    = { { 340, 64, 58 }, { 406, 48, 84 }, { 456, 72, 70 }, { 530, 56, 92 },
                             { 588, 66, 64 }, { 656, 40, 52 }, { 700, 84, 40 }, { 8, 52, 30 }, { 64, 58, 22 } };
const Block farHouses[]  = { { 0, 44, 16 }, { 40, 56, 22 }, { 120, 52, 18 }, { 176, 60, 26 }, { 240, 40, 16 },
                             { 282, 52, 24 }, { 636, 54, 20 }, { 690, 36, 14 }, { 724, 48, 24 }, { 768, 32, 18 } };
constexpr int lampX = 334, lampTopY = 62, towerX = 492, towerBaseY = 48, towerTopY = 14;

struct Star { int x, y, kind; };   // kind 1 = red
const Star stars[] = { { 44, 16, 0 }, { 96, 36, 0 }, { 150, 12, 0 }, { 226, 26, 1 }, { 300, 10, 0 }, { 258, 50, 0 },
                       { 392, 14, 0 }, { 430, 42, 0 }, { 498, 8, 1 }, { 560, 22, 0 }, { 620, 8, 0 }, { 600, 46, 0 },
                       { 676, 32, 0 }, { 744, 14, 0 }, { 772, 44, 1 }, { 20, 56, 0 } };

const char* const cloudA[] = { "......####..........", "....########.####...", "..###########.######", "####################",
                               ".##################.", "...##############..." };
const char* const cloudB[] = { "....###.........", "..#######.####..", "################", ".##############.", "...##########..." };
const char* const cloudC[] = { "...######.........", ".###########.####.", "##################", ".################." , "..##############.." };
struct Cloud { const char* const* rows; int n, w, y, scale; float speed, offset; };
const Cloud clouds[] = { { cloudA, 6, 20, 12, 4, 0.22f, 120.0f }, { cloudB, 5, 16, 40, 3, 0.15f, 520.0f }, { cloudC, 5, 18, 6, 4, 0.30f, 880.0f } };

inline uint32_t hash3 (int a, int b, int c)
{
    uint32_t h = (uint32_t) a * 73856093u ^ (uint32_t) b * 19349663u ^ (uint32_t) c * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

inline void bareTree (juce::Graphics& g, float x, float y, float angle, float len, int depth, juce::Random& rng)
{
    if (depth <= 0) return;
    const float x2 = x + std::cos (angle) * len, y2 = y + std::sin (angle) * len;
    g.setColour (pal::night.brighter (0.12f));
    g.drawLine (x, y, x2, y2, 0.5f + (float) depth * 0.45f);
    const float spread = 0.35f + rng.nextFloat() * 0.35f;
    bareTree (g, x2, y2, angle - spread, len * (0.68f + rng.nextFloat() * 0.1f), depth - 1, rng);
    bareTree (g, x2, y2, angle + spread * (0.6f + rng.nextFloat() * 0.5f), len * (0.66f + rng.nextFloat() * 0.12f), depth - 1, rng);
    if (depth > 3 && rng.nextInt (3) == 0)
        bareTree (g, x2, y2, angle + (rng.nextFloat() - 0.5f) * 0.3f, len * 0.6f, depth - 2, rng);
}

inline void aberrated (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> area, const juce::Font& f,
                       int shift, juce::Justification j, juce::Colour main)
{
    g.setFont (f);
    g.setColour (pal::glitchBlue.withAlpha (0.9f)); g.drawText (s, area.translated (shift, 0), j, false);
    g.setColour (pal::glitchRed.withAlpha (0.9f));  g.drawText (s, area.translated (-shift, 0), j, false);
    g.setColour (main);                             g.drawText (s, area, j, false);
}
}

//==============================================================================
PepelSpaceEditor::PepelSpaceEditor (PepelSpaceProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      rIn (p.apvts, "inGain", "Input"), rOut (p.apvts, "outGain", "Output"), rMix (p.apvts, "mix", "Mix"),
      rWidth (p.apvts, "width", "Width"), rMonoBass (p.apvts, "monoBass", "Mono Bass"),
      rMid (p.apvts, "midGain", "Mid"), rSide (p.apvts, "sideGain", "Side"),
      rPan (p.apvts, "pan", "Pan"), rDepth (p.apvts, "depth", "Depth"), rHeight (p.apvts, "height", "Height"),
      rDelL (p.apvts, "delayL", "Delay L"), rDelR (p.apvts, "delayR", "Delay R"),
      cSlope (p.apvts, "slope", "Slope"),
      bBypass ("BYPASS", pal::red), tabSpace ("SPACE", pal::lampSoft), tabPos ("POSITION", pal::lampSoft),
      padPhL ("PHASE L"), padPhR ("PHASE R"), padSwap ("SWAP"), padMono ("MONO")
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &rIn, &rOut, &rMix, &rWidth, &rMonoBass, &rMid, &rSide,
                                                              &rPan, &rDepth, &rHeight, &rDelL, &rDelR, &cSlope,
                                                              &bBypass, &tabSpace, &tabPos, &padPhL, &padPhR, &padSwap, &padMono })
        addAndMakeVisible (c);

    bBypass.attach (p.apvts, "bypass");
    padPhL.attach (p.apvts, "phaseL"); padPhR.attach (p.apvts, "phaseR");
    padSwap.attach (p.apvts, "swap");  padMono.attach (p.apvts, "monoCheck");
    tabSpace.onClick = [this] { setPage (0); };
    tabPos.onClick   = [this] { setPage (1); };

    setSize (lay::W, lay::H);
    setResizable (false, false);
    setOpaque (true);
    rebuildBackground();
    rebuildOverlays();
    setPage (0);
    startTimerHz (15);   // deliberately choppy retro frame rate
}

PepelSpaceEditor::~PepelSpaceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PepelSpaceEditor::resized()
{
    using namespace lay;
    auto row = [] (juce::Component& c, int col, int i) { c.setBounds (colX[col] + 14, row1Y + 44 + i * ParamRow::rowH, 342, ParamRow::rowH); };
    row (rIn, 0, 0); row (rOut, 0, 1); row (rMix, 0, 2);
    bBypass.setBounds (colX[0] + 14, row1Y + 44 + 3 * ParamRow::rowH + 14, 342, 36);
    juce::Component* pads[] = { &padPhL, &padPhR, &padSwap, &padMono };
    for (int i = 0; i < 4; ++i) pads[i]->setBounds (colX[0] + 14 + i * 88, row2Y + 44, 78, 96);
    row (rWidth, 1, 0); row (rMonoBass, 1, 1); row (cSlope, 1, 2); row (rMid, 1, 3); row (rSide, 1, 4);
    row (rPan, 0, 0); row (rDepth, 0, 1); row (rHeight, 0, 2);
    row (rDelL, 1, 0); row (rDelR, 1, 1);
    tabSpace.setBounds (20, 76, 92, 26);
    tabPos.setBounds (118, 76, 116, 26);
}

void PepelSpaceEditor::setPage (int p)
{
    page = p;
    for (auto* c : std::initializer_list<juce::Component*> { &rIn, &rOut, &rMix, &bBypass, &padPhL, &padPhR, &padSwap, &padMono,
                                                              &rWidth, &rMonoBass, &cSlope, &rMid, &rSide })
        c->setVisible (p == 0);
    for (auto* c : std::initializer_list<juce::Component*> { &rPan, &rDepth, &rHeight, &rDelL, &rDelR })
        c->setVisible (p == 1);
    tabSpace.setOn (p == 0);
    tabPos.setOn (p == 1);
    repaint();
}

void PepelSpaceEditor::prepareSnapshot (int pg, int frames, bool glitch)
{
    stopTimer();
    setPage (pg);
    frame = frames;
    forceGlitch = glitch;
}

void PepelSpaceEditor::timerCallback()
{
    ++frame;
    padPhL.tickFlash(); padPhR.tickFlash(); padSwap.tickFlash(); padMono.tickFlash();
    repaint();
}

bool PepelSpaceEditor::glitchActive() const
{
    if (forceGlitch) return true;
    const int epoch = frame / 60, f = frame % 60;           // about once per 4 s at 15 Hz
    const uint32_t h = scene::hash3 (epoch, 77, 5);
    const int start = 6 + (int) (h % 48u), len = 2 + (int) ((h >> 8) % 3u);   // 2..4 frames
    return f >= start && f < start + len;
}

//==============================================================================
// Cached images
void PepelSpaceEditor::rebuildBackground()
{
    using namespace lay;
    bg = juce::Image (juce::Image::RGB, W, H, true);
    juce::Graphics g (bg);

    // sky: 6 px bands quantised to 14 steps (limited palette, never smoothed)
    const juce::Colour skyA (0xff04051a), skyB (0xff1b2766), yardA (0xff0b1131);
    for (int y = 0; y < headerH; y += 6)
    {
        const int step = std::min (13, (y + 3) * 14 / headerH);
        g.setColour (skyA.interpolatedWith (skyB, (float) step / 13.0f));
        g.fillRect (0, y, W, std::min (6, headerH - y));
    }
    for (int y = headerH; y < H; y += 6)
    {
        const int step = std::min (13, (y - headerH + 3) * 14 / (H - headerH));
        g.setColour (yardA.interpolatedWith (pal::night, (float) step / 13.0f));
        g.fillRect (0, y, W, std::min (6, H - y));
    }

    // moon: soft halo, disc, dark craters
    g.setGradientFill (juce::ColourGradient (pal::lamp.withAlpha (0.24f), 356.0f, 30.0f, pal::lamp.withAlpha (0.0f), 392.0f, 30.0f, true));
    g.fillEllipse (320.0f, -6.0f, 72.0f, 72.0f);
    g.setColour (pal::lamp);  g.fillEllipse (343.0f, 17.0f, 26.0f, 26.0f);
    g.setColour (pal::lampSoft.interpolatedWith (pal::deepBlue, 0.55f));
    g.fillEllipse (349.0f, 22.0f, 7.0f, 7.0f); g.fillEllipse (359.0f, 31.0f, 5.0f, 5.0f); g.fillEllipse (351.0f, 33.0f, 4.0f, 4.0f);

    // far houses
    const auto farCol = pal::navy.interpolatedWith (pal::deepBlue, 0.22f);
    for (auto& b : scene::farHouses)
    {
        g.setColour (farCol);  g.fillRect (b.x, headerH - b.h, b.w, b.h);
        g.setColour (pal::frame.withAlpha (0.25f)); g.fillRect (b.x, headerH - b.h, b.w, 1);
    }

    // radio tower on a roof
    {
        using namespace scene;
        g.setColour (pal::night.brighter (0.2f));
        g.drawLine ((float) towerX - 6, (float) towerBaseY, (float) towerX, (float) towerTopY, 1.0f);
        g.drawLine ((float) towerX + 6, (float) towerBaseY, (float) towerX, (float) towerTopY, 1.0f);
        for (int y = towerBaseY - 4; y > towerTopY + 4; y -= 6)
        {
            const float half = 6.0f * (float) (y - towerTopY) / (float) (towerBaseY - towerTopY);
            g.drawLine (towerX - half, (float) y, towerX + half, (float) y, 1.0f);
            g.drawLine (towerX - half, (float) y, towerX + half * 0.0f, (float) y - 6, 0.6f);
        }
        g.fillRect (towerX, towerTopY - 5, 1, 6);
    }

    // panel blocks with dark windows (lit ones are animated in paint)
    const auto blockCol = pal::navy.interpolatedWith (pal::night, 0.25f);
    for (auto& b : scene::panelki)
    {
        const int top = headerH - b.h;
        g.setColour (blockCol);  g.fillRect (b.x, top, b.w, b.h);
        g.setColour (pal::frame.withAlpha (0.35f)); g.fillRect (b.x, top, b.w, 1);
        g.setColour (pal::night.interpolatedWith (pal::navy2, 0.45f));
        for (int wy = top + 5; wy + 5 <= headerH - 3; wy += 10)
            for (int wx = b.x + 4; wx + 4 <= b.x + b.w - 3; wx += 8)
                g.fillRect (wx, wy, 4, 5);
    }

    // bare trees (deterministic)
    {
        const float xs[] = { 262.0f, 296.0f, 698.0f }, ls[] = { 11.0f, 9.0f, 12.0f };
        for (int i = 0; i < 3; ++i)
        {
            juce::Random rng (1234 + i * 77);
            scene::bareTree (g, xs[i], (float) headerH, -juce::MathConstants<float>::halfPi, ls[i], 6, rng);
        }
    }

    // street lamp silhouette
    {
        using namespace scene;
        g.setColour (pal::night.brighter (0.1f));
        g.fillRect (lampX - 1, lampTopY, 3, headerH - lampTopY);
        g.fillRect (lampX - 1, lampTopY - 1, 14, 2);
        g.fillRect (lampX + 8, lampTopY - 1, 8, 5);
        g.fillRect (lampX - 3, headerH - 5, 7, 5);
    }

    // ground line under the scene
    g.setColour (pal::night); g.fillRect (0, headerH - 2, W, 2);

    // barbed wire stretched above the panels, chains along both edges
    deco::drawBarbedWire (g, { 0.0f, 119.0f }, { 260.0f, 132.0f }, { 540.0f, 132.0f }, { 800.0f, 119.0f });
    deco::drawChain (g, { 12.0f, 126.0f }, { 5.0f, 190.0f }, { 17.0f, 270.0f }, { 11.0f, 340.0f });
    deco::drawChain (g, { 788.0f, 126.0f }, { 795.0f, 190.0f }, { 783.0f, 270.0f }, { 789.0f, 340.0f });
    g.setColour (pal::steel); g.drawEllipse (8.0f, 122.0f, 8.0f, 8.0f, 1.5f); g.drawEllipse (784.0f, 122.0f, 8.0f, 8.0f, 1.5f);

    // dashed strip on the left, checker strip at the bottom
    deco::dashedLineV (g, 3, 134, 538, 8, 6, pal::steelDim, 2);
    for (int x = 0; x < W; x += 4)
        for (int r = 0; r < 2; ++r)
        {
            g.setColour (((x / 4 + r) & 1) ? pal::frame : pal::navy2);
            g.fillRect (x, checkY + r * 4, 4, 4);
        }
}

void PepelSpaceEditor::rebuildOverlays()
{
    using namespace lay;
    scanlines = juce::Image (juce::Image::ARGB, W, H, true);
    {
        juce::Graphics g (scanlines);
        g.setColour (juce::Colours::black.withAlpha (0.26f));
        for (int y = 0; y < H; y += 3) g.fillRect (0, y, W, 1);      // every 3rd row
    }
    vignette = juce::Image (juce::Image::ARGB, W, H, true);
    {
        juce::Graphics g (vignette);
        juce::ColourGradient cg (juce::Colours::transparentBlack, W * 0.5f, H * 0.5f,
                                 juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
        cg.addColour (0.45, juce::Colours::transparentBlack);
        g.setGradientFill (cg);
        g.fillAll();
    }
}

//==============================================================================
// Dynamic scene: stars, clouds, window lights, lamp glow, tower beacon
void PepelSpaceEditor::paintScene (juce::Graphics& g)
{
    using namespace lay;
    const float level = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load());

    int i = 0;
    for (auto& s : scene::stars)
    {
        const int ph = (frame / 2 + i * 3) % 6;
        const int r = 2 + (ph < 2 ? 2 : (ph < 4 ? 1 : 0));
        deco::drawStar (g, (float) s.x, (float) s.y, r, s.kind ? pal::red : pal::lampSoft, 0.6f + 0.1f * (float) r);
        ++i;
    }

    for (auto& c : scene::clouds)
    {
        const int w = c.w * c.scale;
        const float x = std::fmod ((float) frame * c.speed + c.offset, (float) (W + w)) - (float) w;
        deco::drawSprite (g, c.rows, c.n, (int) x, c.y, c.scale, pal::night.withAlpha (0.72f));
    }

    // windows: lit state comes from a hash of (position, frame/40); ~24 % lit, ~3 % of those red
    const int tick = frame / 40;
    for (auto& b : scene::panelki)
    {
        const int top = headerH - b.h;
        for (int wy = top + 5; wy + 5 <= headerH - 3; wy += 10)
            for (int wx = b.x + 4; wx + 4 <= b.x + b.w - 3; wx += 8)
            {
                const uint32_t h = scene::hash3 (wx, wy, tick);
                if (h % 1000u >= 240u) continue;
                const bool redWin = ((h >> 10) % 1000u) < 30u;
                g.setColour (redWin ? pal::red : ((h >> 20) & 1u ? pal::lampSoft : pal::lamp).withAlpha (0.9f));
                g.fillRect (wx, wy, 4, 5);
            }
    }

    // street lamp glow with a small pulse that follows the output level
    const float pulse = 0.88f + 0.08f * std::sin ((float) frame * 0.42f) + 0.2f * level;
    const float lx = (float) scene::lampX + 12.0f, ly = (float) scene::lampTopY + 3.0f;
    g.setGradientFill (juce::ColourGradient (pal::lamp.withAlpha (0.5f * pulse), lx, ly, pal::lamp.withAlpha (0.0f), lx + 40.0f, ly, true));
    g.fillEllipse (lx - 40.0f, ly - 40.0f, 80.0f, 80.0f);
    g.setColour (pal::white.withAlpha (juce::jlimit (0.0f, 1.0f, pulse))); g.fillRect ((int) lx - 3, (int) ly, 7, 3);

    // tower beacon
    if ((frame % 12) < 5)
    {
        const float bx = (float) scene::towerX + 0.5f, by = (float) scene::towerTopY - 5.0f;
        g.setGradientFill (juce::ColourGradient (pal::red.withAlpha (0.65f), bx, by, pal::red.withAlpha (0.0f), bx + 11.0f, by, true));
        g.fillEllipse (bx - 11.0f, by - 11.0f, 22.0f, 22.0f);
        g.setColour (pal::glitchRed); g.fillRect ((int) bx - 1, (int) by - 1, 3, 3);
    }
}

void PepelSpaceEditor::paintHeader (juce::Graphics& g)
{
    const bool gl = glitchActive();
    scene::aberrated (g, "PEPEL SPACE", { 20, 8, 330, 34 }, pixelfont::title (28.0f), gl ? 4 : 2,
                      juce::Justification::centredLeft, pal::white);
    g.setFont (pixelfont::vhs (22.0f));
    g.setColour (pal::textDim);
    g.drawText ("stereo space processor", 22, 44, 300, 22, juce::Justification::centredLeft, false);
    scene::aberrated (g, juce::String::fromUTF8 ("\xE7\x81\xB0\xE7\xA9\xBA"), { 672, 4, 112, 56 }, pixelfont::kanji (46.0f),
                      gl ? 5 : 3, juce::Justification::centredRight, pal::white);
}

void PepelSpaceEditor::paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
{
    deco::drawPanelFrame (g, r, accent);
    g.setColour (pal::navy2); g.fillRect (r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 35);
    deco::dashedLineH (g, r.getX() + 1, r.getY() + 36, r.getWidth() - 2, 4, 3, pal::steelDim);
    deco::drawStar (g, (float) r.getX() + 24.0f, (float) r.getY() + 18.0f, 5, accent, 0.8f);
    g.setFont (pixelfont::title (13.0f));
    g.setColour (pal::text);
    g.drawText (title, r.getX() + 40, r.getY() + 2, r.getWidth() - 60, 34, juce::Justification::centredLeft, false);
    deco::drawPanelBorders (g, r, accent); // borders + corner marks stay on top of the header strip
}

void PepelSpaceEditor::paintPanels (juce::Graphics& g)
{
    using namespace lay;
    auto R = [] (int col, int y, int h) { return juce::Rectangle<int> (colX[col], y, colW, h); };
    const float corr = proc.correlation.load();
    const bool anti = corr < -0.05f;

    if (page == 0)
    {
        paintPanel (g, R (0, row1Y, row1H), "IN-OUT", pal::red);
        paintPanel (g, R (1, row1Y, row1H), "STEREO", pal::lampSoft);
        paintPanel (g, R (0, row2Y, row2H), "UTILS", pal::red);
    }
    else
    {
        paintPanel (g, R (0, row1Y, row1H), "3D POS", pal::red);
        paintPanel (g, R (1, row1Y, row1H), "HAAS", pal::lampSoft);
        paintPanel (g, R (0, row2Y, row2H), "MAP", pal::red);
        g.setFont (pixelfont::vhs (20.0f));
        g.setColour (pal::textDim);
        g.drawText ("FAR = DARK + QUIET",       colX[0] + 14, row1Y + 148, 342, 20, juce::Justification::centredLeft, false);
        g.drawText ("UP = BRIGHT, DOWN = DULL", colX[0] + 14, row1Y + 170, 342, 20, juce::Justification::centredLeft, false);
        g.setColour (pal::red);
        g.drawText ("NO PDC REPORTED",          colX[1] + 14, row1Y + 120, 342, 22, juce::Justification::centredLeft, false);
        g.setColour (pal::text);
        g.drawText ("USE AT 100% WET",          colX[1] + 14, row1Y + 144, 342, 22, juce::Justification::centredLeft, false);
        g.setColour (pal::textDim);
        g.drawText ("SMALL DELAY = WIDE IMAGE", colX[1] + 14, row1Y + 168, 342, 22, juce::Justification::centredLeft, false);
    }
    paintPanel (g, R (1, row2Y, row2H), "FIELD", pal::lampSoft);
    paintField (g, { colX[1] + 14, row2Y + 44, 342, 100 });
    if (page == 1) paintMap (g, { colX[0] + 14, row2Y + 44, 342, 100 });

    paintPanel (g, { colX[0], anY, 760, anH }, "ANALYSIS", anti ? pal::red : pal::lampSoft);
    paintAnalysis (g, { colX[0], anY, 760, anH });
}

void PepelSpaceEditor::paintField (juce::Graphics& g, juce::Rectangle<int> body)
{
    const int cx = body.getCentreX(), cy = body.getCentreY(), R = 48;
    g.setColour (pal::night); g.fillRect (cx - 50, body.getY(), 100, 100);
    g.setColour (pal::frame.withAlpha (0.8f)); g.drawRect (cx - 50, body.getY(), 100, 100, 1);
    deco::dashedLineV (g, cx, body.getY() + 2, 96, 3, 3, pal::steelDim);
    deco::dashedLineH (g, cx - 48, cy, 96, 3, 3, pal::steelDim);
    g.setColour (pal::steelDim.withAlpha (0.55f));
    g.drawLine ((float) cx - 46, (float) cy - 46, (float) cx + 46, (float) cy + 46, 1.0f);
    g.drawLine ((float) cx + 46, (float) cy - 46, (float) cx - 46, (float) cy + 46, 1.0f);
    g.setFont (pixelfont::vhs (20.0f)); g.setColour (pal::textDim);
    g.drawText ("L", cx - 100, cy - 10, 30, 20, juce::Justification::centredRight, false);
    g.drawText ("R", cx + 70, cy - 10, 30, 20, juce::Justification::centredLeft, false);
    g.drawText ("M", cx - 15, body.getY() - 22, 30, 18, juce::Justification::centred, false);

    // goniometer: points rotated 45 degrees, newest brightest
    const int wr = proc.gonioWrite.load (std::memory_order_acquire), N = PepelSpaceProcessor::gonioSize;
    for (int k = 0; k < N; ++k)
    {
        const int idx = (wr + k) % N;
        const float l = proc.gonioL[idx].load (std::memory_order_relaxed), r = proc.gonioR[idx].load (std::memory_order_relaxed);
        const float px = std::tanh (1.6f * (r - l) * 0.5f) * (float) R, py = std::tanh (1.6f * (l + r) * 0.5f) * (float) R;
        const float age = (float) k / (float) N;
        g.setColour (pal::lampSoft.withAlpha (0.12f + 0.7f * age * age));
        g.fillRect ((int) std::round ((float) cx + px) - 1, (int) std::round ((float) cy - py) - 1, 2, 2);
    }
}

void PepelSpaceEditor::paintMap (juce::Graphics& g, juce::Rectangle<int> body)
{
    auto& ap = proc.apvts;
    const float pan = *ap.getRawParameterValue ("pan"), depth = *ap.getRawParameterValue ("depth"), height = *ap.getRawParameterValue ("height");
    const int col = (int) std::lround ((pan + 1.0f) * 0.5f * 10.0f), row = 3 - (int) std::lround (depth * 3.0f);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 11; ++c)
        {
            const juce::Rectangle<int> cell (body.getX() + c * 29, body.getY() + 2 + r * 24, 26, 20);
            g.setColour (pal::steelDim); g.fillRect (cell);
            const bool hit = (c == col && r == row);
            const bool near = (r == row && std::abs (c - col) == 1) || (c == col && std::abs (r - row) == 1);
            g.setColour (hit ? pal::red : (near ? pal::deepBlue : pal::night));
            g.fillRect (cell.reduced (2));
            if (hit) { g.setColour (pal::lamp); g.fillRect (cell.getX() + 2, height >= 0 ? cell.getY() + 2 : cell.getBottom() - 4, 22, 2); }
            g.setColour (pal::steelDim); g.fillRect (cell.getCentreX(), cell.getY() + 2, 1, 16);
        }
    const int bx = body.getX() + 330, by = body.getY() + 2;       // height gauge
    g.setColour (pal::navy2); g.fillRect (bx, by, 12, 92);
    const int my = by + 84 - (int) std::lround ((height + 1.0f) * 0.5f * 84.0f);
    g.setColour (pal::red); g.fillRect (bx - 1, my - 1, 14, 10);
    g.setColour (pal::white); g.fillRect (bx, my, 12, 8);
}

void PepelSpaceEditor::paintAnalysis (juce::Graphics& g, juce::Rectangle<int> p)
{
    const int segW = PixelLookAndFeel::segW, gap = PixelLookAndFeel::segGap, h = PixelLookAndFeel::segH;
    const float corr = proc.correlation.load();
    const bool anti = corr < -0.05f;
    const int yA = p.getY() + 42, yB = p.getY() + 68;

    // correlation: bipolar segment bar (-1 .. +1), centre marked
    g.setFont (pixelfont::vhs (22.0f)); g.setColour (pal::text);
    g.drawText ("CORR", p.getX() + 14, yA - 4, 54, 22, juce::Justification::centredLeft, false);
    const int cx0 = p.getX() + 70, half = 14;
    const int amt = (int) std::lround (std::abs (corr) * (float) half);
    for (int i = 0; i < half * 2; ++i)
    {
        const int sx = cx0 + i * (segW + gap);
        const bool pos = i >= half;
        const int d = pos ? i - half : half - 1 - i;             // distance from the centre
        const bool filled = d < amt && (pos ? corr > 0 : corr < 0);
        juce::Colour c = pal::navy2;
        if (filled) c = pos ? pal::deepBlue.interpolatedWith (pal::lamp, (float) d / (float) (half - 1))
                            : (anti ? pal::red : pal::lampSoft);
        g.setColour (c); g.fillRect (sx, yA, segW, h);
        if (filled) { g.setColour (c.interpolatedWith (pal::white, 0.55f)); g.fillRect (sx, yA, segW, 2); }
    }
    if (amt > 0)
    {
        const int d = amt - 1, i = corr > 0 ? half + d : half - 1 - d, mx = cx0 + i * (segW + gap);
        g.setColour (pal::red); g.fillRect (mx - 1, yA - 1, segW + 2, h + 2);
        g.setColour (pal::white); g.fillRect (mx, yA, segW, h);
    }
    g.setColour (pal::steel); g.fillRect (cx0 + half * (segW + gap) - 2, yA - 4, 2, h + 8);
    g.setColour (anti ? pal::red : pal::textDim);
    g.drawText (juce::String::formatted ("%+.2f", corr), cx0 + half * 2 * (segW + gap) + 8, yA - 4, 60, 22, juce::Justification::centredLeft, false);

    // antiphase warning (the triangle is drawn: the pixel font has no warning sign)
    if (anti)
    {
        const bool on = (frame % 8) < 5;
        const juce::Colour wc = on ? pal::red : pal::redDim;
        juce::Path tri; tri.addTriangle ((float) p.getX() + 21.0f, (float) yB, (float) p.getX() + 14.0f, (float) yB + 14.0f, (float) p.getX() + 28.0f, (float) yB + 14.0f);
        g.setColour (wc); g.fillPath (tri);
        g.setColour (pal::night); g.fillRect (p.getX() + 20, yB + 5, 2, 5); g.fillRect (p.getX() + 20, yB + 11, 2, 2);
        g.setColour (wc); g.setFont (pixelfont::vhs (22.0f));
        g.drawText ("ANTIPHASE", p.getX() + 36, yB - 4, 200, 22, juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (pal::textDim); g.setFont (pixelfont::vhs (22.0f));
        g.drawText ("PHASE OK", p.getX() + 14, yB - 4, 200, 22, juce::Justification::centredLeft, false);
    }

    // level meters: -60 .. 0 dB, red end marker only when clipping
    auto meter = [&] (const char* name, float peak, int y)
    {
        const int n = 32, x0 = p.getX() + 424;
        const float db = 20.0f * std::log10 (std::max (peak, 1.0e-5f));
        const int filled = (int) std::lround (juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (float) n);
        const bool clip = peak >= 0.995f;
        g.setColour (pal::text); g.setFont (pixelfont::vhs (22.0f));
        g.drawText (name, p.getX() + 400, y - 4, 22, 22, juce::Justification::centredLeft, false);
        for (int i = 0; i < n; ++i)
        {
            const int sx = x0 + i * (segW + gap);
            if (i < filled)
            {
                const auto c = (clip && i >= n - 3) ? pal::red : pal::deepBlue.interpolatedWith (pal::lamp, (float) i / (float) (n - 1));
                g.setColour (c); g.fillRect (sx, y, segW, h);
                g.setColour (c.interpolatedWith (pal::white, 0.55f)); g.fillRect (sx, y, segW, 2);
            }
            else { g.setColour (pal::navy2); g.fillRect (sx, y, segW, h); }
        }
        if (filled > 0)
        {
            const int mx = x0 + (filled - 1) * (segW + gap);
            g.setColour (pal::red); g.fillRect (mx - 1, y - 1, segW + 2, h + 2);
            g.setColour (pal::white); g.fillRect (mx, y, segW, h);
        }
        g.setColour (clip ? pal::red : pal::textDim);
        g.drawText (peak < 1.0e-4f ? juce::String ("-inf") : juce::String::formatted ("%.1f", db),
                    x0 + n * (segW + gap) + 4, y - 4, 52, 22, juce::Justification::centredLeft, false);
    };
    meter ("L", proc.peakL.load(), yA);
    meter ("R", proc.peakR.load(), yB);
}

void PepelSpaceEditor::paintScope (juce::Graphics& g)
{
    using namespace lay;
    const juce::Rectangle<int> r (20, scopeY, 760, scopeH);
    g.setColour (pal::night); g.fillRect (0, scopeY, W, scopeH);
    g.setColour (pal::navy);  g.fillRect (r);
    g.setColour (pal::frame); g.drawRect (r, 1);
    for (int x = r.getX() + 40; x < r.getRight(); x += 40)                  // vertical grid every 40 px
        deco::dashedLineV (g, x, r.getY() + 2, scopeH - 4, 2, 2, pal::steelDim.withAlpha (0.45f));
    const int cy = r.getCentreY(); const float amp = (float) scopeH * 0.5f - 3.0f;
    g.setColour (pal::steelDim.withAlpha (0.5f)); g.fillRect (r.getX() + 1, cy, r.getWidth() - 2, 1);

    const int cols = PepelSpaceProcessor::scopeColumns, wr = proc.scopeWrite.load (std::memory_order_acquire);
    for (int pass = 0; pass < 2; ++pass)                                     // glow pass, then crisp line
        for (int i = 0; i < cols; ++i)
        {
            const int idx = (wr + i) % cols;
            const float mn = proc.scopeMin[idx].load (std::memory_order_relaxed), mx = proc.scopeMax[idx].load (std::memory_order_relaxed);
            const int y1 = cy - (int) std::lround (juce::jlimit (-1.0f, 1.0f, mx) * amp);
            const int y2 = cy - (int) std::lround (juce::jlimit (-1.0f, 1.0f, mn) * amp);
            const int x = r.getX() + i * 2, hh = std::max (1, y2 - y1 + 1);
            if (pass == 0) { g.setColour (pal::lampSoft.withAlpha (0.16f)); g.fillRect (x - 1, y1 - 2, 4, hh + 4); }
            else
            {
                g.setColour (pal::lampSoft); g.fillRect (x, y1, 2, hh);
                if (mx >= 1.0f || mn <= -1.0f) { g.setColour (pal::red); g.fillRect (x, mx >= 1.0f ? y1 : y2, 2, 2); }   // clip pixel
            }
        }
    deco::drawStar (g, 9.0f, (float) cy, 5, pal::red, 0.9f);
    deco::drawStar (g, 791.0f, (float) cy, 5, pal::red, 0.9f);
}

//==============================================================================
void PepelSpaceEditor::paint (juce::Graphics& g)
{
    using namespace lay;
    g.drawImageAt (bg, 0, 0);
    paintScene (g);
    paintHeader (g);
    paintPanels (g);
    paintScope (g);

    // signature, utf-8 escapes keep the source encoding-proof: "by " + Cyrillic P, E, P, E + Latin L
    const auto sig = juce::String::fromUTF8 ("by \xD0\x9F\xD0\x95\xD0\x9F\xD0\x95" "L");
    const juce::Rectangle<int> sr (400, sigY, 380, 18);
    g.setFont (pixelfont::title (10.0f));
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawText (sig, sr.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::steel);                            g.drawText (sig, sr, juce::Justification::centredRight, false);
}

void PepelSpaceEditor::paintOverChildren (juce::Graphics& g)
{
    using namespace lay;
    g.drawImageAt (scanlines, 0, 0);
    g.drawImageAt (vignette, 0, 0);

    juce::Random grain (frame * 7919 + 13);                                   // grain: 36 dots per frame
    for (int i = 0; i < 36; ++i)
    {
        g.setColour (i & 1 ? pal::deepBlue.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.4f));
        g.fillRect (grain.nextInt (W), grain.nextInt (H), 2, 1);
    }

    if (glitchActive())
    {
        juce::Random r (frame * 31 + 7);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (0, r.nextInt (H - 40), W, 10 + r.nextInt (22));
        for (int i = 0; i < 4; ++i)
        {
            const int w = 110 + r.nextInt (300), x = r.nextInt (W - w) + (r.nextBool() ? 4 : -4);
            g.setColour ((i & 1 ? pal::glitchRed : pal::glitchBlue).withAlpha (0.5f));
            g.fillRect (x, r.nextInt (H), w, 2 + r.nextInt (3));
        }
    }
}
