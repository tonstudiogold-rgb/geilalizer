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
constexpr int kContentWidth = 2020;
constexpr int kContentHeight = 1460;
constexpr int kDeckHeight = 205;
constexpr int kChannelAreaHeight = 1098;
constexpr int kFooterHeight = 76;
constexpr int kStripWidth = 140;
constexpr int kStripHeight = 478;
constexpr int kStripGap = 7;
constexpr int kBankHeaderHeight = 28;
constexpr int kBankRowGap = 8;
constexpr int kBankCount = 2;
constexpr int kChannelsPerBank = 12;

const juce::Colour background { 0xff11100e };
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

float dbToMeterPeak(float db)
{
    return std::pow(10.0f, juce::jlimit(-60.0f, 0.0f, db) / 20.0f);
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

void drawDbScale(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(muted.withAlpha(0.72f));
    g.setFont(juce::FontOptions(7.5f));
    for (const float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float norm = sanitisePeak(dbToMeterPeak(db));
        const int y = area.getBottom() - static_cast<int>(std::round(norm * static_cast<float>(area.getHeight())));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getX() + 4));
        g.drawText(juce::String(static_cast<int>(db)), area.getX() + 6, y - 5, area.getWidth() - 6, 10, juce::Justification::centredLeft);
    }
}

