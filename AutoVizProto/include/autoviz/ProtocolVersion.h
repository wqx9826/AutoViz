#pragma once

#include <cstdint>

namespace autoviz {

inline constexpr std::uint32_t kProtocolMajor = 2;
inline constexpr std::uint32_t kProtocolMinor = 0;

inline constexpr bool isProtocolMajorCompatible(std::uint32_t peerMajor)
{
    return peerMajor == kProtocolMajor;
}

}  // namespace autoviz
