#define BOOST_TEST_MODULE rmaxx_tests
#include "rmaxx_utils.hpp"
#include "rmaxx/device.hpp"
#include "rmaxx/tx_channel.hpp"
#include "circular_memory.hpp"
#include "utils/sysinfo.hpp"
#include "utils/memory.hpp"
#include "utils/huge_pages.hpp"
#include <boost/winapi/get_last_error.hpp>
#include <boost/system/system_error.hpp>
#include <boost/test/data/test_case.hpp>
//#include <boost/test/data/monomorphic.hpp>
#include "winsock2.h"
#include <thread>
#include <mutex>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cassert>

#include <boost/test/unit_test.hpp>

/*consteval*/ constexpr auto max_payload_size() noexcept { return std::size_t{1408}; }

void alloc(std::size_t packets_n_hint = 16) {
   assert(packets_n_hint != 0);
//  throw std::runtime_error{"can't make layout, not enough space in page to allocate layout"};

   auto const page_size = utils::page_size();
   auto const cache_line_size = utils::cache_line_size();
   assert(page_size % cache_line_size == 0 && "hmm! very interesting CPU, code below should be updated!!!");
   auto const rmax_packets_offset = utils::size_in_blocks(sizeof(rmax_chunk), cache_line_size) * cache_line_size;
   auto const rmax_firtst_iov_offset = utils::size_in_blocks(rmax_packets_offset + sizeof(rmax_packet[1]) * packets_n_hint, page_size) * page_size;
   assert(rmax_firtst_iov_offset % page_size == 0);
   assert(rmax_firtst_iov_offset % cache_line_size == 0);
   auto const io_data_offset = utils::size_in_blocks(sizeof(rmax_iov), cache_line_size) * cache_line_size;
   assert(io_data_offset + max_payload_size() <= page_size && "TODO:add logic to cover this case");
   auto const ios_n_per_page = page_size/(io_data_offset + max_payload_size());
   auto const next_io_offset = page_size / ios_n_per_page;
   assert(next_io_offset < page_size);
   assert(next_io_offset % cache_line_size == 0);
   //auto max_packets_n = (rmax_firtst_iov_offset - rmax_packets_offset) / sizeof(rmax_packet[1]);

   auto arena_size = io_data_offset + (next_io_offset * packets_n_hint);


   auto required_mem_size = std::size_t{0};
   auto mem_region = utils::huge_region{};
   if (auto const huge_page_size = utils::huge_page_size()) {
      required_mem_size = utils::size_in_blocks(arena_size, huge_page_size) * huge_page_size;
      mem_region = utils::alloc_huge_pages(required_mem_size);
   }
   if (!mem_region) {
      required_mem_size = utils::size_in_blocks(arena_size, page_size) * page_size;
      mem_region = utils::alloc_mem(required_mem_size);
      if (!mem_region) {         
         // throw std::bad_alloc{std::to_string(mem_region_size) + "bytes allocation failed" };
         throw boost::system::system_error(boost::winapi::GetLastError(), boost::system::system_category(), std::to_string(required_mem_size) + "bytes allocation failed");
      }
   }
   auto const allocated_iov_n = (std::min)((required_mem_size - rmax_firtst_iov_offset) / next_io_offset, (rmax_firtst_iov_offset - io_data_offset) / sizeof(rmax_packet[1]));
   assert(allocated_iov_n != 0);
   auto begin = mem_region.get();
   auto chunk = static_cast<rmax_chunk*>(begin);
   chunk->packets = reinterpret_cast<decltype(chunk->packets)>(static_cast<char*>(begin) + rmax_packets_offset);
   chunk->size = 0;
   for (auto i = decltype(allocated_iov_n){0}; i != allocated_iov_n; ++i/*, next_iov += next_packet_offset*/) {
      chunk->packets[i].iovec = reinterpret_cast<rmax_iov*>(static_cast<char*>(begin) + rmax_firtst_iov_offset + next_io_offset * i);
      chunk->packets[i].count = 1;
      chunk->packets[i].iovec->addr = reinterpret_cast<decltype(chunk->packets[i].iovec->addr)>(chunk->packets[i].iovec) + io_data_offset;
      chunk->packets[i].iovec->length = 0;
      chunk->packets[i].iovec->mid = 0;
    }
///////////////////////////some sanity checks////////////////////////////////////////////////
    assert(std::is_sorted(chunk->packets, chunk->packets + allocated_iov_n, [](auto const& l, auto const& r) {
       return l.iovec < r.iovec;
    }));
    assert(reinterpret_cast<char const*>(chunk->packets[allocated_iov_n].iovec) < static_cast<char const*>(begin) + required_mem_size);

   //{std::move(mem_region), required_mem_size, allocated_iov_n};
}


