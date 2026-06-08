#include "MainComponent.h"

#include <cmath>
#include <utility>

namespace
{
const auto background = juce::Colour { 0xff101010 };
const auto panel = juce::Colour { 0xff202020 };
const auto panelDark = juce::Colour { 0xff151515 };
const auto panelEdge = juce::Colour { 0xff383838 };
const auto accent = juce::Colour { 0xffffb347 };
const auto red = juce::Colour { 0xffd71919 };
const auto green = juce::Colour { 0xff35c925 };
const auto text = juce::Colour { 0xffeeeeee };
const auto muted = juce::Colour { 0xff9b9b9b };

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title = {})
{
    g.setGradientFill(juce::ColourGradient(panel.brighter(0.08f), area.getTopLeft().toFloat(),
                                           panelDark, area.getBottomRight().toFloat(), false));
    g.fillRoundedRectangle(area.toFloat(), 5.0f);
    g.setColour(panelEdge);
    g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 5.0f, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour(text.withAlpha(0.9f));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), area.reduced(10, 6).removeFromTop(18), juce::Justification::centredLeft);
    }
}

void drawLedMeter(juce::Graphics& g, juce::Rectangle<int> area, int lit = 9, bool stereo = false)
{
    const int columns = stereo ? 2 : 1;
    const int gap = stereo ? 5 : 0;
    const int colWidth = (area.getWidth() - gap) / columns;
    for (int c = 0; c < columns; ++c)
    {
        auto col = area.withX(area.getX() + c * (colWidth + gap)).withWidth(colWidth);
        const int segments = 14;
        const int segGap = 2;
        const int segH = juce::jmax(2, (col.getHeight() - (segments - 1) * segGap) / segments);
        for (int i = 0; i < segments; ++i)
        {
            const int y = col.getBottom() - (i + 1) * segH - i * segGap;
            auto colour = i > 11 ? juce::Colours::red : (i > 8 ? juce::Colours::yellow : juce::Colours::limegreen);
            g.setColour(colour.withAlpha(i < lit ? 0.9f : 0.18f));
            g.fillRoundedRectangle((float) col.getX(), (float) y, (float) col.getWidth(), (float) segH, 1.5f);
        }
    }
}

void drawTransportButton(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                         juce::Colour colour = juce::Colour { 0xff303030 })
{
    g.setGradientFill(juce::ColourGradient(colour.brighter(0.3f), area.getTopLeft().toFloat(),
                                           colour.darker(0.5f), area.getBottomRight().toFloat(), false));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 4.0f, 1.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(label, area, juce::Justification::centred);
}

void drawReel(juce::Graphics& g, juce::Rectangle<int> area, float rotation)
{
    auto b = area.toFloat();
    const auto centre = b.getCentre();
    const float radius = juce::jmin(b.getWidth(), b.getHeight()) * 0.48f;

    g.setGradientFill(juce::ColourGradient(juce::Colour { 0xff777777 }, b.getTopLeft(),
                                           juce::Colour { 0xff111111 }, b.getBottomRight(), true));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

    for (int i = 0; i < 6; ++i)
    {
        const float a = rotation + i * juce::MathConstants<float>::twoPi / 6.0f;
        juce::Path slot;
        slot.addRoundedRectangle(-radius * 0.10f, -radius * 0.70f, radius * 0.20f, radius * 0.48f, 8.0f);
        slot.applyTransform(juce::AffineTransform::rotation(a).translated(centre));
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillPath(slot);
    }

    g.setGradientFill(juce::ColourGradient(juce::Colour { 0xffdedede }, centre.translated(-18.0f, -18.0f),
                                           juce::Colour { 0xff333333 }, centre.translated(18.0f, 18.0f), false));
    g.fillEllipse(centre.x - radius * 0.18f, centre.y - radius * 0.18f, radius * 0.36f, radius * 0.36f);
}

