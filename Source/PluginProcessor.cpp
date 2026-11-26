#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout TheVocProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto C = [&] (const String& id, const String& name, StringArray ch, int def)
        { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, ch, def)); };
    auto F = [&] (const String& id, const String& name, float lo, float hi, float def)
        { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def)); };
    auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
        { p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto B = [&] (const String& id, const String& name, bool def)
        { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    C ("range", "Input Range", { "Soprano", "Alto/Tenor", "Baritone/Bass", "Instrument" }, 1);
    F ("tracking", "Tracking", 0.f, 100.f, 40.f);
    C ("source", "Harmony Source", { "Fixed Interval", "Scale Interval", "Chord Name", "Chord Degrees", "Chord Via MIDI", "MIDI Omni", "MIDI Channels" }, 1);
    C ("key", "Key / Root", { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 0);
    C ("scale", "Scale", { "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic" }, 0);
    C ("chordType", "Chord", { "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Maj7", "Dom7", "Min7", "Min7b5", "Dim7", "Maj6", "Min6" }, 0);
    C ("chordDeg", "Chord Degree", { "1st", "2nd", "3rd", "4th", "5th", "6th", "7th", "1st 7", "2nd 7", "3rd 7", "4th 7", "5th 7", "6th 7", "7th 7" }, 0);
    C ("inversion", "Inversion", { "Root", "1st Inv", "2nd Inv", "3rd Inv" }, 0);
    F ("spread", "Spread", 0.f, 100.f, 30.f);
    F ("register", "Register", -24.f, 24.f, 0.f);
    F ("velSens", "MIDI Velocity Sens", 0.f, 100.f, 60.f);
    F ("natural", "Naturalize", 0.f, 100.f, 50.f);
    F ("pitchVar", "Pitch Variation", 0.f, 100.f, 20.f);
    F ("timeVar", "Timing Variation", 0.f, 100.f, 15.f);
    F ("glide", "Glide", 0.f, 100.f, 30.f);
    B ("freeze", "Freeze", false);
    B ("choirBypass", "Choir Bypass", false);
    C ("choirSize", "Choir Size", { "2 Voices", "4 Voices", "8 Voices", "16 Voices" }, 2);
    F ("chVib", "Choir Vibrato", 0.f, 100.f, 20.f);
    F ("chPitch", "Choir Pitch", 0.f, 100.f, 30.f);
    F ("chTime", "Choir Timing", 0.f, 100.f, 40.f);
    F ("chSpread", "Choir Spread", 0.f, 100.f, 60.f);

    const float defGain[5] = { 0.f, -6.f, -7.f, -8.f, -8.f };
    const float defPan[5]  = { 0.f, -35.f, 35.f, -70.f, 70.f };
    const int defInt[5]    = { 0, 2, 4, -2, -7 };
    for (int v = 0; v < 5; ++v)
    {
        const String s (v), nm = v == 0 ? "Input " : "Voice " + s + " ";
        F ("gain" + s, nm + "Gain", -60.f, 12.f, defGain[v]);
        F ("pan" + s, nm + "Pan", -100.f, 100.f, defPan[v]);
        B ("mute" + s, nm + "Mute", false);
        B ("solo" + s, nm + "Solo", false);
        B ("choirOn" + s, nm + "Choir", false);
        if (v > 0)
        {
            I ("interval" + s, nm + "Interval", -24, 24, defInt[v]);
            F ("throat" + s, nm + "Throat Length", 0.7f, 1.4f, 1.f);
            F ("vibRate" + s, nm + "Vibrato Rate", 3.f, 8.f, 5.5f);
            F ("vibOnset" + s, nm + "Vibrato Onset", 0.f, 500.f, 80.f);
            F ("vibPitch" + s, nm + "Vibrato Pitch", 0.f, 100.f, 0.f);
            F ("vibAmp" + s, nm + "Vibrato Amp", 0.f, 100.f, 0.f);
        }
    }
    return { p.begin(), p.end() };
}

TheVocProcessor::TheVocProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool TheVocProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == AudioChannelSet::stereo() || l.getMainInputChannelSet() == AudioChannelSet::mono();
}

void TheVocProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    setLatencySamples (engine.latencySamples());
}

void TheVocProcessor::updateParams()
{
    auto g = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };
    voc::Params& p = params;
    p.range = (int) g ("range"); p.tracking = g ("tracking");
    p.source = (int) g ("source"); p.key = (int) g ("key"); p.scale = (int) g ("scale");
    p.chordType = (int) g ("chordType"); p.chordDegree = (int) g ("chordDeg"); p.inversion = (int) g ("inversion");
    p.spread = g ("spread"); p.registerSemis = g ("register"); p.velSens = g ("velSens");
    p.naturalize = g ("natural"); p.pitchVar = g ("pitchVar"); p.timeVar = g ("timeVar"); p.glide = g ("glide");
    p.freeze = g ("freeze") > 0.5f; p.choirBypass = g ("choirBypass") > 0.5f;
    p.choirSize = 2 << (int) g ("choirSize");
    p.chVib = g ("chVib"); p.chPitch = g ("chPitch"); p.chTime = g ("chTime"); p.chSpread = g ("chSpread");
    for (int v = 0; v < 5; ++v)
    {
        const String s (v);
        voc::VoiceParams& vp = p.v[v];
        vp.gainDb = g ("gain" + s); vp.pan = g ("pan" + s) / 100.f;
        vp.mute = g ("mute" + s) > 0.5f; vp.solo = g ("solo" + s) > 0.5f; vp.choirOn = g ("choirOn" + s) > 0.5f;
        if (v > 0)
        {
            vp.interval = (int) std::lround (g ("interval" + s)); vp.throat = g ("throat" + s);
            vp.vibRate = g ("vibRate" + s); vp.vibOnset = g ("vibOnset" + s);
            vp.vibPitch = g ("vibPitch" + s); vp.vibAmp = g ("vibAmp" + s);
        }
    }
    engine.setParams (p);
}

void TheVocProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getChannel(), m.getNoteNumber(), (int) m.getVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getChannel(), m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    midi.clear();
    updateParams();

    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : l;
    const float* inR = getTotalNumInputChannels() > 1 ? r : l;
    engine.process (l, inR, l, r, n);
}

AudioProcessorEditor* TheVocProcessor::createEditor() { return new TheVocEditor (*this); }

void TheVocProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void TheVocProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TheVocProcessor(); }
