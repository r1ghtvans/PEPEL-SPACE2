// DspTest - checks the PEPEL SPACE engine without a DAW. Exit code 0 = all passed.
#include "PepelDsp.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>

// ---- allocation counter: proves that process() never touches the heap ----
static std::atomic<long> gAllocs { 0 };
static std::atomic<bool> gCount { false };
void* operator new (std::size_t n) { if (gCount) ++gAllocs; if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[] (std::size_t n) { if (gCount) ++gAllocs; if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using pepel::Engine; using pepel::Params;
using Buf = std::vector<float>;
static int failed = 0, total = 0;

static void check (bool ok, const char* name, const char* fmt = "", double a = 0, double b = 0)
{
    ++total; if (! ok) ++failed;
    char extra[160]; std::snprintf (extra, sizeof extra, fmt, a, b);
    std::printf ("  [%s] %-52s %s\n", ok ? "PASS" : "FAIL", name, extra);
}

static Buf sine (double fs, double f, float amp, int n, double ph = 0.0)
{ Buf b ((size_t) n); for (int i = 0; i < n; ++i) b[(size_t) i] = amp * (float) std::sin (6.283185307 * f * i / fs + ph); return b; }
static Buf noise (int n, unsigned seed, float amp = 0.5f)
{ std::mt19937 g (seed); std::uniform_real_distribution<float> d (-amp, amp); Buf b ((size_t) n); for (auto& v : b) v = d (g); return b; }

// processes in blocks; in/out are copied, the engine works in place
static void run (Engine& e, const Params& p, const Buf& inL, const Buf& inR, Buf& oL, Buf& oR, int block = 512)
{
    oL = inL; oR = inR; const int n = (int) inL.size();
    for (int pos = 0; pos < n; pos += block)
        e.process (oL.data() + pos, oR.data() + pos, std::min (block, n - pos), p);
}
static double rms (const Buf& b, int from = 0) { double s = 0; for (size_t i = (size_t) from; i < b.size(); ++i) s += (double) b[i] * b[i]; return std::sqrt (s / std::max<size_t> (1, b.size() - (size_t) from)); }
static double dB (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }
static double maxDiff (const Buf& a, const Buf& b, int from = 0) { double m = 0; for (size_t i = (size_t) from; i < a.size(); ++i) m = std::max (m, (double) std::abs (a[i] - b[i])); return m; }
static Engine make (double fs = 48000.0) { Engine e; e.prepare (fs); return e; }

int main()
{
    juce::ScopedNoDenormals noDenormals;
    const double fs = 48000.0; const int N = (int) fs * 2, S = (int) fs / 2;   // 2 s, skip first 0.5 s
    Buf a = noise (N, 1), b = noise (N, 2), oL, oR;
    std::printf ("PEPEL SPACE DSP test\n");

    { auto e = make(); Params p; run (e, p, a, b, oL, oR);
      const double d = std::max (maxDiff (oL, a), maxDiff (oR, b));
      check (d < 1e-5, "defaults are transparent (identity)", "max err %.2e", d); }

    { auto e = make(); Params p; p.width = 0; run (e, p, a, b, oL, oR);
      check (maxDiff (oL, oR, S) < 1e-5, "width 0% -> mono (L == R)"); }

    { auto e = make(); Params p; p.width = 2; run (e, p, a, b, oL, oR);
      double si = 0, so = 0, mi = 0, mo = 0;
      for (int i = S; i < N; ++i) { si += std::pow (a[(size_t) i] - b[(size_t) i], 2); so += std::pow (oL[(size_t) i] - oR[(size_t) i], 2);
                                    mi += std::pow (a[(size_t) i] + b[(size_t) i], 2); mo += std::pow (oL[(size_t) i] + oR[(size_t) i], 2); }
      check (std::abs (so / si - 4.0) < 0.02 && std::abs (mo / mi - 1.0) < 1e-3, "width 200% -> side x2 (power x4), mid unchanged", "side x%.3f  mid x%.4f", so / si, mo / mi); }

    for (int s12 = 0; s12 < 2; ++s12)
    {   // mono bass: anti-phase 60 Hz must collapse, anti-phase 5 kHz must stay
        auto lo = sine (fs, 60, 0.5f, N), hi = sine (fs, 5000, 0.5f, N); Buf nlo = lo, nhi = hi; for (auto& v : nlo) v = -v; for (auto& v : nhi) v = -v;
        auto e1 = make(), e2 = make(); Params p; p.monoBassHz = 200; p.slope12 = s12;
        Buf l1, r1, l2, r2; run (e1, p, lo, nlo, l1, r1); run (e2, p, hi, nhi, l2, r2);
        Buf s1 (l1.size()), s2 (l2.size()); for (size_t i = 0; i < s1.size(); ++i) { s1[i] = 0.5f * (l1[i] - r1[i]); s2[i] = 0.5f * (l2[i] - r2[i]); }
        const double att = dB (rms (s1, S) / rms (lo, S)), keep = dB (rms (s2, S) / rms (hi, S));
        check (att < (s12 ? -19.0 : -9.0) && std::abs (keep) < 0.5, s12 ? "mono bass 200 Hz, 12 dB/oct" : "mono bass 200 Hz, 6 dB/oct", "60 Hz side %.1f dB, 5 kHz side %.2f dB", att, keep);
    }

    { auto m = noise (N, 3); auto e = make(); Params p; p.pan = 1; run (e, p, m, m, oL, oR);
      check (rms (oL, S) < 1e-4 && std::abs (rms (oR, S) / rms (m, S) - 1.41421) < 0.01, "pan hard R: L silent, R = +3 dB", "L %.2e  R x%.3f", rms (oL, S), rms (oR, S) / rms (m, S));
      auto e2 = make(); p.pan = -0.3f; run (e2, p, m, m, oL, oR);
      const double pw = (std::pow (rms (oL, S), 2) + std::pow (rms (oR, S), 2)) / std::pow (rms (m, S), 2);
      check (std::abs (pw - 2.0) < 0.01, "pan law is equal power (L^2+R^2 = const)", "power sum %.4f (2.0 = centre)", pw); }

    {   auto s100 = sine (fs, 100, 0.5f, N), s10k = sine (fs, 10000, 0.5f, N); auto e1 = make(), e2 = make(); Params p; p.depth = 1;
        Buf x, y; run (e1, p, s100, s100, x, y); const double g1 = dB (rms (x, S) / rms (s100, S));
        run (e2, p, s10k, s10k, x, y); const double g2 = dB (rms (x, S) / rms (s10k, S));
        check (std::abs (g1 + 4.0) < 0.4 && g2 < -13.0 && g2 > -20.0, "depth 100%: quieter and darker", "100 Hz %.2f dB, 10 kHz %.2f dB", g1, g2); }

    {   auto s100 = sine (fs, 100, 0.5f, N), s8k = sine (fs, 8000, 0.5f, N); auto e1 = make(), e2 = make(); Params p; p.height = 1;
        Buf x, y; run (e1, p, s100, s100, x, y); const double g1 = dB (rms (x, S) / rms (s100, S));
        run (e2, p, s8k, s8k, x, y); const double g2 = dB (rms (x, S) / rms (s8k, S));
        check (g1 < -4.9 && g1 > -6.5 && g2 > 5.0 && g2 < 6.5, "height +100%: spectral tilt", "100 Hz %.2f dB, 8 kHz %.2f dB", g1, g2); }

    {   Buf z ((size_t) N, 0.0f), iL = z, iR = z; iL[1000] = 1.0f; iR[1000] = 1.0f; auto e = make(); Params p; p.delayLms = 10; p.delayRms = 5;
        run (e, p, iL, iR, oL, oR);
        auto peak = [] (const Buf& v) { int k = 0; for (size_t i = 0; i < v.size(); ++i) if (std::abs (v[i]) > std::abs (v[(size_t) k])) k = (int) i; return k; };
        check (peak (oL) == 1480 && peak (oR) == 1240 && std::abs (oL[1480] - 1.0f) < 1e-3f, "Haas delays: L 10 ms, R 5 ms", "L at %.0f, R at %.0f", peak (oL), peak (oR));
        auto e0 = make(); Params q; run (e0, q, iL, iR, oL, oR);
        check (peak (oL) == 1000 && peak (oR) == 1000, "no delay -> zero latency (nothing to report to PDC)"); }

    { auto e = make(); Params p; p.phaseL = true; run (e, p, a, b, oL, oR); check (maxDiff (oL, a) > 0 && std::abs (oL[1000] + a[1000]) < 1e-6f && std::abs (oR[1000] - b[1000]) < 1e-6f, "phase invert L"); }
    { auto e = make(); Params p; p.swap = true;   run (e, p, a, b, oL, oR); check (maxDiff (oL, b) < 1e-5 && maxDiff (oR, a) < 1e-5, "channel swap"); }
    { auto e = make(); Params p; p.monoCheck = true; run (e, p, a, b, oL, oR); check (maxDiff (oL, oR) < 1e-6, "mono check (L == R)"); }
    { auto e = make(); Params p; p.bypass = true; p.width = 0; p.pan = 1; p.depth = 1; p.delayLms = 20; p.inGainDb = 12; run (e, p, a, b, oL, oR);
      check (maxDiff (oL, a) < 1e-6 && maxDiff (oR, b) < 1e-6, "bypass is bit-transparent"); }
    { auto e = make(); Params p; p.mix = 0; p.width = 0; p.pan = 1; p.height = 1; p.delayRms = 30; p.depth = 1; p.inGainDb = -6; run (e, p, a, b, oL, oR);
      const float g = pepel::dbToGain (-6); double d = 0; for (int i = 0; i < N; ++i) d = std::max ({ d, (double) std::abs (oL[(size_t) i] - g * a[(size_t) i]), (double) std::abs (oR[(size_t) i] - g * b[(size_t) i]) });
      check (d < 1e-5, "dry/wet 0%: only input gain remains", "max err %.2e", d); }

    {   Buf c = noise (N, 5); auto e = make(); Params p; Buf x, y; double cs = 0, ci = 0, cn = 0;
        run (e, p, c, c, x, y); cs = e.correlation(); Buf ni = c; for (auto& v : ni) v = -v;
        auto e2 = make(); run (e2, p, c, ni, x, y); ci = e2.correlation();
        auto e3 = make(); run (e3, p, a, b, x, y); cn = e3.correlation();
        check (cs > 0.99 && ci < -0.99 && std::abs (cn) < 0.15, "correlation meter: +1 / -1 / ~0", "%+.3f / %+.3f", cs, ci);
        check (ci < -0.05, "antiphase threshold (< -0.05) triggers"); }

    {   Params p; p.width = 1.7f; p.monoBassHz = 120; p.slope12 = true; p.pan = -0.4f; p.depth = 0.5f; p.height = 0.3f; p.delayLms = 7; p.delayRms = 13; p.midDb = 3; p.sideDb = -2; p.outGainDb = 4;
        auto e1 = make(), e2 = make(); Buf x1, y1, x2, y2; run (e1, p, a, b, x1, y1, 512); run (e2, p, a, b, x2, y2, 37);
        check (std::max (maxDiff (x1, x2), maxDiff (y1, y2)) < 1e-5, "result does not depend on host block size", "max diff %.2e", std::max (maxDiff (x1, x2), maxDiff (y1, y2))); }

    {   bool ok = true; for (double r : { 44100.0, 96000.0, 192000.0 })
        {   Params p; p.width = 2; p.monoBassHz = 300; p.slope12 = true; p.pan = 1; p.depth = 1; p.height = -1; p.delayLms = 30; p.delayRms = 30; p.inGainDb = 24; p.outGainDb = 24; p.midDb = 12; p.sideDb = 12;
            auto e = make (r); const int n = (int) r; Buf x = noise (n, 9, 4.0f), y = noise (n, 10, 4.0f); x[100] = 50.0f; y[2000] = -50.0f; Buf ox, oy; run (e, p, x, y, ox, oy, 256);
            for (int i = 0; i < n; ++i) if (! std::isfinite (ox[(size_t) i]) || ! std::isfinite (oy[(size_t) i])) ok = false;
            Buf z ((size_t) n, 0.0f); run (e, p, z, z, ox, oy);   // silence afterwards: tails must die out
            if (rms (ox, n - 100) > 1e-3 || rms (oy, n - 100) > 1e-3) ok = false; }
        check (ok, "extreme settings at 44.1/96/192 kHz: finite, tails decay"); }

    {   auto dc = Buf ((size_t) N, 0.5f); double worst = 0;
        auto step = [&] (Params p0, Params p1, const char* what) { auto e = make(); Buf x = dc, y = dc; Buf ox = x, oy = y; e.process (ox.data(), oy.data(), 4800, p0);
            Buf px (4800, 0.5f), py (4800, 0.5f); e.process (px.data(), py.data(), 4800, p1); double m = 0; for (int i = 1; i < 4800; ++i) m = std::max ({ m, (double) std::abs (px[(size_t) i] - px[(size_t) i - 1]), (double) std::abs (py[(size_t) i] - py[(size_t) i - 1]) }); worst = std::max (worst, m); (void) what; };
        Params a0, a1; a1.outGainDb = -24; step (a0, a1, "out");
        Params b1; b1.phaseL = true; step (a0, b1, "phase");
        Params c1; c1.bypass = true; c1.pan = 1; step (c1, a0, "bypass");
        Params d1; d1.swap = true; step (a0, d1, "swap");
        Params f1; f1.width = 0; f1.mix = 0.2f; f1.inGainDb = 12; step (a0, f1, "misc");
        check (worst < 0.01, "parameter jumps are smoothed (no clicks)", "worst step %.5f", worst); }

    {   // no heap activity inside process()
        auto e = make(); Params p; p.width = 1.3f; p.monoBassHz = 90; p.depth = 0.4f; p.delayLms = 9; Buf x = noise (4096, 11), y = noise (4096, 12);
        Buf wx = x, wy = y; e.process (wx.data(), wy.data(), 4096, p); gAllocs = 0; gCount = true;
        for (int i = 0; i < 200; ++i) { p.pan = (float) std::sin (i * 0.1); std::copy (x.begin(), x.begin() + 512, wx.begin()); std::copy (y.begin(), y.begin() + 512, wy.begin()); e.process (wx.data(), wy.data(), 512, p); }
        gCount = false; check (gAllocs.load() == 0, "no heap allocation inside process()", "%.0f allocations", (double) gAllocs.load()); }

    {   // CPU cost, everything on, 60 s of stereo audio at 48 kHz
        auto e = make(); Params p; p.width = 1.4f; p.monoBassHz = 140; p.slope12 = true; p.pan = 0.2f; p.depth = 0.5f; p.height = 0.3f; p.delayLms = 8; p.delayRms = 14; p.midDb = 1; p.sideDb = 1;
        Buf x = noise (512, 13), y = noise (512, 14); const int blocks = (int) (fs * 60 / 512);
        Buf wx = x, wy = y;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < blocks; ++i) { std::copy (x.begin(), x.end(), wx.begin()); std::copy (y.begin(), y.end(), wy.begin()); e.process (wx.data(), wy.data(), 512, p); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  [INFO] 60 s of audio processed in %.3f s -> %.2f% of one core (%.0fx realtime)\n", sec, sec / 60.0 * 100.0, 60.0 / sec); }

    std::printf ("%d/%d checks passed\n", total - failed, total);
    return failed == 0 ? 0 : 1;
}
