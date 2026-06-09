#include "MainComponent.h"

#include "core/ImportPlanner.h"
#include "core/LevelMeter.h"
#include "core/LinearResampler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
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

float sanitisePeak(float peak)
{
    if (!std::isfinite(peak) || peak < 0.0f)
        return 0.0f;
    return std::min(peak, 1.0f);
}

int peakToSegments(float peak)
{
    return juce::jlimit(0, 14, static_cast<int>(std::ceil(sanitisePeak(peak) * 14.0f)));
}

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
    const float cy = (float) area.getCentreY();
    p.startNewSubPath((float) area.getX() + 3.0f, cy);
    p.lineTo((float) area.getRight() - 7.0f, cy);
    p.lineTo((float) area.getRight() - 12.0f, cy - 5.0f);
    p.startNewSubPath((float) area.getRight() - 7.0f, cy);
    p.lineTo((float) area.getRight() - 12.0f, cy + 5.0f);
    g.setColour(muted.withAlpha(0.8f));
    g.strokePath(p, juce::PathStrokeType(1.7f));
}
} // namespace

class MainComponent::MeterPlaceholder final : public juce::Component
{
public:
    explicit MeterPlaceholder(juce::String labelText) : label(std::move(labelText)) {}

    void setLevels(float newLeft, float newRight)
    {
        left.pushPeak(newLeft);
        right.pushPeak(newRight);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        drawPanel(g, getLocalBounds(), {});
        auto area = getLocalBounds().reduced(10, 12);
        const bool stereo = label.containsIgnoreCase("L / R");
        const int lit = stereo ? peakToSegments(std::max(left.displayPeak(), right.displayPeak())) : peakToSegments(left.displayPeak());
        drawLedMeter(g, area.removeFromTop(area.getHeight() - 24), lit, stereo);
        g.setColour(text.withAlpha(0.8f));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(label, area, juce::Justification::centred);
        if (left.clipped() || right.clipped())
        {
            g.setColour(red);
            g.drawText("CLIP", getLocalBounds().reduced(8).removeFromTop(15), juce::Justification::centredRight);
        }
        left.decay();
        right.decay();
    }

private:
    juce::String label;
    geilalizer::core::LevelMeter left;
    geilalizer::core::LevelMeter right;
};

