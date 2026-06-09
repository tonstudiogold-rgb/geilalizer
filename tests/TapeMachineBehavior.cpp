#include "core/TapeMachineState.h"

#include <cassert>
#include <vector>

int main()
{
    using namespace geilalizer::core;

    TapeMachineState tape;
    const auto initial = tape.snapshot();
    assert(initial.trackCount == 24);
    assert(initial.banks.size() == 3);
    assert((initial.banks[0].trackNumbers == std::vector<int> { 1, 2, 3, 4, 5, 6, 7, 8 }));
    assert((initial.banks[1].trackNumbers == std::vector<int> { 9, 10, 11, 12, 13, 14, 15, 16 }));
    assert((initial.banks[2].trackNumbers == std::vector<int> { 17, 18, 19, 20, 21, 22, 23, 24 }));
    assert(! initial.timelineMode);

    auto s = tape.toggleArm(15);
    assert(! s.tracks[15].armed);
    assert(s.message.find("Input") != std::string::npos);

    s = tape.setInput(15, 15);
    assert(s.tracks[15].inputAssigned);
    assert(s.tracks[15].inputChannelIndex == 15);
    s = tape.toggleArm(15);
    assert(s.tracks[15].armed);
    s = tape.toggleArm(15);
    assert(! s.tracks[15].armed);

    s = tape.play(0);
    assert(s.transport == TransportState::Playing);
    assert(s.emptyPlayback);
    s = tape.advance(1200);
    assert(s.playheadFrame == 1200);
    s = tape.stop();
    assert(s.transport == TransportState::Stopped);
    assert(s.playheadFrame == 1200);
    s = tape.stop();
    assert(s.playheadFrame == 0);

    tape.setInput(15, 15);
    tape.toggleArm(15);
    tape.play(0);
    tape.advance(4800);
    s = tape.toggleRecord();
    assert(s.recordEnabled);
    assert(s.transport == TransportState::Recording);
    assert(s.recordStartFrame == 4800);
    s = tape.advance(2400);
    assert(s.playheadFrame == 7200);
    s = tape.toggleRecord();
    assert(! s.recordEnabled);
    assert(s.transport == TransportState::Playing);
    assert(s.lastRecording.has_value());
    assert(s.lastRecording->startFrame == 4800);
    assert(s.lastRecording->endFrame == 7200);
    assert(s.lastRecording->tracks.size() == 1);
    assert(s.lastRecording->tracks[0] == 15);
    assert(s.canUndo);
    s = tape.undoLastRecording();
    assert(! s.canUndo);
    assert(! s.lastRecording.has_value());

    tape.setInput(0, 0);
    tape.toggleArm(0);
    tape.toggleMute(0);
    tape.toggleSolo(0);
    tape.play(0);
    tape.toggleRecord();
    s = tape.advance(256);
    assert(s.tracks[0].muted);
    assert(s.tracks[0].solo);
    assert(s.tracks[0].monitoringAudible);
    assert(s.recordEnabled);

    return 0;
}
