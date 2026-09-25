#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/** Automation-friendly gain fader. "fader" is the parameter to automate. */
class StepFaderProcessor : public juce::AudioProcessor
{
public:
    static constexpr float minDb = -80.0f;   // bottom of the fader = -inf (silence)
    static constexpr float maxDb = 12.0f;
    static constexpr int numMemories = 3;

    StepFaderProcessor();

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // message-thread state saved with the project
    std::array<std::atomic<float>, numMemories> memories;   // NaN = empty
    std::atomic<float> preMuteDb { 0.0f };

    // for the editor
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, bpm { 120.0f };

    static juce::String dbToText (float db);

private:
    std::atomic<float>* faderDb = nullptr;
    std::atomic<float>* trimDb = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepFaderProcessor)
};