class MainComponent::ChannelStrip final : public juce::Component,
                                         public juce::FileDragAndDropTarget
{
public:
    ChannelStrip(int channelIndex, MainComponent& ownerComponent) : index(channelIndex), owner(ownerComponent)
    {
        nameEditor.setText(juce::String(index).paddedLeft('0', 2), juce::dontSendNotification);
        nameEditor.setJustificationType(juce::Justification::centred);
        nameEditor.setColour(juce::Label::textColourId, text);
        nameEditor.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
        nameEditor.setEditable(true, true, false);
        nameEditor.onTextChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).name = nameEditor.getText().toStdString();
        };
        addAndMakeVisible(nameEditor);

        inputLabel.setText("IN", juce::dontSendNotification);
        inputLabel.setJustificationType(juce::Justification::centred);
        inputLabel.setColour(juce::Label::textColourId, text);
        addAndMakeVisible(inputLabel);

        armButton.setClickingTogglesState(true);
        armButton.setButtonText("ARM");
        armButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff540b0b });
        armButton.setColour(juce::TextButton::buttonOnColourId, red);
        armButton.setColour(juce::TextButton::textColourOffId, text);
        armButton.onClick = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).armed = armButton.getToggleState();
        };
        addAndMakeVisible(armButton);

        inputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        inputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 17);
        inputGain.setRange(geilalizer::core::kInputGainMinDb, geilalizer::core::kInputGainMaxDb, 0.1);
        inputGain.setValue(geilalizer::core::kDefaultInputGainDb, juce::dontSendNotification);
        inputGain.setTextValueSuffix(" dB");
        inputGain.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setInputGainDb(static_cast<float>(inputGain.getValue()));
        };
        addAndMakeVisible(inputGain);

        fader.setSliderStyle(juce::Slider::LinearVertical);
        fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange(geilalizer::core::kFaderGainMinDb, geilalizer::core::kFaderGainMaxDb, 0.1);
        fader.setValue(geilalizer::core::kDefaultFaderGainDb, juce::dontSendNotification);
        fader.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setFaderGainDb(static_cast<float>(fader.getValue()));
        };
        addAndMakeVisible(fader);

        pan.setSliderStyle(juce::Slider::LinearHorizontal);
        pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange(-1.0, 1.0, 0.01);
        pan.setValue(0.0, juce::dontSendNotification);
        pan.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setPan(static_cast<float>(pan.getValue()));
        };
        addAndMakeVisible(pan);

        clearButton.setButtonText("CLR");
        clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff282828 });
        clearButton.setColour(juce::TextButton::textColourOffId, muted);
        clearButton.onClick = [this] { owner.clearChannel(static_cast<std::size_t>(index - 1)); };
        addAndMakeVisible(clearButton);
    }

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dragging = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dragging = false;
        if (! files.isEmpty())
            owner.importFileToChannel(juce::File(files[0]), static_cast<std::size_t>(index - 1));
        repaint();
    }

    void setLoaded(bool shouldBeLoaded)
    {
        loaded = shouldBeLoaded;
        repaint();
    }

    void setMeterPeak(float peak)
    {
        meter.pushPeak(peak);
        repaint();
    }

    void setArmState(bool armed)
    {
        armButton.setToggleState(armed, juce::dontSendNotification);
    }

    void setInputGainValue(float db)
    {
        inputGain.setValue(db, juce::dontSendNotification);
    }

    void setFaderValue(float db)
    {
        fader.setValue(db, juce::dontSendNotification);
    }

    void setPanValue(float value)
    {
        pan.setValue(value, juce::dontSendNotification);
    }

    void setNameText(const juce::String& name)
    {
        nameEditor.setText(name, juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        drawPanel(g, area, {});

        g.setColour(text);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));

        auto meterArea = juce::Rectangle<int>(7, 30, 13, 94);
        drawLedMeter(g, meterArea, peakToSegments(meter.displayPeak()), false);

        g.setColour(muted);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("PEAK", 24, 31, getWidth() - 28, 14, juce::Justification::centred);
        g.setColour(loaded ? green : red.withAlpha(0.45f));
        g.fillEllipse(31.0f, 50.0f, 5.0f, 5.0f);
        g.setColour(meter.clipped() ? red : muted);
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(meter.clipped() ? "CLIP" : juce::String(meter.displayDb(), 0) + " dB",
                   22, 72, getWidth() - 26, 12, juce::Justification::centred);
        meter.decay();

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
        nameEditor.setBounds(24, 6, getWidth() - 30, 18);
        inputLabel.setBounds(22, 29, 26, 14);
        inputGain.setBounds(20, 43, 42, 46);
        fader.setBounds(getWidth() - 24, 28, 18, 82);
        pan.setBounds(24, 102, getWidth() - 34, 16);
        armButton.setBounds(20, getHeight() - 49, getWidth() - 34, 20);
        clearButton.setBounds(20, getHeight() - 25, getWidth() - 34, 18);
    }

private:
    int index = 0;
    MainComponent& owner;
    bool dragging = false;
    bool loaded = false;
    geilalizer::core::LevelMeter meter;
    juce::Label nameEditor;
    juce::Label inputLabel;
    juce::Slider inputGain;
    juce::Slider fader;
    juce::Slider pan;
    juce::TextButton armButton;
    juce::TextButton clearButton;
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
        setSize(410, 160);
    }
    void paint(juce::Graphics& g) override { drawPanel(g, getLocalBounds(), {}); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(24, 18);
        heading.setBounds(area.removeFromTop(36));
        format.setBounds(area.removeFromTop(28));
        resolution.setBounds(area.removeFromTop(28));
        speed.setBounds(area.removeFromTop(28));
    }
private:
    juce::Label heading, format, resolution, speed;
};

