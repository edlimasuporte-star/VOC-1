#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Visual minimalista anos 80: azul-petroleo, laranja e creme.
namespace theme
{
    inline juce::Colour orange() { return juce::Colour (0xffee7d4d); }
    inline juce::Colour cream()  { return juce::Colour (0xfff2e6d0); }
    inline juce::Colour navy()   { return juce::Colour (0xff0a2233); }
    inline juce::Colour teal()   { return juce::Colour (0xff4f9fbc); }
    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class VocLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VocLookAndFeel()
    {
        using namespace juce;
        setColour (Label::textColourId, theme::cream());
        setColour (Slider::textBoxTextColourId, theme::cream());
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (ComboBox::textColourId, theme::cream());
        setColour (PopupMenu::backgroundColourId, theme::navy());
        setColour (PopupMenu::textColourId, theme::cream());
        setColour (PopupMenu::highlightedBackgroundColourId, theme::orange());
        setColour (PopupMenu::highlightedTextColourId, theme::navy());
        setColour (TextEditor::backgroundColourId, theme::navy());
        setColour (TextEditor::textColourId, theme::cream());
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
        const float rad = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const float al = s.isEnabled() ? 1.f : 0.35f;
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, a1, true);
        val.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, ang, true);
        g.setColour (theme::cream().withAlpha (0.22f * al));
        g.strokePath (track, juce::PathStrokeType (2.f));
        g.setColour (theme::orange().withAlpha (al));
        g.strokePath (val, juce::PathStrokeType (3.f));
        const float sn = std::sin (ang), cs = std::cos (ang);
        g.setColour (theme::cream().withAlpha (al));
        g.drawLine (c.x + sn * rad * 0.35f, c.y - cs * rad * 0.35f, c.x + sn * rad * 0.8f, c.y - cs * rad * 0.8f, 2.f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
    {
        const float al = s.isEnabled() ? 1.f : 0.35f;
        if (st == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            g.setColour (theme::cream().withAlpha (0.22f * al)); g.fillRect (cx - 1.5f, (float) y, 3.f, (float) h);
            g.setColour (theme::orange().withAlpha (al));       g.fillRect (cx - 1.5f, pos, 3.f, (float) (y + h) - pos);
            g.setColour (theme::cream().withAlpha (al));        g.fillRect (cx - 13.f, pos - 3.f, 26.f, 6.f);
        }
        else if (st == juce::Slider::LinearHorizontal)
        {
            const float cy = (float) y + (float) h * 0.5f;
            g.setColour (theme::cream().withAlpha (0.22f * al)); g.fillRect ((float) x, cy - 1.f, (float) w, 2.f);
            g.setColour (theme::orange().withAlpha (al));       g.fillRect ((float) x, cy - 1.f, pos - (float) x, 2.f);
            g.setColour (theme::cream().withAlpha (al));        g.fillRect (pos - 2.f, cy - 6.f, 4.f, 12.f);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        if (on) { g.setColour (theme::orange()); g.fillRect (r); g.setColour (theme::navy()); }
        else    { g.setColour (theme::cream().withAlpha (0.45f)); g.drawRect (r, 1.2f); g.setColour (theme::cream().withAlpha (hl ? 1.f : 0.8f)); }
        g.setFont (theme::font (11.f, true));
        g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        const float al = cb.isEnabled() ? 1.f : 0.35f;
        g.setColour (theme::navy().withAlpha (0.55f)); g.fillRect (r);
        g.setColour (theme::orange().withAlpha (al));  g.drawRect (r, 1.f);
        juce::Path p; p.addTriangle ((float) w - 15.f, (float) h * 0.5f - 2.f, (float) w - 7.f, (float) h * 0.5f - 2.f, (float) w - 11.f, (float) h * 0.5f + 3.f);
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (12.f); }
    juce::Font getLabelFont (juce::Label&) override { return theme::font (11.f); }
};
