// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#include "app_shared.hpp"
#include <iostream>

int main()
{
    app_shared::LegacyV02 obj{
        .can_bus_speed            = 1000000,
        .uavcan_node_id           = 50,
        .uavcan_fw_server_node_id = 127,
        .uavcan_file_name         = {},
        .stay_in_bootloader       = true,
    };
    const std::array<std::uint8_t, 8> fixed_point_name{0xA2, 0x35, 0xD2, 0x12, 0xAB, 0x45, 0x27, 0x02};
    std::memcpy(obj.uavcan_file_name.data(), fixed_point_name.data(), fixed_point_name.size());
    const auto buffer = composeWithLeadingCRC(obj);
    std::cout.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return 0;
}