void drawSignalBlock(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                     const juce::String& subtitle, bool highlight = false)
{
    g.setColour(highlight ? panel.brighter(0.18f) : panelDark.brighter(0.05f));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    g.setColour(highlight ? accent.withAlpha(0.55f) : juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 4.0f, 1.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(title, area.reduced(6, 7).removeFromTop(17), juce::Justification::centred);
    g.setColour(muted);
    g.setFont(juce::FontOptions(9.5f));
    g.drawFittedText(subtitle, area.reduced(7, 30), juce::Justification::centred, 2);
}

void drawArrow(juce::Graphics& g, juce::Rectangle<int> area)
{
    juce::Path p;
    const float cx = (float) area.getCentreX();
    const float cy = (float) area.getCentreY();
    p.startNewSubPath((float) area.getX() + 3.0f, cy);
    p.lineTo((float) area.getRight() - 7.0f, cy);
    p.lineTo((float) area.getRight() - 12.0f, cy - 5.0f);
    p.startNewSubPath((float) area.getRight() - 7.0f, cy);
    p.lineTo((float) area.getRight() - 12.0f, cy + 5.0f);
    g.setColour(muted.withAlpha(0.8f));
    g.strokePath(p, juce::PathStrokeType(1.7f));
    (void) cx;
}
} // namespace

class MainComponent::MeterPlaceholder final : public juce::Component
{
public:
    explicit MeterPlaceholder(juce::String labelText) : label(std::move(labelText)) {}

    void paint(juce::Graphics& g) override
    {
        drawPanel(g, getLocalBounds(), {});
        auto area = getLocalBounds().reduced(10, 12);
        drawLedMeter(g, area.removeFromTop(area.getHeight() - 24), 11, label.containsIgnoreCase("master"));
        g.setColour(text.withAlpha(0.8f));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(label, area, juce::Justification::centred);
    }

private:
    juce::String label;
};

class MainComponent::ChannelStrip final : public juce::Component,
                                         public juce::FileDragAndDropTarget
{
public:
    explicit ChannelStrip(int channelIndex) : index(channelIndex)
    {
        inputLabel.setText("INPUT", juce::dontSendNotification);
        inputLabel.setJustificationType(juce::Justification::centred);
        inputLabel.setColour(juce::Label::textColourId, text);
        addAndMakeVisible(inputLabel);

        armButton.setClickingTogglesState(true);
        armButton.setButtonText("ARM");
        armButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff540b0b });
        armButton.setColour(juce::TextButton::buttonOnColourId, red);
        armButton.setColour(juce::TextButton::textColourOffId, text);
        addAndMakeVisible(armButton);

        inputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        inputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 17);
        inputGain.setRange(-12.0, 12.0, 0.1);
        inputGain.setValue((channelIndex % 7 - 3) * 0.7, juce::dontSendNotification);
        inputGain.setTextValueSuffix(" dB");
        addAndMakeVisible(inputGain);
    }

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dragging = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dragging = false;
        loaded = ! files.isEmpty();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        drawPanel(g, area, {});

        g.setColour(text);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String(index), area.removeFromTop(20), juce::Justification::centred);

        auto meterArea = juce::Rectangle<int>(7, 30, 13, 94);
        drawLedMeter(g, meterArea, loaded ? 12 : (6 + index % 6), false);

        g.setColour(muted);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("PEAK", 24, 31, getWidth() - 28, 14, juce::Justification::centred);
        g.setColour(loaded ? green : red.withAlpha(0.45f));
        g.fillEllipse(31.0f, 50.0f, 5.0f, 5.0f);

        if (dragging)
        {
            g.setColour(accent.withAlpha(0.22f));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 5.0f);
            g.setColour(accent);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 5.0f, 2.0f);
        }
    }

    void resized() override
    {
        inputLabel.setBounds(22, 64, getWidth() - 26, 16);
        inputGain.setBounds(20, 80, getWidth() - 24, 58);
        armButton.setBounds(20, getHeight() - 31, getWidth() - 34, 22);
    }

private:
    int index = 0;
    bool dragging = false;
    bool loaded = false;
    juce::Label inputLabel;
    juce::Slider inputGain;
    juce::TextButton armButton;
};

