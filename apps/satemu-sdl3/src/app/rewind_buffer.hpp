#pragma once

#include <satemu/state/state.hpp>

#include <satemu/util/event.hpp>

#include <satemu/core/types.hpp>

#include <array>
#include <map>
#include <thread>
#include <vector>

namespace app {

class RewindBuffer {
public:
    RewindBuffer();
    ~RewindBuffer();

    void Reset();

    void Start();
    void Stop();

    bool IsRunning() const {
        return m_running;
    }

    // Tells the rewind buffer processor thread that the next state is ready to be processed.
    // Should be invoked by the emulator thread after saving a state to NextState.
    void ProcessState() {
        m_stateProcessedEvent.Wait(true);
        m_nextStateEvent.Set();
    }

    // Next state to be processed. Should be filled in by the emulator before invoking ProcessState()
    satemu::state::State NextState;

    int LZ4Accel = 64; // LZ4 acceleration factor (1 to 65537)

private:
    bool m_running = false;

    std::thread m_procThread;
    util::Event m_nextStateEvent{false};     // Raised by emulator to ask rewind buffer to process next state
    util::Event m_stateProcessedEvent{true}; // Raised by rewind buffer to tell emulator it's done processing

    std::array<std::vector<char>, 2> m_buffers; // Buffers for serialized states (current and next)
    bool m_bufferFlip = false;                  // Which buffer is which
    std::vector<char> m_deltaBuffer;            // XOR delta buffer

    struct Segment {
        size_t offset;
        size_t length;
    };

    struct Frame {
        std::vector<char> data;        // Compressed serialized states
        std::vector<Segment> segments; // Frame segments
        uint64 seqNum;                 // Frame sequence number
    };

    std::map<uint64, Frame> m_frames; // Frames in the rewind buffer
    uint64 m_nextFrameSeq;            // Next frame sequence number

    void ProcThread();

    // Gets and clears the next buffer and flips the buffer pointer.
    std::vector<char> &GetBuffer();

    // Gets the current frame to work on.
    Frame &GetFrame();

    void ProcessFrame();
};

} // namespace app