MainComponent::MainComponent()
{
    setOpaque(true);
    setSize(1280, 1180);
    formatManager.registerBasicFormats();
    audioBlockInputs.resize(channelCount);

    titleLabel.setText("24-TRACK STANDALONE GEILALIZER – TAPE MACHINE", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(titleLabel);

    loadButton.setButtonText("LOAD");
    rewButton.setButtonText("REW");
    stopButton.setButtonText("STOP");
    playButton.setButtonText("PLAY");
    exportButton.setButtonText("RENDER");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff116b12 });
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff5b5b5b });
    for (auto* button : { &loadButton, &rewButton, &playButton, &stopButton, &exportButton })
        addAndMakeVisible(button);

    loadButton.onClick = [this]
    {
        activeFileChooser = std::make_shared<juce::FileChooser>("Load audio file", juce::File{}, "*.wav;*.aiff;*.aif;*.flac;*.mp3");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser = activeFileChooser](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                    importFileToFirstAvailableChannel(file);
            });
    };
    rewButton.onClick = [this] { rewind(); };
    stopButton.onClick = [this] { stop(); };
    playButton.onClick = [this] { play(); };
    exportButton.onClick = [this] { showExportDialog(); };

    channelViewport.setViewedComponent(&channelContainer, false);
    channelViewport.setScrollBarsShown(false, true);
    channelViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(channelViewport);

    for (int i = 1; i <= channelCount; ++i)
    {
        auto* strip = channels.add(new ChannelStrip(i, *this));
        channelContainer.addAndMakeVisible(strip);
    }

    addAndMakeVisible(masterGroup);
    masterGroup.setText("MASTER");
    masterGroup.setColour(juce::GroupComponent::textColourId, text);

    limiterToggle.setButtonText("LIMITER ON");
    limiterToggle.setToggleState(true, juce::dontSendNotification);
    limiterToggle.setColour(juce::ToggleButton::textColourId, text);
    limiterToggle.onClick = [this]
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        audioEngine.session().master.limiterEnabled = limiterToggle.getToggleState();
    };
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
    outputTrim.onValueChange = [this]
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        audioEngine.session().master.outputTrimDb = static_cast<float>(outputTrim.getValue());
    };
    addAndMakeVisible(outputTrim);

    exportRateLabel.setText("RATE", juce::dontSendNotification);
    exportRateLabel.setJustificationType(juce::Justification::centred);
    exportRateLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(exportRateLabel);
    exportSampleRate.addItem("44.1 kHz", 1);
    exportSampleRate.addItem("48 kHz", 2);
    exportSampleRate.setSelectedId(2, juce::dontSendNotification);
    exportSampleRate.setColour(juce::ComboBox::textColourId, text);
    exportSampleRate.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
    addAndMakeVisible(exportSampleRate);

    exportDepthLabel.setText("DEPTH", juce::dontSendNotification);
    exportDepthLabel.setJustificationType(juce::Justification::centred);
    exportDepthLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(exportDepthLabel);
    exportBitDepth.addItem("16 bit", 1);
    exportBitDepth.addItem("24 bit", 2);
    exportBitDepth.setSelectedId(2, juce::dontSendNotification);
    exportBitDepth.setColour(juce::ComboBox::textColourId, text);
    exportBitDepth.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
    addAndMakeVisible(exportBitDepth);

    renderStartLabel.setText("START", juce::dontSendNotification);
    renderStartLabel.setJustificationType(juce::Justification::centred);
    renderStartLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(renderStartLabel);
    renderStartSeconds.setSliderStyle(juce::Slider::LinearHorizontal);
    renderStartSeconds.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
    renderStartSeconds.setRange(0.0, 600.0, 0.1);
    renderStartSeconds.setTextValueSuffix(" s");
    addAndMakeVisible(renderStartSeconds);

    renderLengthLabel.setText("LENGTH", juce::dontSendNotification);
    renderLengthLabel.setJustificationType(juce::Justification::centred);
    renderLengthLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(renderLengthLabel);
    renderLengthSeconds.setSliderStyle(juce::Slider::LinearHorizontal);
    renderLengthSeconds.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
    renderLengthSeconds.setRange(0.0, 600.0, 0.1);
    renderLengthSeconds.setTextValueSuffix(" s");
    renderLengthSeconds.setValue(0.0, juce::dontSendNotification);
    addAndMakeVisible(renderLengthSeconds);

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

    startTimerHz(30);
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    deviceSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    deviceBlockSize = samplesPerBlockExpected > 0 ? samplesPerBlockExpected : 512;
    audioEngine.prepare(deviceSampleRate, deviceBlockSize);
    audioBlockInterleaved.assign(static_cast<std::size_t>(deviceBlockSize) * 2, 0.0f);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr)
        return;

    info.clearActiveBufferRegion();
    if (! playing.load())
        return;

    const int numSamples = info.numSamples;
    const auto startFrame = playheadFrame.load();
    bool reachedEnd = false;

    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        if (audioBlockInputs.size() != channelCount)
            audioBlockInputs.resize(channelCount);
        for (auto& channel : audioBlockInputs)
            channel.assign(static_cast<std::size_t>(numSamples), 0.0f);

        for (std::size_t ch = 0; ch < trackBuffers.size(); ++ch)
        {
            const auto& src = trackBuffers[ch];
            if (src.empty())
                continue;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto frame = static_cast<std::size_t>(std::max<std::int64_t>(0, startFrame + i));
                if (frame < src.size())
                    audioBlockInputs[ch][static_cast<std::size_t>(i)] = src[frame];
            }
        }

        audioBlockInterleaved.assign(static_cast<std::size_t>(numSamples) * 2, 0.0f);
        audioEngine.processRealtime(audioBlockInputs, static_cast<std::size_t>(numSamples), audioBlockInterleaved);
    }

    const int channelsToWrite = std::min(2, info.buffer->getNumChannels());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float left = audioBlockInterleaved[static_cast<std::size_t>(sample) * 2];
        const float right = audioBlockInterleaved[static_cast<std::size_t>(sample) * 2 + 1];
        if (channelsToWrite > 0) info.buffer->setSample(0, info.startSample + sample, left);
        if (channelsToWrite > 1) info.buffer->setSample(1, info.startSample + sample, right);
    }

    const auto nextFrame = startFrame + numSamples;
    playheadFrame.store(nextFrame);
    reachedEnd = maxLoadedFrames() > 0 && static_cast<std::size_t>(nextFrame) >= maxLoadedFrames();
    if (reachedEnd)
        playing.store(false);
}