void drawLedMeter(juce::Graphics& g, juce::Rectangle<int> area, int lit = 9, bool stereo = false, int holdLit = -1, bool showScale = false)
{
    const int columns = stereo ? 2 : 1;
    const int gap = stereo ? 5 : 0;
    auto meterArea = showScale ? area.withTrimmedRight(26) : area;
    const int colWidth = (meterArea.getWidth() - gap) / columns;
    int holdY = -1;
    for (int c = 0; c < columns; ++c)
    {
        auto col = meterArea.withX(meterArea.getX() + c * (colWidth + gap)).withWidth(colWidth);
        const int segments = 14;
        const int segGap = 2;
        const int segH = juce::jmax(2, (col.getHeight() - (segments - 1) * segGap) / segments);
        for (int i = 0; i < segments; ++i)
        {
            const int y = col.getBottom() - (i + 1) * segH - i * segGap;
            if (i == juce::jlimit(0, segments - 1, holdLit - 1))
                holdY = y;
            auto colour = i > 11 ? juce::Colours::red : (i > 8 ? juce::Colours::yellow : juce::Colours::limegreen);
            g.setColour(colour.withAlpha(i < lit ? 0.9f : 0.18f));
            g.fillRoundedRectangle((float) col.getX(), (float) y, (float) col.getWidth(), (float) segH, 1.5f);
        }
    }
    if (holdLit > 0 && holdY >= 0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.fillRect(meterArea.getX(), holdY - 1, meterArea.getWidth(), 2);
    }
    if (showScale)
        drawDbScale(g, area.removeFromRight(24));
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
        const int hold = stereo ? peakToSegments(std::max(left.holdPeak(), right.holdPeak())) : peakToSegments(left.holdPeak());
        drawLedMeter(g, area.removeFromTop(area.getHeight() - 30), lit, stereo, hold, true);
        g.setColour(text.withAlpha(0.8f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(stereo ? "L        R" : label, area.removeFromTop(14), juce::Justification::centred);
        if (stereo)
        {
            g.setFont(juce::FontOptions(8.0f));
            g.drawText("dB SCALE", area, juce::Justification::centred);
        }
        if (left.clipped() || right.clipped())
        {
            g.setColour(red);
            g.drawText("CLIP", getLocalBounds().reduced(8).removeFromTop(15), juce::Justification::centredRight);
        }
        left.decay();
        right.decay();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        left.resetClip();
        right.resetClip();
        repaint();
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

        inputLabel.setText("INPUT", juce::dontSendNotification);
        inputLabel.setJustificationType(juce::Justification::centred);
        inputLabel.setColour(juce::Label::textColourId, text);
        addAndMakeVisible(inputLabel);

        inputSelect.addItem("--", 1);
        for (int i = 1; i <= channelCount; ++i)
            inputSelect.addItem("IN " + juce::String(i), i + 1);
        inputSelect.setSelectedId(1, juce::dontSendNotification);
        inputSelect.setColour(juce::ComboBox::textColourId, text);
        inputSelect.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
        inputSelect.onChange = [this]
        {
            const int selected = inputSelect.getSelectedId() - 2;
            geilalizer::core::TapeMachineSnapshot snap;
            {
                const std::lock_guard<std::mutex> lock(owner.trackMutex);
                snap = owner.tapeMachine.setInput(index - 1, selected);
                auto& state = owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1));
                state.inputAssigned = selected >= 0;
                state.inputChannelIndex = selected;
                if (! state.inputAssigned)
                    state.armed = false;
                owner.syncRealtimeStateFromSessionLocked();
            }
            owner.applyTapeSnapshot(snap);
            owner.refreshChannelVisuals();
        };
        addAndMakeVisible(inputSelect);

        armButton.setClickingTogglesState(false);
        armButton.setButtonText("ARM");
        armButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff540b0b });
        armButton.setColour(juce::TextButton::buttonOnColourId, red);
        armButton.setColour(juce::TextButton::textColourOffId, text);
        armButton.onClick = [this]
        {
            geilalizer::core::TapeMachineSnapshot snap;
            {
                const std::lock_guard<std::mutex> lock(owner.trackMutex);
                snap = owner.tapeMachine.toggleArm(index - 1);
                owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).armed = snap.tracks[static_cast<std::size_t>(index - 1)].armed;
                owner.syncRealtimeStateFromSessionLocked();
            }
            owner.applyTapeSnapshot(snap);
            owner.refreshChannelVisuals();
        };
        addAndMakeVisible(armButton);

        inputGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        inputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 18);
        inputGain.setRange(geilalizer::core::kInputGainMinDb, geilalizer::core::kInputGainMaxDb, 0.1);
        inputGain.setValue(geilalizer::core::kDefaultInputGainDb, juce::dontSendNotification);
        inputGain.setTextValueSuffix(" dB");
        inputGain.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setInputGainDb(static_cast<float>(inputGain.getValue()));
            owner.syncRealtimeStateFromSessionLocked();
        };
        addAndMakeVisible(inputGain);

        fader.setSliderStyle(juce::Slider::LinearVertical);
        fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
        fader.setRange(geilalizer::core::kFaderGainMinDb, geilalizer::core::kFaderGainMaxDb, 0.1);
        fader.setValue(geilalizer::core::kDefaultFaderGainDb, juce::dontSendNotification);
        fader.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setFaderGainDb(static_cast<float>(fader.getValue()));
            owner.syncRealtimeStateFromSessionLocked();
        };
        addAndMakeVisible(fader);

        pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange(-1.0, 1.0, 0.01);
        pan.setValue(0.0, juce::dontSendNotification);
        pan.onValueChange = [this]
        {
            const std::lock_guard<std::mutex> lock(owner.trackMutex);
            if (std::abs(pan.getValue()) > 0.0001)
                pan.setValue(0.0, juce::dontSendNotification);
            owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1)).setPan(0.0f);
            owner.syncRealtimeStateFromSessionLocked();
        };
        addAndMakeVisible(pan);

        muteButton.setButtonText("MUTE");
        muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff333333 });
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour { 0xff666666 });
        muteButton.setColour(juce::TextButton::textColourOffId, muted);
        muteButton.onClick = [this]
        {
            geilalizer::core::TapeMachineSnapshot snap;
            {
                const std::lock_guard<std::mutex> lock(owner.trackMutex);
                snap = owner.tapeMachine.toggleMute(index - 1);
                auto& state = owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1));
                state.muted = snap.tracks[static_cast<std::size_t>(index - 1)].muted;
                owner.syncRealtimeStateFromSessionLocked();
            }
            owner.applyTapeSnapshot(snap);
            owner.refreshChannelVisuals();
        };
        addAndMakeVisible(muteButton);

        soloButton.setButtonText("SOLO");
        soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff3b3112 });
        soloButton.setColour(juce::TextButton::buttonOnColourId, accent);
        soloButton.setColour(juce::TextButton::textColourOffId, muted);
        soloButton.onClick = [this]
        {
            geilalizer::core::TapeMachineSnapshot snap;
            {
                const std::lock_guard<std::mutex> lock(owner.trackMutex);
                snap = owner.tapeMachine.toggleSolo(index - 1);
                auto& state = owner.audioEngine.session().channel(static_cast<std::size_t>(index - 1));
                state.solo = snap.tracks[static_cast<std::size_t>(index - 1)].solo;
                owner.syncRealtimeStateFromSessionLocked();
            }
            owner.applyTapeSnapshot(snap);
            owner.refreshChannelVisuals();
        };
        addAndMakeVisible(soloButton);
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

    void setInputSelection(int selectedZeroBasedInput)
    {
        inputSelect.setSelectedId(selectedZeroBasedInput >= 0 ? selectedZeroBasedInput + 2 : 1, juce::dontSendNotification);
    }

    void setMuteSoloState(bool muted, bool solo)
    {
        muteButton.setToggleState(muted, juce::dontSendNotification);
        soloButton.setToggleState(solo, juce::dontSendNotification);
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

        auto meterArea = juce::Rectangle<int>(86, 260, 30, 145);
        drawLedMeter(g, meterArea, peakToSegments(meter.displayPeak()), false, peakToSegments(meter.holdPeak()), false);

        g.setColour(muted);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("PRE", 10, 75, 70, 13, juce::Justification::centred);
        g.drawText("PEAK", 80, 244, 42, 14, juce::Justification::centred);
        g.setColour(loaded ? green : red.withAlpha(0.45f));
        g.fillEllipse(78.0f, 264.0f, 6.0f, 6.0f);
        g.setColour(meter.clipped() ? red : muted);
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText(meter.clipped() ? "CLIP" : juce::String(meter.displayDb(), 0) + " dB",
                   76, 278, 52, 13, juce::Justification::centred);
        g.setColour(muted);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("PAN", 14, 159, 62, 13, juce::Justification::centred);
        g.drawText("CENTER", 14, 214, 62, 12, juce::Justification::centred);
        g.drawText("FADER", 14, 244, 54, 13, juce::Justification::centred);
        meter.decay();

        if (dragging)
        {
            g.setColour(accent.withAlpha(0.22f));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 5.0f);
            g.setColour(accent);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 5.0f, 2.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (juce::Rectangle<int>(7, 30, 42, 100).contains(event.getPosition()))
        {
            meter.resetClip();
            repaint();
        }
    }

    void resized() override
    {
        const auto w = getWidth();
        nameEditor.setBounds(8, 7, w - 16, 20);
        inputLabel.setBounds(8, 31, w - 16, 12);
        inputSelect.setBounds(8, 45, w - 16, 24);

        inputGain.setBounds(10, 88, 70, 68);
        pan.setBounds(20, 172, 50, 50);
        fader.setBounds(14, 260, 54, 155);

        armButton.setBounds(8, 425, w - 16, 22);
        auto buttons = juce::Rectangle<int>(8, 450, w - 16, 20);
        muteButton.setBounds(buttons.removeFromLeft((buttons.getWidth() - 6) / 2));
        buttons.removeFromLeft(6);
        soloButton.setBounds(buttons);
    }

