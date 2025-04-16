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

bool RewindBuffer::PopState() {
    std::unique_lock lock{m_lock};

    // Get latest frame
    Frame *frame = GetLastFrame();
    if (frame == nullptr) {
        return false;
    }

    // Decompress latest segment into the previous buffer
    Segment &segment = frame->segments.back();
    const size_t size = segment.origLength;
    std::vector<char> &buffer = m_buffers[m_bufferFlip];
    buffer.resize(size);
    int result = LZ4_decompress_safe(&frame->data[segment.offset], &buffer[0], segment.length, size);
    assert(result == size);

    // Use pointers to allow for vectorization
    char *out = &m_buffers[m_bufferFlip ^ 1][0];
    char *b0 = m_buffers[0].empty() ? nullptr : &m_buffers[0][0];
    char *b1 = m_buffers[1].empty() ? nullptr : &m_buffers[1][0];

    // Apply XOR delta
    size_t i = 0;
    for (; i + sizeof(uint64) < size; i += sizeof(uint64)) {
        util::WriteNE<uint64>(&out[i], util::ReadNE<uint64>(&b0[i]) ^ util::ReadNE<uint64>(&b1[i]));
    }
    for (; i < size; i++) {
        out[i] = b0[i] ^ b1[i];
    }

    // Deserialize state
    cereal::BinaryVectorInputArchive archive{m_buffers[m_bufferFlip ^ 1]};
    archive(NextState);

    // Remove segment
    frame->data.resize(frame->data.size() - segment.length);
    frame->segments.pop_back();
    // Delete frame if the only remaining segment is the keyframe
    if (frame->segments.size() <= 1) {
        --m_nextFrameSeq;
        m_frames.erase(m_nextFrameSeq);
    }

    return true;
}

FLATTEN void RewindBuffer::ProcThread() {
    util::SetCurrentThreadName("Rewind buffer processor");

    while (m_running) {
        m_nextStateEvent.Wait(true);
        if (!m_running) {
            break;
        }

        std::unique_lock lock{m_lock};

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

RewindBuffer::Frame &RewindBuffer::GetNextFrame() {
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

    // Keep only the latest 30 frames for now
    // TODO: somehow preserve older frames, either by merging deltas or by selectively deleting them
    m_frames.erase(m_nextFrameSeq - 30);

    return newFrame;
}

RewindBuffer::Frame *RewindBuffer::GetLastFrame() {
    // Bail out if there are no frames
    if (m_frames.empty()) {
        return nullptr;
    }

    // Get the latest frame that has segments
    for (auto it = m_frames.rbegin(); it != m_frames.rend(); it++) {
        Frame &frame = it->second;
        if (!frame.segments.empty()) {
            return &frame;
        }
    }

    // No frames have segments
    return nullptr;
}

void RewindBuffer::ProcessFrame() {
    // Get work frames
    Frame &nextFrame = GetNextFrame();
    Frame *workFrame = GetLastFrame();
    if (workFrame == nullptr) {
        // This is the very first frame
        workFrame = &nextFrame;
    }

    // This is a fresh frame if it has no segments
    const bool freshFrame = nextFrame.segments.empty();

    if (freshFrame) {
        // Write the full state

        // Compute sizes
        auto &srcBuffer = m_buffers[m_bufferFlip ^ 1];
        const size_t srcSize = srcBuffer.size();
        const size_t dstSize = LZ4_compressBound(srcSize);

        // Create the first segment
        Segment &segment = nextFrame.segments.emplace_back();
        segment.offset = nextFrame.data.size();

        // Compress state into frame data buffer
        nextFrame.data.resize(dstSize);
        const char *const src = srcBuffer.data();
        char *const dst = nextFrame.data.data();
        segment.length = LZ4_compress_fast(src, dst, srcSize, dstSize, LZ4Accel);
        segment.origLength = srcSize;
        nextFrame.data.resize(segment.length);
    }

    // Compute delta

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
    const size_t baseDataSize = workFrame->data.size();

    // Create a new segment
    Segment &segment = workFrame->segments.emplace_back();
    segment.offset = baseDataSize;

    // Compress state into frame data buffer
    workFrame->data.resize(baseDataSize + dstSize);
    const char *const src = m_deltaBuffer.data();
    char *const dst = workFrame->data.data() + baseDataSize;
    segment.length = LZ4_compress_fast(src, dst, srcSize, dstSize, LZ4Accel);
    segment.origLength = srcSize;
    workFrame->data.resize(baseDataSize + segment.length);

    if (workFrame->segments.size() == 60) {
        workFrame->data.shrink_to_fit();
    }
}

} // namespace app