void MainComponent::releaseResources()
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    audioEngine.reset();
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return ! files.isEmpty();
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        if (! importFileToFirstAvailableChannel(juce::File(path)))
            break;
    }
}

bool MainComponent::importFileToFirstAvailableChannel(const juce::File& file)
{
    if (! file.existsAsFile())
    {
        setStatus("Import failed: file not found.");
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        setStatus("Import failed: unsupported format.");
        return false;
    }

    const auto channelsNeeded = static_cast<std::size_t>(std::min<int>(2, static_cast<int>(reader->numChannels)));
    if (channelsNeeded == 0)
    {
        setStatus("Import failed: file has no readable audio channels.");
        return false;
    }

    const auto placement = geilalizer::core::findFirstContiguousFreeChannels(snapshotOccupiedChannels(), channelsNeeded);
    if (! placement.has_value())
    {
        setStatus(channelsNeeded > 1 ? "Stereo import needs two adjacent free mono tracks."
                                     : "Import failed: all 24 mono tracks are occupied.");
        return false;
    }

    return importFileToChannel(file, *placement);
}

bool MainComponent::importFileToChannel(const juce::File& file, std::size_t firstChannelIndex)
{
    if (! file.existsAsFile() || firstChannelIndex >= trackBuffers.size())
    {
        setStatus("Import failed: invalid file or target track.");
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        setStatus("Import failed: unsupported format.");
        return false;
    }

    const auto numFrames = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, static_cast<juce::int64>(std::numeric_limits<int>::max())));
    if (numFrames <= 0)
    {
        setStatus("Import failed: file contains no samples.");
        return false;
    }

    const int channelsToRead = std::min<int>(2, static_cast<int>(reader->numChannels));
    if (channelsToRead <= 0)
    {
        setStatus("Import failed: file has no readable audio channels.");
        return false;
    }

    const auto channelsNeeded = static_cast<std::size_t>(channelsToRead);
    if (! geilalizer::core::canImportAtChannel(snapshotOccupiedChannels(), firstChannelIndex, channelsNeeded))
    {
        setStatus(channelsToRead > 1 ? "Stereo import needs two adjacent empty mono tracks. Clear tracks first."
                                     : "Import target track is occupied. Clear it first.");
        return false;
    }

    juce::AudioBuffer<float> buffer(channelsToRead, numFrames);
    buffer.clear();
    reader->read(&buffer, 0, numFrames, 0, true, channelsToRead > 1);

    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        for (int ch = 0; ch < channelsToRead; ++ch)
        {
            auto& dest = trackBuffers[firstChannelIndex + static_cast<std::size_t>(ch)];
            std::vector<float> imported(static_cast<std::size_t>(numFrames), 0.0f);
            const float* read = buffer.getReadPointer(ch);
            std::copy(read, read + numFrames, imported.begin());
            dest = geilalizer::core::resampleLinear(imported, reader->sampleRate, deviceSampleRate);
            auto& state = audioEngine.session().channel(firstChannelIndex + static_cast<std::size_t>(ch));
            state.setLoadedAudioFile(file.getFullPathName().toStdString(), ch, dest.size());
            state.armed = true;
        }
    }

    projectName = file.getFileNameWithoutExtension();
    setStatus(file.getFileName() + " geladen auf Kanal " + juce::String(static_cast<int>(firstChannelIndex + 1))
              + (channelsToRead > 1 ? " / " + juce::String(static_cast<int>(firstChannelIndex + 2)) : ""));
    refreshChannelVisuals();
    repaint();
    return true;
}

