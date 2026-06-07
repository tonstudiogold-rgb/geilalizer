#include "MainComponent.h"

#include <cmath>
#include <utility>

namespace
{
constexpr auto background = juce::Colour { 0xff151515 };
constexpr auto panel = juce::Colour { 0xff222222 };
constexpr auto panelDark = juce::Colour { 0xff1a1a1a };
constexpr auto accent = juce::Colour { 0xffffb347 };
constexpr auto text = juce::Colour { 0xffeeeeee };

void styleRotary(juce::Slider& slider, double min, double max, double value, const juce::String& suffix = {})
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    slider.setRange(min, max, 0.1);
    slider.setValue(value, juce::dontSendNotification);
    slider.setTextValueSuffix(suffix);
}

void styleSteppedRotary(juce::Slider& slider, double min, double max, double value)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setRange(min, max, 1.0);
    slider.setValue(value, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, value);
}

void styleLinear(juce::Slider& slider, double min, double max, double value, const juce::String& suffix = {})
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    slider.setRange(min, max, 0.1);
    slider.setValue(value, juce::dontSendNotification);
    slider.setTextValueSuffix(suffix);
}
} // namespace

class MainComponent::MeterPlaceholder final : public juce::Component
{
public:
    explicit MeterPlaceholder(juce::String labelText) : label(std::move(labelText)) {}

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        auto meterArea = getLocalBounds().reduced(6, 8);
        const auto barCount = 8;
        const auto gap = 3;
        const auto barHeight = (meterArea.getHeight() - gap * (barCount - 1)) / barCount;
        for (int i = 0; i < barCount; ++i)
        {
            const auto y = meterArea.getBottom() - (i + 1) * barHeight - i * gap;
            const auto levelColour = i > 5 ? juce::Colours::red : (i > 3 ? juce::Colours::yellow : juce::Colours::limegreen);
            g.setColour(levelColour.withAlpha(0.28f));
            g.fillRoundedRectangle((float) meterArea.getX(), (float) y,
                                   (float) meterArea.getWidth(), (float) barHeight, 2.0f);
        }

        g.setColour(text.withAlpha(0.75f));
        g.setFont(12.0f);
        g.drawFittedText(label, getLocalBounds().reduced(3), juce::Justification::centred, 2);
    }

private:
    juce::String label;
};

class MainComponent::ChannelStrip final : public juce::Component,
                                         public juce::FileDragAndDropTarget
{
public:
    explicit ChannelStrip(int channelIndex) : index(channelIndex), meter("Meter")
    {
        numberName.setText(juce::String(index).paddedLeft('0', 2), juce::dontSendNotification);
        numberName.setEditable(true, true, false);
        numberName.setJustificationType(juce::Justification::centred);
        numberName.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
        numberName.setColour(juce::Label::textColourId, text);
        numberName.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.2f));
        addAndMakeVisible(numberName);

        armButton.setClickingTogglesState(true);
        armButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        armButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        addAndMakeVisible(armButton);

        inputGainLabel.setText("Input", juce::dontSendNotification);
        inputGainLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(inputGainLabel);
        styleRotary(inputGain, -24.0, 24.0, 0.0, " dB");
        addAndMakeVisible(inputGain);

        preampLabel.setText("Preamp", juce::dontSendNotification);
        preampLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(preampLabel);
        styleSteppedRotary(preamp, 0.0, 10.0, 5.0);
        preamp.textFromValueFunction = [] (double value)
        {
            static const char* labels[] = { "20", "30", "40", "45", "50", "55", "60", "65", "70", "75", "80" };
            const auto indexToUse = juce::jlimit(0, 10, static_cast<int>(std::round(value)));
            return juce::String("Neve ") + labels[indexToUse];
        };
        preamp.valueFromTextFunction = [] (const juce::String& textToParse)
        {
            const auto textValue = textToParse.retainCharacters("0123456789").getIntValue();
            switch (textValue)
            {
                case 20: return 0.0;
                case 30: return 1.0;
                case 40: return 2.0;
                case 45: return 3.0;
                case 50: return 4.0;
                case 55: return 5.0;
                case 60: return 6.0;
                case 65: return 7.0;
                case 70: return 8.0;
                case 75: return 9.0;
                case 80: return 10.0;
                default: return 5.0;
            }
        };
        addAndMakeVisible(preamp);

        faderLabel.setText("Fader", juce::dontSendNotification);
        faderLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(faderLabel);
        styleLinear(fader, -60.0, 12.0, 0.0, " dB");
        fader.setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(fader);

        panLabel.setText("Pan", juce::dontSendNotification);
        panLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(panLabel);
        styleRotary(pan, -100.0, 100.0, 0.0);
        pan.textFromValueFunction = [] (double value)
        {
            if (std::abs(value) < 0.05)
                return juce::String("C");
            return juce::String(std::abs(value), 0) + (value < 0.0 ? " L" : " R");
        };
        addAndMakeVisible(pan);

        addAndMakeVisible(meter);

        loadDropButton.setButtonText("Load / Drop");
        addAndMakeVisible(loadDropButton);
    }

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        dragging = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        dragging = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dragging = false;
        if (! files.isEmpty())
            loadDropButton.setButtonText("Loaded");
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(dragging ? panel.brighter(0.22f) : panel);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(dragging ? accent : juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, 6.0f, dragging ? 2.0f : 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(7, 8);
        numberName.setBounds(area.removeFromTop(26));
        area.removeFromTop(5);
        armButton.setBounds(area.removeFromTop(26));
        area.removeFromTop(5);
        inputGainLabel.setBounds(area.removeFromTop(18));
        inputGain.setBounds(area.removeFromTop(66));
        area.removeFromTop(4);
        preampLabel.setBounds(area.removeFromTop(18));
        preamp.setBounds(area.removeFromTop(66));
        area.removeFromTop(4);
        faderLabel.setBounds(area.removeFromTop(18));
        fader.setBounds(area.removeFromTop(112));
        area.removeFromTop(5);
        panLabel.setBounds(area.removeFromTop(18));
        pan.setBounds(area.removeFromTop(70));
        area.removeFromTop(6);
        meter.setBounds(area.removeFromTop(86));
        area.removeFromTop(7);
        loadDropButton.setBounds(area.removeFromTop(30));
    }

