#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace geilalizer::core
{

enum class TransportState
{
    Stopped,
    Playing,
    Recording
};

struct TapeTrackState
{
    int number = 1;
    bool inputAssigned = false;
    int inputChannelIndex = -1;
    bool armed = false;
    bool muted = false;
    bool solo = false;
    bool monitoringAudible = true;
};

struct TapeBankState
{
    int index = 0;
    std::string label;
    bool collapsed = false;
    std::vector<int> trackNumbers;
};

struct TapeRecordingSpan
{
    std::int64_t startFrame = 0;
    std::int64_t endFrame = 0;
    std::vector<int> tracks;
};

struct TapeMachineSnapshot
{
    int trackCount = 24;
    bool timelineMode = false;
    TransportState transport = TransportState::Stopped;
    bool recordEnabled = false;
    bool emptyPlayback = true;
    bool canUndo = false;
    std::int64_t playheadFrame = 0;
    std::int64_t recordStartFrame = 0;
    std::string message;
    std::vector<TapeBankState> banks;
    std::vector<TapeTrackState> tracks;
    std::optional<TapeRecordingSpan> lastRecording;
};

class TapeMachineState
{
public:
    TapeMachineState()
    {
        tracks.reserve(kTrackCount);
        for (int i = 0; i < kTrackCount; ++i)
        {
            TapeTrackState track;
            track.number = i + 1;
            tracks.push_back(track);
        }
    }

    TapeMachineSnapshot snapshot() const
    {
        TapeMachineSnapshot s;
        s.trackCount = kTrackCount;
        s.timelineMode = false;
        s.transport = transport;
        s.recordEnabled = recordEnabled;
        s.emptyPlayback = knownMaterialFrames <= 0;
        s.canUndo = lastRecording.has_value();
        s.playheadFrame = playheadFrame;
        s.recordStartFrame = recordStartFrame;
        s.message = message;
        s.banks = banks();
        s.tracks = tracks;
        const bool anySolo = std::any_of(s.tracks.begin(), s.tracks.end(), [](const auto& t) { return t.solo; });
        for (auto& t : s.tracks)
            t.monitoringAudible = anySolo ? t.solo : ! t.muted;
        s.lastRecording = lastRecording;
        return s;
    }

    TapeMachineSnapshot setInput(int zeroBasedTrack, int zeroBasedInput)
    {
        if (! validTrack(zeroBasedTrack))
            return snapshot();
        auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
        if (zeroBasedInput < 0)
        {
            track.inputAssigned = false;
            track.inputChannelIndex = -1;
            track.armed = false;
            message = "Input entfernt; ARM deaktiviert.";
        }
        else
        {
            track.inputAssigned = true;
            track.inputChannelIndex = zeroBasedInput;
            message = "Input " + std::to_string(zeroBasedInput + 1) + " auf Spur " + std::to_string(zeroBasedTrack + 1) + ".";
        }
        return snapshot();
    }

    TapeMachineSnapshot toggleArm(int zeroBasedTrack)
    {
        if (! validTrack(zeroBasedTrack))
            return snapshot();
        auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
        if (! track.inputAssigned)
        {
            track.armed = false;
            message = "ARM nur mit gewähltem Input möglich.";
            return snapshot();
        }
        track.armed = ! track.armed;
        message = "ARM Spur " + std::to_string(zeroBasedTrack + 1) + (track.armed ? " an." : " aus.");
        return snapshot();
    }

    TapeMachineSnapshot toggleMute(int zeroBasedTrack)
    {
        if (validTrack(zeroBasedTrack))
        {
            auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
            track.muted = ! track.muted;
            message = "MUTE wirkt nur auf Monitoring/Mix, nicht auf Aufnahme.";
        }
        return snapshot();
    }

    TapeMachineSnapshot toggleSolo(int zeroBasedTrack)
    {
        if (validTrack(zeroBasedTrack))
        {
            auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
            track.solo = ! track.solo;
            message = "SOLO wirkt nur auf Monitoring/Mix, nicht auf Aufnahme.";
        }
        return snapshot();
    }

    TapeMachineSnapshot setKnownMaterialFrames(std::int64_t frames)
    {
        knownMaterialFrames = std::max<std::int64_t>(0, frames);
        return snapshot();
    }

    TapeMachineSnapshot setPlayheadFrame(std::int64_t frame)
    {
        playheadFrame = std::max<std::int64_t>(0, frame);
        return snapshot();
    }

    TapeMachineSnapshot play(std::int64_t materialFrames)
    {
        knownMaterialFrames = std::max<std::int64_t>(0, materialFrames);
        if (recordEnabled && ! armedInputTracks().empty())
            beginRecordingIfNeeded();
        else
            transport = TransportState::Playing;
        stopArmedForRewind = false;
        message = knownMaterialFrames > 0 ? "PLAY." : "PLAY leer: Bandmaschine läuft auch ohne geladenes Material.";
        return snapshot();
    }

    TapeMachineSnapshot stop()
    {
        if (transport == TransportState::Stopped)
        {
            if (stopArmedForRewind && playheadFrame > 0)
            {
                playheadFrame = 0;
                stopArmedForRewind = false;
                message = "STOP erneut: rewind zum Bandanfang.";
            }
            else
            {
                message = "STOP.";
            }
            recordEnabled = false;
            return snapshot();
        }

        finishRecordingIfNeeded();
        transport = TransportState::Stopped;
        recordEnabled = false;
        stopArmedForRewind = true;
        message = "STOP: Position gehalten; nochmal STOP = rewind.";
        return snapshot();
    }

    TapeMachineSnapshot toggleRecord()
    {
        if (recordEnabled)
        {
            finishRecordingIfNeeded();
            recordEnabled = false;
            if (transport == TransportState::Recording)
                transport = TransportState::Playing;
            message = "REC aus: Punch-Out zurück zu PLAY.";
            return snapshot();
        }

        recordEnabled = true;
        if (transport == TransportState::Playing && ! armedInputTracks().empty())
        {
            beginRecordingIfNeeded();
            message = "REC Punch-In während PLAY.";
        }
        else
        {
            message = "REC ready; PLAY startet Aufnahme auf geARMten Input-Spuren.";
        }
        return snapshot();
    }

    TapeMachineSnapshot advance(std::int64_t frames)
    {
        const auto delta = std::max<std::int64_t>(0, frames);
        if ((transport == TransportState::Playing || transport == TransportState::Recording) && delta > 0)
        {
            if (recordEnabled && ! armedInputTracks().empty() && transport != TransportState::Recording)
                beginRecordingIfNeeded();
            playheadFrame += delta;
            stopArmedForRewind = false;
        }
        return snapshot();
    }

    TapeMachineSnapshot undoLastRecording()
    {
        lastRecording.reset();
        recordEnabled = false;
        if (transport == TransportState::Recording)
            transport = TransportState::Playing;
        message = "1-Schritt-Undo: letzte Aufnahme entfernt.";
        return snapshot();
    }

    bool shouldMonitorTrack(int zeroBasedTrack) const
    {
        if (! validTrack(zeroBasedTrack))
            return false;
        const bool anySolo = std::any_of(tracks.begin(), tracks.end(), [](const auto& t) { return t.solo; });
        const auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
        return anySolo ? track.solo : ! track.muted;
    }

    bool shouldRecordTrack(int zeroBasedTrack) const
    {
        if (! validTrack(zeroBasedTrack))
            return false;
        const auto& track = tracks[static_cast<std::size_t>(zeroBasedTrack)];
        return recordEnabled && track.armed && track.inputAssigned;
    }

private:
    static constexpr int kTrackCount = 24;

    static bool validTrack(int zeroBasedTrack)
    {
        return zeroBasedTrack >= 0 && zeroBasedTrack < kTrackCount;
    }

    std::vector<TapeBankState> banks() const
    {
        std::vector<TapeBankState> result;
        for (int bank = 0; bank < 3; ++bank)
        {
            TapeBankState b;
            b.index = bank;
            b.label = "BANK " + std::string(1, static_cast<char>('A' + bank)) + " | " + std::to_string(bank * 8 + 1) + "-" + std::to_string(bank * 8 + 8);
            b.collapsed = bankCollapsed[static_cast<std::size_t>(bank)];
            for (int i = 0; i < 8; ++i)
                b.trackNumbers.push_back(bank * 8 + i + 1);
            result.push_back(b);
        }
        return result;
    }

    std::vector<int> armedInputTracks() const
    {
        std::vector<int> result;
        for (int i = 0; i < kTrackCount; ++i)
            if (tracks[static_cast<std::size_t>(i)].armed && tracks[static_cast<std::size_t>(i)].inputAssigned)
                result.push_back(i);
        return result;
    }

    void beginRecordingIfNeeded()
    {
        if (transport != TransportState::Recording)
        {
            recordStartFrame = playheadFrame;
            transport = TransportState::Recording;
        }
    }

    void finishRecordingIfNeeded()
    {
        if (transport != TransportState::Recording)
            return;
        TapeRecordingSpan span;
        span.startFrame = recordStartFrame;
        span.endFrame = playheadFrame;
        span.tracks = armedInputTracks();
        if (span.endFrame > span.startFrame && ! span.tracks.empty())
            lastRecording = span;
    }

    TransportState transport = TransportState::Stopped;
    bool recordEnabled = false;
    bool stopArmedForRewind = false;
    std::int64_t playheadFrame = 0;
    std::int64_t recordStartFrame = 0;
    std::int64_t knownMaterialFrames = 0;
    std::string message = "Bereit: 24-Spur-Bandmaschine, kein DAW/Timeline-Modus.";
    std::vector<TapeTrackState> tracks;
    std::optional<TapeRecordingSpan> lastRecording;
    std::vector<bool> bankCollapsed = { false, false, false };
};

} // namespace geilalizer::core