private:
    int index = 0;
    MainComponent& owner;
    bool dragging = false;
    bool loaded = false;
    geilalizer::core::LevelMeter meter;
    juce::Label nameEditor;
    juce::Label inputLabel;
    juce::ComboBox inputSelect;
    juce::Slider inputGain;
    juce::Slider fader;
    juce::Slider pan;
    juce::TextButton armButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
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
    setSize(kContentWidth, kContentHeight);
    formatManager.registerBasicFormats();
    audioBlockInputs.resize(channelCount);
    for (std::size_t ch = 0; ch < realtimeTrackBuffers.size(); ++ch)
        syncRealtimeTrackBuffer(ch);
    syncRealtimeStateFromSessionLocked();
    audioEngine.loadDefaultAssetsBeforeAudioStart();

    titleLabel.setText("24-TRACK STANDALONE GEILALIZER - TAPE MACHINE", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(titleLabel);

    loadButton.setButtonText("LOAD");
    rewButton.setButtonText("REW");
    stopButton.setButtonText("STOP");
    playButton.setButtonText("PLAY");
    recButton.setButtonText("REC");
    undoButton.setButtonText("UNDO");
    exportButton.setButtonText("RENDER");
    recButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff540b0b });
    recButton.setColour(juce::TextButton::buttonOnColourId, red);
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff116b12 });
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff5b5b5b });
    for (auto* button : { &loadButton, &rewButton, &playButton, &stopButton, &recButton, &undoButton, &exportButton })
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
    recButton.onClick = [this] { toggleRecord(); };
    undoButton.onClick = [this] { undoLastRecording(); };
    exportButton.onClick = [this] { showExportDialog(); };

    channelViewport.setViewedComponent(&channelContainer, false);
    channelViewport.setScrollBarsShown(true, true);
    channelViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(channelViewport);

    for (int bank = 0; bank < kBankCount; ++bank)
    {
        auto& toggle = bankToggles[static_cast<std::size_t>(bank)];
        toggle.setButtonText("BANK " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + bank))
                             + " | CH " + juce::String(bank * kChannelsPerBank + 1) + "-" + juce::String(bank * kChannelsPerBank + kChannelsPerBank));
        toggle.setToggleState(true, juce::dontSendNotification);
        toggle.setColour(juce::ToggleButton::textColourId, text);
        toggle.onClick = [this, bank]
        {
            bankOpen[static_cast<std::size_t>(bank)] = bankToggles[static_cast<std::size_t>(bank)].getToggleState();
            resized();
            repaint();
        };
        channelContainer.addAndMakeVisible(toggle);
    }

    for (int i = 1; i <= channelCount; ++i)
    {
        auto* strip = channels.add(new ChannelStrip(i, *this));
        channelContainer.addAndMakeVisible(strip);
    }

    masterGroup.setVisible(false);

    limiterToggle.setButtonText("LIMITER ON");
    limiterToggle.setToggleState(true, juce::dontSendNotification);
    limiterToggle.setColour(juce::ToggleButton::textColourId, text);
    limiterToggle.onClick = [this]
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        audioEngine.session().master.limiterEnabled = limiterToggle.getToggleState();
        syncRealtimeStateFromSessionLocked();
    };
    addAndMakeVisible(limiterToggle);

    outputTrimLabel.setText("OUTPUT", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centred);
    outputTrimLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    outputTrimLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(outputTrimLabel);
    outputTrim.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputTrim.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    outputTrim.setRange(-12.0, 12.0, 0.1);
    outputTrim.setValue(0.0, juce::dontSendNotification);
    outputTrim.setTextValueSuffix(" dB");
    outputTrim.onValueChange = [this]
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        audioEngine.session().master.outputTrimDb = static_cast<float>(outputTrim.getValue());
        syncRealtimeStateFromSessionLocked();
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
    renderStartSeconds.onValueChange = [this] { repaint(); };
    addAndMakeVisible(renderStartSeconds);

    renderFullSongToggle.setToggleState(true, juce::dontSendNotification);
    renderFullSongToggle.setColour(juce::ToggleButton::textColourId, text);
    renderFullSongToggle.onClick = [this]
    {
        const bool full = renderFullSongToggle.getToggleState();
        renderStartSeconds.setEnabled(! full);
        renderEndSeconds.setEnabled(! full);
        repaint();
    };
    addAndMakeVisible(renderFullSongToggle);

    renderEndLabel.setText("END", juce::dontSendNotification);
    renderEndLabel.setJustificationType(juce::Justification::centred);
    renderEndLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(renderEndLabel);
    renderEndSeconds.setSliderStyle(juce::Slider::LinearHorizontal);
    renderEndSeconds.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
    renderEndSeconds.setRange(0.0, 600.0, 0.1);
    renderEndSeconds.setTextValueSuffix(" s");
    renderEndSeconds.setValue(0.0, juce::dontSendNotification);
    renderEndSeconds.onValueChange = [this] { repaint(); };
    renderEndSeconds.setEnabled(false);
    renderStartSeconds.setEnabled(false);
    addAndMakeVisible(renderEndSeconds);

    masterMeter = std::make_unique<MeterPlaceholder>("L / R");
    addAndMakeVisible(*masterMeter);

    limiterActivityLabel.setText({}, juce::dontSendNotification);
    limiterActivityLabel.setVisible(false);
    addAndMakeVisible(limiterActivityLabel);

    auto& lf = getLookAndFeel();
    lf.setColour(juce::Slider::thumbColourId, juce::Colour { 0xffd9d9d9 });
    lf.setColour(juce::Slider::rotarySliderFillColourId, red);
    lf.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour { 0xff454545 });
    lf.setColour(juce::TextButton::buttonColourId, juce::Colour { 0xff303030 });
    lf.setColour(juce::TextButton::textColourOffId, text);
    lf.setColour(juce::Label::textColourId, text);

    startTimerHz(30);
    resized();
    setAudioChannels(channelCount, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::syncRealtimeStateFromSessionLocked()
{
    const auto& session = audioEngine.session();
    for (std::size_t i = 0; i < realtimeChannels.size(); ++i)
    {
        const auto& src = session.channel(i);
        auto& dst = realtimeChannels[i];
        dst.inputGainDb.store(src.inputGainDb, std::memory_order_release);
        dst.faderGainDb.store(src.faderGainDb, std::memory_order_release);
        dst.pan.store(src.pan, std::memory_order_release);
        dst.muted.store(src.muted, std::memory_order_release);
        dst.solo.store(src.solo, std::memory_order_release);
        dst.armed.store(src.armed, std::memory_order_release);
        dst.inputAssigned.store(src.inputAssigned, std::memory_order_release);
        dst.inputChannelIndex.store(src.inputChannelIndex, std::memory_order_release);
        dst.hasAudioFile.store(src.hasAudioFile, std::memory_order_release);
        dst.lengthFrames.store(src.lengthFrames, std::memory_order_release);
    }
    realtimeMaster.limiterEnabled.store(session.master.limiterEnabled, std::memory_order_release);
    realtimeMaster.outputTrimDb.store(session.master.outputTrimDb, std::memory_order_release);
}

void MainComponent::syncRealtimeTrackBuffer(std::size_t channelIndex)
{
    if (channelIndex >= trackBuffers.size())
        return;
    std::atomic_store_explicit(&realtimeTrackBuffers[channelIndex],
                               std::make_shared<std::vector<float>>(trackBuffers[channelIndex]),
                               std::memory_order_release);
}

void MainComponent::preallocateRecordBuffersForArmedTracks(std::int64_t startFrame)
{
    constexpr double kRecordPreallocateSeconds = 300.0;
    const auto baseFrame = static_cast<std::size_t>(std::max<std::int64_t>(0, startFrame));
    const auto reserveFrames = baseFrame + static_cast<std::size_t>(std::max(1.0, deviceSampleRate) * kRecordPreallocateSeconds);
    for (std::size_t i = 0; i < trackBuffers.size(); ++i)
    {
        const auto& state = audioEngine.session().channel(i);
        if (! state.armed || ! state.inputAssigned)
            continue;
        if (trackBuffers[i].size() < reserveFrames)
            trackBuffers[i].resize(reserveFrames, 0.0f);
        auto& mutableState = audioEngine.session().channel(i);
        mutableState.setLoadedAudioFile("live-input-" + std::to_string(mutableState.inputChannelIndex + 1),
                                        mutableState.inputChannelIndex,
                                        trackBuffers[i].size());
        syncRealtimeTrackBuffer(i);
    }
    syncRealtimeStateFromSessionLocked();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    const std::lock_guard<std::mutex> lock(trackMutex);
    deviceSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    deviceBlockSize = samplesPerBlockExpected > 0 ? samplesPerBlockExpected : 512;
    audioEngine.prepare(deviceSampleRate, deviceBlockSize);
    audioBlockInputs.resize(channelCount);
    for (auto& channel : audioBlockInputs)
        channel.resize(static_cast<std::size_t>(deviceBlockSize), 0.0f);
    for (auto& channel : audioInputScratch)
        channel.resize(static_cast<std::size_t>(deviceBlockSize), 0.0f);
    audioBlockInterleaved.resize(static_cast<std::size_t>(deviceBlockSize) * 2, 0.0f);
    syncRealtimeStateFromSessionLocked();
    for (std::size_t ch = 0; ch < realtimeTrackBuffers.size(); ++ch)
        syncRealtimeTrackBuffer(ch);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr)
        return;

    const int numSamples = info.numSamples;
    if (numSamples <= 0
        || static_cast<std::size_t>(numSamples) > audioBlockInterleaved.size() / 2
        || audioBlockInputs.size() != channelCount)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const int availableInputChannels = std::min(channelCount, info.buffer->getNumChannels());
    for (auto& channel : audioInputScratch)
        std::fill_n(channel.begin(), static_cast<std::size_t>(numSamples), 0.0f);
    for (int ch = 0; ch < availableInputChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            audioInputScratch[static_cast<std::size_t>(ch)][static_cast<std::size_t>(i)] = info.buffer->getSample(ch, info.startSample + i);

    info.clearActiveBufferRegion();
    if (! playing.load())
        return;

    const auto startFrame = playheadFrame.load();
    bool anySolo = false;
    for (const auto& state : realtimeChannels)
        anySolo = anySolo || state.solo.load(std::memory_order_acquire);

    for (std::size_t ch = 0; ch < realtimeChannels.size(); ++ch)
    {
        const auto& state = realtimeChannels[ch];
        auto& dstState = realtimeSession.channel(ch);
        dstState.inputGainDb = state.inputGainDb.load(std::memory_order_acquire);
        dstState.faderGainDb = state.faderGainDb.load(std::memory_order_acquire);
        dstState.pan = state.pan.load(std::memory_order_acquire);
        dstState.muted = state.muted.load(std::memory_order_acquire);
        dstState.solo = state.solo.load(std::memory_order_acquire);
        dstState.armed = state.armed.load(std::memory_order_acquire);
        dstState.inputAssigned = state.inputAssigned.load(std::memory_order_acquire);
        dstState.inputChannelIndex = state.inputChannelIndex.load(std::memory_order_acquire);
        dstState.hasAudioFile = state.hasAudioFile.load(std::memory_order_acquire);
        dstState.lengthFrames = state.lengthFrames.load(std::memory_order_acquire);

        auto& inputs = audioBlockInputs[ch];
        std::fill_n(inputs.begin(), static_cast<std::size_t>(numSamples), 0.0f);

        const bool shouldRecord = recording.load(std::memory_order_acquire)
            && dstState.armed && dstState.inputAssigned;
        const int inputIndex = dstState.inputChannelIndex;
        const auto source = std::atomic_load_explicit(&realtimeTrackBuffers[ch], std::memory_order_acquire);
        if (shouldRecord && inputIndex >= 0 && inputIndex < availableInputChannels && source != nullptr)
        {
            const auto writeEnd = static_cast<std::size_t>(std::max<std::int64_t>(0, startFrame + numSamples));
            if (source->size() >= writeEnd)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto frame = static_cast<std::size_t>(std::max<std::int64_t>(0, startFrame + i));
                    const auto sample = audioInputScratch[static_cast<std::size_t>(inputIndex)][static_cast<std::size_t>(i)];
                    (*source)[frame] = sample;
                    inputs[static_cast<std::size_t>(i)] = sample;
                }
                continue;
            }
            recordingBufferOverrun.store(true, std::memory_order_release);
        }

        if ((anySolo && ! dstState.solo) || (! anySolo && dstState.muted) || source == nullptr)
            continue;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto frame = static_cast<std::size_t>(std::max<std::int64_t>(0, startFrame + i));
            if (frame < source->size())
                inputs[static_cast<std::size_t>(i)] = (*source)[frame];
        }
    }

    realtimeSession.master.limiterEnabled = realtimeMaster.limiterEnabled.load(std::memory_order_acquire);
    realtimeSession.master.outputTrimDb = realtimeMaster.outputTrimDb.load(std::memory_order_acquire);
    std::fill_n(audioBlockInterleaved.begin(), static_cast<std::size_t>(numSamples) * 2, 0.0f);
    audioEngine.processRealtime(realtimeSession, audioBlockInputs, static_cast<std::size_t>(numSamples), audioBlockInterleaved, realtimeMeters);

    const int channelsToWrite = std::min(2, info.buffer->getNumChannels());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float left = audioBlockInterleaved[static_cast<std::size_t>(sample) * 2];
        const float right = audioBlockInterleaved[static_cast<std::size_t>(sample) * 2 + 1];
        if (channelsToWrite > 0) info.buffer->setSample(0, info.startSample + sample, left);
        if (channelsToWrite > 1) info.buffer->setSample(1, info.startSample + sample, right);
    }

    for (std::size_t ch = 0; ch < realtimeChannels.size(); ++ch)
        realtimeChannels[ch].meterPeak.store(realtimeMeters.channelPeaks[ch], std::memory_order_release);
    realtimeMaster.meterLeftPeak.store(realtimeMeters.masterLeftPeak, std::memory_order_release);
    realtimeMaster.meterRightPeak.store(realtimeMeters.masterRightPeak, std::memory_order_release);
    realtimeMaster.limiterActive.store(realtimeMeters.limiterActive, std::memory_order_release);
    playheadFrame.store(startFrame + numSamples, std::memory_order_release);
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
            state.inputAssigned = false;
            state.inputChannelIndex = -1;
            state.armed = false;
            syncRealtimeTrackBuffer(firstChannelIndex + static_cast<std::size_t>(ch));
        }
        syncRealtimeStateFromSessionLocked();
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
        syncRealtimeTrackBuffer(channelIndex);
        syncRealtimeStateFromSessionLocked();
    }

    setStatus("Track " + juce::String(static_cast<int>(channelIndex + 1)) + " cleared.");
    refreshChannelVisuals();
    repaint();
}

