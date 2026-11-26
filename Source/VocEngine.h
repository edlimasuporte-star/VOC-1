// THE VOC - motor DSP (C++ puro, sem JUCE, testavel em linha de comando)
// Harmonizador vocal: rastreador de pitch (YIN) + 4 vozes PSOLA com formante ajustavel + coro.
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace voc
{
enum Source { FixedInterval = 0, ScaleInterval, ChordName, ChordDegrees, ChordViaMidi, MidiOmni, MidiChannels };

struct VoiceParams
{
    float gainDb = 0.f, pan = 0.f, throat = 1.f;   // pan -1..1
    bool mute = false, solo = false, choirOn = false;
    int interval = 0;                               // semitons (Fixed) ou graus (Scale)
    float vibRate = 5.5f, vibOnset = 80.f, vibPitch = 0.f, vibAmp = 0.f;
};

struct Params
{
    int range = 1;            // 0 Soprano, 1 Alto/Tenor, 2 Baritono/Baixo, 3 Instrumento
    float tracking = 40.f;    // 0 exigente .. 100 relaxado
    int source = 1, key = 0, scale = 0, chordType = 0, chordDegree = 0, inversion = 0;
    float spread = 30.f, registerSemis = 0.f, velSens = 60.f;
    float naturalize = 50.f, pitchVar = 20.f, timeVar = 15.f, glide = 30.f;
    bool freeze = false, choirBypass = false;
    int choirSize = 8;
    float chVib = 20.f, chPitch = 30.f, chTime = 40.f, chSpread = 60.f;
    VoiceParams v[5];         // 0 = entrada, 1..4 = vozes harmonicas
};

static const int kScaleSize[11] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5 };
static const int kScales[11][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 11 },
    { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 },
    { 0, 1, 3, 5, 6, 8, 10 }, { 0, 2, 4, 7, 9, 0, 0 }, { 0, 3, 5, 7, 10, 0, 0 } };
static const int kChordLen[13] = { 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4 };
static const int kChords[13][4] = {
    { 0, 4, 7, 0 }, { 0, 3, 7, 0 }, { 0, 3, 6, 0 }, { 0, 4, 8, 0 }, { 0, 2, 7, 0 }, { 0, 5, 7, 0 }, { 0, 4, 7, 11 },
    { 0, 4, 7, 10 }, { 0, 3, 7, 10 }, { 0, 3, 6, 10 }, { 0, 3, 6, 9 }, { 0, 4, 7, 9 }, { 0, 3, 7, 9 } };