BOOST_AUTO_TEST_SUITE(rmax_utils_test_suite)

BOOST_AUTO_TEST_CASE(mem_mgr_test)
{
   alloc();  
   alloc(1024);  



   //assert(page_size != 0);
   //const auto packet_size_in_pages = utils::size_in_blocks(max_payload_size, page_size);
   //auto mem_region_size_in_pages = packet_size_in_pages * max_packets * page_size;
   //auto mem_region_size = mem_region_size_in_pages * page_size;
   //auto const huge_page_size = utils::huge_page_size();
   //if (huge_page_size != 0) {
   //   assert(huge_page_size >= page_size);
   //   auto const pages_in_huge_page = huge_page_size / page_size;
   //   mem_region_size = utils::size_in_blocks(mem_region_size_in_pages, pages_in_huge_page) * huge_page_size;
   //}
}



BOOST_AUTO_TEST_CASE(basic_buffer_test)
{
   constexpr auto base_packet_size = 1024;
   auto params = utils::allocation_parameters{{}, 1024 * 10, utils::allocation_parameters::allocation_type::relaxed, 0};
   auto mem = utils::alloc_huge_region(params);
   BOOST_REQUIRE_NE(params.size, 0);
   auto buf = rmaxx::circular_memory{0, static_cast<char*>(mem.get()), static_cast<rmaxx::circular_memory::size_type>(params.size)};
   BOOST_CHECK(buf.empty());
   BOOST_CHECK_EQUAL(buf.free(), buf.size());
   BOOST_CHECK_EQUAL(buf.allocate_iov(params.size + 1), nullptr);
   auto const mem1_size = params.size / 2;
   auto mem1 = buf.allocate_iov(mem1_size);
   auto mem1_raw_size = buf.used_memory(mem1);
   BOOST_REQUIRE_LE(mem1_size + sizeof(rmax_iov), mem1_raw_size);
   BOOST_CHECK_EQUAL(std::prev(reinterpret_cast<char const*>(mem1), mem1_raw_size - mem1_size - sizeof(rmax_iov)), static_cast<char const*>(mem.get()));
   BOOST_CHECK_EQUAL(mem1->length, mem1_size);
   BOOST_CHECK(!buf.empty());
   auto packets = std::vector<rmax_packet>{};
   while (auto iovec = buf.allocate_iov(base_packet_size))
      packets.emplace_back(rmax_packet{iovec, 1});
   BOOST_CHECK(!packets.empty());
   auto size = packets.size();
   buf.release(mem1);
   while (auto iovec = buf.allocate_iov(base_packet_size))
      packets.emplace_back(rmax_packet{iovec, 1});
   BOOST_CHECK_NE(size, packets.size());
   for (auto const& packet : packets)
      buf.release(packet.iovec, packet.count);
}