void MainComponent::play()
{
    const auto loadedFrames = static_cast<std::int64_t>(maxLoadedFrames());
    geilalizer::core::TapeMachineSnapshot snap;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        tapeMachine.setPlayheadFrame(playheadFrame.load(std::memory_order_acquire));
        snap = tapeMachine.play(loadedFrames);
        if (snap.transport == geilalizer::core::TransportState::Recording)
            preallocateRecordBuffersForArmedTracks(snap.recordStartFrame);
    }
    applyTapeSnapshot(snap);
}

void MainComponent::stop()
{
    geilalizer::core::TapeMachineSnapshot snap;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        tapeMachine.setPlayheadFrame(playheadFrame.load(std::memory_order_acquire));
        snap = tapeMachine.stop();
        for (std::size_t i = 0; i < trackBuffers.size(); ++i)
        {
            const auto recorded = std::atomic_load_explicit(&realtimeTrackBuffers[i], std::memory_order_acquire);
            if (recorded != nullptr)
            {
                trackBuffers[i] = *recorded;
                audioEngine.session().channel(i).lengthFrames = trackBuffers[i].size();
            }
        }
        syncRealtimeStateFromSessionLocked();
    }
    if (snap.lastRecording.has_value())
        pendingUndoSpan = snap.lastRecording;
    applyTapeSnapshot(snap);
}

