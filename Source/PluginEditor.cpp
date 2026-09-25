#include "PluginEditor.h"

namespace
{
    juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    const juce::Colour bg       { 0xff1b1e23 };
    const juce::Colour panel    { 0xff252a31 };
    const juce::Colour edge     { 0xff343a43 };
    const juce::Colour textCol  { 0xffe4e6ea };
    const juce::Colour dimText  { 0xff9aa3ad };
    const juce::Colour upCol    { 0xff2e6b45 };
    const juce::Colour downCol  { 0xff7a3434 };
    const juce::Colour neutral  { 0xff2f3640 };
    const juce::Colour memCol   { 0xff34507a };
    const juce::Colour accent   { 0xff5fb3ff };

    constexpr float minDb = StepFaderProcessor::minDb, maxDb = StepFaderProcessor::maxDb;
    constexpr int editorW = 420, editorH = 500;

    juce::String pickFontName()
    {
        static const juce::String name = []
        {
            const auto names = juce::Font::findAllTypefaceNames();
            for (auto* n : { "Yu Gothic UI", "Meiryo UI", "Meiryo", "MS UI Gothic",
                             "Hiragino Sans", "Noto Sans CJK JP", "Noto Sans JP" })
                if (names.contains (n)) return juce::String (n);
            return juce::String();
        }();
        return name;
    }

    juce::Font uiFont (float size, bool bold = false)
    {
        juce::FontOptions o;
        if (const auto n = pickFontName(); n.isNotEmpty()) o = o.withName (n);
        return juce::Font (o.withHeight (size).withStyle (bold ? "Bold" : "Regular"));
    }

    bool isSilent (float db) { return db <= minDb + 0.05f; }
}

//==============================================================================
FaderLookAndFeel::FaderLookAndFeel()
{
    if (const auto n = pickFontName(); n.isNotEmpty())
        setDefaultSansSerifTypefaceName (n);
    setColour (juce::TextButton::textColourOffId, textCol);
    setColour (juce::TextButton::textColourOnId, textCol);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff15181c));
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::textColourId, textCol);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff22262c));
    setColour (juce::Slider::textBoxTextColourId, textCol);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, edge);
    setColour (juce::Slider::thumbColourId, textCol);
    setColour (juce::Label::textColourId, dimText);
}

void FaderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    const float cx = (float) x + (float) w * 0.5f;
    const float top = (float) y, bottom = (float) (y + h);

    // groove
    g.setColour (juce::Colour (0xff101215));
    g.fillRoundedRectangle (cx - 3.0f, top, 6.0f, bottom - top, 3.0f);
    g.setColour (accent.withAlpha (0.35f));
    g.fillRoundedRectangle (cx - 3.0f, pos, 6.0f, bottom - pos, 3.0f);

    // cap
    const float capW = juce::jmin (46.0f, (float) w - 4.0f), capH = 30.0f;
    juce::Rectangle<float> cap (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
    juce::ColourGradient grad (juce::Colour (0xffd9dde2), cap.getX(), cap.getY(),
                               juce::Colour (0xff8d949c), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (juce::Colour (0xff202328));
    g.fillRect (cap.getX() + 5.0f, pos - 1.0f, capW - 10.0f, 2.0f);
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
}

void FaderLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = base;
    if (down) c = c.brighter (0.35f);
    else if (highlighted) c = c.brighter (0.15f);
    if (! b.isEnabled()) c = c.withMultipliedSaturation (0.3f).darker (0.3f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (c.brighter (0.25f).withAlpha (0.6f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

//==============================================================================
StepFaderEditor::StepFaderEditor (StepFaderProcessor& p)
    : AudioProcessorEditor (p), proc (p), faderParam (*p.apvts.getParameter ("fader"))
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    // fader
    faderAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "fader", fader);
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setTooltip (jp (u8"ドラッグ／ホイール0.5dB（Shiftで0.1dB）／ダブルクリックで0dB"));
    fader.onDragStart = [this] { cancelGlide(); };
    fader.onWheelStep = [this] (float d) { stepBy (d, false); };
    addAndMakeVisible (fader);

    // step buttons
    const float steps[] = { 6.0f, 3.0f, 1.0f, -1.0f, -3.0f, -6.0f };
    for (float s : steps)
    {
        auto* b = stepButtons.add (new juce::TextButton ((s > 0 ? "+" : "") + juce::String ((int) s)));
        b->setColour (juce::TextButton::buttonColourId, s > 0 ? upCol : downCol);
        b->onClick = [this, s] { stepBy (s); };
        addAndMakeVisible (b);
    }

    zeroButton.setColour (juce::TextButton::buttonColourId, neutral);
    zeroButton.setTooltip (jp (u8"0 dB に戻す"));
    zeroButton.onClick = [this] { goTo (0.0f); };
    addAndMakeVisible (zeroButton);

    muteButton.setColour (juce::TextButton::buttonColourId, neutral);
    muteButton.setTooltip (jp (u8"-∞ まで下げる／もう一度押すと元の値へ戻す"));
    muteButton.onClick = [this] { toggleMute(); };
    addAndMakeVisible (muteButton);

    for (int i = 0; i < StepFaderProcessor::numMemories; ++i)
    {
        auto& m = memButtons[i];
        m.setColour (juce::TextButton::buttonColourId, memCol);
        m.setTooltip (jp (u8"クリックで呼び出し／右クリック（またはShift+クリック）で今の値を記憶"));
        m.onStore  = [this, i] { proc.memories[(size_t) i] = currentDb(); updateButtons();
                                 proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged (true)); };
        m.onRecall = [this, i] { const float v = proc.memories[(size_t) i].load(); if (! std::isnan (v)) goTo (v); };
        addAndMakeVisible (m);
    }

    // glide
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("glide")))
        glideBox.addItemList (choice->choices, 1);
    glideAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "glide", glideBox);
    glideBox.setTooltip (jp (u8"ボタン操作で値が変わるまでの時間（オートメーションに傾きとして書かれます）"));
    glideLabel.setText (jp (u8"グライド"), juce::dontSendNotification);
    glideLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (glideBox);
    addAndMakeVisible (glideLabel);

    // trim
    trimAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "trim", trim);
    trim.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    trim.setDoubleClickReturnValue (true, 0.0);
    trim.setTooltip (jp (u8"フェーダーに足す固定の補正（オートメーションとは別に使う）"));
    trimLabel.setText (jp (u8"トリム"), juce::dontSendNotification);
    trimLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (trim);
    addAndMakeVisible (trimLabel);

    setSize (editorW, editorH);
    updateButtons();
    startTimerHz (60);
}

StepFaderEditor::~StepFaderEditor()
{
    stopTimer();
    cancelGlide();
    setLookAndFeel (nullptr);
}

//==============================================================================
float StepFaderEditor::currentDb() const
{
    return faderParam.convertFrom0to1 (faderParam.getValue());
}

float StepFaderEditor::targetDb() const
{
    return gliding ? glideTo : currentDb();
}

void StepFaderEditor::setDb (float db)
{
    faderParam.setValueNotifyingHost (faderParam.convertTo0to1 (juce::jlimit (minDb, maxDb, db)));
}

double StepFaderEditor::glideMs() const
{
    const int idx = glideBox.getSelectedItemIndex();
    const double beatMs = 60000.0 / juce::jmax (20.0f, proc.bpm.load());
    switch (idx)
    {
        case 1: return 30.0;
        case 2: return 100.0;
        case 3: return 300.0;
        case 4: return 1000.0;
        case 5: return beatMs;
        case 6: return beatMs * 2.0;
        case 7: return beatMs * 4.0;
        default: return 0.0;
    }
}

void StepFaderEditor::stepBy (float db, bool useGlide)
{
    float base = targetDb();
    if (isSilent (base) && db > 0.0f) base = -60.0f;   // stepping up from -inf starts at -60
    goTo (base + db, useGlide);
}

