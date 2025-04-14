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
    ~RewindBuffer() {
        Stop();
        if (m_procThread.joinable()) {
            m_procThread.join();
        }
        if (m_scratch != nullptr) {
            delete[] m_scratch;
        }
    }

    void Start() {
        if (!m_running) {
            if (m_procThread.joinable()) {
                m_procThread.join();
            }
            m_procThread = std::thread([&] { ProcThread(); });
        }
    }

    void Stop() {
        if (m_running) {
            m_running = false;
            m_nextStateEvent.Set(true);
        }
    }

    bool IsRunning() const {
        return m_running;
    }

    void ProcessState() {
        m_stateProcessedEvent.Wait(true);
        m_nextStateEvent.Set();
    }

    // Next state to be processed. Should be filled in by emulator before invoking ProcessState()
    satemu::state::State NextState;

private:
    void ProcThread() {
        util::SetCurrentThreadName("Rewind buffer processor");

        m_running = true;
        while (m_running) {
            m_nextStateEvent.Wait(true);
            if (!m_running) {
                break;
            }

            // Serialize state to next buffer
            std::vector<uint8> &buffer = GetBuffer();
            cereal::BinaryVectorOutputArchive archive{buffer};
            archive(NextState);

            // Compute delta between both buffers
            CalcDelta();

            // TODO: RLE compress the delta
            // TODO: write to rewind buffer

            m_stateProcessedEvent.Set();
        }
    }

    bool m_running = false;

    std::thread m_procThread;
    util::Event m_nextStateEvent{false};     // Raised by emulator to ask rewind buffer to process next state
    util::Event m_stateProcessedEvent{true}; // Raised by rewind buffer to tell emulator it's done processing

    std::array<std::vector<uint8>, 2> m_buffers; // Buffers for serialized states (current and next)
    bool m_bufferFlip = false;                   // Which buffer is which
    uint8 *m_scratch = nullptr;                  // Scratch buffer for XOR delta and RLE compression
    size_t m_scratchSize = 0;                    // Scratch buffer size

    // Gets and clears the next buffer and flips the buffer pointer.
    std::vector<uint8> &GetBuffer() {
        auto &buffer = m_buffers[m_bufferFlip];
        m_bufferFlip ^= true;
        buffer.clear();
        return buffer;
    }

    void CalcDelta() {
        const size_t maxSize = std::max(m_buffers[0].size(), m_buffers[1].size());
        const size_t minSize = std::min(m_buffers[0].size(), m_buffers[1].size());
        if (maxSize == 0) [[unlikely]] {
            return;
        }

        // Reallocate scratch buffer if needed
        if (m_scratchSize < maxSize) [[unlikely]] {
            if (m_scratch != nullptr) {
                delete[] m_scratch;
            }
            m_scratch = new uint8[maxSize];
            m_scratchSize = maxSize;
        }

        // Use pointers to allow for vectorization
        uint8 *out = &m_scratch[0];
        uint8 *b0 = m_buffers[0].empty() ? nullptr : &m_buffers[0][0];
        uint8 *b1 = m_buffers[1].empty() ? nullptr : &m_buffers[1][0];

        // Compute delta
        for (size_t i = 0; i < minSize; i++) {
            out[i] = b0[i] ^ b1[i];
        }

        // If one of the buffers is smaller than the other, copy the tail from the larger one
        // (effectively XOR with zeros)
        if (minSize < maxSize) [[unlikely]] {
            if (m_buffers[0].size() == maxSize) {
                std::copy(&b0[minSize], &b0[maxSize], &out[minSize]);
            } else if (m_buffers[1].size() == maxSize) {
                std::copy(&b1[minSize], &b1[maxSize], &out[minSize]);
            }
        }
    }
};

} // namespace app
