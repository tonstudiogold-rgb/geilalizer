#pragma once

#include "core/AudioEngine.h"
#include "core/RenderEngine.h"
#include "core/TapeMachineState.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MainComponent final : public juce::AudioAppComponent,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    static constexpr int channelCount = 24;

    class MeterPlaceholder;
    class ChannelStrip;
    class ExportSettingsComponent;

    void showExportDialog();
    void timerCallback() override;

    bool importFileToChannel(const juce::File& file, std::size_t firstChannelIndex);
    bool importFileToFirstAvailableChannel(const juce::File& file);
    void clearChannel(std::size_t channelIndex);
    std::array<bool, channelCount> snapshotOccupiedChannels() const;
    void play();
    void stop();
    void rewind();
    void toggleRecord();
    void undoLastRecording();
    void applyTapeSnapshot(const geilalizer::core::TapeMachineSnapshot& snapshot);
    void overwriteRecordedRange(const geilalizer::core::TapeRecordingSpan& span);
    void updateTransportStatus();
    void renderToFileAsync(const juce::File& outputFile, double sampleRate, int bitDepth,
                           std::size_t startFrame, std::size_t numFrames, bool fullLength);
    std::vector<std::vector<float>> snapshotTrackBuffers() const;
    std::size_t maxLoadedFrames() const;
    juce::String currentTimeText() const;
    void refreshChannelVisuals();
    void refreshMasterVisuals();
    void setStatus(juce::String message);

    juce::Label titleLabel;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton rewButton { "REW" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton recButton { "REC" };
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton exportButton { "RENDER" };
    std::shared_ptr<juce::FileChooser> activeFileChooser;

    juce::Viewport channelViewport;
    juce::Component channelContainer;
    std::array<juce::ToggleButton, 3> bankToggles;
    std::array<bool, 3> bankOpen { true, true, true };
    juce::OwnedArray<ChannelStrip> channels;

    juce::GroupComponent masterGroup { {}, "Master" };
    juce::ToggleButton limiterToggle { "Limiter On" };
    juce::Label outputTrimLabel;
    juce::Slider outputTrim;
    juce::Label exportRateLabel;
    juce::ComboBox exportSampleRate;
    juce::Label exportDepthLabel;
    juce::ComboBox exportBitDepth;
    juce::ToggleButton renderFullSongToggle { "Use full song" };
    juce::Label renderStartLabel;
    juce::Slider renderStartSeconds;
    juce::Label renderEndLabel;
    juce::Slider renderEndSeconds;
    std::unique_ptr<MeterPlaceholder> masterMeter;
    juce::Label limiterActivityLabel;

    geilalizer::core::AudioEngine audioEngine;
    geilalizer::core::RenderEngine renderEngine;
    geilalizer::core::TapeMachineState tapeMachine;
    juce::AudioFormatManager formatManager;

    mutable std::mutex trackMutex;
    std::array<std::vector<float>, channelCount> trackBuffers;
    std::vector<std::vector<float>> audioBlockInputs;
    std::vector<float> audioBlockInterleaved;

    std::atomic<bool> playing { false };
    std::atomic<bool> recording { false };
    std::atomic<std::int64_t> playheadFrame { 0 };
    std::array<std::vector<float>, channelCount> undoTrackBuffers;
    std::optional<geilalizer::core::TapeRecordingSpan> pendingUndoSpan;
    double deviceSampleRate = 44100.0;
    int deviceBlockSize = 512;
    juce::String statusText { "Ready - drag WAV/AIFF/FLAC/MP3 onto a track or click RENDER." };
    juce::String projectName { "Geilalizer Session" };
    juce::CriticalSection statusLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
