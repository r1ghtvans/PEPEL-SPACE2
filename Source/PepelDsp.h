#pragma once
// PEPEL SPACE - DSP engine. Header-only, depends only on juce_audio_basics (SmoothedValue),
// so it can be tested by DspTest without a DAW or any GUI code.
//
// Signal flow (per sample):
//   in -> InGain -> [Phase L/R, Swap] -> MonoBass (one-pole split) -> Mid/Side + Width
//      -> Pan (equal power) -> Depth (one-pole LP + gain) -> Height (one-pole tilt)
//      -> Haas delays (L/R) -> Dry/Wet mix -> OutGain -> Bypass xfade -> [analysis] -> Mono check
// Nothing here allocates after prepare(). No latency is reported to the host (no PDC).

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace pepel
{
struct Params
{
    float inGainDb = 0.0f, outGainDb = 0.0f;
    float mix = 1.0f;            // 0..1
    float width = 1.0f;          // 0..2
    float monoBassHz = 20.0f;    // 20 == OFF
    float midDb = 0.0f, sideDb = 0.0f;
    float pan = 0.0f;            // -1..1
    float depth = 0.0f;          // 0..1  (0 = near, 1 = far)
    float height = 0.0f;         // -1..1 (spectral tilt)
    float delayLms = 0.0f, delayRms = 0.0f; // 0..30 ms
    bool bypass = false, phaseL = false, phaseR = false, swap = false, monoCheck = false;
    bool slope12 = false;        // mono-bass crossover: false = 6 dB/oct, true = 12 dB/oct
};

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

class Engine
{
public:
    static constexpr float maxDelayMs = 30.0f;
    static constexpr float monoBassOffHz = 20.5f;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int need = (int) std::ceil (maxDelayMs * 0.001f * fs) + 4;
        int size = 1;
        while (size < need) size <<= 1;
        for (auto& d : delay) d.assign ((size_t) size, 0.0f);
        mask = size - 1;
        maxDelaySamples = maxDelayMs * 0.001f * fs;
        tiltCoef = onePole (1000.0f);
        corrK = 1.0f - std::exp (-1.0f / (fs * 0.3f));

        auto setup = [this] (auto& s, double sec) { s.reset (fs, sec); };
        setup (sIn, 0.02);   setup (sOut, 0.02);  setup (sMix, 0.02);   setup (sWidth, 0.03);
        setup (sMid, 0.02);  setup (sSide, 0.02); setup (sPanL, 0.03);  setup (sPanR, 0.03);
        setup (sMonoAmt, 0.03); setup (sSlope, 0.03); setup (sBypass, 0.015); setup (sMonoChk, 0.015);
        setup (sPhL, 0.01);  setup (sPhR, 0.01);  setup (sSwap, 0.015);
        setup (sDelL, 0.06); setup (sDelR, 0.06);
        setup (sDepth, 0.05); setup (sHeight, 0.05); setup (sMonoFreq, 0.05);
        reset();
        first = true;
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (int c = 0; c < 2; ++c) lp1[c] = lp2[c] = lpDepth[c] = lpTilt[c] = 0.0f;
        wpos = 0; eLL = eRR = eLR = 0.0f; pkL = pkR = 0.0f; corr = 0.0f; lvl = 0.0f;
        first = true;
    }

    // In-place stereo processing. L and R may be the same buffers for in and out.
    void process (float* L, float* R, int n, const Params& p) noexcept
    {
        applyTargets (p);

        float blkPkL = 0.0f, blkPkR = 0.0f, sumSq = 0.0f;
        int pos = 0;
        while (pos < n)
        {
            const int c = std::min (kChunk, n - pos);

            // chunk-rate (coefficient) updates
            const float fMono = sMonoFreq.skip (c);
            const float aMono = onePole (fMono);
            const float dV = sDepth.skip (c);
            const float fcD = std::min (20000.0f * std::exp (-dV * 2.0794415f), 0.45f * fs);
            const float aDepth = onePole (fcD);
            const float kDepth = std::min (1.0f, dV * 10.0f);
            const float gDepth = dbToGain (-4.0f * dV);
            const float hV = sHeight.skip (c);
            const float gHi = dbToGain (6.0f * hV), gLo = dbToGain (-6.0f * hV);

            for (int i = 0; i < c; ++i)
            {
                const int idx = pos + i;
                const float rawL = L[idx], rawR = R[idx];

                const float inG = sIn.getNextValue();
                float l = rawL * inG, r = rawR * inG;
                const float dryL = l, dryR = r;

                // utilities
                l *= sPhL.getNextValue();
                r *= sPhR.getNextValue();
                const float sw = sSwap.getNextValue();
                { const float tl = l + sw * (r - l), tr = r + sw * (l - r); l = tl; r = tr; }

                // mono bass: complementary one-pole split, lows summed to mono
                {
                    lp1[0] += aMono * (l - lp1[0]);  lp2[0] += aMono * (lp1[0] - lp2[0]);
                    lp1[1] += aMono * (r - lp1[1]);  lp2[1] += aMono * (lp1[1] - lp2[1]);
                    const float sl = sSlope.getNextValue();
                    // 6 dB/oct: low = H.  12 dB/oct: low = 2H - H^2, i.e. high = (1-H)^2 (true 2nd-order roll-off of the stereo part)
                    const float lowL = lp1[0] + sl * (lp1[0] - lp2[0]);
                    const float lowR = lp1[1] + sl * (lp1[1] - lp2[1]);
                    const float mono = 0.5f * (lowL + lowR);
                    const float amt = sMonoAmt.getNextValue();
                    l += amt * (mono - lowL);
                    r += amt * (mono - lowR);
                }

                // mid / side
                {
                    const float mid = 0.5f * (l + r) * sMid.getNextValue();
                    const float side = 0.5f * (l - r) * sSide.getNextValue() * sWidth.getNextValue();
                    l = mid + side;
                    r = mid - side;
                }

                // pan (equal power, centre = unity)
                l *= sPanL.getNextValue();
                r *= sPanR.getNextValue();

                // depth: darker + quieter
                lpDepth[0] += aDepth * (l - lpDepth[0]);
                lpDepth[1] += aDepth * (r - lpDepth[1]);
                l = (l + kDepth * (lpDepth[0] - l)) * gDepth;
                r = (r + kDepth * (lpDepth[1] - r)) * gDepth;

                // height: first-order tilt around 1 kHz
                lpTilt[0] += tiltCoef * (l - lpTilt[0]);
                lpTilt[1] += tiltCoef * (r - lpTilt[1]);
                l = lpTilt[0] * gLo + (l - lpTilt[0]) * gHi;
                r = lpTilt[1] * gLo + (r - lpTilt[1]) * gHi;

                // Haas micro delays (linear interpolation, long smoothing)
                delay[0][(size_t) wpos] = l;
                delay[1][(size_t) wpos] = r;
                l = readDelay (delay[0], sDelL.getNextValue());
                r = readDelay (delay[1], sDelR.getNextValue());
                wpos = (wpos + 1) & mask;

                // dry/wet and output gain
                const float mx = sMix.getNextValue();
                const float og = sOut.getNextValue();
                float outL = (dryL + mx * (l - dryL)) * og;
                float outR = (dryR + mx * (r - dryR)) * og;

                // bypass crossfade to the untouched input
                const float by = sBypass.getNextValue();
                outL += by * (rawL - outL);
                outR += by * (rawR - outR);

                // analysis (pre mono-check)
                eLL += corrK * (outL * outL - eLL);
                eRR += corrK * (outR * outR - eRR);
                eLR += corrK * (outL * outR - eLR);
                blkPkL = std::max (blkPkL, std::abs (outL));
                blkPkR = std::max (blkPkR, std::abs (outR));
                sumSq += 0.5f * (outL * outL + outR * outR);

                // mono check
                const float mc = sMonoChk.getNextValue();
                const float m = 0.5f * (outL + outR);
                L[idx] = outL + mc * (m - outL);
                R[idx] = outR + mc * (m - outR);
            }
            pos += c;
        }

        // block-rate meters
        const float decay = std::exp (-(float) n / (fs * 0.5f));
        pkL = std::max (blkPkL, pkL * decay);
        pkR = std::max (blkPkR, pkR * decay);
        const float den = eLL * eRR;
        corr = den > 1.0e-10f ? std::clamp (eLR / std::sqrt (den), -1.0f, 1.0f) : 0.0f;
        const float rms = std::sqrt (sumSq / (float) std::max (1, n));
        lvl += 0.35f * (rms - lvl);
    }

    float peakL() const noexcept { return pkL; }
    float peakR() const noexcept { return pkR; }
    float correlation() const noexcept { return corr; }
    float level() const noexcept { return lvl; }

private:
    static constexpr int kChunk = 32;
    using Smooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    float onePole (float hz) const noexcept
    {
        hz = std::clamp (hz, 1.0f, 0.45f * fs);
        return 1.0f - std::exp (-6.2831853f * hz / fs);
    }

    float readDelay (const std::vector<float>& buf, float d) const noexcept
    {
        d = std::clamp (d, 0.0f, maxDelaySamples);
        float rp = (float) wpos - d;
        if (rp < 0.0f) rp += (float) (mask + 1);
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) & mask;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    void applyTargets (const Params& p) noexcept
    {
        const float ang = (std::clamp (p.pan, -1.0f, 1.0f) + 1.0f) * 0.78539816f; // 0..pi/2
        const float gl = std::cos (ang) * 1.41421356f, gr = std::sin (ang) * 1.41421356f;
        const bool off = p.monoBassHz <= monoBassOffHz;

        auto set = [this] (Smooth& s, float v) { if (first) s.setCurrentAndTargetValue (v); else s.setTargetValue (v); };
        set (sIn, dbToGain (p.inGainDb));      set (sOut, dbToGain (p.outGainDb));
        set (sMix, std::clamp (p.mix, 0.0f, 1.0f));
        set (sWidth, std::clamp (p.width, 0.0f, 2.0f));
        set (sMid, dbToGain (p.midDb));        set (sSide, dbToGain (p.sideDb));
        set (sPanL, gl);                       set (sPanR, gr);
        set (sMonoAmt, off ? 0.0f : 1.0f);     set (sSlope, p.slope12 ? 1.0f : 0.0f);
        set (sMonoFreq, std::clamp (p.monoBassHz, 20.0f, 300.0f));
        set (sBypass, p.bypass ? 1.0f : 0.0f); set (sMonoChk, p.monoCheck ? 1.0f : 0.0f);
        set (sPhL, p.phaseL ? -1.0f : 1.0f);   set (sPhR, p.phaseR ? -1.0f : 1.0f);
        set (sSwap, p.swap ? 1.0f : 0.0f);
        set (sDepth, std::clamp (p.depth, 0.0f, 1.0f));
        set (sHeight, std::clamp (p.height, -1.0f, 1.0f));
        set (sDelL, std::clamp (p.delayLms, 0.0f, maxDelayMs) * 0.001f * fs);
        set (sDelR, std::clamp (p.delayRms, 0.0f, maxDelayMs) * 0.001f * fs);
        first = false;
    }

    float fs = 44100.0f, maxDelaySamples = 0.0f, tiltCoef = 0.0f, corrK = 0.0f;
    bool first = true;
    Smooth sIn, sOut, sMix, sWidth, sMid, sSide, sPanL, sPanR, sMonoAmt, sSlope, sBypass, sMonoChk,
           sPhL, sPhR, sSwap, sDelL, sDelR, sDepth, sHeight, sMonoFreq;
    float lp1[2] {}, lp2[2] {}, lpDepth[2] {}, lpTilt[2] {};
    std::vector<float> delay[2];
    int mask = 0, wpos = 0;
    float eLL = 0, eRR = 0, eLR = 0, pkL = 0, pkR = 0, corr = 0, lvl = 0;
};
} // namespace pepel
