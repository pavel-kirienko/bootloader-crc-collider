// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#pragma once

#include "hash.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <iterator>
#include <iostream>

namespace app_shared
{

template <typename Container>
inline std::optional<Container> parseWithTrailingCRC(const void* const ptr) noexcept
{
    class ContainerWrapper final
    {
    public:
        Container container{};

        [[nodiscard]] bool isValid() const
        {
            hash::CRC64WE crc_computer;
            crc_computer.update(reinterpret_cast<const std::uint8_t*>(&container), sizeof(container));  // NOLINT
            return crc_ == crc_computer.get();
        }

    private:
        std::uint64_t crc_{};
    };
    if (const auto* const wrapper = reinterpret_cast<const ContainerWrapper*>(ptr); wrapper->isValid())
    {
        return wrapper->container;
    }
    return {};
}

template <typename Container>
inline auto composeWithLeadingCRC(const Container& cont) noexcept
{
    class AppSharedMarshaller
    {
    public:
        class ContainerWrapper
        {
            std::uint64_t crc_{};

        public:
            Container container{};

            ContainerWrapper() = default;

            explicit ContainerWrapper(const Container& c) : container(c)
            {
                hash::CRC64WE crc_computer;
                crc_computer.update(reinterpret_cast<const std::uint8_t*>(&container), sizeof(container));
                crc_ = crc_computer.get();
            }
        };

        explicit AppSharedMarshaller(void* loc) : location_(loc) {}

        void write(const Container& cont)
        {
            ContainerWrapper wrapper(cont);
            std::memmove(location_, &wrapper, sizeof(wrapper));
        }

    private:
        void* location_;
    };

    std::array<std::uint8_t, sizeof(typename AppSharedMarshaller::ContainerWrapper)> buffer{};
    AppSharedMarshaller                                                              marshaller(buffer.data());
    marshaller.write(cont);
    return buffer;
}

struct LegacyV02 final
{
    [[maybe_unused]] std::uint32_t         reserved_a               = 0;
    [[maybe_unused]] std::uint32_t         reserved_b               = 0;
    [[maybe_unused]] std::uint32_t         can_bus_speed            = 0;
    [[maybe_unused]] std::uint8_t          uavcan_node_id           = 0;
    [[maybe_unused]] std::uint8_t          uavcan_fw_server_node_id = 0;
    [[maybe_unused]] std::array<char, 201> uavcan_file_name{};
    [[maybe_unused]] bool                  stay_in_bootloader = true;
    [[maybe_unused]] std::uint64_t         reserved_c         = 0;
    [[maybe_unused]] std::uint64_t         reserved_d         = 0;
};

inline std::ostream& operator<<(std::ostream& os, const LegacyV02& obj)
{
    const auto f = os.flags();
    os << std::boolalpha;
    os << "can_bus_speed: " << obj.can_bus_speed << '\n';
    os << "uavcan_node_id: " << static_cast<std::int64_t>(obj.uavcan_node_id) << '\n';
    os << "uavcan_fw_server_node_id: " << static_cast<std::int64_t>(obj.uavcan_fw_server_node_id) << '\n';
    os << "uavcan_file_name: {" << std::hex;
    for (const auto c : obj.uavcan_file_name)
    {
        os.width(2);
        os.fill('0');
        os << (static_cast<std::uint16_t>(c) & 0xFFU) << ',';
    }
    os << "}\n";
    os << "stay_in_bootloader: " << obj.stay_in_bootloader << '\n';
    os << std::flush;
    os.flags(f);
    return os;
}

}  // namespace app_shared