BOOST_AUTO_TEST_CASE(buffer_test)
{
   constexpr auto base_packet_size = 1024;
   auto params = utils::allocation_parameters{{}, 1024 * 10, utils::allocation_parameters::allocation_type::relaxed, 0};
   auto mem = utils::alloc_huge_region(params);
   BOOST_REQUIRE_NE(params.size, 0);
   auto buf = rmaxx::circular_memory{0, static_cast<char*>(mem.get()), static_cast<rmaxx::circular_memory::size_type>(params.size)};
   BOOST_CHECK_EQUAL(buf.free(), buf.size());
   BOOST_CHECK_EQUAL(buf.allocate_iov(params.size + 1), nullptr);

   auto fill_iovec = [](rmax_iov* iov, char v = 0xdc) noexcept {
      std::fill_n(reinterpret_cast<char*>(iov->addr), iov->length, v);
   };

   BOOST_CHECK(buf.empty());
   auto vars = std::vector<std::size_t>{15, 1021,23, 707, 331, 800, 11, 13, 111};

   for (auto i = 0; i != 5; ++i) {
      auto free = buf.free();
      BOOST_CHECK_EQUAL(free, buf.size());
      auto packets = std::vector<rmax_packet>{};

      for (auto i = std::size_t{0}; auto iovec = buf.allocate_iov(base_packet_size + vars[i % vars.size()]); ++i) {
         BOOST_CHECK_EQUAL(iovec->length, base_packet_size + vars[i % vars.size()]);
         BOOST_REQUIRE_LE(iovec->length, buf.used_memory(iovec));
         free -= buf.used_memory(iovec);
         BOOST_CHECK_EQUAL(free, buf.free());
         packets.emplace_back(rmax_packet{iovec, 1});
      }

      for (auto const& packet : packets) {
         BOOST_REQUIRE_EQUAL(packet.count, 1);
         BOOST_REQUIRE_LE(packet.iovec->length, buf.used_memory(packet.iovec));
         fill_iovec(packet.iovec);
      }

      BOOST_CHECK(!buf.empty());
      for (auto const& packet : packets) {
         buf.release(packet.iovec, packet.count);
      }
      BOOST_CHECK(buf.empty());
   }
}

int const packets[] = {100, 1000, 5000};
int const threads[] = {1, 3, 4};

BOOST_DATA_TEST_CASE(tx_queue_test, boost::unit_test::data::make(packets) ^ threads, packets_n, threads_n)
{
   auto params = utils::allocation_parameters{};
   params.size = 1024 * 100;
   params.type = utils::allocation_parameters::allocation_type::relaxed;
   auto huge_region = utils::alloc_huge_region(params);

   struct in_addr out_dev_addr;
   out_dev_addr.s_addr = INADDR_ANY;
// BOOST_REQUIRE_EQUAL(inet_pton(AF_INET, "127.0.0.1", &out_dev_addr), 1);
   auto device = rmaxx::device{out_dev_addr, std::move(huge_region), params.size, params.size};

   auto const packet_size = std::size_t{3 * 1024};
   
   //std::iota(begin(data), end(data), 1);
   auto id = std::uint64_t{0};
   auto tx_queue_guard = std::mutex{}; 
   auto tx_queue = rmaxx::tx_queue(device, 13);
   auto send = [&id, &tx_queue, &tx_queue_guard](std::size_t n) {
      auto data = std::vector<std::uint64_t>(packet_size / sizeof(std::uint64_t));
      while (n != 0) {
         auto lock = std::lock_guard{tx_queue_guard};
         data.front() = data.back() = ++id;
         if (tx_queue.push_back(data.data(), data.size()*sizeof(typename decltype(data)::value_type)))
            --n;
      }
   };

   auto threads = std::vector<std::thread>{};
   threads.reserve(threads_n-1);
   auto const packets_n_per_tread = packets_n / threads_n;
   for (auto i = decltype(threads_n){0}; i != threads_n; ++i)
      threads.emplace_back(send, packets_n_per_tread);

   auto id_check = decltype(id){0};
   auto packets_processed = decltype(packets_n_per_tread){0};
   while (packets_processed < packets_n_per_tread * threads_n) {
      auto const& chunk = tx_queue.front();
      if (auto n = chunk.size) {
         for (auto i = decltype(n){0}; i != n; ++i) {
            BOOST_CHECK_EQUAL(chunk.packets[i].count, 1);
            auto addr = reinterpret_cast<std::uint64_t const*>(chunk.packets[i].iovec[0].addr);
            BOOST_CHECK_GT(*addr, id_check);
            id_check = *addr;
         }
         packets_processed += n;
         tx_queue.pop();
      }
   }

   std::for_each(begin(threads), end(threads), std::bind(&std::thread::join, std::placeholders::_1));
   BOOST_CHECK_EQUAL(packets_processed, packets_n_per_tread * threads_n);
   BOOST_CHECK(tx_queue.empty());
}

BOOST_AUTO_TEST_SUITE_END()
