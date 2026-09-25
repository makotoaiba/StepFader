#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float totalGain (float fader, float trim)
    {
        return fader <= StepFaderProcessor::minDb + 0.05f ? 0.0f
                                                          : juce::Decibels::decibelsToGain (fader + trim);
    }
}

juce::String StepFaderProcessor::dbToText (float db)
{
    if (db <= minDb + 0.05f) return juce::String::fromUTF8 (u8"-∞ dB");
    return (db > 0.049f ? "+" : "") + juce::String (db, 1) + " dB";
}

juce::AudioProcessorValueTreeState::ParameterLayout StepFaderProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    juce::NormalisableRange<float> faderRange (minDb, maxDb, 0.01f);
    faderRange.setSkewForCentre (-12.0f);

    auto textFn = [] (float v, int) { return dbToText (v); };
    auto fromText = [] (const juce::String& t)
    {
        if (t.containsChar (0x221e) || t.containsIgnoreCase ("inf")) return minDb;
        return juce::jlimit (minDb, maxDb, t.retainCharacters ("+-.0123456789").getFloatValue());
    };

    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fader", 1 }, "Fader", faderRange, 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (textFn)
                                             .withValueFromStringFunction (fromText)
                                             .withLabel ("dB")));

    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "trim", 1 }, "Trim", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.049f ? "+" : "") + juce::String (v, 1) + " dB"; })
            .withLabel ("dB")));

    // UI setting, not meant for automation
    l.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "glide", 1 }, "Glide",
        juce::StringArray { juce::String::fromUTF8 (u8"即時"), "30 ms", "100 ms", "300 ms", "1 s",
                            juce::String::fromUTF8 (u8"1拍"), juce::String::fromUTF8 (u8"2拍"),
                            juce::String::fromUTF8 (u8"1小節") },
        2, juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    return l;
}

StepFaderProcessor::StepFaderProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StepFader", createLayout())
{
    faderDb = apvts.getRawParameterValue ("fader");
    trimDb  = apvts.getRawParameterValue ("trim");
    for (auto& m : memories) m = std::numeric_limits<float>::quiet_NaN();
}

bool StepFaderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out.isDisabled()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void StepFaderProcessor::prepareToPlay (double sampleRate, int)
{
    gainSm.reset (sampleRate, 0.02);   // 20 ms de-zipper
    gainSm.setCurrentAndTargetValue (totalGain (faderDb->load(), trimDb->load()));
}

void StepFaderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = getTotalNumInputChannels();

    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = (float) juce::jlimit (20.0, 999.0, *b);

    gainSm.setTargetValue (totalGain (faderDb->load(), trimDb->load()));

    if (gainSm.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }
    }
    else
    {
        buffer.applyGain (0, n, gainSm.getCurrentValue());
    }

    const float pl = numCh > 0 ? buffer.getMagnitude (0, 0, n) : 0.0f;
    const float pr = numCh > 1 ? buffer.getMagnitude (1, 0, n) : pl;
    peakL = juce::jmax (pl, peakL.load() * 0.92f);
    peakR = juce::jmax (pr, peakR.load() * 0.92f);
}

juce::AudioProcessorEditor* StepFaderProcessor::createEditor()
{
    return new StepFaderEditor (*this);
}

void StepFaderProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int i = 0; i < numMemories; ++i)
    {
        const float v = memories[(size_t) i].load();
        state.setProperty ("mem" + juce::String (i), std::isnan (v) ? juce::var() : juce::var (v), nullptr);
    }
    state.setProperty ("preMute", preMuteDb.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void StepFaderProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < numMemories; ++i)
    {
        const auto v = state.getProperty ("mem" + juce::String (i));
        memories[(size_t) i] = v.isVoid() ? std::numeric_limits<float>::quiet_NaN() : (float) v;
    }
    preMuteDb = (float) state.getProperty ("preMute", 0.0f);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StepFaderProcessor();
}
