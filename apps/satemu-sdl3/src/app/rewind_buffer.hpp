#pragma once

#include <satemu/state/state.hpp>

#include <satemu/util/event.hpp>
#include <satemu/util/thread_name.hpp>

#include <satemu/core/types.hpp>

#include <cereal/cereal.hpp>

#include <serdes/cereal_archive_vector.hpp>
#include <serdes/state_v1_cereal.hpp>

#include <array>
#include <thread>
#include <vector>

namespace app {

class RewindBuffer {
public:
    ~RewindBuffer();

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

private:
    bool m_running = false;

    std::thread m_procThread;
    util::Event m_nextStateEvent{false};     // Raised by emulator to ask rewind buffer to process next state
    util::Event m_stateProcessedEvent{true}; // Raised by rewind buffer to tell emulator it's done processing

    std::array<std::vector<char>, 2> m_buffers; // Buffers for serialized states (current and next)
    bool m_bufferFlip = false;                  // Which buffer is which
    std::vector<char> m_deltaBuffer;            // XOR delta buffer
    std::vector<char> m_lz4Buffer;              // LZ4-compressed delta buffer

    void ProcThread();

    // Gets and clears the next buffer and flips the buffer pointer.
    std::vector<char> &GetBuffer();

    void CalcDelta();
    void CompressDelta();
};

} // namespace app
