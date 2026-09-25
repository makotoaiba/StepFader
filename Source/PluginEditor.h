#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class FaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FaderLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 16; }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& bg,
                               bool highlighted, bool down) override;
};

/** Fader whose mouse wheel steps in dB (handled by the editor). */
class FaderSlider : public juce::Slider
{
public:
    FaderSlider() : juce::Slider (LinearVertical, NoTextBox) {}
    std::function<void (float)> onWheelStep;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (onWheelStep == nullptr || std::abs (w.deltaY) < 1.0e-4f) return;
        const float step = e.mods.isShiftDown() ? 0.1f : 0.5f;
        onWheelStep (w.deltaY > 0 ? step : -step);
    }
};

/** Click = recall, right-click or Shift+click = store. */
class MemoryButton : public juce::TextButton
{
public:
    std::function<void()> onStore, onRecall;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || e.mods.isShiftDown()) { if (onStore) onStore(); return; }
        juce::TextButton::mouseDown (e);
    }
    void clicked() override { if (onRecall) onRecall(); }
};

class StepFaderEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit StepFaderEditor (StepFaderProcessor&);
    ~StepFaderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    StepFaderProcessor& proc;
    juce::RangedAudioParameter& faderParam;
    FaderLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    FaderSlider fader;
    juce::Slider trim { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::ComboBox glideBox;
    juce::Label glideLabel, trimLabel;
    std::unique_ptr<APVTS::SliderAttachment> faderAtt, trimAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> glideAtt;

    juce::OwnedArray<juce::TextButton> stepButtons;
    juce::TextButton zeroButton { "0 dB" }, muteButton;
    MemoryButton memButtons[StepFaderProcessor::numMemories];

    // glide (runs on the message thread, writes automation through a gesture)
    bool gliding = false, gestureOpen = false;
    double glideStartMs = 0.0, glideDurMs = 0.0;
    float glideFrom = 0.0f, glideTo = 0.0f;

    juce::Rectangle<int> readoutArea, scaleArea, meterArea, hintArea;
    float shownDb = 0.0f, meterL = 0.0f, meterR = 0.0f;

    float currentDb() const;
    float targetDb() const;             // where we are heading (for accumulating steps)
    void setDb (float db);              // raw write (inside a gesture)
    void stepBy (float db, bool useGlide = true);
    void goTo (float db, bool useGlide = true);
    void cancelGlide();
    double glideMs() const;
    void toggleMute();
    void updateButtons();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepFaderEditor)
};