std::array<bool, MainComponent::channelCount> MainComponent::snapshotOccupiedChannels() const
{
    std::array<bool, channelCount> occupied {};
    const std::lock_guard<std::mutex> lock(trackMutex);
    for (std::size_t i = 0; i < trackBuffers.size(); ++i)
        occupied[i] = ! trackBuffers[i].empty() || audioEngine.session().channel(i).hasAudioFile;
    return occupied;
}

void MainComponent::clearChannel(std::size_t channelIndex)
{
    if (channelIndex >= trackBuffers.size())
        return;

    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        trackBuffers[channelIndex].clear();
        auto& state = audioEngine.session().channel(channelIndex);
        state.clearLoadedAudioFile();
        state.armed = false;
    }

    setStatus("Track " + juce::String(static_cast<int>(channelIndex + 1)) + " cleared.");
    refreshChannelVisuals();
    repaint();
}

void MainComponent::play()
{
    if (maxLoadedFrames() == 0)
    {
        setStatus("Nothing loaded – import audio with LOAD or drag and drop first.");
        return;
    }
    if (static_cast<std::size_t>(std::max<std::int64_t>(0, playheadFrame.load())) >= maxLoadedFrames())
        rewind();
    playing.store(true);
    setStatus("PLAY – fixed tape-machine signal path is running.");
}

void MainComponent::stop()
{
    playing.store(false);
    setStatus("STOP");
}

void MainComponent::rewind()
{
    playing.store(false);
    playheadFrame.store(0);
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        audioEngine.reset();
    }
    setStatus("REW – Position auf Start zurückgesetzt.");
}

std::vector<std::vector<float>> MainComponent::snapshotTrackBuffers() const
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    std::vector<std::vector<float>> snapshot;
    snapshot.reserve(trackBuffers.size());
    for (const auto& buffer : trackBuffers)
        snapshot.push_back(buffer);
    return snapshot;
}

std::size_t MainComponent::maxLoadedFrames() const
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    std::size_t frames = 0;
    for (const auto& buffer : trackBuffers)
        frames = std::max(frames, buffer.size());
    return frames;
}

juce::String MainComponent::currentTimeText() const
{
    const auto frame = std::max<std::int64_t>(0, playheadFrame.load());
    const auto seconds = deviceSampleRate > 0.0 ? static_cast<double>(frame) / deviceSampleRate : 0.0;
    const int mins = static_cast<int>(seconds / 60.0);
    const int secs = static_cast<int>(seconds) % 60;
    const int frames = static_cast<int>((seconds - std::floor(seconds)) * 25.0);
    return juce::String::formatted("%02d:%02d:%02d", mins, secs, frames);
}

void MainComponent::showExportDialog()
{
    if (maxLoadedFrames() == 0)
    {
        setStatus("Render unavailable: no audio files loaded.");
        return;
    }

    const double totalSeconds = deviceSampleRate > 0.0 ? static_cast<double>(maxLoadedFrames()) / deviceSampleRate : 0.0;
    renderStartSeconds.setRange(0.0, totalSeconds, 0.1);
    renderLengthSeconds.setRange(0.0, totalSeconds, 0.1);

    activeFileChooser = std::make_shared<juce::FileChooser>("Save Geilalizer render",
                                                            juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                                                                .getChildFile(projectName + "-geilalized.wav"),
                                                            "*.wav");
    activeFileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser = activeFileChooser](const juce::FileChooser& fc)
        {
            auto out = fc.getResult();
            if (out == juce::File{})
                return;
            if (! out.hasFileExtension("wav"))
                out = out.withFileExtension("wav");
            const double selectedRate = exportSampleRate.getSelectedId() == 1 ? 44100.0 : 48000.0;
            const int selectedDepth = exportBitDepth.getSelectedId() == 1 ? 16 : 24;
            const auto startFrame = static_cast<std::size_t>(std::max(0.0, renderStartSeconds.getValue()) * deviceSampleRate);
            const auto requestedFrames = static_cast<std::size_t>(std::max(0.0, renderLengthSeconds.getValue()) * deviceSampleRate);
            const bool fullLength = requestedFrames == 0;
            renderToFileAsync(out, selectedRate, selectedDepth, startFrame, requestedFrames, fullLength);
        });
}

