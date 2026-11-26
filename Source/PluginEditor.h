#pragma once
#include "PluginProcessor.h"
#include "VocLookAndFeel.h"

using APVTS = juce::AudioProcessorValueTreeState;

struct VKnob : juce::Component
{
    VKnob (APVTS& a, const juce::String& id, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
        l.setFont (theme::font (10.f, true)); l.setColour (juce::Label::textColourId, theme::cream().withAlpha (0.8f));
        addAndMakeVisible (s); addAndMakeVisible (l);
        att = std::make_unique<APVTS::SliderAttachment> (a, id, s);
    }
    void resized() override { auto r = getLocalBounds(); l.setBounds (r.removeFromTop (12)); s.setBounds (r); }
    juce::Slider s; juce::Label l; std::unique_ptr<APVTS::SliderAttachment> att;
};

struct VHSlider : juce::Component   // slider horizontal com nome a esquerda
{
    VHSlider (APVTS& a, const juce::String& id, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 16);
        l.setText (name, juce::dontSendNotification); l.setFont (theme::font (9.f, true));
        l.setColour (juce::Label::textColourId, theme::cream().withAlpha (0.8f));
        addAndMakeVisible (s); addAndMakeVisible (l);
        att = std::make_unique<APVTS::SliderAttachment> (a, id, s);
    }
    void resized() override { auto r = getLocalBounds(); l.setBounds (r.removeFromLeft (34)); s.setBounds (r); }
    juce::Slider s; juce::Label l; std::unique_ptr<APVTS::SliderAttachment> att;
};

struct VFader : juce::Component
{
    VFader (APVTS& a, const juce::String& id)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
        s.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (s);
        att = std::make_unique<APVTS::SliderAttachment> (a, id, s);
    }
    void resized() override { s.setBounds (getLocalBounds()); }
    juce::Slider s; std::unique_ptr<APVTS::SliderAttachment> att;
};

struct VCombo : juce::Component
{
    VCombo (APVTS& a, const juce::String& id, const juce::String& name)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (a.getParameter (id))) cb.addItemList (p->choices, 1);
        l.setText (name, juce::dontSendNotification); l.setFont (theme::font (10.f, true));
        l.setColour (juce::Label::textColourId, theme::cream().withAlpha (0.8f));
        addAndMakeVisible (cb); addAndMakeVisible (l);
        att = std::make_unique<APVTS::ComboBoxAttachment> (a, id, cb);
    }
    void resized() override { auto r = getLocalBounds(); l.setBounds (r.removeFromTop (13)); cb.setBounds (r); }
    juce::ComboBox cb; juce::Label l; std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

struct VToggle : juce::ToggleButton
{
    VToggle (APVTS& a, const juce::String& id, const juce::String& text)
    {
        setButtonText (text);
        att = std::make_unique<APVTS::ButtonAttachment> (a, id, *this);
    }
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

class TheVocEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TheVocEditor (TheVocProcessor&);
    ~TheVocEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    VKnob* knob (const juce::String& id, const juce::String& name);
    VCombo* combo (const juce::String& id, const juce::String& name);
    VToggle* toggle (const juce::String& id, const juce::String& text);

    TheVocProcessor& proc;
    VocLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<juce::Point<float>> speckles;
    int lastSource = -1;

    VCombo *source {}, *key {}, *scaleC {}, *chordType {}, *chordDeg {}, *inversion {}, *range {}, *choirSize {};
    VKnob *spread {}, *reg {}, *velSens {}, *natural {}, *pitchVar {}, *timeVar {}, *glide {}, *chVib {}, *chPitch {}, *chTime {}, *chSpread {};
    VKnob *interval[5] {}, *throat[5] {}, *pan[5] {};
    VFader* fader[5] {};
    VToggle *mute[5] {}, *solo[5] {}, *choirOn[5] {}, *freeze {}, *choirBypass {};
    VHSlider *tracking {}, *vib[5][4] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheVocEditor)
};