private:
    int index = 0;
    bool dragging = false;

    juce::Label numberName;
    juce::TextButton armButton { "ARM" };
    juce::Slider inputGain;
    juce::Label inputGainLabel;
    juce::Slider preamp;
    juce::Label preampLabel;
    juce::Slider fader;
    juce::Label faderLabel;
    juce::Slider pan;
    juce::Label panLabel;
    MeterPlaceholder meter;
    juce::TextButton loadDropButton;
};

class MainComponent::ExportSettingsComponent final : public juce::Component
{
public:
    ExportSettingsComponent()
    {
        heading.setText("Export WAV", juce::dontSendNotification);
        heading.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        heading.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(heading);

        recordingLength.setText("Recording length: 00:00 (no audio loaded)", juce::dontSendNotification);
        addAndMakeVisible(recordingLength);

        fullRange.setButtonText("Full export");
        fullRange.setRadioGroupId(1);
        fullRange.setToggleState(true, juce::dontSendNotification);
        customRange.setButtonText("Custom range");
        customRange.setRadioGroupId(1);
        addAndMakeVisible(fullRange);
        addAndMakeVisible(customRange);

        addField(fromMin, "0");
        addField(fromSec, "0");
        addField(toMin, "0");
        addField(toSec, "0");

        sampleRate.addItem("44.1 kHz", 1);
        sampleRate.addItem("48 kHz", 2);
        sampleRate.setSelectedId(1);
        bitDepth.addItem("16 bit", 1);
        bitDepth.addItem("24 bit", 2);
        bitDepth.setSelectedId(2);
        addAndMakeVisible(sampleRate);
        addAndMakeVisible(bitDepth);

        exportButton.setButtonText("Export");
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        exportButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(1);
        };
        addAndMakeVisible(exportButton);
        addAndMakeVisible(cancelButton);

        setSize(430, 310);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(panelDark);
        g.setColour(text);
        g.setFont(14.0f);
        g.drawText("From", 24, 130, 70, 24, juce::Justification::centredLeft);
        g.drawText("To", 24, 166, 70, 24, juce::Justification::centredLeft);
        g.drawText("min", 165, 130, 38, 24, juce::Justification::centredLeft);
        g.drawText("sec", 270, 130, 38, 24, juce::Justification::centredLeft);
        g.drawText("min", 165, 166, 38, 24, juce::Justification::centredLeft);
        g.drawText("sec", 270, 166, 38, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(24, 18);
        heading.setBounds(area.removeFromTop(32));
        recordingLength.setBounds(area.removeFromTop(28));
        area.removeFromTop(6);
        fullRange.setBounds(area.removeFromTop(26));
        customRange.setBounds(area.removeFromTop(26));

        fromMin.setBounds(96, 130, 62, 24);
        fromSec.setBounds(205, 130, 62, 24);
        toMin.setBounds(96, 166, 62, 24);
        toSec.setBounds(205, 166, 62, 24);
        sampleRate.setBounds(296, 130, 110, 26);
        bitDepth.setBounds(296, 166, 110, 26);
        exportButton.setBounds(218, 252, 88, 30);
        cancelButton.setBounds(318, 252, 88, 30);
    }

private:
    void addField(juce::TextEditor& editor, const juce::String& textToShow)
    {
        editor.setInputRestrictions(2, "0123456789");
        editor.setText(textToShow);
        addAndMakeVisible(editor);
    }