void MainComponent::rewind()
{
    playing.store(false);
    recording.store(false);
    playheadFrame.store(0);
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        tapeMachine.setPlayheadFrame(playheadFrame.load(std::memory_order_acquire));
        audioEngine.reset();
        tapeMachine.stop();
        tapeMachine.stop();
        syncRealtimeStateFromSessionLocked();
    }
    setStatus(juce::String::fromUTF8("REW - Position auf Start zur\303\274ckgesetzt."));
}

void MainComponent::toggleRecord()
{
    geilalizer::core::TapeMachineSnapshot snap;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        if (! recording.load())
            undoTrackBuffers = trackBuffers;
        tapeMachine.setPlayheadFrame(playheadFrame.load(std::memory_order_acquire));
        snap = tapeMachine.toggleRecord();
        if (snap.transport == geilalizer::core::TransportState::Recording)
            preallocateRecordBuffersForArmedTracks(snap.recordStartFrame);
        else
            syncRealtimeStateFromSessionLocked();
    }
    if (snap.lastRecording.has_value())
        pendingUndoSpan = snap.lastRecording;
    applyTapeSnapshot(snap);
}

void MainComponent::undoLastRecording()
{
    if (pendingUndoSpan.has_value())
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        trackBuffers = undoTrackBuffers;
        for (int i = 0; i < channelCount; ++i)
        {
            audioEngine.session().channel(static_cast<std::size_t>(i)).lengthFrames = trackBuffers[static_cast<std::size_t>(i)].size();
            syncRealtimeTrackBuffer(static_cast<std::size_t>(i));
        }
        syncRealtimeStateFromSessionLocked();
    }
    geilalizer::core::TapeMachineSnapshot snap;
    {
        const std::lock_guard<std::mutex> lock(trackMutex);
        snap = tapeMachine.undoLastRecording();
    }
    pendingUndoSpan.reset();
    applyTapeSnapshot(snap);
    refreshChannelVisuals();
}

