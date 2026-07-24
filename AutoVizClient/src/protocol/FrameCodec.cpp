#include "FrameCodec.h"

namespace autoviz::protocol {

std::string encodeFrame(const v1::Envelope& envelope)
{
    std::string payload;
    if (!envelope.SerializeToString(&payload) || payload.size() > kMaxFrameSize) {
        return {};
    }

    const auto size = static_cast<std::uint32_t>(payload.size());
    std::string frame(kFrameHeaderSize, '\0');
    frame[0] = static_cast<char>((size >> 24U) & 0xffU);
    frame[1] = static_cast<char>((size >> 16U) & 0xffU);
    frame[2] = static_cast<char>((size >> 8U) & 0xffU);
    frame[3] = static_cast<char>(size & 0xffU);
    frame += payload;
    return frame;
}

bool FrameDecoder::append(const char* data,
                          std::size_t size,
                          std::vector<v1::Envelope>* envelopes,
                          std::string* errorMessage)
{
    if (envelopes == nullptr || (data == nullptr && size != 0U)) {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid decoder argument";
        }
        return false;
    }

    m_buffer.append(data, size);
    for (;;) {
        if (m_buffer.size() < kFrameHeaderSize) {
            return true;
        }

        const auto* bytes = reinterpret_cast<const unsigned char*>(m_buffer.data());
        const std::uint32_t payloadSize = (static_cast<std::uint32_t>(bytes[0]) << 24U)
                                          | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                                          | (static_cast<std::uint32_t>(bytes[2]) << 8U)
                                          | static_cast<std::uint32_t>(bytes[3]);
        if (payloadSize == 0U || payloadSize > kMaxFrameSize) {
            if (errorMessage != nullptr) {
                *errorMessage = "invalid or oversized frame";
            }
            reset();
            return false;
        }
        if (m_buffer.size() < kFrameHeaderSize + payloadSize) {
            return true;
        }

        v1::Envelope envelope;
        if (!envelope.ParseFromArray(m_buffer.data() + kFrameHeaderSize,
                                     static_cast<int>(payloadSize))) {
            if (errorMessage != nullptr) {
                *errorMessage = "protobuf envelope parse failed";
            }
            reset();
            return false;
        }
        envelopes->push_back(std::move(envelope));
        m_buffer.erase(0, kFrameHeaderSize + payloadSize);
    }
}

void FrameDecoder::reset()
{
    m_buffer.clear();
}

}  // namespace autoviz::protocol