void StepFaderEditor::goTo (float db, bool useGlide)
{
    db = juce::jlimit (minDb, maxDb, db);
    const double dur = useGlide ? glideMs() : 0.0;

    if (dur <= 0.0)
    {
        if (! gestureOpen) faderParam.beginChangeGesture();
        setDb (db);
        faderParam.endChangeGesture();
        gestureOpen = false;
        gliding = false;
        updateButtons();
        return;
    }

    // start or retarget a glide from where the fader is right now
    if (! gestureOpen) { faderParam.beginChangeGesture(); gestureOpen = true; }
    glideFrom = currentDb();
    glideTo = db;
    glideStartMs = juce::Time::getMillisecondCounterHiRes();
    glideDurMs = dur;
    gliding = true;
}

void StepFaderEditor::cancelGlide()
{
    if (gestureOpen) faderParam.endChangeGesture();
    gestureOpen = false;
    gliding = false;
}

void StepFaderEditor::toggleMute()
{
    const float now = targetDb();
    if (! isSilent (now))
    {
        proc.preMuteDb = now;
        goTo (minDb);
    }
    else
    {
        const float back = proc.preMuteDb.load();
        goTo (isSilent (back) ? 0.0f : back);
    }
}

void StepFaderEditor::updateButtons()
{
    muteButton.setButtonText (isSilent (targetDb()) ? jp (u8"戻す") : jp (u8"-∞"));

    const char* names[] = { "A", "B", "C" };
    for (int i = 0; i < StepFaderProcessor::numMemories; ++i)
    {
        const float v = proc.memories[(size_t) i].load();
        memButtons[i].setButtonText (juce::String (names[i]) + "\n"
                                     + (std::isnan (v) ? jp (u8"（空）") : StepFaderProcessor::dbToText (v)));
    }
}

bool StepFaderEditor::keyPressed (const juce::KeyPress& key)
{
    const bool shift = key.getModifiers().isShiftDown();
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::upKey)       { stepBy (shift ? 3.0f : 1.0f);   return true; }
    if (code == juce::KeyPress::downKey)     { stepBy (shift ? -3.0f : -1.0f); return true; }
    if (code == juce::KeyPress::pageUpKey)   { stepBy (6.0f);  return true; }
    if (code == juce::KeyPress::pageDownKey) { stepBy (-6.0f); return true; }

    const auto c = key.getTextCharacter();
    if (c == '0') { goTo (0.0f); return true; }
    if (c == 'm' || c == 'M') { toggleMute(); return true; }
    if (c >= '1' && c <= '3')
    {
        const float v = proc.memories[(size_t) (c - '1')].load();
        if (! std::isnan (v)) goTo (v);
        return true;
    }
    return false;   // everything else goes back to the host (transport keys etc.)
}

void StepFaderEditor::timerCallback()
{
    if (gliding)
    {
        const double t = (juce::Time::getMillisecondCounterHiRes() - glideStartMs) / glideDurMs;
        if (t >= 1.0)
        {
            setDb (glideTo);
            cancelGlide();
            updateButtons();
        }
        else
        {
            setDb (glideFrom + (glideTo - glideFrom) * (float) t);
        }
    }

    const float db = currentDb();
    const float l = proc.peakL.load(), r = proc.peakR.load();
    if (std::abs (db - shownDb) > 0.001f || std::abs (l - meterL) > 1.0e-4f || std::abs (r - meterR) > 1.0e-4f)
    {
        const bool silentChanged = isSilent (db) != isSilent (shownDb);
        shownDb = db; meterL = l; meterR = r;
        repaint (readoutArea);
        repaint (meterArea);
        if (silentChanged) updateButtons();
    }
}

