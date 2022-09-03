// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#pragma once

#include <cstdint>
#include <array>

namespace crc_collider
{

class CRC64WE final
{
public:
    static constexpr std::size_t Size = 8U;

    void update(const std::uint8_t* const data, const std::size_t len)
    {
        const auto* bytes = data;
        for (auto remaining = len; remaining > 0; remaining--)
        {
            crc_ ^= static_cast<std::uint64_t>(*bytes) << InputShift;
            ++bytes;
            // Unrolled for performance reasons. This path directly affects the boot-up time, so it is very
            // important to keep it optimized for speed. Rolling this into a loop causes a significant performance
            // degradation at least with GCC since the compiler refuses to unroll the loop when size optimization
            // is selected (which is normally used for bootloaders).
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
            crc_ = ((crc_ & Mask) != 0) ? ((crc_ << 1U) ^ Poly) : (crc_ << 1U);
        }
    }

    /// The current CRC value.
    [[nodiscard]] auto get() const { return crc_ ^ Xor; }

    /// The current CRC value represented as a big-endian sequence of bytes.
    /// This method is designed for inserting the computed CRC value after the data.
    [[nodiscard]] auto getBytes() const -> std::array<std::uint8_t, Size>
    {
        auto                           x = get();
        std::array<std::uint8_t, Size> out{};
        const auto                     rend = std::rend(out);
        for (auto it = std::rbegin(out); it != rend; ++it)
        {
            *it = static_cast<std::uint8_t>(x);
            x >>= 8U;
        }
        return out;
    }

    /// True if the current CRC value is a correct residue (i.e., CRC verification successful).
    [[nodiscard]] auto isResidueCorrect() const { return crc_ == Residue; }

private:
    static constexpr auto Poly    = static_cast<std::uint64_t>(0x42F0'E1EB'A9EA'3693ULL);
    static constexpr auto Mask    = static_cast<std::uint64_t>(1) << 63U;
    static constexpr auto Xor     = static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFFULL);
    static constexpr auto Residue = static_cast<std::uint64_t>(0xFCAC'BEBD'5931'A992ULL);

    static constexpr auto InputShift = 56U;

    std::uint64_t crc_ = Xor;
};

}  // namespace crc_collider::hash
