#pragma once

#include <memory>

#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component
{
public:
    MainComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int channelCount = 24;

    class MeterPlaceholder;
    class ChannelStrip;
    class ExportSettingsComponent;

    void showExportDialog();

    juce::Label titleLabel;
    juce::TextButton loadButton { "Load" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton exportButton { "Export" };

    juce::Viewport channelViewport;
    juce::Component channelContainer;
    juce::OwnedArray<ChannelStrip> channels;

    juce::GroupComponent masterGroup { {}, "Master" };
    juce::ToggleButton limiterToggle { "Limiter On" };
    juce::Label outputTrimLabel;
    juce::Slider outputTrim;
    std::unique_ptr<MeterPlaceholder> masterMeter;
    juce::Label limiterActivityLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