    juce::Label heading;
    juce::Label recordingLength;
    juce::ToggleButton fullRange;
    juce::ToggleButton customRange;
    juce::TextEditor fromMin;
    juce::TextEditor fromSec;
    juce::TextEditor toMin;
    juce::TextEditor toSec;
    juce::ComboBox sampleRate;
    juce::ComboBox bitDepth;
    juce::TextButton exportButton;
    juce::TextButton cancelButton;
};

MainComponent::MainComponent()
{
    setOpaque(true);
    setSize(1400, 820);

    titleLabel.setText("24-Track Standalone Geilalizer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(titleLabel);

    for (auto* button : { &loadButton, &playButton, &stopButton, &exportButton })
        addAndMakeVisible(button);

    exportButton.onClick = [this] { showExportDialog(); };

    channelViewport.setViewedComponent(&channelContainer, false);
    channelViewport.setScrollBarsShown(false, true);
    addAndMakeVisible(channelViewport);

    for (int i = 1; i <= channelCount; ++i)
    {
        auto* strip = channels.add(new ChannelStrip(i));
        channelContainer.addAndMakeVisible(strip);
    }

    addAndMakeVisible(masterGroup);
    limiterToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(limiterToggle);

    outputTrimLabel.setText("Output Trim", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputTrimLabel);
    styleRotary(outputTrim, -12.0, 12.0, 0.0, " dB");
    addAndMakeVisible(outputTrim);

    masterMeter = std::make_unique<MeterPlaceholder>("Master Meter");
    addAndMakeVisible(*masterMeter);

    limiterActivityLabel.setText("Limiter Activity: idle", juce::dontSendNotification);
    limiterActivityLabel.setJustificationType(juce::Justification::centred);
    limiterActivityLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
    limiterActivityLabel.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(limiterActivityLabel);

    getLookAndFeel().setColour(juce::Slider::thumbColourId, accent);
    getLookAndFeel().setColour(juce::Slider::rotarySliderFillColourId, accent);
    getLookAndFeel().setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff333333 });
    getLookAndFeel().setColour(juce::TextButton::textColourOffId, text);
    getLookAndFeel().setColour(juce::Label::textColourId, text);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(accent);
    g.drawHorizontalLine(64, 20.0f, (float) getWidth() - 20.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto header = area.removeFromTop(48);
    titleLabel.setBounds(header.removeFromLeft(360));

    auto transport = header.removeFromRight(420);
    loadButton.setBounds(transport.removeFromLeft(92).reduced(4));
    playButton.setBounds(transport.removeFromLeft(92).reduced(4));
    stopButton.setBounds(transport.removeFromLeft(92).reduced(4));
    exportButton.setBounds(transport.removeFromLeft(110).reduced(4));

    area.removeFromTop(12);
    auto masterArea = area.removeFromRight(210);
    area.removeFromRight(14);
    channelViewport.setBounds(area);

    constexpr int stripWidth = 92;
    constexpr int stripGap = 8;
    const int stripHeight = juce::jmax(570, area.getHeight() - 18);
    channelContainer.setBounds(0, 0, channelCount * (stripWidth + stripGap) + stripGap, stripHeight);
    auto channelArea = channelContainer.getLocalBounds().reduced(stripGap, 0);
    for (auto* strip : channels)
    {
        strip->setBounds(channelArea.removeFromLeft(stripWidth).withTrimmedBottom(4));
        channelArea.removeFromLeft(stripGap);
    }

    masterGroup.setBounds(masterArea);
    auto m = masterArea.reduced(18, 34);
    limiterToggle.setBounds(m.removeFromTop(30));
    m.removeFromTop(14);
    outputTrimLabel.setBounds(m.removeFromTop(22));
    outputTrim.setBounds(m.removeFromTop(110));
    m.removeFromTop(18);
    if (masterMeter != nullptr)
        masterMeter->setBounds(m.removeFromTop(230));
    m.removeFromTop(18);
    limiterActivityLabel.setBounds(m.removeFromTop(36));
}

void MainComponent::showExportDialog()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Export";
    options.dialogBackgroundColour = panelDark;
    options.content.setOwned(new ExportSettingsComponent());
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
