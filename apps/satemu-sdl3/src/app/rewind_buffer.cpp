#include "rewind_buffer.hpp"

#include <satemu/util/data_ops.hpp>
#include <satemu/util/thread_name.hpp>

#include <cereal/cereal.hpp>

#include <serdes/cereal_archive_vector.hpp>
#include <serdes/state_v1_cereal.hpp>

#include <lz4.h>

namespace app {

RewindBuffer::RewindBuffer() {
    Reset();
}

RewindBuffer::~RewindBuffer() {
    Stop();
    if (m_procThread.joinable()) {
        m_procThread.join();
    }
}

void RewindBuffer::Reset() {
    for (auto &buffer : m_buffers) {
        buffer.clear();
    }
    m_deltaBuffer.clear();

    m_frames.clear();
    m_nextFrameSeq = 0;
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

        // Process frame from next buffer
        ProcessFrame();
    }

    // TODO: pop frames (run backwards)
    // TODO: implement keyframes to allow fast jumps to arbitrary points in the timeline
    // TODO: reset/clear buffer
}

std::vector<char> &RewindBuffer::GetBuffer() {
    auto &buffer = m_buffers[m_bufferFlip];
    m_bufferFlip ^= true;
    buffer.clear();
    return buffer;
}

RewindBuffer::Frame &RewindBuffer::GetFrame() {
    // Do we have a frame?
    if (m_frames.empty()) {
        // Create one
        return m_frames[m_nextFrameSeq++];
    }

    // Get the latest frame we're working on
    Frame &latest = m_frames.rbegin()->second;

    // If there's room for more state, use this frame
    // TODO: configurable keyframe interval
    if (latest.segments.size() < 60) {
        return latest;
    }

    // Otherwise, make a new frame
    Frame &newFrame = m_frames[m_nextFrameSeq++];

    // TODO: Delete or merge old frames

    return newFrame;
}

void RewindBuffer::ProcessFrame() {
    // Get work frame
    Frame &frame = GetFrame();

    if (frame.segments.empty()) {
        // This is a new frame; compress state into it

        // Compute sizes
        auto &srcBuffer = m_buffers[m_bufferFlip ^ 1];
        const size_t srcSize = srcBuffer.size();
        const size_t dstSize = LZ4_compressBound(srcSize);

        // Create the first segment
        Segment &segment = frame.segments.emplace_back();
        segment.offset = frame.data.size();

        // Compress state into frame data buffer
        frame.data.resize(dstSize);
        const char *const src = srcBuffer.data();
        char *const dst = frame.data.data();
        segment.length = LZ4_compress_fast(src, dst, srcSize, dstSize, LZ4Accel);
        frame.data.resize(segment.length);
    } else {
        // This frame is in progress; compute delta

        // Resize delta buffer if needed
        const size_t maxSize = std::max(m_buffers[0].size(), m_buffers[1].size());
        const size_t minSize = std::min(m_buffers[0].size(), m_buffers[1].size());
        if (m_deltaBuffer.size() < maxSize) [[unlikely]] {
            m_deltaBuffer.resize(maxSize);
        }

        // Use pointers to allow for vectorization
        char *out = &m_deltaBuffer[0];
        char *b0 = m_buffers[0].empty() ? nullptr : &m_buffers[0][0];
        char *b1 = m_buffers[1].empty() ? nullptr : &m_buffers[1][0];

        // Compute XOR delta
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

        // Compute sizes
        const size_t srcSize = m_deltaBuffer.size();
        const size_t dstSize = LZ4_compressBound(srcSize);
        const size_t baseDataSize = frame.data.size();

        // Create a new segment
        Segment &segment = frame.segments.emplace_back();
        segment.offset = baseDataSize;

        // Compress state into frame data buffer
        frame.data.resize(baseDataSize + dstSize);
        const char *const src = m_deltaBuffer.data();
        char *const dst = frame.data.data() + baseDataSize;
        segment.length = LZ4_compress_fast(src, dst, srcSize, dstSize, LZ4Accel);
        frame.data.resize(baseDataSize + segment.length);

        if (frame.segments.size() == 60) {
            frame.data.shrink_to_fit();
        }
    }
}

} // namespace app
