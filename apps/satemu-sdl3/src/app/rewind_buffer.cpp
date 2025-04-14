#include "rewind_buffer.hpp"

namespace app {

RewindBuffer::~RewindBuffer() {
    Stop();
    if (m_procThread.joinable()) {
        m_procThread.join();
    }
    if (m_deltaBuffer != nullptr) {
        delete[] m_deltaBuffer;
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

// Gets and clears the next buffer and flips the buffer pointer.
std::vector<uint8> &RewindBuffer::GetBuffer() {
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

void RewindBuffer::CompressDelta() {
    // TODO: replace with a *fast* compression algorithm or try optimizing this
    // https://github.com/lz4/lz4

    // Encoding scheme:
    //   0x00..0x5E: write 0x01..0x5F zeros
    //         0x5F: write [following uint24le + 0x60] zeros
    //   0x60..0x7E: copy the next 0x01..0x1F bytes verbatim
    //         0x7F: copy the next [following uint16le + 0x20] bytes verbatim
    //   0x80..0xFD: repeat next byte 0x02..0x7F times
    //         0xFE: repeat next byte [following uint24le + 0x80] times
    //         0xFF: end of stream
    if (m_deltaBufferSize == 0) {
        return;
    }

    m_rleBuffer.clear();

    size_t pos = 1;

    uint32 repeatCount = 1;
    uint32 copyCount = 1;
    uint8 lastByte = m_deltaBuffer[0];
    while (pos < m_deltaBufferSize) {
        const uint8 currByte = m_deltaBuffer[pos];

        if (currByte == lastByte) {
            ++repeatCount;
            if (repeatCount > 2 && copyCount > 1) {
                --copyCount;
                if (copyCount <= 0x1F) {
                    m_rleBuffer.push_back(copyCount - 1 + 0x60);
                    m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
                } else {
                    m_rleBuffer.push_back(0x7F);
                    m_rleBuffer.push_back((copyCount - 0x20) >> 0u);
                    m_rleBuffer.push_back((copyCount - 0x20) >> 8u);
                    m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
                }
                copyCount = 0;
            } else if (repeatCount == 2) {
                ++copyCount;
                if (copyCount == 0xFFFF + 0x20) {
                    if (copyCount <= 0x1F) {
                        m_rleBuffer.push_back(copyCount - 1 + 0x60);
                        m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount],
                                           &m_deltaBuffer[pos - 1]);
                    } else {
                        m_rleBuffer.push_back(0x7F);
                        m_rleBuffer.push_back((copyCount - 0x20) >> 0u);
                        m_rleBuffer.push_back((copyCount - 0x20) >> 8u);
                        m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount],
                                           &m_deltaBuffer[pos - 1]);
                    }
                    copyCount = 0;
                }
            }
            if ((lastByte == 0 && repeatCount == 0xFFFFFF + 0x60) ||
                (lastByte != 0 && repeatCount == 0xFFFFFF + 0x80)) {
                if (lastByte == 0) {
                    if (repeatCount <= 0x5F) {
                        m_rleBuffer.push_back(repeatCount - 1);
                    } else {
                        m_rleBuffer.push_back(0x5F);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 0u);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 8u);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 16u);
                    }
                } else if (repeatCount <= 0x7E) {
                    m_rleBuffer.push_back(repeatCount - 2 + 0x80);
                    m_rleBuffer.push_back(lastByte);
                } else {
                    m_rleBuffer.push_back(0xFE);
                    m_rleBuffer.push_back(lastByte);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 0u);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 8u);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 16u);
                }

                repeatCount = 1;
            }
        } else {
            if (repeatCount > 2) {
                if (lastByte == 0) {
                    if (repeatCount <= 0x5F) {
                        m_rleBuffer.push_back(repeatCount - 1);
                    } else {
                        m_rleBuffer.push_back(0x5F);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 0u);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 8u);
                        m_rleBuffer.push_back((repeatCount - 0x60) >> 16u);
                    }
                } else if (repeatCount <= 0x7E) {
                    m_rleBuffer.push_back(repeatCount - 2 + 0x80);
                    m_rleBuffer.push_back(lastByte);
                } else {
                    m_rleBuffer.push_back(0xFE);
                    m_rleBuffer.push_back(lastByte);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 0u);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 8u);
                    m_rleBuffer.push_back((repeatCount - 0x80) >> 16u);
                }
            }
            repeatCount = 1;
            ++copyCount;
            if (copyCount == 0xFFFF + 0x20) {
                if (copyCount <= 0x1F) {
                    m_rleBuffer.push_back(copyCount - 1 + 0x60);
                    m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
                } else {
                    m_rleBuffer.push_back(0x7F);
                    m_rleBuffer.push_back((copyCount - 0x20) >> 0u);
                    m_rleBuffer.push_back((copyCount - 0x20) >> 8u);
                    m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
                }
                copyCount = 0;
            }
        }

        lastByte = currByte;
        ++pos;
    }

    if (copyCount > 1) {
        if (copyCount <= 0x1F) {
            m_rleBuffer.push_back(copyCount - 1 + 0x60);
            m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
        } else {
            m_rleBuffer.push_back(0x7F);
            m_rleBuffer.push_back((copyCount - 0x20) >> 0u);
            m_rleBuffer.push_back((copyCount - 0x20) >> 8u);
            m_rleBuffer.insert(m_rleBuffer.end(), &m_deltaBuffer[pos - 1 - copyCount], &m_deltaBuffer[pos - 1]);
        }
    }
    if (repeatCount > 1) {
        if (lastByte == 0) {
            if (repeatCount <= 0x5F) {
                m_rleBuffer.push_back(repeatCount - 1);
            } else {
                m_rleBuffer.push_back(0x5F);
                m_rleBuffer.push_back((repeatCount - 0x60) >> 0u);
                m_rleBuffer.push_back((repeatCount - 0x60) >> 8u);
                m_rleBuffer.push_back((repeatCount - 0x60) >> 16u);
            }
        } else if (repeatCount <= 0x7E) {
            m_rleBuffer.push_back(repeatCount - 2 + 0x80);
            m_rleBuffer.push_back(lastByte);
        } else {
            m_rleBuffer.push_back(0xFE);
            m_rleBuffer.push_back(lastByte);
            m_rleBuffer.push_back((repeatCount - 0x80) >> 0u);
            m_rleBuffer.push_back((repeatCount - 0x80) >> 8u);
            m_rleBuffer.push_back((repeatCount - 0x80) >> 16u);
        }
    } else if (repeatCount == 1 && copyCount == 1) {
        m_rleBuffer.push_back(0x60);
        m_rleBuffer.push_back(lastByte);
    }

    m_rleBuffer.push_back(0xFF);
}

} // namespace app