void MainComponent::applyTapeSnapshot(const geilalizer::core::TapeMachineSnapshot& snapshot)
{
    playing.store(snapshot.transport == geilalizer::core::TransportState::Playing
                  || snapshot.transport == geilalizer::core::TransportState::Recording);
    recording.store(snapshot.transport == geilalizer::core::TransportState::Recording);
    playheadFrame.store(snapshot.playheadFrame);
    recButton.setToggleState(snapshot.recordEnabled || snapshot.transport == geilalizer::core::TransportState::Recording, juce::dontSendNotification);
    playButton.setToggleState(snapshot.transport == geilalizer::core::TransportState::Playing
                              || snapshot.transport == geilalizer::core::TransportState::Recording, juce::dontSendNotification);
    undoButton.setEnabled(snapshot.canUndo || pendingUndoSpan.has_value());
    setStatus(juce::String::fromUTF8(snapshot.message.c_str()));
    repaint();
}

void MainComponent::overwriteRecordedRange(const geilalizer::core::TapeRecordingSpan& span)
{
    const auto start = static_cast<std::size_t>(std::max<std::int64_t>(0, span.startFrame));
    const auto end = static_cast<std::size_t>(std::max<std::int64_t>(span.startFrame, span.endFrame));
    if (end <= start)
        return;
    for (const int trackIndex : span.tracks)
    {
        if (trackIndex < 0 || trackIndex >= channelCount)
            continue;
        auto& dest = trackBuffers[static_cast<std::size_t>(trackIndex)];
        if (dest.size() < end)
            dest.resize(end, 0.0f);
        auto& state = audioEngine.session().channel(static_cast<std::size_t>(trackIndex));
        state.setLoadedAudioFile("live-input-" + std::to_string(state.inputChannelIndex + 1), state.inputChannelIndex, dest.size());
        state.inputAssigned = true;
        state.armed = true;
    }
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

    const auto loadedFrames = maxLoadedFrames();
    const double totalSeconds = deviceSampleRate > 0.0 ? static_cast<double>(loadedFrames) / deviceSampleRate : 0.0;
    renderStartSeconds.setRange(0.0, totalSeconds, 0.1);
    renderEndSeconds.setRange(0.0, totalSeconds, 0.1);
    if (renderEndSeconds.getValue() <= renderStartSeconds.getValue() || renderFullSongToggle.getToggleState())
        renderEndSeconds.setValue(totalSeconds, juce::dontSendNotification);

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
            const bool fullLength = renderFullSongToggle.getToggleState();
            const auto totalFrames = maxLoadedFrames();
            const auto startFrame = static_cast<std::size_t>(std::max(0.0, renderStartSeconds.getValue()) * deviceSampleRate);
            const auto endFrame = static_cast<std::size_t>(std::max(0.0, renderEndSeconds.getValue()) * deviceSampleRate);
            const auto clampedStart = std::min(startFrame, totalFrames);
            const auto clampedEnd = std::min(std::max(endFrame, clampedStart), totalFrames);
            const auto requestedFrames = fullLength ? 0 : clampedEnd - clampedStart;
            if (! fullLength && (clampedStart != startFrame || clampedEnd != endFrame || requestedFrames == 0))
                setStatus("Render range clamped to loaded song length.");
            renderToFileAsync(out, selectedRate, selectedDepth, clampedStart, requestedFrames, fullLength);
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

    setStatus("Rendering...");
    exportButton.setEnabled(false);
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    std::thread([safeThis, outputFile, sampleRate, bitDepth, startFrame, numFrames, fullLength, buffers, sessionCopy]() mutable
    {
        juce::String message;
        bool ok = true;
        outputFile.deleteFile();
        auto fileStream = outputFile.createOutputStream();
        if (fileStream == nullptr || ! fileStream->openedOk())
        {
            ok = false;
            message = "Output file cannot be opened.";
        }

        std::unique_ptr<juce::OutputStream> stream;
        if (ok)
            stream = std::move(fileStream);

        std::unique_ptr<juce::AudioFormatWriter> writer;
        if (ok)
        {
            juce::WavAudioFormat wav;
            const auto options = juce::AudioFormatWriterOptions()
                .withSampleRate(sampleRate)
                .withNumChannels(2)
                .withBitsPerSample(bitDepth);
            writer = wav.createWriterFor(stream, options);
            if (writer == nullptr)
            {
                ok = false;
                message = "WAV writer could not be created.";
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
    for (int i = 0; i < channels.size(); ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        auto& state = realtimeChannels[index];
        channels[i]->setLoaded(state.hasAudioFile.load(std::memory_order_acquire));
        channels[i]->setArmState(state.armed.load(std::memory_order_acquire));
        channels[i]->setInputGainValue(state.inputGainDb.load(std::memory_order_acquire));
        channels[i]->setFaderValue(state.faderGainDb.load(std::memory_order_acquire));
        channels[i]->setPanValue(state.pan.load(std::memory_order_acquire));
        const bool inputAssigned = state.inputAssigned.load(std::memory_order_acquire);
        channels[i]->setInputSelection(inputAssigned ? state.inputChannelIndex.load(std::memory_order_acquire) : -1);
        channels[i]->setMuteSoloState(state.muted.load(std::memory_order_acquire),
                                      state.solo.load(std::memory_order_acquire));
        {
            const std::lock_guard<std::mutex> lock(trackMutex);
            channels[i]->setNameText(juce::String(audioEngine.session().channel(index).name));
        }
        channels[i]->setMeterPeak(state.meterPeak.load(std::memory_order_acquire));
    }
}

void MainComponent::refreshMasterVisuals()
{
    if (masterMeter != nullptr)
        masterMeter->setLevels(realtimeMaster.meterLeftPeak.load(std::memory_order_acquire),
                               realtimeMaster.meterRightPeak.load(std::memory_order_acquire));
    limiterToggle.setToggleState(realtimeMaster.limiterEnabled.load(std::memory_order_acquire), juce::dontSendNotification);
    outputTrim.setValue(realtimeMaster.outputTrimDb.load(std::memory_order_acquire), juce::dontSendNotification);
    (void) realtimeMaster.limiterActive.load(std::memory_order_acquire);
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

    auto deck = bounds.removeFromTop(kDeckHeight);
    drawPanel(g, deck, "OVERVIEW");
    auto deckInner = deck.reduced(18, 26);
    drawReel(g, deckInner.removeFromLeft(315).reduced(6, 2), playing.load() ? 0.95f : 0.35f);
    drawReel(g, deckInner.removeFromRight(315).reduced(6, 2), playing.load() ? -0.85f : -0.22f);

    auto transportPanel = deck.withSizeKeepingCentre(470, 104).translated(0, -12);
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

    auto channelArea = bounds.removeFromTop(kChannelAreaHeight);
    drawPanel(g, channelArea, "24-TRACK CHANNELS");

    auto footer = bounds.removeFromTop(kFooterHeight);
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

    if (channels.size() < channelCount)
        return;

    bounds.removeFromTop(kDeckHeight);
    auto channelPanel = bounds.removeFromTop(kChannelAreaHeight).reduced(16, 30);
    auto masterArea = channelPanel.removeFromRight(150);
    channelPanel.removeFromRight(12);

    channelViewport.setBounds(channelPanel);
    const int cols = kChannelsPerBank;
    const int containerWidth = cols * (kStripWidth + kStripGap) + kStripGap;
    int y = 0;
    for (int bank = 0; bank < kBankCount; ++bank)
    {
        auto& toggle = bankToggles[static_cast<std::size_t>(bank)];
        toggle.setBounds(kStripGap, y, containerWidth - 2 * kStripGap, kBankHeaderHeight);
        y += kBankHeaderHeight;
        if (! bankOpen[static_cast<std::size_t>(bank)])
        {
            for (int i = bank * kChannelsPerBank; i < bank * kChannelsPerBank + kChannelsPerBank; ++i)
                channels[i]->setVisible(false);
            y += kBankRowGap;
            continue;
        }
        y += 4;
        for (int i = bank * kChannelsPerBank; i < bank * kChannelsPerBank + kChannelsPerBank; ++i)
        {
            const int col = i % cols;
            channels[i]->setVisible(true);
            channels[i]->setBounds(kStripGap + col * (kStripWidth + kStripGap), y, kStripWidth, kStripHeight);
        }
        y += kStripHeight + kBankRowGap;
    }
    channelContainer.setBounds(0, 0, containerWidth, y + 2);

    masterGroup.setBounds(masterArea);
    const int firstBankStripTop = masterArea.getY() + kBankHeaderHeight + 4;
    const int secondBankStripTop = firstBankStripTop + kStripHeight + kBankRowGap + kBankHeaderHeight + 4;
    auto masterMeterArea = juce::Rectangle<int>(masterArea.getX() + 12,
                                                 firstBankStripTop,
                                                 masterArea.getWidth() - 24,
                                                 kStripHeight);
    if (masterMeter != nullptr)
        masterMeter->setBounds(masterMeterArea);

    auto bankBControls = juce::Rectangle<int>(masterArea.getX() + 12,
                                              secondBankStripTop,
                                              masterArea.getWidth() - 24,
                                              kStripHeight);
    const int slot = bankBControls.getHeight() / 5;
    const int x = bankBControls.getX();
    const int w = bankBControls.getWidth();

    outputTrimLabel.setBounds(x, bankBControls.getY() + 24, w, 24);
    outputTrim.setBounds(x + 12, bankBControls.getY() + 45, w - 24, 91);

    const int rateLabelTop = bankBControls.getY() + 244;
    exportRateLabel.setBounds(x, rateLabelTop, w, 16);
    exportSampleRate.setBounds(x, rateLabelTop + 24, w, 28);

    const int bitDepthBoxHeight = 28;
    const int bitDepthLabelTop = rateLabelTop + 64;
    exportDepthLabel.setBounds(x, bitDepthLabelTop, w, 16);
    exportBitDepth.setBounds(x, bitDepthLabelTop + 24, w, bitDepthBoxHeight);

    renderStartLabel.setBounds({});
    renderStartSeconds.setBounds({});
    renderFullSongToggle.setBounds(x, bankBControls.getY() + 425, w, 24);
    renderEndLabel.setBounds({});
    renderEndSeconds.setBounds({});

    auto limiterSlot = juce::Rectangle<int>(x, bankBControls.getY() + slot * 4, w, bankBControls.getHeight() - slot * 4);
    limiterActivityLabel.setBounds({});
    limiterToggle.setBounds(x, limiterSlot.getBottom() - 28, w, 24);

    auto local = getLocalBounds().reduced(16);
    local.removeFromTop(32);
    auto deck = local.removeFromTop(kDeckHeight);
    auto transportPanel = deck.withSizeKeepingCentre(470, 104).translated(0, -12);
    auto deckArea = juce::Rectangle<int>(transportPanel.getX() + 12, transportPanel.getBottom() - 36, transportPanel.getWidth() - 24, 26);
    loadButton.setBounds(deckArea.removeFromLeft(62));
    deckArea.removeFromLeft(6);
    rewButton.setBounds(deckArea.removeFromLeft(52));
    deckArea.removeFromLeft(6);
    stopButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    playButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    recButton.setBounds(deckArea.removeFromLeft(55));
    deckArea.removeFromLeft(6);
    undoButton.setBounds(deckArea.removeFromLeft(68));
    deckArea.removeFromLeft(6);
    exportButton.setBounds(deckArea.removeFromLeft(70));
}
