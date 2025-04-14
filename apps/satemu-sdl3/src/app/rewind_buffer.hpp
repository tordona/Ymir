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
        if (m_deltaBuffer != nullptr) {
            delete[] m_deltaBuffer;
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

    // Tells the rewind buffer processor thread that the next state is ready to be processed.
    // Should be invoked by the emulator thread after saving a state to NextState.
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
            m_stateProcessedEvent.Set();

            // Compute delta between both buffers
            CalcDelta();

            // RLE compress the delta
            CompressDelta();

            // TODO: write to rewind buffer
        }
    }

    bool m_running = false;

    std::thread m_procThread;
    util::Event m_nextStateEvent{false};     // Raised by emulator to ask rewind buffer to process next state
    util::Event m_stateProcessedEvent{true}; // Raised by rewind buffer to tell emulator it's done processing

    std::array<std::vector<uint8>, 2> m_buffers; // Buffers for serialized states (current and next)
    bool m_bufferFlip = false;                   // Which buffer is which
    uint8 *m_deltaBuffer = nullptr;              // XOR delta buffer
    size_t m_deltaBufferSize = 0;                // Delta buffer size
    std::vector<uint8> m_rleBuffer;

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
        if (m_deltaBufferSize < maxSize) [[unlikely]] {
            if (m_deltaBuffer != nullptr) {
                delete[] m_deltaBuffer;
            }
            m_deltaBuffer = new uint8[maxSize];
            m_deltaBufferSize = maxSize;
        }

        // Use pointers to allow for vectorization
        uint8 *out = &m_deltaBuffer[0];
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

    void CompressDelta() {
        // Encoding scheme:
        //   0x00..0x5E: write 0x01..0x5F zeros
        //         0x5F: write [following uint24le + 0x60] zeros
        //   0x60..0x7E: copy the next 0x01..0x1F bytes verbatim
        //         0x7F: copy the next [following uint16le + 0x20] bytes verbatim
        //   0x80..0xFD: repeat next byte 0x01..0x7E times
        //         0xFE: repeat next byte [following uint24le + 0x7F] times
        //         0xFF: end of stream
        if (m_deltaBufferSize == 0) {
            return;
        }

        m_rleBuffer.clear();

        size_t pos = 1;

        auto writeZeros = [&](uint32 count) {
            assert(count > 0);
            assert(count <= 0xFFFFFF + 0x60);
            if (count <= 0x5F) {
                m_rleBuffer.push_back(count - 1);
            } else {
                m_rleBuffer.push_back(0x5F);
                m_rleBuffer.push_back((count - 0x60) >> 0u);
                m_rleBuffer.push_back((count - 0x60) >> 8u);
                m_rleBuffer.push_back((count - 0x60) >> 16u);
            }
        };

        auto writeRepeat = [&](uint32 count, uint8 byte) {
            if (byte == 0) {
                writeZeros(count);
                return;
            }

            assert(count > 0);
            assert(count <= 0xFFFFFF + 0x7F);
            if (count <= 0x7E) {
                m_rleBuffer.push_back(count - 1 + 0x80);
                m_rleBuffer.push_back(byte);
            } else {
                m_rleBuffer.push_back(0xFE);
                m_rleBuffer.push_back(byte);
                m_rleBuffer.push_back((count - 0x7F) >> 0u);
                m_rleBuffer.push_back((count - 0x7F) >> 8u);
                m_rleBuffer.push_back((count - 0x7F) >> 16u);
            }
        };

        auto writeCopy = [&](uint32 count) {
            assert(count > 0);
            assert(count <= 0xFFFF + 0x20);
            if (count <= 0x1F) {
                m_rleBuffer.push_back(count - 1 + 0x60);
                m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - count], &m_deltaBuffer[pos - 1]);
            } else {
                m_rleBuffer.push_back(0x7F);
                m_rleBuffer.push_back((count - 0x20) >> 0u);
                m_rleBuffer.push_back((count - 0x20) >> 8u);
                m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - count], &m_deltaBuffer[pos - 1]);
            }
        };

        uint32 repeatCount = 1;
        uint32 copyCount = 1;
        uint8 lastByte = m_deltaBuffer[0];
        while (pos < m_deltaBufferSize) {
            const uint8 currByte = m_deltaBuffer[pos];

            if (currByte == lastByte) {
                if (copyCount > 1) {
                    writeCopy(copyCount - 1);
                    copyCount = 0;
                }
                ++repeatCount;
                if ((lastByte == 0 && repeatCount == 0xFFFFFF + 0x60) ||
                    (lastByte != 0 && repeatCount == 0xFFFFFF + 0x7F)) {
                    writeRepeat(repeatCount, lastByte);
                    repeatCount = 1;
                }
            } else {
                if (repeatCount > 1) {
                    writeRepeat(repeatCount, lastByte);
                    repeatCount = 1;
                }
                ++copyCount;
                if (copyCount == 0xFFFF + 0x20) {
                    writeCopy(copyCount);
                }
            }

            lastByte = currByte;
            ++pos;
        }

        if (copyCount > 1) {
            writeCopy(copyCount - 1);
        }
        writeRepeat(repeatCount, lastByte);

        m_rleBuffer.push_back(0xFF);
    }
};

} // namespace app