class MainComponent::ExportSettingsComponent final : public juce::Component
{
public:
    ExportSettingsComponent()
    {
        heading.setText("Render / Export", juce::dontSendNotification);
        heading.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        heading.setColour(juce::Label::textColourId, text);
        addAndMakeVisible(heading);

        for (auto* label : { &format, &resolution, &speed })
        {
            label->setColour(juce::Label::textColourId, text);
            addAndMakeVisible(label);
        }
        format.setText("Format: WAV", juce::dontSendNotification);
        resolution.setText("Resolution: 24 Bit", juce::dontSendNotification);
        speed.setText("Tape speed: 15 IPS", juce::dontSendNotification);

        exportButton.setButtonText("RENDER");
        exportButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(1);
        };
        addAndMakeVisible(exportButton);
        setSize(410, 230);
    }

    void paint(juce::Graphics& g) override { drawPanel(g, getLocalBounds(), {}); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(24, 18);
        heading.setBounds(area.removeFromTop(36));
        format.setBounds(area.removeFromTop(28));
        resolution.setBounds(area.removeFromTop(28));
        speed.setBounds(area.removeFromTop(28));
        area.removeFromTop(22);
        exportButton.setBounds(area.removeFromTop(34).withSizeKeepingCentre(130, 34));
    }

private:
    juce::Label heading, format, resolution, speed;
    juce::TextButton exportButton;
};

