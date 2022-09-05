// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#include "app_shared.hpp"
#include "forge.h"
#include <iostream>
#include <vector>
#include <concepts>
#include <climits>
#include <numeric>
#include <sstream>

namespace solver
{
namespace
{
app_shared::LegacyV02 g_obj;

template <typename>
[[maybe_unused]] inline constexpr bool DependentFalsity = false;

constexpr std::size_t NameOffsetBytes = 14U;

template <std::integral T>
::bigint makeBigInt(const T& value)
{
    ::bigint res{};
    (void) bigint_init(&res, sizeof(T) * CHAR_BIT);
    if constexpr (sizeof(T) <= sizeof(::limb_t))
    {
        res.limb[0] = value;
    }
    else if constexpr (sizeof(T) == (sizeof(::limb_t) * 2))
    {
        res.limb[0] = value & std::numeric_limits<::limb_t>::max();
        res.limb[1] = (value >> (sizeof(::limb_t) * CHAR_BIT)) & std::numeric_limits<::limb_t>::max();
    }
    else
    {
        static_assert(DependentFalsity<T>, "Unsupported type");
    }
    return res;
}

void forgeH(const std::size_t flip_bit_index, ::bigint* const out_hash) noexcept
{
    auto obj = g_obj;
    if (flip_bit_index < sizeof(app_shared::LegacyV02) * CHAR_BIT)
    {
        // const auto pos = (flip_bit_index & ~7U) | (7U - (flip_bit_index & 7U)); // if CRC is reflected
        const auto pos         = flip_bit_index;
        const auto pos_in_name = pos - (NameOffsetBytes + hash::CRC64WE::Size) * CHAR_BIT;
        *reinterpret_cast<std::uint8_t*>(obj.uavcan_file_name.data() + (pos_in_name / CHAR_BIT)) ^=
            (1U << (pos_in_name % CHAR_BIT));
    }
    const auto                                              msg = composeWithLeadingCRC(obj);
    std::array<std::uint8_t, sizeof(app_shared::LegacyV02)> shifted{};
    std::copy(msg.begin(), msg.begin() + shifted.size(), shifted.begin());
    hash::CRC64WE crc;
    crc.update(shifted.data(), shifted.size());
    *out_hash = makeBigInt(crc.get());
}

}  // namespace
}  // namespace solver

int main(const int argc, const char* const argv[])
{
    using solver::g_obj;
    const std::vector<std::string> args(argv + 1, argv + argc);
    try
    {
        g_obj.can_bus_speed            = static_cast<std::uint32_t>(std::stoul(args.at(0)));
        g_obj.uavcan_node_id           = static_cast<std::uint8_t>(std::stoul(args.at(1)));
        g_obj.uavcan_fw_server_node_id = static_cast<std::uint8_t>(std::stoul(args.at(2)));
        g_obj.stay_in_bootloader       = true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Invalid usage: " << ex.what() << std::endl;
        return 1;
    }
    std::cerr << "Seed:\n" << g_obj << std::endl;
    const ::bigint               target_checksum = solver::makeBigInt<std::uint64_t>(0);
    std::array<std::size_t, 64U> flippable_bits_indices{};
    const auto                   name_offset = (hash::CRC64WE::Size + solver::NameOffsetBytes) * CHAR_BIT;
    std::iota(flippable_bits_indices.begin(), flippable_bits_indices.end(), name_offset);
    const auto forge_result = ::forge(sizeof(app_shared::LegacyV02),  // length specified in bytes!
                                      &target_checksum,
                                      &solver::forgeH,
                                      flippable_bits_indices.data(),
                                      flippable_bits_indices.size(),
                                      nullptr);  // bug https://github.com/resilar/crchack/issues/10
    if (forge_result >= 0)
    {
        std::cerr << "Solution found with " << forge_result << " bits flipped" << std::endl;
        auto obj = g_obj;
        for (std::size_t i = 0; i < static_cast<std::size_t>(forge_result); i++)
        {
            const auto idx = flippable_bits_indices.at(i);
            std::cerr << idx << ",";
            // const auto pos = (flip_bit_index & ~7U) | (7U - (flip_bit_index & 7U)); // if CRC is reflected
            const auto pos         = idx;
            const auto pos_in_name = pos - (solver::NameOffsetBytes + hash::CRC64WE::Size) * CHAR_BIT;
            *reinterpret_cast<std::uint8_t*>(obj.uavcan_file_name.data() + (pos_in_name / CHAR_BIT)) ^=
                (1U << (pos_in_name % CHAR_BIT));
        }
        std::cerr << std::endl;
        const auto out = composeWithLeadingCRC(obj);
        std::cout.write(reinterpret_cast<const char*>(out.data()), out.size());

        std::array<std::uint8_t, hash::CRC64WE::Size + sizeof(app_shared::LegacyV02)> test_buffer{};
        std::copy(out.begin(), out.end(), test_buffer.begin());
        if (const auto parsed = app_shared::parseWithTrailingCRC<app_shared::LegacyV02>(test_buffer.data()))
        {
            std::cerr << "\nParsed as seen by the bootloader (FYI, do not use):\n" << *parsed << std::endl;
            std::cerr << "USE THIS FILE NAME: {";
            std::ostringstream oss;
            for (std::size_t i = name_offset / 8U;
                 i < (name_offset / 8U + app_shared::LegacyV02().uavcan_file_name.size());
                 i++)
            {
                oss.width(2);
                oss.fill('0');
                oss << std::hex << (static_cast<std::uint16_t>(out.at(i)) & 0xFFU) << ',';
            }
            std::cerr << oss.str() << "}\n";
        }
        else
        {
            std::cerr << "Self-check failed" << std::endl;
        }
    }
    else
    {
        std::cerr << "No solution found; error " << forge_result << std::endl;
    }
    return 0;
}
