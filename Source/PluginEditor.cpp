#include "PluginEditor.h"

using namespace juce;

namespace { constexpr int kW = 980, kH = 580, kStripX = 20, kStripY = 178, kStripW = 122, kStripH = 362, kGap = 6; }

VKnob* TheVocEditor::knob (const String& id, const String& name)
{ auto* k = new VKnob (proc.apvts, id, name); owned.emplace_back (k); addAndMakeVisible (k); return k; }
VCombo* TheVocEditor::combo (const String& id, const String& name)
{ auto* c = new VCombo (proc.apvts, id, name); owned.emplace_back (c); addAndMakeVisible (c); return c; }
VToggle* TheVocEditor::toggle (const String& id, const String& text)
{ auto* t = new VToggle (proc.apvts, id, text); owned.emplace_back (t); addAndMakeVisible (t); return t; }

TheVocEditor::TheVocEditor (TheVocProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (kW, kH);

    Random rng (80);
    for (int i = 0; i < 160; ++i) speckles.push_back ({ rng.nextFloat() * kW, rng.nextFloat() * kH });

    source = combo ("source", "HARMONY SOURCE");
    key = combo ("key", "KEY / ROOT");
    scaleC = combo ("scale", "SCALE");
    chordType = combo ("chordType", "CHORD");
    chordDeg = combo ("chordDeg", "CHORD");
    inversion = combo ("inversion", "INVERSION");
    spread = knob ("spread", "SPREAD");
    reg = knob ("register", "REGISTER");
    velSens = knob ("velSens", "MIDI VEL");
    natural = knob ("natural", "NATURALIZE");
    pitchVar = knob ("pitchVar", "PITCH VAR");
    timeVar = knob ("timeVar", "TIME VAR");
    glide = knob ("glide", "GLIDE");
    freeze = toggle ("freeze", "FREEZE");
    choirSize = combo ("choirSize", "CHOIR SIZE");
    choirBypass = toggle ("choirBypass", "CHOIR BYPASS");
    chVib = knob ("chVib", "VIBRATO");
    chPitch = knob ("chPitch", "PITCH");
    chTime = knob ("chTime", "TIMING");
    chSpread = knob ("chSpread", "STEREO");
    range = combo ("range", "INPUT RANGE");
    {
        auto* t = new VHSlider (proc.apvts, "tracking", "TRACK"); owned.emplace_back (t); addAndMakeVisible (t); tracking = t;
    }

    const char* vn[4] = { "RATE", "ONSET", "PITCH", "AMP" };
    const char* vid[4] = { "vibRate", "vibOnset", "vibPitch", "vibAmp" };
    for (int v = 0; v < 5; ++v)
    {
        const String s (v);
        auto* f = new VFader (proc.apvts, "gain" + s); owned.emplace_back (f); addAndMakeVisible (f); fader[v] = f;
        f->s.setTextValueSuffix (" dB");
        pan[v] = knob ("pan" + s, "PAN");
        mute[v] = toggle ("mute" + s, "M");
        solo[v] = toggle ("solo" + s, "S");
        choirOn[v] = toggle ("choirOn" + s, "CHOIR");
        if (v > 0)
        {
            interval[v] = knob ("interval" + s, "INTERVAL");
            throat[v] = knob ("throat" + s, "THROAT");
            for (int k = 0; k < 4; ++k)
            {
                auto* h = new VHSlider (proc.apvts, String (vid[k]) + s, vn[k]); owned.emplace_back (h); addAndMakeVisible (h); vib[v][k] = h;
            }
            auto* sl = &interval[v]->s;
            sl->textFromValueFunction = [this] (double val)
            {
                const int iv = (int) std::lround (val);
                const bool scaleMode = ((int) proc.apvts.getRawParameterValue ("source")->load()) == 1;
                if (! scaleMode) return String (iv > 0 ? "+" : "") + String (iv) + " st";
                const int a = std::min (std::abs (iv), 14);
                String nm = a == 0 ? "U" : a == 7 ? "8ve" : a == 14 ? "15ma" : String (a + 1) + (a == 1 ? "nd" : a == 2 ? "rd" : "th");
                return String (iv > 0 ? "+" : iv < 0 ? "-" : "") + nm;
            };
            sl->valueFromTextFunction = [] (const String& t) { return (double) t.getIntValue(); };
            sl->updateText();
        }
    }
    startTimerHz (15);
    timerCallback();
}

TheVocEditor::~TheVocEditor() { setLookAndFeel (nullptr); }

void TheVocEditor::timerCallback()
{
    const int s = (int) proc.apvts.getRawParameterValue ("source")->load();
    if (s == lastSource) return;
    lastSource = s;
    auto en = [] (Component* c, bool on) { c->setEnabled (on); c->setAlpha (on ? 1.f : 0.35f); };
    en (key, s >= 1 && s <= 3);
    en (scaleC, s == 1 || s == 3);
    en (chordType, s == 2); en (chordDeg, s == 3);
    chordType->setVisible (s != 3); chordDeg->setVisible (s == 3);
    en (inversion, s == 2 || s == 3);
    en (spread, s >= 2 && s <= 4); en (reg, s >= 2 && s <= 4);
    en (velSens, s >= 4);
    for (int v = 1; v < 5; ++v) { en (interval[v], s <= 1); interval[v]->s.updateText(); }
}

void TheVocEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (theme::navy().brighter (0.15f), 0.f, 0.f, theme::navy().brighter (0.15f), (float) kW, 0.f, false));
    g.fillAll();
    ColourGradient grad (theme::navy().brighter (0.15f), 0.f, 0.f, theme::navy().brighter (0.15f), (float) kW, 0.f, false);
    grad.addColour (0.5, theme::teal());
    g.setGradientFill (grad); g.fillAll();
    g.setColour (theme::orange().withAlpha (0.5f));
    for (auto& p : speckles) g.fillEllipse (p.x, p.y, 2.f, 2.f);

    // titulo
    g.setColour (theme::orange());
    g.setFont (theme::font (30.f, false)); g.drawText ("THE", 26, 10, 120, 34, Justification::left);
    g.setFont (theme::font (150.f, true).withHorizontalScale (0.72f)); g.drawText ("VOC", 20, 20, 520, 150, Justification::centredLeft);
    g.setColour (theme::cream());
    g.setFont (theme::font (11.f, true));
    g.drawText ("VOCAL HARMONY GENERATOR", 470, 118, 180, 14, Justification::left);
    g.drawText ("4 VOICES  /  CHOIR  /  MIDI", 470, 132, 180, 14, Justification::left);

    // faixas
    const char* names[5] = { "INPUT", "1", "2", "3", "4" };
    for (int v = 0; v < 5; ++v)
    {
        auto r = Rectangle<int> (kStripX + v * (kStripW + kGap), kStripY, kStripW, kStripH);
        g.setColour (theme::navy().withAlpha (0.35f)); g.fillRect (r);
        g.setColour (theme::cream().withAlpha (0.25f)); g.drawRect (r, 1);
        g.setColour (theme::orange());
        g.setFont (theme::font (v == 0 ? 14.f : 26.f, true));
        g.drawText (names[v], r.getX() + 8, r.getY() + 4, 60, 24, Justification::left);
    }

    // rotulos de secao
    auto label = [&] (const String& t, int x, int y, int w)
    {
        g.setColour (theme::cream()); g.setFont (theme::font (11.f, true)); g.drawText (t, x, y, w, 14, Justification::left);
        g.setColour (theme::cream().withAlpha (0.3f)); g.fillRect (x + (int) theme::font (11.f, true).getStringWidthFloat (t) + 8, y + 7, w - (int) theme::font (11.f, true).getStringWidthFloat (t) - 8, 1);
    };
    label ("HARMONY", 672, 8, 288);
    label ("HUMANIZE", 672, 262, 288);
    label ("CHOIR", 672, 420, 288);
}

void TheVocEditor::resized()
{
    // coluna direita
    const int rx = 672;
    source->setBounds (rx, 26, 288, 40);
    key->setBounds (rx, 72, 92, 40); scaleC->setBounds (rx + 98, 72, 92, 40); inversion->setBounds (rx + 196, 72, 92, 40);
    chordType->setBounds (rx, 118, 288, 40); chordDeg->setBounds (rx, 118, 288, 40);
    spread->setBounds (rx, 166, 92, 84); reg->setBounds (rx + 98, 166, 92, 84); velSens->setBounds (rx + 196, 166, 92, 84);

    natural->setBounds (rx, 282, 70, 84); pitchVar->setBounds (rx + 72, 282, 70, 84);
    timeVar->setBounds (rx + 144, 282, 70, 84); glide->setBounds (rx + 216, 282, 72, 84);
    freeze->setBounds (rx, 376, 288, 26);

    choirSize->setBounds (rx, 438, 140, 40); choirBypass->setBounds (rx + 148, 452, 140, 26);
    chVib->setBounds (rx, 486, 70, 84); chPitch->setBounds (rx + 72, 486, 70, 84);
    chTime->setBounds (rx + 144, 486, 70, 84); chSpread->setBounds (rx + 216, 486, 72, 84);

    // faixas
    for (int v = 0; v < 5; ++v)
    {
        const int x = kStripX + v * (kStripW + kGap), y = kStripY;
        if (v == 0)
        {
            range->setBounds (x + 6, y + 32, kStripW - 12, 40);
            tracking->setBounds (x + 6, y + 78, kStripW - 12, 18);
        }
        else
        {
            interval[v]->setBounds (x + 2, y + 32, 58, 70); throat[v]->setBounds (x + 62, y + 32, 58, 70);
        }
        fader[v]->setBounds (x + 6, y + 108, 52, 168);
        mute[v]->setBounds (x + 66, y + 112, 22, 22); solo[v]->setBounds (x + 92, y + 112, 22, 22);
        pan[v]->setBounds (x + 62, y + 142, 56, 74);
        choirOn[v]->setBounds (x + 66, y + 224, 50, 22);
        if (v > 0)
            for (int k = 0; k < 4; ++k) vib[v][k]->setBounds (x + 6, y + 284 + k * 19, kStripW - 12, 18);
    }
}