inline int fdiv (int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
inline int pmod (int a, int b) { return a - b * fdiv (a, b); }

class Engine
{
public:
    void prepare (double fs)
    {
        sr = fs;
        decim = std::max (1, (int) std::lround (fs / 12000.0));
        srD = fs / decim;
        hop = 64 * decim;
        S = (int) (0.04 * fs);       // atraso de analise (marcas de pitch precisam existir)
        D = (int) (0.07 * fs);       // latencia total
        maxHo = (int) (0.03 * fs) - 1;
        look = 128 * decim;
        monoRing.assign (RB, 0.f); inLRing.assign (RB, 0.f); inRRing.assign (RB, 0.f);
        oL.assign (OB, 0.f); oR.assign (OB, 0.f);
        dec.assign (DB, 0.f); hist.assign (HN, Frame());
        xb.assign (700, 0.f); dn.assign (400, 0.f);
        for (int i = 0; i <= 2048; ++i) hann[i] = 0.5f * (1.f - std::cos (6.28318530718f * (float) i / 2048.f));
        reset();
    }

    void reset()
    {
        n = 0; decWritten = 0; decAcc = 0.f; decCnt = 0; lastHop = -1;
        markCount = 0; haveMark = false; lastMark = 0; nT = 0; prevVoiced = false; vTime = 0.f; frozen = false;
        nHeld = 0; rs = 12345u;
        std::fill (monoRing.begin(), monoRing.end(), 0.f); std::fill (inLRing.begin(), inLRing.end(), 0.f);
        std::fill (inRRing.begin(), inRRing.end(), 0.f); std::fill (oL.begin(), oL.end(), 0.f); std::fill (oR.begin(), oR.end(), 0.f);
        std::fill (dec.begin(), dec.end(), 0.f);
        for (auto& f : hist) f = Frame();
        for (int v = 0; v < 4; ++v)
        {
            tgtSm[v] = 60.0; prevAct[v] = false; vibPhase[v] = 1.3f * (float) v; pvTarget[v] = 0.f; pvVal[v] = 0.f; pvCnt[v] = 1;
            tv[v] = 0.5f * (rnd() + 1.f);
        }
        for (int v = 0; v < 5; ++v)
            for (int c = 0; c < 16; ++c)
            {
                Eng& e = engs[v][c];
                e = Eng();
                e.r1 = rnd(); e.r2 = rnd(); e.r3 = rnd();
                e.rate = 4.6f + 1.8f * (rnd() + 1.f) * 0.5f;
                e.phase = 3.14159f * (rnd() + 1.f);
            }
    }

    void setParams (const Params& p) { P = p; }
    int latencySamples() const { return D; }

    void noteOn (int ch, int note, int vel)
    {
        if (vel <= 0) { noteOff (ch, note); return; }
        if (nHeld < 32) held[nHeld++] = { ch, note, vel };
    }
    void noteOff (int ch, int note)
    {
        for (int i = 0; i < nHeld; ++i)
            if (held[i].note == note && held[i].ch == ch)
            {
                for (int k = i; k < nHeld - 1; ++k) held[k] = held[k + 1];
                --nHeld; return;
            }
    }
    void allNotesOff() { nHeld = 0; }

    // inR pode apontar para inL (entrada mono). Entrada e saida podem ser o mesmo buffer.
    void process (const float* inL, const float* inR, float* outL, float* outR, int num)
    {
        bool anySolo = false;
        for (int v = 0; v < 5; ++v) anySolo = anySolo || P.v[v].solo;
        const int csz = P.choirSize >= 16 ? 16 : P.choirSize >= 8 ? 8 : P.choirSize >= 4 ? 4 : 2;
        float lvl[5]; int nE[5];
        for (int v = 0; v < 5; ++v)
        {
            const bool audible = ! P.v[v].mute && (! anySolo || P.v[v].solo);
            lvl[v] = audible ? std::pow (10.f, P.v[v].gainDb / 20.f) : 0.f;
            nE[v] = (! P.choirBypass && P.v[v].choirOn) ? csz : (v == 0 ? 0 : 1);
            if (lvl[v] <= 0.f) nE[v] = 0;
        }
        const bool dryOn = lvl[0] > 0.f && ! (! P.choirBypass && P.v[0].choirOn);
        for (int v = 0; v < 5; ++v)
            for (int c = 0; c < 16; ++c)
            {
                Eng& e = engs[v][c];
                const bool a = c < nE[v];
                if (a && ! e.active) e.nextE = n - S;
                e.active = a;
            }
        const float p0 = std::clamp (P.v[0].pan, -1.f, 1.f);
        const float gL0 = p0 > 0.f ? 1.f - p0 : 1.f, gR0 = p0 < 0.f ? 1.f + p0 : 1.f;

        for (int i = 0; i < num; ++i)
        {
            const float l = inL[i], r = inR[i];
            const int idx = (int) (n & RM);
            inLRing[idx] = l; inRRing[idx] = r; monoRing[idx] = 0.5f * (l + r);
            decAcc += monoRing[idx];
            if (++decCnt >= decim) { dec[(size_t) (decWritten & DM)] = decAcc / (float) decim; ++decWritten; decAcc = 0.f; decCnt = 0; }
            if (((n + 1) % hop) == 0) analyse();

            for (int v = 0; v < 5; ++v)
                for (int c = 0; c < nE[v]; ++c)
                {
                    Eng& e = engs[v][c];
                    int guard = 0;
                    while (e.nextE <= n - S && guard++ < 8) grain (v, c, nE[v], lvl[v]);
                }

            const int64_t rd = n - D;
            const int ri = (int) (rd & OM);
            float ol = oL[ri], orr = oR[ri];
            oL[ri] = 0.f; oR[ri] = 0.f;
            if (dryOn)
            {
                const int di = (int) (rd & RM);
                ol += inLRing[di] * lvl[0] * gL0;
                orr += inRRing[di] * lvl[0] * gR0;
            }
            outL[i] = ol; outR[i] = orr;
            ++n;
        }
    }

private:
    struct Frame
    {
        bool voiced = false; float T = 0.f, m = 0.f;
        float shift[4] { 0.f, 0.f, 0.f, 0.f };
        float gain[4] { 1.f, 1.f, 1.f, 1.f };
        bool active[4] { true, true, true, true };
    };
    struct Eng { int64_t nextE = 0; float r1 = 0.f, r2 = 0.f, r3 = 0.f, rate = 5.f, phase = 0.f; bool active = false; };
    struct Held { int ch, note, vel; };

    static constexpr int RB = 1 << 16, RM = RB - 1, OB = 1 << 15, OM = OB - 1, DB = 2048, DM = DB - 1, HN = 1024, HM = HN - 1, MN = 512, MAXTAU = 330;

    Params P;
    double sr = 48000.0, srD = 12000.0;
    int decim = 4, hop = 256, S = 1920, D = 3360, maxHo = 1439, look = 512;
    int64_t n = 0;
    std::vector<float> monoRing, inLRing, inRRing, oL, oR, dec, xb, dn;
    std::vector<Frame> hist;
    float hann[2049];
    int64_t decWritten = 0, lastHop = -1;
    float decAcc = 0.f; int decCnt = 0;
    int64_t marks[MN]; int64_t markCount = 0, lastMark = 0; bool haveMark = false;
    float Tb[3] { 0.f, 0.f, 0.f }; int nT = 0; bool prevVoiced = false; float vTime = 0.f;
    double tgtSm[4]; bool prevAct[4]; float vibPhase[4], pvTarget[4], pvVal[4], tv[4]; int pvCnt[4];
    bool frozen = false; double frozenTgt[4] { 60, 60, 60, 60 }; bool frozenAct[4] { true, true, true, true };
    Held held[32]; int nHeld = 0;
    Eng engs[5][16];
    uint32_t rs = 12345u;

    float rnd() { rs = rs * 1664525u + 1013904223u; return (float) (rs >> 8) * (1.f / 8388608.f) - 1.f; }

    const Frame& lookup (int64_t tq) const
    {
        static const Frame empty;
        if (tq < 0 || lastHop < 0) return empty;
        int64_t h = std::min<int64_t> (tq / hop, lastHop);
        if (lastHop - h >= HN - 2) return empty;
        return hist[(size_t) (h & HM)];
    }

    void pushMark (int64_t m) { marks[markCount % MN] = m; ++markCount; lastMark = m; }

    int64_t argmaxRange (int64_t a, int64_t b) const
    {
        int64_t bi = a; float bv = -1e30f;
        for (int64_t k = a; k <= b; ++k) { const float v = monoRing[(size_t) (k & RM)]; if (v > bv) { bv = v; bi = k; } }
        return bi;
    }

    void updateMarks (float T)
    {
        const int64_t lim = n - (int64_t) (1.4f * T);
        if (haveMark && lastMark < n - 6000) haveMark = false;
        if (! haveMark) { pushMark (argmaxRange (lim - (int64_t) T, lim)); haveMark = true; }
        int guard = 0;
        while (lastMark + (int64_t) (1.3f * T) <= lim && guard++ < 8)
            pushMark (argmaxRange (lastMark + (int64_t) (0.7f * T), lastMark + (int64_t) (1.3f * T)));
    }

    int64_t snapMark (double cen, float T) const
    {
        int64_t best = -1; double bd = 1e18;
        for (int64_t k = markCount - 1; k >= 0 && markCount - k <= MN; --k)
        {
            const int64_t mk = marks[k % MN];
            if ((double) mk < cen - T) break;
            const double d = std::fabs ((double) mk - cen);
            if (d < bd) { bd = d; best = mk; }
        }
        if (best >= 0 && bd < 0.6 * T) return best;
        return (int64_t) std::llround (cen);
    }

    // ---------------- harmonia ----------------
    void voiceChord (const int* pcs, int k, int inv, double* tgt)
    {
        int rot[4];
        for (int i = 0; i < 4; ++i) rot[i] = pmod (pcs[(inv + i) % k], 12);
        int notes[4];
        int b = 55 + (int) std::lround (P.registerSemis);
        while (pmod (b, 12) != rot[0]) ++b;
        notes[0] = b;
        for (int i = 1; i < 4; ++i) { int q = notes[i - 1] + 1; while (pmod (q, 12) != rot[i]) ++q; notes[i] = q; }
        const float sp = P.spread / 100.f;
        if (sp > 0.25f) notes[1] += 12;
        if (sp > 0.60f) notes[0] -= 12;
        std::sort (notes, notes + 4);
        for (int v = 0; v < 4; ++v) tgt[v] = notes[3 - v];     // voz 1 = mais aguda
    }

    void computeTargets (int mr, double* tgt, bool* act, float* vel)
    {
        for (int v = 0; v < 4; ++v) { tgt[v] = mr; act[v] = true; vel[v] = 127.f; }
        const int sc = std::clamp (P.scale, 0, 10), sz = kScaleSize[sc], key = std::clamp (P.key, 0, 11);
        auto noteAt = [&] (int idx) { return key + 12 * fdiv (idx, sz) + kScales[sc][pmod (idx, sz)]; };
        switch (P.source)
        {
            case FixedInterval:
                for (int v = 0; v < 4; ++v) tgt[v] = mr + P.v[v + 1].interval;
                break;
            case ScaleInterval:
            {
                const int rel = mr - key, oct = fdiv (rel, 12), pc = rel - 12 * oct;
                int bk = 0, bd = 99;
                for (int k = 0; k <= sz; ++k) { const int s = k < sz ? kScales[sc][k] : 12; const int d = std::abs (pc - s); if (d < bd) { bd = d; bk = k; } }
                const int idx = oct * sz + bk;
                for (int v = 0; v < 4; ++v) tgt[v] = noteAt (idx + std::clamp (P.v[v + 1].interval, -14, 14));
                break;
            }
            case ChordName:
            {
                const int len = kChordLen[std::clamp (P.chordType, 0, 12)];
                int pcs[4];
                for (int i = 0; i < len; ++i) pcs[i] = key + kChords[std::clamp (P.chordType, 0, 12)][i];
                voiceChord (pcs, len, P.inversion % len, tgt);
                break;
            }
            case ChordDegrees:
            {
                const int cd = std::clamp (P.chordDegree, 0, 13), deg = (cd % 7) % sz, len = cd >= 7 ? 4 : 3;
                int pcs[4];
                for (int i = 0; i < len; ++i) pcs[i] = noteAt (deg + 2 * i);
                voiceChord (pcs, len, P.inversion % len, tgt);
                break;
            }
            case ChordViaMidi:
            {
                if (nHeld == 0) { for (int v = 0; v < 4; ++v) act[v] = false; break; }
                int sorted[32]; float sv = 0.f;
                for (int i = 0; i < nHeld; ++i) { sorted[i] = held[i].note; sv += (float) held[i].vel; }
                std::sort (sorted, sorted + nHeld);
                int pcs[4], k = 0;
                for (int i = 0; i < nHeld && k < 4; ++i)
                {
                    bool dup = false;
                    for (int j = 0; j < k; ++j) dup = dup || pmod (pcs[j], 12) == pmod (sorted[i], 12);
                    if (! dup) pcs[k++] = sorted[i];
                }
                voiceChord (pcs, k, 0, tgt);
                for (int v = 0; v < 4; ++v) vel[v] = sv / (float) nHeld;
                break;
            }
            case MidiOmni:
            {
                int sorted[32], idxs[32];
                for (int i = 0; i < nHeld; ++i) idxs[i] = i;
                std::sort (idxs, idxs + nHeld, [&] (int a, int b) { return held[a].note > held[b].note; });
                for (int v = 0; v < 4; ++v)
                {
                    if (v < nHeld) { tgt[v] = held[idxs[v]].note; vel[v] = (float) held[idxs[v]].vel; }
                    else act[v] = false;
                }
                (void) sorted;
                break;
            }
            case MidiChannels:
            {
                for (int v = 0; v < 4; ++v)
                {
                    act[v] = false;
                    for (int i = nHeld - 1; i >= 0; --i)
                        if (held[i].ch == v + 1) { tgt[v] = held[i].note; vel[v] = (float) held[i].vel; act[v] = true; break; }
                }
                break;
            }
            default: break;
        }
    }

    // ---------------- analise (a cada "hop") ----------------
    void analyse()
    {
        static const float fminT[4] = { 150.f, 80.f, 60.f, 50.f };
        static const float fmaxT[4] = { 1400.f, 800.f, 450.f, 1500.f };
        const int rg = std::clamp (P.range, 0, 3);
        const int minTau = std::max (2, (int) (srD / fmaxT[rg]));
        const int maxTau = std::min ((int) (srD / fminT[rg]), MAXTAU);
        const int Wn = 256, total = Wn + maxTau;
        bool voiced = false; float Tin = 0.f;

        if (decWritten >= total)
        {
            for (int k = 0; k < total; ++k) xb[(size_t) k] = dec[(size_t) ((decWritten - total + k) & DM)];
            float e = 0.f;
            for (int k = total - Wn; k < total; ++k) e += xb[(size_t) k] * xb[(size_t) k];
            if (std::sqrt (e / (float) Wn) > 0.0015f)
            {
                const float thr = 0.10f + std::clamp (P.tracking, 0.f, 100.f) / 100.f * 0.25f;
                dn[0] = 1.f; float run = 0.f;
                for (int tau = 1; tau < maxTau; ++tau)
                {
                    float d = 0.f;
                    for (int j = 0; j < Wn; ++j) { const float df = xb[(size_t) j] - xb[(size_t) (j + tau)]; d += df * df; }
                    run += d;
                    dn[(size_t) tau] = run > 0.f ? d * (float) tau / run : 1.f;
                }
                int te = -1;
                for (int tau = minTau; tau < maxTau - 1; ++tau)
                    if (dn[(size_t) tau] < thr)
                    {
                        while (tau + 1 < maxTau - 1 && dn[(size_t) (tau + 1)] < dn[(size_t) tau]) ++tau;
                        te = tau; break;
                    }
                if (te > 1 && te < maxTau - 1)
                {
                    const float y0 = dn[(size_t) (te - 1)], y1 = dn[(size_t) te], y2 = dn[(size_t) (te + 1)];
                    const float den = y0 - 2.f * y1 + y2;
                    const float tr = (float) te + (std::fabs (den) > 1e-9f ? 0.5f * (y0 - y2) / den : 0.f);
                    Tin = tr * (float) decim; voiced = true;
                }
            }
        }
        if (voiced)
        {
            if (! prevVoiced) nT = 0;
            Tb[nT % 3] = Tin; ++nT;
            if (nT >= 3) Tin = std::max (std::min (Tb[0], Tb[1]), std::min (std::max (Tb[0], Tb[1]), Tb[2]));
        }

        Frame f;
        const int64_t hopIdx = n / hop;
        const float dt = (float) hop / (float) sr;
        for (int v = 0; v < 4; ++v)
        {
            vibPhase[v] += 6.28318530718f * P.v[v + 1].vibRate * dt;
            if (vibPhase[v] > 6.28318530718f) vibPhase[v] -= 6.28318530718f;
        }

        if (voiced)
        {
            const float m = 69.f + 12.f * std::log2 ((float) sr / Tin / 440.f);
            const int mr = (int) std::lround (m);
            const float dev = m - (float) mr;
            double tgt[4]; bool act[4]; float vel[4];
            computeTargets (mr, tgt, act, vel);
            if (P.freeze)
            {
                if (! frozen) { frozen = true; for (int v = 0; v < 4; ++v) { frozenTgt[v] = tgt[v]; frozenAct[v] = act[v]; } }
                for (int v = 0; v < 4; ++v) { tgt[v] = frozenTgt[v]; act[v] = frozenAct[v]; }
            }
            else frozen = false;

            const float nat = std::clamp (P.naturalize, 0.f, 100.f) / 100.f;
            const float gl = std::clamp (P.glide, 0.f, 100.f) / 100.f;
            const float a = 1.f - std::exp (-dt / (0.002f + gl * gl * 0.25f));
            const bool midiSrc = P.source >= ChordViaMidi;
            const float vs = std::clamp (P.velSens, 0.f, 100.f) / 100.f;
            for (int v = 0; v < 4; ++v)
            {
                if (! prevVoiced || ! prevAct[v]) tgtSm[v] = tgt[v]; else tgtSm[v] += (double) a * (tgt[v] - tgtSm[v]);
                prevAct[v] = act[v];
                const VoiceParams& vp = P.v[v + 1];
                const float ms = vTime * 1000.f;
                const float ramp = ms > vp.vibOnset ? std::min (1.f, (ms - vp.vibOnset) / 200.f) : 0.f;
                const float sn = std::sin (vibPhase[v]);
                const float vibS = vp.vibPitch / 100.f * 0.5f * ramp * sn;
                const float am = 1.f + vp.vibAmp / 100.f * 0.4f * ramp * sn;
                if (--pvCnt[v] <= 0) { pvTarget[v] = rnd(); pvCnt[v] = (int) (0.3f / dt * (1.f + 0.5f * (rnd() + 1.f))); }
                pvVal[v] += 0.15f * (pvTarget[v] - pvVal[v]);
                const double outP = tgtSm[v] + (double) (nat * dev + vibS + pvVal[v] * P.pitchVar / 100.f * 0.3f);
                f.shift[v] = (float) std::clamp (outP - (double) m, -24.0, 24.0);
                f.gain[v] = am * (midiSrc ? (1.f - vs) + vs * vel[v] / 127.f : 1.f);
                f.active[v] = act[v];
            }
            f.voiced = true; f.T = Tin; f.m = m;
            vTime += dt;
            updateMarks (Tin);
        }
        else
        {
            haveMark = false; vTime = 0.f;
            for (int v = 0; v < 4; ++v) prevAct[v] = false;
        }
        prevVoiced = voiced;
        hist[(size_t) (hopIdx & HM)] = f;
        lastHop = hopIdx;
    }

    // ---------------- sintese PSOLA ----------------
    void grain (int v, int c, int nEng, float lvl)
    {
        Eng& e = engs[v][c];
        const int64_t t = e.nextE;
        const Frame& fr = lookup (t + look);
        const bool vo = fr.voiced;
        const float Tin = vo ? fr.T : 0.008f * (float) sr;
        float semis = 0.f, amp = 1.f; bool act = true;
        if (v > 0 && vo) { semis = fr.shift[v - 1]; amp = fr.gain[v - 1]; act = fr.active[v - 1]; }
        const bool choir = nEng > 1;
        float off = 0.f;
        if (choir)
        {
            semis += e.r1 * P.chPitch / 100.f * 0.35f;
            semis += P.chVib / 100.f * 0.30f * (0.5f + 0.5f * std::fabs (e.r3)) * std::sin (e.phase);
            off += P.chTime / 100.f * 0.025f * (float) sr * (0.5f + 0.5f * e.r2);
        }
        if (v > 0) off += P.timeVar / 100.f * 0.02f * (float) sr * tv[v - 1];
        const float ratio = std::clamp (std::exp2 (semis / 12.f), 0.25f, 4.f);
        const float Tout = std::max (Tin / ratio, 32.f);
        e.phase += 6.28318530718f * e.rate * Tout / (float) sr;
        if (e.phase > 6.28318530718f) e.phase -= 6.28318530718f;
        e.nextE = t + (int64_t) std::lround (Tout);
        if (! act) return;

        const float throat = v > 0 ? std::clamp (P.v[v].throat, 0.5f, 2.f) : 1.f;
        const float ho = std::min (Tin * throat, (float) maxHo);
        const int hoI = (int) ho;
        if (hoI < 8) return;
        const int64_t c0 = snapMark ((double) t - off, Tin);
        float g = Tout / (Tin * throat) * amp * lvl;
        if (choir) g /= std::sqrt ((float) nEng);
        float pan = std::clamp (P.v[v].pan, -1.f, 1.f);
        if (choir) pan += P.chSpread / 100.f * (2.f * ((float) c + 0.5f) / (float) nEng - 1.f);
        pan = std::clamp (pan, -1.f, 1.f);
        const float ang = (pan + 1.f) * 0.78539816f;
        const float gl = g * std::cos (ang) * 1.41421356f, gr = g * std::sin (ang) * 1.41421356f;
        const float step = 1.f / throat, invHo = 1.f / (2.f * ho);
        int j0 = -hoI;
        if (t + j0 < n - D) j0 = (int) (n - D - t);
        for (int j = j0; j <= hoI; ++j)
        {
            const float w = hann[(int) (((float) j + ho) * invHo * 2048.f)];
            const double sp = (double) c0 + (double) j * step;
            const int64_t si = (int64_t) std::floor (sp);
            const float fr2 = (float) (sp - (double) si);
            const float s0 = monoRing[(size_t) (si & RM)], s1 = monoRing[(size_t) ((si + 1) & RM)];
            const float s = (s0 + (s1 - s0) * fr2) * w;
            const size_t oi = (size_t) ((t + j) & OM);
            oL[oi] += s * gl; oR[oi] += s * gr;
        }
    }
};
} // namespace voc
