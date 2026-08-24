#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "autoviz/transport.pb.h"

namespace autoviz {

constexpr std::size_t kFrameHeaderSize = 4;
constexpr std::size_t kMaxFrameSize = 16U * 1024U * 1024U;

// std::string 在这里是二进制字节容器，不表示文本。protobuf 原生使用
// SerializeToString，Qt/Asio 也都能用 data()+size() 直接发送其中的零字节。
using FrameBytes = std::string;

// 将 Envelope 序列化并添加 4 字节大端长度头。失败时返回 false，并清空 frame。
bool encodeFrame(const Envelope& envelope, FrameBytes& frame, std::string& errorMessage);

class FrameDecoder {
public:
    // TCP 是字节流，一次 read 可能只有半帧，也可能包含多帧。本函数把 bytes
    // 追加到内部缓存，并把当前已经完整的 0..N 个 Envelope 放入 envelopes。
    bool decode(std::string_view bytes,
                std::vector<Envelope>& envelopes,
                std::string& errorMessage);
    void reset();

private:
    FrameBytes m_buffer;
};

}  // namespace autoviz
