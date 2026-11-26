// Teste do motor DSP sem JUCE: verifica se as vozes saem nas notas certas.
#include "../Source/VocEngine.h"
#include <cstdio>
#include <chrono>

static const double FS = 48000.0;

static std::vector<float> makeVowel (double f0, double secs, double vibCents = 0.0)
{
    std::vector<float> x ((size_t) (FS * secs));
    double ph = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / FS;
        const double f = f0 * std::pow (2.0, vibCents * std::sin (2 * M_PI * 5.5 * t) / 1200.0);
        ph += 2 * M_PI * f / FS;
        double s = 0;
        for (int h = 1; h <= 25; ++h)
        {
            const double fh = f * h;
            const double env = std::exp (-std::pow ((fh - 700.0) / 500.0, 2)) + 0.6 * std::exp (-std::pow ((fh - 1200.0) / 600.0, 2)) + 0.05;
            s += env / h * std::sin (h * ph);
        }
        x[i] = (float) (0.25 * s);
    }
    return x;
}

static double goertzel (const std::vector<float>& x, size_t start, size_t len, double f)
{
    double s1 = 0, s2 = 0; const double c = 2 * std::cos (2 * M_PI * f / FS);
    for (size_t i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * M_PI * (double) i / (double) len); const double s0 = x[start + i] * w + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) len;
}
static double bestFreq (const std::vector<float>& x, size_t start, size_t len, double lo, double hi)
{
    double bf = lo, bp = -1;
    for (double f = lo; f < hi; f *= 1.002) { const double p = goertzel (x, start, len, f); if (p > bp) { bp = p; bf = f; } }
    return bf;
}
static std::vector<float> run (voc::Engine& e, const std::vector<float>& in)
{
    std::vector<float> o (in.size()), tmpL (512), tmpR (512), oR (512);
    for (size_t i = 0; i < in.size(); i += 512)
    {
        const int nn = (int) std::min<size_t> (512, in.size() - i);
        std::copy (in.begin() + (long) i, in.begin() + (long) i + nn, tmpL.begin());
        e.process (tmpL.data(), tmpL.data(), tmpL.data(), oR.data(), nn);
        std::copy (tmpL.begin(), tmpL.begin() + nn, o.begin() + (long) i);
    }
    return o;
}
static int fails = 0;
static void check (const char* name, double got, double want, double tolPct)
{
    const double err = std::fabs (got - want) / want * 100.0;
    std::printf ("%-44s got %8.2f Hz  want %8.2f Hz  err %.2f%%  %s\n", name, got, want, err, err <= tolPct ? "OK" : "FALHOU");
    if (err > tolPct) ++fails;
}

int main()
{
    const auto in = makeVowel (220.0, 3.0, 12.0);
    const size_t st = (size_t) (FS * 2.0), len = 16384;

    { // A: intervalo fixo +7 st na voz 1, resto mudo
        voc::Engine e; e.prepare (FS);
        voc::Params p; p.source = voc::FixedInterval; p.naturalize = 100; p.glide = 0; p.pitchVar = 0; p.timeVar = 0;
        for (int v = 0; v < 5; ++v) { p.v[v].mute = (v != 1); p.v[v].choirOn = false; }
        p.v[1].interval = 7; e.setParams (p);
        const auto o = run (e, in);
        check ("A fixo +7 st (voz1)", bestFreq (o, st, len, 150, 700), 220.0 * std::pow (2.0, 7.0 / 12.0), 2.0);
    }
    { // B: escala C maior, entrada 220 Hz (A3), voz +2 graus (terca) -> C4 = 261.6 Hz
        voc::Engine e; e.prepare (FS);
        voc::Params p; p.source = voc::ScaleInterval; p.key = 0; p.scale = 0; p.naturalize = 0; p.glide = 0; p.pitchVar = 0; p.timeVar = 0;
        for (int v = 0; v < 5; ++v) { p.v[v].mute = (v != 1); p.v[v].choirOn = false; }
        p.v[1].interval = 2; e.setParams (p);
        const auto o = run (e, in);
        check ("B escala C maior, A3 + 3a = C4", bestFreq (o, st, len, 150, 700), 261.63, 2.0);
    }
    { // C: acorde C maior (nome), 4 vozes -> C4 E4 G4 C5
        voc::Engine e; e.prepare (FS);
        voc::Params p; p.source = voc::ChordName; p.key = 0; p.chordType = 0; p.naturalize = 0; p.glide = 0; p.pitchVar = 0; p.timeVar = 0; p.spread = 0;
        for (int v = 0; v < 5; ++v) { p.v[v].mute = (v == 0); p.v[v].choirOn = false; }
        e.setParams (p);
        const auto o = run (e, in);
        const double notes[4] = { 261.63, 329.63, 392.00, 523.25 };
        for (double f : notes)
        {
            const double on = goertzel (o, st, len, f), off = goertzel (o, st, len, f * 1.0595 * 1.0 ) ;
            std::printf ("C acorde: nota %.1f Hz potencia %.5f vs semitom acima %.5f  %s\n", f, on, off, on > 2 * off ? "OK" : "FALHOU");
            if (! (on > 2 * off)) ++fails;
        }
    }
    { // D: desempenho com coro 16 em todas as vozes
        voc::Engine e; e.prepare (FS);
        voc::Params p; p.source = voc::ScaleInterval; p.choirSize = 16;
        for (int v = 0; v < 5; ++v) p.v[v].choirOn = true;
        for (int v = 1; v < 5; ++v) p.v[v].interval = 2 * v - 1;
        e.setParams (p);
        const auto t0 = std::chrono::steady_clock::now();
        const auto o = run (e, in);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool bad = false; double mx = 0;
        for (float s : o) { if (! std::isfinite (s)) bad = true; mx = std::max (mx, (double) std::fabs (s)); }
        std::printf ("D coro 16x5: pico %.2f, tempo %.2fs para 3s de audio (%.1f%% de CPU) %s\n", mx, sec, sec / 3.0 * 100, bad ? "FALHOU (NaN)" : "OK");
        if (bad) ++fails;
    }
    std::printf (fails ? "\n%d teste(s) falharam\n" : "\nTodos os testes passaram\n", fails);
    return fails ? 1 : 0;
}
