#include "rewind_buffer.hpp"

#include <satemu/util/data_ops.hpp>

#include <lz4.h>

namespace app {

RewindBuffer::~RewindBuffer() {
    Stop();
    if (m_procThread.joinable()) {
        m_procThread.join();
    }
}

void RewindBuffer::Start() {
    if (!m_running) {
        if (m_procThread.joinable()) {
            m_procThread.join();
        }
        m_running = true;
        m_procThread = std::thread([&] { ProcThread(); });
    }
}

void RewindBuffer::Stop() {
    if (m_running) {
        m_running = false;
        m_nextStateEvent.Set();
        m_stateProcessedEvent.Set();
    }
}

FLATTEN void RewindBuffer::ProcThread() {
    util::SetCurrentThreadName("Rewind buffer processor");

    while (m_running) {
        m_nextStateEvent.Wait(true);
        if (!m_running) {
            break;
        }

        // Serialize state to next buffer
        std::vector<char> &buffer = GetBuffer();
        cereal::BinaryVectorOutputArchive archive{buffer};
        archive(NextState);
        m_stateProcessedEvent.Set();

        // Compute delta between both buffers
        CalcDelta();
        CompressDelta();
        // TODO: write delta

        // fmt::println("{} / {} bytes", m_lz4Buffer.size(), m_deltaBuffer.size());
    }
}

std::vector<char> &RewindBuffer::GetBuffer() {
    auto &buffer = m_buffers[m_bufferFlip];
    m_bufferFlip ^= true;
    buffer.clear();
    return buffer;
}

void RewindBuffer::CalcDelta() {
    const size_t maxSize = std::max(m_buffers[0].size(), m_buffers[1].size());
    const size_t minSize = std::min(m_buffers[0].size(), m_buffers[1].size());
    if (maxSize == 0) [[unlikely]] {
        return;
    }

    // Reallocate scratch buffer if needed
    if (m_deltaBuffer.size() < maxSize) [[unlikely]] {
        m_deltaBuffer.resize(maxSize);
    }

    // Use pointers to allow for vectorization
    char *out = &m_deltaBuffer[0];
    char *b0 = m_buffers[0].empty() ? nullptr : &m_buffers[0][0];
    char *b1 = m_buffers[1].empty() ? nullptr : &m_buffers[1][0];

    // Compute delta
    size_t i = 0;
    for (; i + sizeof(uint64) < minSize; i += sizeof(uint64)) {
        util::WriteNE<uint64>(&out[i], util::ReadNE<uint64>(&b0[i]) ^ util::ReadNE<uint64>(&b1[i]));
    }
    for (; i < minSize; i++) {
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

void RewindBuffer::CompressDelta() {
    if (m_deltaBuffer.empty()) {
        return;
    }

    const size_t srcSize = m_deltaBuffer.size();
    const size_t dstSize = LZ4_compressBound(srcSize);
    m_lz4Buffer.resize(dstSize);

    const char *const src = m_deltaBuffer.data();
    char *const dst = m_lz4Buffer.data();

    const int size = LZ4_compress_fast(src, dst, srcSize, dstSize, LZ4Accel);
    m_lz4Buffer.resize(size);
}

} // namespace app
