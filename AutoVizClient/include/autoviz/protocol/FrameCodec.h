#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "autoviz/protocol/v1/transport.pb.h"

namespace autoviz::protocol {

constexpr std::size_t kFrameHeaderSize = 4;
constexpr std::size_t kMaxFrameSize = 16U * 1024U * 1024U;

std::string encodeFrame(const v1::Envelope& envelope);

class FrameDecoder {
public:
    bool append(const char* data,
                std::size_t size,
                std::vector<v1::Envelope>* envelopes,
                std::string* errorMessage);
    void reset();

private:
    std::string m_buffer;
};

}  // namespace autoviz::protocol
