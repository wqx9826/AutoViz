#include "autoviz/FrameCodec.h"

namespace autoviz {

bool encodeFrame(const Envelope& envelope, FrameBytes& frame, std::string& errorMessage)
{
    std::string payload;
    errorMessage.clear();
    frame.clear();
    if (!envelope.SerializeToString(&payload)) {
        errorMessage = "protobuf envelope serialization failed";
        return false;
    }
    if (payload.empty() || payload.size() > kMaxFrameSize) {
        errorMessage = "invalid or oversized protobuf payload";
        return false;
    }

    const auto size = static_cast<std::uint32_t>(payload.size());
    frame.assign(kFrameHeaderSize, '\0');
    frame[0] = static_cast<char>((size >> 24U) & 0xffU);
    frame[1] = static_cast<char>((size >> 16U) & 0xffU);
    frame[2] = static_cast<char>((size >> 8U) & 0xffU);
    frame[3] = static_cast<char>(size & 0xffU);
    frame += payload;
    return true;
}

bool FrameDecoder::decode(std::string_view bytes,
                          std::vector<Envelope>& envelopes,
                          std::string& errorMessage)
{
    errorMessage.clear();
    m_buffer.append(bytes.data(), bytes.size());
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
            errorMessage = "invalid or oversized frame";
            reset();
            return false;
        }
        if (m_buffer.size() < kFrameHeaderSize + payloadSize) {
            return true;
        }

        Envelope envelope;
        if (!envelope.ParseFromArray(m_buffer.data() + kFrameHeaderSize,
                                     static_cast<int>(payloadSize))) {
            errorMessage = "protobuf envelope parse failed";
            reset();
            return false;
        }
        envelopes.push_back(std::move(envelope));
        m_buffer.erase(0, kFrameHeaderSize + payloadSize);
    }
}

void FrameDecoder::reset()
{
    m_buffer.clear();
}

}  // namespace autoviz
