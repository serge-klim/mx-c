#include "rmaxx/rmaxx_utils.hpp"
#include "rmaxx/tx_channel.hpp"
#include "rmaxx/device.hpp"

#include "utils/nic_addresses.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>
#include <thread>
#include <latch>

namespace {

struct fixture
{
	//fixture() {
 //     bool loaded = true;
 //     for (auto retry = 0; retry != 5; ++retry) {
 //        HMODULE h;
 //        loaded = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "rivermax.dll", &h) == TRUE;
 //        if (!loaded)
 //           break;
 //        rmax_cleanup();
 //        FreeLibrary(h);
 //        CoFreeUnusedLibrariesEx(0, 0);
 //        std::this_thread::sleep_for(std::chrono::microseconds{500} * retry);
 //     }
 //     bool rixermax_loaded = true;
 //  }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(rivermax_rio_suite, fixture)

BOOST_AUTO_TEST_CASE(rmax_out_create_gen_stream_test)
{
   [[maybe_unused]] auto sentry = rmaxx::initialize();

   auto out_dev_addr = sockaddr_in{0};
   out_dev_addr.sin_family = AF_INET;
   auto addrs = utils::net::get_nic_addresses<in_addr>("Wi-Fi");
   BOOST_REQUIRE(!addrs.empty()); //  throw std::runtime_error{"can't find adapter \"" + str + "\" address"};
   out_dev_addr.sin_addr = addrs.front();
   //auto device = rmaxx::device{addrs.front(), std::move(huge_region), params.size, params.size};

   sockaddr_in mcast_out_addr{0};
   mcast_out_addr.sin_family = AF_INET;
   mcast_out_addr.sin_port = htons(30001);
   mcast_out_addr.sin_addr.s_addr = inet_addr("127.0.0.1");


   rmax_out_gen_stream_params out_gen_stream_params;
   out_gen_stream_params.local_addr = reinterpret_cast<struct sockaddr*>(&out_dev_addr);
   out_gen_stream_params.max_iovec_num = 4;  ////// max_payload_size > page_size ? utils::size_in_blocks(max_payload_size, expected_payload_size) : 1 /*(max_payload_size + mem_block_info.page_size - 1) / mem_block_info.page_size*/;
   out_gen_stream_params.max_chunk_size = 2; // number of packets in chunk
   out_gen_stream_params.size_in_chunks = 0;
   out_gen_stream_params.opt_field_mask = 0;
   out_gen_stream_params.remote_addr = reinterpret_cast<struct sockaddr*>(&mcast_out_addr);
   out_gen_stream_params.flags = RMAX_OUT_STREAM_REM_ADDR;

   for (auto&& _ : std::ranges::iota_view{0, 10}) {
       rmax_stream_id stream_id;
	   BOOST_CHECK_EQUAL(rmax_out_create_gen_stream(&out_gen_stream_params, &stream_id), RMAX_OK);
	   BOOST_CHECK_EQUAL(rmax_out_destroy_stream(stream_id), RMAX_OK);
       BOOST_CHECK_EQUAL(stream_id, 0);
   }

   auto ids_gen = 
   std::ranges::iota_view{0, 3} 
	   | std::views::transform([&](auto) {
			rmax_stream_id stream_id;
			BOOST_CHECK_EQUAL(rmax_out_create_gen_stream(&out_gen_stream_params, &stream_id), RMAX_OK);
			return stream_id;
       }) 
       // | std::ranges::to<std::vector>()
	   ;

   auto ids = std::vector<rmax_stream_id>{std::ranges::begin(ids_gen), std::ranges::end(ids_gen)};
   BOOST_CHECK_EQUAL(ids.size(), 3);
   {
       rmax_stream_id stream_id;
       BOOST_CHECK_EQUAL(rmax_out_create_gen_stream(&out_gen_stream_params, &stream_id), RMAX_ERR_EXCEEDS_LIMIT);
       BOOST_CHECK_EQUAL(rmax_out_create_gen_stream(&out_gen_stream_params, &stream_id), RMAX_ERR_EXCEEDS_LIMIT);
   }

   BOOST_CHECK(std::ranges::all_of(
	ids 
	| std::views::transform(&rmax_out_destroy_stream) 
       , std::bind(std::ranges::equal_to{}, RMAX_OK, std::placeholders::_1)
	));

 //  //std::ranges::equal_to
}

//BOOST_AUTO_TEST_CASE(rmax_send_test)
//{
//   [[maybe_unused]] auto sentry = rmaxx::initialize();
//
//   auto out_dev_addr = utils::net::get_nic_addresses<in_addr>("Wi-Fi").front();
//
//   auto params = utils::allocation_parameters{};
//   params.size = 1024 * 100;
//   params.type = utils::allocation_parameters::allocation_type::relaxed;
//   auto huge_region = utils::alloc_huge_region(params);
//   auto device = rmaxx::device{out_dev_addr, std::move(huge_region), params.size, params.size};
//
//
//   sockaddr_in mcast_out_addr{0};
//   mcast_out_addr.sin_family = AF_INET;
//   mcast_out_addr.sin_port = htons(30001);
//   mcast_out_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//   auto tx = rmaxx::tx_channel{device, mcast_out_addr, 7};
//
//   static auto constexpr queue_size = 15;
//   static auto constexpr packet_size = 1024;
//   auto data = std::vector<char>(packet_size * queue_size);
//   std::iota(begin(data), end(data), 1);
//
//   auto tx_queue = rmaxx::tx_queue(device, queue_size);
//   tx_queue.push_back(data.data(), packet_size);
//   tx.commit(tx_queue);
//}

BOOST_AUTO_TEST_CASE(rmax_send_ex_test)
{
   static const auto multicast_address = "239.255.0.1";
   static auto constexpr multicast_port = 3001;
   static auto constexpr queue_size = 15;
   static auto constexpr packet_size = 1024;
   static auto constexpr bytes_to_transfer = packet_size * queue_size * 3 - packet_size * 2;

   boost::asio::io_context ioctx;
   auto source_address = boost::asio::ip::make_address(multicast_address);
   auto endpoint = boost::asio::ip::udp::endpoint{source_address, multicast_port};
   auto socket_in = boost::asio::ip::udp::socket{ioctx, endpoint.protocol()};
   socket_in.bind(boost::asio::ip::udp::endpoint{boost::asio::ip::address_v4::any(), endpoint.port()});
   auto dev_addr = utils::net::get_nic_addresses<in_addr>("Wi-Fi").front(); // for multihost boxes
   //if (auto nic = options["if-in"]; nic.empty())
       socket_in.set_option(
            //boost::asio::ip::multicast::join_group{source_address}                
            boost::asio::ip::multicast::join_group{source_address.to_v4(), boost::asio::ip::address_v4{ntohl(dev_addr.s_addr)}}
       );
   //else
   // socket_in.set_option(
   //     boost::asio::ip::multicast::join_group{
   //      source_address.to_v4(), nic.as < boost::asio::ip::add

    auto ready = std::latch{1};
    auto coro_receiver = [&](boost::asio::ip::udp::socket in, std::size_t n) -> boost::asio::awaitable<std::size_t> {
        ready.count_down();
        auto data = std::array<char, packet_size>{};
        auto source = boost::asio::ip::udp::endpoint{};
        while( n != 0 ) {
           auto received = co_await in.async_receive_from(boost::asio::buffer(data.data() /*+ n*/, data.size() /*- n*/), source, boost::asio::use_awaitable);
           BOOST_CHECK_LE(received, n);
           n -= received;
        }
        co_return 0;
    };

    boost::asio::co_spawn(ioctx, coro_receiver(std::move(socket_in), bytes_to_transfer), boost::asio::detached);
    auto receiver = std::thread{[&ioctx] {
       while (!ioctx.stopped())
         (ioctx.run)();
    }};
    ready.wait();
    std::this_thread::sleep_for(std::chrono::seconds{1});

    [[maybe_unused]] auto sentry = rmaxx::initialize();

    auto params = utils::allocation_parameters{};
    params.size = 1024 * 100;
    params.type = utils::allocation_parameters::allocation_type::relaxed;
    auto huge_region = utils::alloc_huge_region(params);
    auto device = rmaxx::device{dev_addr, std::move(huge_region), params.size, params.size};

    sockaddr_in mcast_out_addr{0};
    mcast_out_addr.sin_family = AF_INET;
    mcast_out_addr.sin_port = htons(multicast_port);
    //mcast_out_addr.sin_addr.s_addr = inet_addr(multicast_address);
    int rc = inet_pton(AF_INET, multicast_address, &mcast_out_addr.sin_addr);
    BOOST_REQUIRE_MESSAGE(rc == 1, "Failed to parse destination network address");

    auto tx = rmaxx::tx_channel{device, mcast_out_addr, 7};

    auto data = std::vector<char>(queue_size * packet_size);
    std::iota(begin(data), end(data), 1);

    auto tx_queue = rmaxx::tx_queue(device, queue_size);
    
    assert(bytes_to_transfer % packet_size == 0);
    auto transfered = 0;
    do {
       auto queued = 0;
       for (auto offset = 0; tx_queue.push_back(data.data() + offset, packet_size); offset = (offset + packet_size) % (data.size() - packet_size)) {
          queued += packet_size;
       }
       BOOST_CHECK_EQUAL(tx.commit(tx_queue), RMAX_OK);
       transfered += queued;
    } while (bytes_to_transfer > transfered);
    receiver.join();
}


BOOST_AUTO_TEST_CASE(rmax_concurrent_ch_send_test)
{
    static const auto multicast_address = "239.255.0.1";
    static auto constexpr multicast_port = 3001;
    static auto constexpr queue_size = 15;
    static auto constexpr packet_size = 1024;
    static auto constexpr bytes_to_transfer = packet_size * queue_size * 10 - packet_size * 2;

    unsigned int api_major;
    unsigned int api_minor;
    unsigned int release;
    unsigned int build;

    /* Verify version mismatch */
    rmax_get_version(&api_major, &api_minor, &release, &build);

    boost::asio::io_context ioctx;
    auto source_address = boost::asio::ip::make_address(multicast_address);
    auto endpoint = boost::asio::ip::udp::endpoint{source_address, multicast_port};
    auto socket_in = boost::asio::ip::udp::socket{ioctx, endpoint.protocol()};
    socket_in.bind(boost::asio::ip::udp::endpoint{boost::asio::ip::address_v4::any(), endpoint.port()});
    auto dev_addr = utils::net::get_nic_addresses<in_addr>("Wi-Fi").front(); // for multihost boxes
                                                                             // if (auto nic = options["if-in"]; nic.empty())
    socket_in.set_option(
        // boost::asio::ip::multicast::join_group{source_address}
        boost::asio::ip::multicast::join_group{source_address.to_v4(), boost::asio::ip::address_v4{ntohl(dev_addr.s_addr)}});
    // else
    //  socket_in.set_option(
    //      boost::asio::ip::multicast::join_group{
    //       source_address.to_v4(), nic.as < boost::asio::ip::add

    auto ready = std::latch{1};
    auto coro_receiver = [&](boost::asio::ip::udp::socket in, std::size_t n) -> boost::asio::awaitable<std::size_t> {
       ready.count_down();
       auto data = std::array<char, packet_size>{};
       auto source = boost::asio::ip::udp::endpoint{};
       while (n != 0) {
          BOOST_TEST_MESSAGE("waiting for " << n << " bytes more...");
          auto received = co_await in.async_receive_from(boost::asio::buffer(data.data() /*+ n*/, data.size() /*- n*/), source, boost::asio::use_awaitable);
          BOOST_CHECK_LE(received, n);
          n -= received;
       }
       co_return 0;
    };

    boost::asio::co_spawn(ioctx, coro_receiver(std::move(socket_in), bytes_to_transfer), boost::asio::detached);
    auto receiver = std::thread{[&ioctx] {
       while (!ioctx.stopped())
          (ioctx.run)();
    }};
    ready.wait();
    std::this_thread::sleep_for(std::chrono::seconds{1});

    [[maybe_unused]] auto sentry = rmaxx::initialize();

    auto params = utils::allocation_parameters{};
    params.size = 1024 * 100;
    params.type = utils::allocation_parameters::allocation_type::relaxed;
    auto huge_region = utils::alloc_huge_region(params);
    auto device = rmaxx::device{dev_addr, std::move(huge_region), params.size, params.size};

    sockaddr_in mcast_out_addr{0};
    mcast_out_addr.sin_family = AF_INET;
    mcast_out_addr.sin_port = htons(multicast_port);
    // mcast_out_addr.sin_addr.s_addr = inet_addr(multicast_address);
    int rc = inet_pton(AF_INET, multicast_address, &mcast_out_addr.sin_addr);
    BOOST_REQUIRE_MESSAGE(rc == 1, "Failed to parse destination network address");

    auto data = std::vector<char>(queue_size * packet_size);
    std::iota(begin(data), end(data), 1);

    auto tx = rmaxx::concurrent::tx_channel{device, mcast_out_addr, queue_size};

    assert(bytes_to_transfer % packet_size == 0);
    auto transfered = 0;
    for (auto offset = 0; bytes_to_transfer > transfered; offset = (offset + packet_size) % (data.size() - packet_size)) {
       BOOST_CHECK_EQUAL(tx.send(data.data() + offset, packet_size), S_OK);
       transfered += packet_size;
    }
    BOOST_TEST_MESSAGE(transfered << " bytes has been transfered...");
    receiver.join();
}

BOOST_AUTO_TEST_SUITE_END()