void MainComponent::renderToFileAsync(const juce::File& outputFile, double sampleRate, int bitDepth,
                                      std::size_t startFrame, std::size_t numFrames, bool fullLength)
{
    const auto buffers = snapshotTrackBuffers();
    geilalizer::core::SessionState sessionCopy;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        sessionCopy = audioEngine.session();
    }

    setStatus("Rendering…");
    exportButton.setEnabled(false);
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    std::thread([safeThis, outputFile, sampleRate, bitDepth, startFrame, numFrames, fullLength, buffers, sessionCopy]() mutable
    {
        juce::String message;
        bool ok = true;
        outputFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream(outputFile.createOutputStream());
        if (stream == nullptr || ! stream->openedOk())
        {
            ok = false;
            message = "Output file cannot be opened.";
        }

        std::unique_ptr<juce::AudioFormatWriter> writer;
        if (ok)
        {
            juce::WavAudioFormat wav;
            writer.reset(wav.createWriterFor(stream.get(), sampleRate, 2u, bitDepth, {}, 0));
            if (writer == nullptr)
            {
                ok = false;
                message = "WAV writer could not be created.";
            }
            else
            {
                stream.release();
            }
        }

        geilalizer::core::RenderResult result;
        if (ok)
        {
            geilalizer::core::RenderEngine renderer;
            geilalizer::core::RenderRequest request;
            request.sampleRate = sampleRate;
            request.bitDepth = bitDepth;
            request.fullLength = fullLength;
            request.startFrame = startFrame;
            request.numFrames = numFrames;
            request.blockSize = 32768;

            result = renderer.renderToSink(sessionCopy, buffers, request,
                [&writer](const float* interleavedStereo, std::size_t frames)
                {
                    if (writer == nullptr || interleavedStereo == nullptr)
                        return false;
                    juce::AudioBuffer<float> buffer(2, static_cast<int>(frames));
                    for (int i = 0; i < static_cast<int>(frames); ++i)
                    {
                        buffer.setSample(0, i, interleavedStereo[static_cast<std::size_t>(i) * 2]);
                        buffer.setSample(1, i, interleavedStereo[static_cast<std::size_t>(i) * 2 + 1]);
                    }
                    return writer->writeFromAudioSampleBuffer(buffer, 0, static_cast<int>(frames));
                });
            ok = result.completed;
            if (! ok)
                message = result.message;
        }

        writer.reset();
        juce::MessageManager::callAsync([safeThis, ok, outputFile, message]
        {
            if (safeThis == nullptr)
                return;
            safeThis->exportButton.setEnabled(true);
            safeThis->setStatus(ok ? "Render complete: " + outputFile.getFullPathName() : "Render failed: " + message);
        });
    }).detach();
}

void MainComponent::timerCallback()
{
    refreshChannelVisuals();
    refreshMasterVisuals();
    repaint();
}

void MainComponent::refreshChannelVisuals()
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    for (int i = 0; i < channels.size(); ++i)
    {
        auto& state = audioEngine.session().channel(static_cast<std::size_t>(i));
        channels[i]->setLoaded(state.hasAudioFile);
        channels[i]->setArmState(state.armed);
        channels[i]->setInputGainValue(state.inputGainDb);
        channels[i]->setFaderValue(state.faderGainDb);
        channels[i]->setPanValue(state.pan);
        channels[i]->setNameText(juce::String(state.name));
        channels[i]->setMeterPeak(state.meterPeak);
    }
}

void MainComponent::refreshMasterVisuals()
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    const auto& master = audioEngine.session().master;
    if (masterMeter != nullptr)
        masterMeter->setLevels(master.meterLeftPeak, master.meterRightPeak);
    limiterToggle.setToggleState(master.limiterEnabled, juce::dontSendNotification);
    outputTrim.setValue(master.outputTrimDb, juce::dontSendNotification);
    limiterActivityLabel.setText((playing.load() ? "PLAY" : "STOP") + juce::String("  |  ")
                                 + (master.limiterActive ? "LIMITER ACTIVE" : "LIMITER READY")
                                 + "  |  15 IPS", juce::dontSendNotification);
}

