// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#include "app_shared.hpp"
#include "forge.h"
#include <iostream>
#include <vector>
#include <concepts>
#include <climits>
#include <numeric>

namespace solver
{
namespace
{
app_shared::LegacyV02 g_obj;

template <typename>
inline constexpr bool DependentFalsity = false;

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

auto makeMessage()
{
    const auto                                              msg = composeWithLeadingCRC(g_obj);
    std::array<std::uint8_t, sizeof(app_shared::LegacyV02)> shifted{};
    std::copy(msg.begin(), msg.begin() + shifted.size(), shifted.begin());
    // std::copy(msg.begin() + 8U, msg.end(), shifted.begin());
    return shifted;
}

void forgeH(const std::size_t flip_bit_index, ::bigint* const out_hash) noexcept
{
    auto shifted = makeMessage();
    if (flip_bit_index < shifted.size() * CHAR_BIT)
    {
        // const auto pos = (flip_bit_index & ~7U) | (7U - (flip_bit_index & 7U)); // if CRC is reflected
        const auto pos = flip_bit_index;
        shifted.at(pos / CHAR_BIT) ^= (1U << (pos % CHAR_BIT));
    }
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
    const auto                   name_offset = (hash::CRC64WE::Size + 14U) * CHAR_BIT;
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
        auto out = solver::makeMessage();
        for (std::size_t i = 0; i < static_cast<std::size_t>(forge_result); i++)
        {
            const auto idx = flippable_bits_indices.at(i);
            std::cerr << idx << ",";
            out.at(idx / CHAR_BIT) ^= (1U << (idx % CHAR_BIT));
        }
        std::cerr << std::endl;
        std::cout.write(reinterpret_cast<const char*>(out.data()), out.size());
    }
    else
    {
        std::cerr << "No solution found; error " << forge_result << std::endl;
    }
    return 0;
}
