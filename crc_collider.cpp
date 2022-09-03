// Copyright (c) 2022  Zubax Robotics  <info@zubax.com>

#include "hash.hpp"
#include <random>
#include <thread>
#include <vector>
#include <cstring>
#include <numeric>
#include <optional>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <syncstream>
#include <functional>

#define DEBUG 0

#define REQUIRE(x)                 \
    do                             \
    {                              \
        if (!static_cast<bool>(x)) \
        {                          \
            std::abort();          \
        }                          \
    } while (false)

namespace crc_collider
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
            CRC64WE crc_computer;
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
                CRC64WE crc_computer;
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

struct LegacyAppSharedV02 final
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

// operator<< overload for LegacyAppSharedV02
std::ostream& operator<<(std::ostream& os, const LegacyAppSharedV02& obj)
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

void worker(const LegacyAppSharedV02& seed, const std::function<void(std::uint64_t)>& progress_reporter) noexcept
{
    auto obj = seed;
    {
        std::osyncstream os(std::cerr);
        os << "Initial seed for thread " << std::this_thread::get_id() << ":\n" << obj << std::endl;
    }
    constexpr std::uint64_t NotifierPeriod = 1000000;
    auto* const             nonce_ptr_unaligned =
        reinterpret_cast<std::uint8_t*>(obj.uavcan_file_name.data() + obj.uavcan_file_name.size() - 8 - 1);
    auto* const nonce_ptr = reinterpret_cast<std::uint64_t*>(
        nonce_ptr_unaligned - (reinterpret_cast<std::uintptr_t>(nonce_ptr_unaligned) % alignof(std::uint64_t)));
    REQUIRE(reinterpret_cast<std::size_t>(nonce_ptr) % alignof(std::uint64_t) == 0);
    for (std::uint64_t i = 0; i < std::numeric_limits<std::uint64_t>::max(); i++)
    {
        (*nonce_ptr)++;
        const auto buffer = composeWithLeadingCRC(obj);
        if (const auto parsed = parseWithTrailingCRC<LegacyAppSharedV02>(buffer.data()))
        {
            std::osyncstream os(std::cout);
            [[unlikely]] os << "SOLUTION:\n" << obj << std::endl;
        }
        if (0 == (i % NotifierPeriod))
        {
            [[unlikely]] progress_reporter(i);
        }
#if DEBUG
        {
            std::osyncstream os(std::cerr);
            os << obj;
        }
#endif
    }
}

void testCRC64WE()
{
    CRC64WE     crc;
    const char* val = "12345";
    crc.update(reinterpret_cast<const std::uint8_t*>(val), 5);
    crc.update(nullptr, 0);
    val = "6789";
    crc.update(reinterpret_cast<const std::uint8_t*>(val), 4);
    REQUIRE(0x62EC'59E3'F1A4'F00AULL == crc.get());
    REQUIRE(crc.getBytes().at(0) == 0x62U);
    REQUIRE(crc.getBytes().at(1) == 0xECU);
    REQUIRE(crc.getBytes().at(2) == 0x59U);
    REQUIRE(crc.getBytes().at(3) == 0xE3U);
    REQUIRE(crc.getBytes().at(4) == 0xF1U);
    REQUIRE(crc.getBytes().at(5) == 0xA4U);
    REQUIRE(crc.getBytes().at(6) == 0xF0U);
    REQUIRE(crc.getBytes().at(7) == 0x0AU);
    REQUIRE(!crc.isResidueCorrect());
    crc.update(crc.getBytes().data(), 8);
    REQUIRE(crc.isResidueCorrect());
    REQUIRE(0xFCAC'BEBD'5931'A992ULL == (~crc.get()));
}

}  // namespace crc_collider

int main()
{
    crc_collider::testCRC64WE();
    crc_collider::LegacyAppSharedV02 obj{
        .can_bus_speed            = 1000000,
        .uavcan_node_id           = 50,
        .uavcan_fw_server_node_id = 127,
        .uavcan_file_name         = {},
        .stay_in_bootloader       = true,
    };
    std::random_device                          rnd_device;
    std::mt19937                                mersenne_engine{rnd_device()};
    std::uniform_int_distribution<std::uint8_t> dist_ascii{0x20, 0x7E};  // Use only printable chars for simplicity.
    static const auto                           thread_count =
#if DEBUG
        1U;
#else
        static_cast<std::uint32_t>(std::max(1, static_cast<std::int32_t>(std::thread::hardware_concurrency()) - 2));
#endif
    std::cout << "Thread count: " << thread_count << std::endl;
    std::mutex                 progress_mutex;
    std::vector<std::uint64_t> thread_hash_counters(thread_count, 0);
    const auto                 progress_reporter = [&](const std::size_t thread_index, const std::uint64_t hash_count)
    {
        std::lock_guard lock(progress_mutex);
        thread_hash_counters.at(thread_index) = hash_count;
    };
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::uint32_t i = 0; i < thread_count; i++)
    {
        obj.uavcan_fw_server_node_id = static_cast<std::uint8_t>(127 - i);
        std::generate(obj.uavcan_file_name.begin(),
                      obj.uavcan_file_name.end() - sizeof(std::uint64_t) - 1U,
                      [&dist_ascii, &mersenne_engine]() { return dist_ascii(mersenne_engine); });
        obj.uavcan_file_name.back() = 0;
        threads.emplace_back(crc_collider::worker,
                             obj,
                             [i, &progress_reporter](const std::uint64_t hash_count)
                             { progress_reporter(i, hash_count); });
    }
    const auto started_at = std::chrono::steady_clock::now();
    while (true)  // NOLINT
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        const auto    elapsed          = std::chrono::steady_clock::now() - started_at;
        std::uint64_t total_hash_count = 0;
        {
            std::lock_guard lock(progress_mutex);
            total_hash_count = std::accumulate(thread_hash_counters.begin(), thread_hash_counters.end(), 0ULL);
        }
        const auto hash_rate = static_cast<double>(total_hash_count) /
                               std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
        std::osyncstream os(std::cerr);
        os << '\r'  //
           << "Elapsed " << std::chrono::duration_cast<std::chrono::minutes>(elapsed).count() << " minutes; "
           << "hash count " << (static_cast<double>(total_hash_count) * 1e-6) << " M; "
           << "hash rate " << (hash_rate * 1e-6) << " MH/s"  //
           << '\r' << std::flush;
#if DEBUG
        std::quick_exit(0);
#endif
    }
    return 0;
}