MainComponent::MainComponent()
{
    setOpaque(true);
    setSize(1280, 1180);

    titleLabel.setText("24-SPUR STANDALONE GEILALIZER – BANDMASCHINE", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(titleLabel);

    loadButton.setButtonText("REW");
    stopButton.setButtonText("STOP");
    playButton.setButtonText("PLAY");
    exportButton.setButtonText("RENDER");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff116b12 });
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff5b5b5b });
    for (auto* button : { &loadButton, &playButton, &stopButton, &exportButton })
        addAndMakeVisible(button);
    exportButton.onClick = [this] { showExportDialog(); };

    channelViewport.setViewedComponent(&channelContainer, false);
    channelViewport.setScrollBarsShown(false, true);
    channelViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(channelViewport);

    for (int i = 1; i <= channelCount; ++i)
    {
        auto* strip = channels.add(new ChannelStrip(i));
        channelContainer.addAndMakeVisible(strip);
    }

    addAndMakeVisible(masterGroup);
    masterGroup.setText("MASTER");
    masterGroup.setColour(juce::GroupComponent::textColourId, text);

    limiterToggle.setButtonText("LIMITER ON");
    limiterToggle.setToggleState(true, juce::dontSendNotification);
    limiterToggle.setColour(juce::ToggleButton::textColourId, text);
    addAndMakeVisible(limiterToggle);

    outputTrimLabel.setText("OUTPUT", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centred);
    outputTrimLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(outputTrimLabel);
    outputTrim.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputTrim.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    outputTrim.setRange(-12.0, 12.0, 0.1);
    outputTrim.setValue(0.0, juce::dontSendNotification);
    outputTrim.setTextValueSuffix(" dB");
    addAndMakeVisible(outputTrim);

    masterMeter = std::make_unique<MeterPlaceholder>("L / R");
    addAndMakeVisible(*masterMeter);

    limiterActivityLabel.setText("MONITOR: STEREO  |  15 IPS", juce::dontSendNotification);
    limiterActivityLabel.setJustificationType(juce::Justification::centred);
    limiterActivityLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
    limiterActivityLabel.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(limiterActivityLabel);

    auto& lf = getLookAndFeel();
    lf.setColour(juce::Slider::thumbColourId, juce::Colour { 0xffd9d9d9 });
    lf.setColour(juce::Slider::rotarySliderFillColourId, red);
    lf.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour { 0xff454545 });
    lf.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff303030 });
    lf.setColour(juce::TextButton::textColourOffId, text);
    lf.setColour(juce::Label::textColourId, text);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto bounds = getLocalBounds().reduced(16);
    auto header = bounds.removeFromTop(32);
    (void) header;

    auto deck = bounds.removeFromTop(205);
    drawPanel(g, deck, "ÜBERSICHT");
    auto deckInner = deck.reduced(18, 26);
    drawReel(g, deckInner.removeFromLeft(315).reduced(6, 2), 0.35f);
    drawReel(g, deckInner.removeFromRight(315).reduced(6, 2), -0.22f);

    auto transportPanel = deck.withSizeKeepingCentre(360, 104).translated(0, -12);
    drawPanel(g, transportPanel, {});
    auto t = transportPanel.reduced(14, 10);
    g.setColour(muted);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("PROJECT", t.removeFromTop(16).removeFromLeft(75), juce::Justification::centredLeft);
    g.setColour(juce::Colours::black);
    auto projectBox = juce::Rectangle<int>(transportPanel.getX() + 98, transportPanel.getY() + 11, 210, 18);
    g.fillRect(projectBox);
    g.setColour(text);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("My Mix – Geilalizer Session", projectBox.reduced(5, 0), juce::Justification::centred);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("01:02:15:12", transportPanel.getX() + 108, transportPanel.getY() + 39, 140, 24, juce::Justification::centred);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("15 IPS", transportPanel.getRight() - 72, transportPanel.getY() + 42, 50, 18, juce::Justification::centred);

    auto btnRow = juce::Rectangle<int>(transportPanel.getX() + 29, transportPanel.getBottom() - 36, 302, 26);
    drawTransportButton(g, btnRow.removeFromLeft(48), "REW"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "FFWD"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "STOP"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "PLAY", juce::Colour { 0xff128111 }); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "REC", red.darker(0.2f));

    auto machine = juce::Rectangle<int>(deck.getCentreX() - 170, deck.getBottom() - 70, 340, 54);
    g.setGradientFill(juce::ColourGradient(juce::Colour { 0xff9d9284 }, machine.getTopLeft().toFloat(),
                                           juce::Colour { 0xff3f3933 }, machine.getBottomRight().toFloat(), false));
    g.fillRoundedRectangle(machine.toFloat(), 8.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(machine.toFloat(), 8.0f, 2.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.drawText("GEILALIZER", machine, juce::Justification::centred);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("24 TRACK TAPE MACHINE", machine.withTrimmedTop(32), juce::Justification::centred);

    auto bandY = deck.getY() + 134;
    g.setColour(juce::Colour { 0xff6b4028 });
    g.drawLine((float) deck.getX() + 210.0f, (float) bandY, (float) machine.getX(), (float) bandY + 20.0f, 7.0f);
    g.drawLine((float) machine.getRight(), (float) bandY + 20.0f, (float) deck.getRight() - 210.0f, (float) bandY, 7.0f);

    auto channelArea = bounds.removeFromTop(392);
    drawPanel(g, channelArea, "24-SPUR KANÄLE");

    auto masterTextArea = channelArea.removeFromRight(116).reduced(10, 34);
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MASTER", masterTextArea.removeFromTop(18), juce::Justification::centred);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("OUTPUT / MONITOR", masterTextArea.removeFromBottom(18), juce::Justification::centred);

    bounds.removeFromTop(10);
    auto perChannel = bounds.removeFromTop(128);
    drawPanel(g, perChannel, "KANAL STRUKTUR (PRO KANAL)");
    auto flow = perChannel.reduced(18, 34);
    const juce::StringArray channelTitles { "AUDIO FILE", "INPUT", "NAM 1", "NAM 2", "NAM 3", "FADER", "CHANNEL OUT" };
    const juce::StringArray channelSubs { "Drag / Drop WAV", "Gain staging", "PRE / INPUT AMP", "CHANNEL / LINE", "HEAD / TAPE RESPONSE", "Post-tape level", "24 → Mixbus" };
    for (int i = 0; i < channelTitles.size(); ++i)
    {
        auto block = flow.removeFromLeft(i == 5 ? 72 : 136);
        drawSignalBlock(g, block, channelTitles[i], channelSubs[i], i >= 2 && i <= 4);
        if (i != channelTitles.size() - 1)
            drawArrow(g, flow.removeFromLeft(28));
    }

    bounds.removeFromTop(10);
    auto mixbus = bounds.removeFromTop(128);
    drawPanel(g, mixbus, "MIXBUS ARCHITEKTUR");
    auto mixFlow = mixbus.reduced(18, 34);
    const juce::StringArray mixTitles { "CHANNEL OUTPUTS", "MIXBUS SUMMING", "NAM 4", "NAM 5", "NAM 6", "NAM 7", "MASTER OUTPUT" };
    const juce::StringArray mixSubs { "1–24", "Analog summing", "MIXBUS / FARBE", "TAPE", "LIMIT", "CONVERTER", "L / R" };
    for (int i = 0; i < mixTitles.size(); ++i)
    {
        auto block = mixFlow.removeFromLeft(i == 0 ? 140 : 130);
        drawSignalBlock(g, block, mixTitles[i], mixSubs[i], i >= 2 && i <= 5);
        if (i != mixTitles.size() - 1)
            drawArrow(g, mixFlow.removeFromLeft(24));
    }

    bounds.removeFromTop(10);
    auto bottom = bounds.removeFromTop(150);
    drawPanel(g, bottom, "HAUPTSTEUERUNG & RENDER");
    auto b = bottom.reduced(18, 34);
    auto transport = b.removeFromLeft(320);
    drawPanel(g, transport, "TRANSPORT");
    auto tr = transport.reduced(14, 38);
    drawTransportButton(g, tr.removeFromLeft(54), "<<"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), ">>"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "STOP"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "PLAY", juce::Colour { 0xff128111 }); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "REC", red.darker(0.2f));

    b.removeFromLeft(18);
    auto time = b.removeFromLeft(170);
    drawPanel(g, time, "TIME / POSITION");
    g.setColour(text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("01:02:15:12", time.reduced(14, 46), juce::Justification::centred);

    b.removeFromLeft(18);
    auto project = b.removeFromLeft(230);
    drawPanel(g, project, "PROJECT");
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("My Mix – Geilalizer Session", project.reduced(14, 45), juce::Justification::centred);
    g.drawText("48.0 kHz  |  24 Bit  |  15 IPS", project.reduced(14, 70), juce::Justification::centred);

    b.removeFromLeft(18);
    auto render = b.removeFromLeft(200);
    drawPanel(g, render, "RENDER / EXPORT");
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("FORMAT   WAV", render.reduced(14, 40).removeFromTop(22), juce::Justification::centredLeft);
    g.drawText("RESOLUTION   24 Bit", render.reduced(14, 62).removeFromTop(22), juce::Justification::centredLeft);
    drawTransportButton(g, render.withSizeKeepingCentre(120, 30).translated(0, 35), "RENDER", juce::Colour { 0xff5b5b5b });

    b.removeFromLeft(18);
    drawPanel(g, b, "OUTPUT METERS");
    drawLedMeter(g, b.reduced(35, 34), 12, true);

    auto footer = bounds.removeFromTop(76);
    drawPanel(g, footer, {});
    g.setColour(text.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("NAM-Modelle sind fest eingebaut und werden beim Start geladen. Alle Fader arbeiten Post-Processing. Limiter-Station kann ein/aus geschaltet werden.",
               footer.reduced(18), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
    g.drawText("GEILALIZER", footer.removeFromRight(250), juce::Justification::centred);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    auto title = bounds.removeFromTop(32);
    titleLabel.setBounds(title.removeFromLeft(650));

    bounds.removeFromTop(205);
    auto channelPanel = bounds.removeFromTop(392).reduced(16, 30);
    auto masterArea = channelPanel.removeFromRight(122);
    channelPanel.removeFromRight(8);

    channelViewport.setBounds(channelPanel);
    constexpr int stripWidth = 76;
    constexpr int stripHeight = 158;
    constexpr int stripGap = 7;
    const int rowGap = 8;
    const int cols = 12;
    channelContainer.setBounds(0, 0, cols * (stripWidth + stripGap) + stripGap, stripHeight * 2 + rowGap + 2);
    for (int i = 0; i < channels.size(); ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        channels[i]->setBounds(stripGap + col * (stripWidth + stripGap), row * (stripHeight + rowGap), stripWidth, stripHeight);
    }

    masterGroup.setBounds(masterArea);
    auto m = masterArea.reduced(12, 26);
    if (masterMeter != nullptr)
        masterMeter->setBounds(m.removeFromTop(155));
    m.removeFromTop(8);
    outputTrimLabel.setBounds(m.removeFromTop(18));
    outputTrim.setBounds(m.removeFromTop(82));
    m.removeFromTop(8);
    limiterToggle.setBounds(m.removeFromTop(24));
    limiterActivityLabel.setBounds(m.removeFromTop(42));

    auto deckArea = juce::Rectangle<int>(getWidth() / 2 - 150, 92, 300, 28);
    loadButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    stopButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    playButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    exportButton.setBounds(deckArea.removeFromLeft(78));
}

void MainComponent::showExportDialog()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Render / Export";
    options.dialogBackgroundColour = panelDark;
    options.content.setOwned(new ExportSettingsComponent());
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