void MainComponent::setStatus(juce::String message)
{
    const juce::ScopedLock lock(statusLock);
    statusText = std::move(message);
    repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto bounds = getLocalBounds().reduced(16);
    bounds.removeFromTop(32);

    auto deck = bounds.removeFromTop(205);
    drawPanel(g, deck, "OVERVIEW");
    auto deckInner = deck.reduced(18, 26);
    drawReel(g, deckInner.removeFromLeft(315).reduced(6, 2), playing.load() ? 0.95f : 0.35f);
    drawReel(g, deckInner.removeFromRight(315).reduced(6, 2), playing.load() ? -0.85f : -0.22f);

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
    g.drawText(projectName, projectBox.reduced(5, 0), juce::Justification::centred);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText(currentTimeText(), transportPanel.getX() + 108, transportPanel.getY() + 39, 140, 24, juce::Justification::centred);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("15 IPS", transportPanel.getRight() - 72, transportPanel.getY() + 42, 50, 18, juce::Justification::centred);

    auto btnRow = juce::Rectangle<int>(transportPanel.getX() + 29, transportPanel.getBottom() - 36, 302, 26);
    drawTransportButton(g, btnRow.removeFromLeft(48), "LOAD"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "REW"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "STOP"); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "PLAY", juce::Colour { 0xff128111 }); btnRow.removeFromLeft(7);
    drawTransportButton(g, btnRow.removeFromLeft(48), "READY", red.darker(0.2f));

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
    drawPanel(g, channelArea, "24-TRACK CHANNELS");
    auto masterTextArea = channelArea.removeFromRight(116).reduced(10, 34);
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MASTER", masterTextArea.removeFromTop(18), juce::Justification::centred);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("OUTPUT / MONITOR", masterTextArea.removeFromBottom(18), juce::Justification::centred);

    bounds.removeFromTop(10);
    auto perChannel = bounds.removeFromTop(128);
    drawPanel(g, perChannel, "CHANNEL STRUCTURE (PER TRACK)");
    auto flow = perChannel.reduced(18, 34);
    const juce::StringArray channelTitles { "AUDIO FILE", "INPUT", "PREAMP", "CHANNEL", "TAPE HEAD", "FADER", "CHANNEL OUT" };
    const juce::StringArray channelSubs { "Drag / Drop WAV", "Gain staging", "Input iron", "Line amplifier", "15 IPS response", "Post-tape level", "24 → Mixbus" };
    for (int i = 0; i < channelTitles.size(); ++i)
    {
        auto block = flow.removeFromLeft(i == 5 ? 72 : 136);
        drawSignalBlock(g, block, channelTitles[i], channelSubs[i], i >= 2 && i <= 4);
        if (i != channelTitles.size() - 1)
            drawArrow(g, flow.removeFromLeft(28));
    }

    bounds.removeFromTop(10);
    auto mixbus = bounds.removeFromTop(128);
    drawPanel(g, mixbus, "MIXBUS ARCHITECTURE");
    auto mixFlow = mixbus.reduced(18, 34);
    const juce::StringArray mixTitles { "CHANNEL OUTPUTS", "MIXBUS SUMMING", "BUS DRIVE", "TAPE RETURN", "LIMITER", "CONVERTER", "MASTER OUTPUT" };
    const juce::StringArray mixSubs { "1–24", "Analog summing", "Drive color", "15 IPS", "Protection", "A/D/A finish", "L / R" };
    for (int i = 0; i < mixTitles.size(); ++i)
    {
        auto block = mixFlow.removeFromLeft(i == 0 ? 140 : 130);
        drawSignalBlock(g, block, mixTitles[i], mixSubs[i], i >= 2 && i <= 5);
        if (i != mixTitles.size() - 1)
            drawArrow(g, mixFlow.removeFromLeft(24));
    }

    bounds.removeFromTop(10);
    auto bottom = bounds.removeFromTop(150);
    drawPanel(g, bottom, "MASTER CONTROL & RENDER");
    auto b = bottom.reduced(18, 34);
    auto transport = b.removeFromLeft(320);
    drawPanel(g, transport, "TRANSPORT");
    auto tr = transport.reduced(14, 38);
    drawTransportButton(g, tr.removeFromLeft(54), "LOAD"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "REW"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "STOP"); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "PLAY", juce::Colour { 0xff128111 }); tr.removeFromLeft(8);
    drawTransportButton(g, tr.removeFromLeft(54), "READY", red.darker(0.2f));

    b.removeFromLeft(18);
    auto time = b.removeFromLeft(170);
    drawPanel(g, time, "TIME / POSITION");
    g.setColour(text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(currentTimeText(), time.reduced(14, 46), juce::Justification::centred);

    b.removeFromLeft(18);
    auto project = b.removeFromLeft(230);
    drawPanel(g, project, "PROJECT");
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(projectName, project.reduced(14, 45), juce::Justification::centred);
    g.drawText(juce::String(deviceSampleRate / 1000.0, 1) + " kHz  |  24 Bit  |  15 IPS", project.reduced(14, 70), juce::Justification::centred);

    b.removeFromLeft(18);
    auto render = b.removeFromLeft(200);
    drawPanel(g, render, "RENDER / EXPORT");
    g.setColour(text);
    g.setFont(juce::FontOptions(11.0f));
    const juce::String renderRate = exportSampleRate.getSelectedId() == 1 ? "44.1 kHz" : "48 kHz";
    const juce::String renderDepth = exportBitDepth.getSelectedId() == 1 ? "16 Bit" : "24 Bit";
    g.drawText("FORMAT   WAV", render.reduced(14, 28).removeFromTop(18), juce::Justification::centredLeft);
    g.drawText("RATE     " + renderRate, render.reduced(14, 46).removeFromTop(18), juce::Justification::centredLeft);
    g.drawText("DEPTH    " + renderDepth, render.reduced(14, 64).removeFromTop(18), juce::Justification::centredLeft);
    const juce::String rangeText = renderLengthSeconds.getValue() <= 0.0 ? "FULL" : juce::String(renderStartSeconds.getValue(), 1) + "s + " + juce::String(renderLengthSeconds.getValue(), 1) + "s";
    g.drawText("RANGE    " + rangeText, render.reduced(14, 82).removeFromTop(18), juce::Justification::centredLeft);
    drawTransportButton(g, render.withSizeKeepingCentre(120, 28).translated(0, 42), "RENDER", juce::Colour { 0xff5b5b5b });

    b.removeFromLeft(18);
    drawPanel(g, b, "OUTPUT METERS");
    int lit = 0;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        lit = peakToSegments(std::max(audioEngine.session().master.meterLeftPeak, audioEngine.session().master.meterRightPeak));
    }
    drawLedMeter(g, b.reduced(35, 34), lit, true);

    auto footer = bounds.removeFromTop(76);
    drawPanel(g, footer, {});
    juce::String status;
    {
        const juce::ScopedLock lock(statusLock);
        status = statusText;
    }
    g.setColour(text.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(status, footer.reduced(18), juce::Justification::centredLeft);
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
        masterMeter->setBounds(m.removeFromTop(112));
    m.removeFromTop(6);
    outputTrimLabel.setBounds(m.removeFromTop(16));
    outputTrim.setBounds(m.removeFromTop(60));
    m.removeFromTop(4);
    exportRateLabel.setBounds(m.removeFromTop(14));
    exportSampleRate.setBounds(m.removeFromTop(24));
    m.removeFromTop(4);
    exportDepthLabel.setBounds(m.removeFromTop(14));
    exportBitDepth.setBounds(m.removeFromTop(24));
    m.removeFromTop(4);
    renderStartLabel.setBounds(m.removeFromTop(12));
    renderStartSeconds.setBounds(m.removeFromTop(22));
    m.removeFromTop(3);
    renderLengthLabel.setBounds(m.removeFromTop(12));
    renderLengthSeconds.setBounds(m.removeFromTop(22));
    m.removeFromTop(6);
    limiterToggle.setBounds(m.removeFromTop(22));
    limiterActivityLabel.setBounds(m.removeFromTop(42));

    auto deckArea = juce::Rectangle<int>(getWidth() / 2 - 180, 92, 410, 28);
    loadButton.setBounds(deckArea.removeFromLeft(66));
    deckArea.removeFromLeft(6);
    rewButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    stopButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    playButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    exportButton.setBounds(deckArea.removeFromLeft(88));
}