//==============================================================================
void StepFaderEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (juce::Colour (0xff15181c));
    g.fillRect (0, 0, getWidth(), 40);
    g.setColour (textCol);
    g.setFont (uiFont (17.0f, true));
    g.drawText ("Step Fader", 12, 0, 150, 40, juce::Justification::centredLeft);

    // readout
    g.setColour (panel);
    g.fillRoundedRectangle (readoutArea.toFloat(), 6.0f);
    g.setColour (isSilent (shownDb) ? dimText : textCol);
    g.setFont (uiFont (34.0f, true));
    g.drawText (StepFaderProcessor::dbToText (shownDb), readoutArea, juce::Justification::centred);

    // scale (aligned with the fader)
    g.setFont (uiFont (11.0f));
    const float marks[] = { 12, 6, 0, -6, -12, -24, -40, -60 };
    for (float m : marks)
    {
        const float y = (float) fader.getY() + (float) fader.getPositionOfValue (m);
        g.setColour (std::abs (m) < 0.01f ? textCol : dimText);
        g.drawText ((m > 0 ? "+" : "") + juce::String ((int) m), scaleArea.getX(), (int) y - 7,
                    scaleArea.getWidth() - 8, 14, juce::Justification::centredRight);
        g.setColour (edge);
        g.drawHorizontalLine ((int) y, (float) scaleArea.getRight() - 6.0f, (float) scaleArea.getRight());
    }
    {
        const float y = (float) fader.getY() + (float) fader.getPositionOfValue (minDb);
        g.setColour (dimText);
        g.drawText (jp (u8"-∞"), scaleArea.getX(), (int) y - 7, scaleArea.getWidth() - 8, 14,
                    juce::Justification::centredRight);
    }

    // meters (same scale as the fader)
    auto drawMeter = [&] (juce::Rectangle<float> r, float peak)
    {
        const float top = (float) fader.getY() + (float) fader.getPositionOfValue (maxDb);
        const float bottom = (float) fader.getY() + (float) fader.getPositionOfValue (minDb);
        r = r.withY (top).withBottom (bottom);
        g.setColour (juce::Colour (0xff101215));
        g.fillRect (r);
        const float db = juce::jlimit (minDb, maxDb, juce::Decibels::gainToDecibels (peak, minDb));
        if (db > minDb + 0.5f)
        {
            const float y = (float) fader.getY() + (float) fader.getPositionOfValue (db);
            g.setColour (db > 0.0f ? juce::Colour (0xffff6b6b) : juce::Colour (0xff7bd88f));
            g.fillRect (r.withTop (y));
        }
    };
    auto m = meterArea.toFloat();
    drawMeter (m.removeFromLeft (m.getWidth() * 0.5f).reduced (1.0f, 0.0f), meterL);
    drawMeter (m.reduced (1.0f, 0.0f), meterR);

    g.setColour (dimText);
    g.setFont (uiFont (11.5f));
    g.drawFittedText (jp (u8"↑↓: ±1dB　Shift+↑↓: ±3dB　PgUp/PgDn: ±6dB\n0: 0dB　M: -∞/戻す　1〜3: メモリー呼び出し"),
                      hintArea, juce::Justification::centredLeft, 2);
}

void StepFaderEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (40);
    glideBox.setBounds (header.removeFromRight (110).reduced (6, 8));
    glideLabel.setBounds (header.removeFromRight (70));

    r.reduce (12, 10);
    hintArea = r.removeFromBottom (34);
    r.removeFromBottom (6);

    // left: scale | fader | meters
    auto left = r.removeFromLeft (150);
    scaleArea = left.removeFromLeft (40);
    meterArea = left.removeFromRight (20);
    fader.setBounds (left.reduced (4, 0));

    r.removeFromLeft (14);

    // right column
    readoutArea = r.removeFromTop (58);
    r.removeFromTop (10);

    auto grid = r.removeFromTop (48 * 2 + 6);
    const int bw = (grid.getWidth() - 12) / 3;
    for (int i = 0; i < stepButtons.size(); ++i)
    {
        const int row = i / 3, col = i % 3;
        stepButtons[i]->setBounds (grid.getX() + col * (bw + 6), grid.getY() + row * 54, bw, 48);
    }

    r.removeFromTop (8);
    auto row3 = r.removeFromTop (40);
    const int hw = (row3.getWidth() - 6) / 2;
    zeroButton.setBounds (row3.removeFromLeft (hw));
    row3.removeFromLeft (6);
    muteButton.setBounds (row3);

    r.removeFromTop (8);
    auto row4 = r.removeFromTop (46);
    for (int i = 0; i < StepFaderProcessor::numMemories; ++i)
        memButtons[i].setBounds (row4.getX() + i * (bw + 6), row4.getY(), bw, row4.getHeight());

    r.removeFromTop (6);
    auto trimArea = r.withSizeKeepingCentre (90, juce::jmin (r.getHeight(), 100));
    trimLabel.setBounds (trimArea.removeFromTop (16));
    trim.setBounds (trimArea);
}
