#include "stats.h"
#include "rmaxx/rx_channel.hpp"
#include "rmaxx/tx_channel.hpp"
#include "rmaxx/device.hpp"
#include "rmaxx/rmaxx_utils.hpp"

#include "utils/nic_addresses.hpp"
#include "utils/program_options/net.hpp"
#include "utils/thread.hpp"
#include "utils/sysinfo.hpp"
//#include "utils/win/experemental/sysinfo.hpp"
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

//#include <ranges>
#include <thread>
#include <atomic>
#include <limits>
#include <csignal>
#include <cstdint>
#include <cassert>

namespace {

struct in_addr nic_address(std::string const& /*boost::asio::ip::address*/ str)
{
   struct in_addr res;
   if (inet_pton(AF_INET, str.c_str(), &res) == 1)
      return res;
   auto addrs = utils::net::get_nic_addresses<in_addr>(str);
   if (addrs.empty())
      throw std::runtime_error{"can't find adapter \"" + str + "\" address"};
   return addrs.front();
   // switch (addrs.size())
   //{
   //    case 0:
   //       throw std::runtime_error{"can't find adapter \"" + str + "\" address"};
   //    case 1:
   //       return addrs.front();
   //    default:
   //       throw std::runtime_error{"more than one adapter \"" + str + "\" address found"};
   // }
}
} // namespace

namespace {
auto& stop_token() noexcept {
   static auto stop = std::atomic<bool>{false};
   return stop;
}

}

void echo(boost::program_options::variables_map const& options, rmaxx::rx_channel& rx, rmaxx::device& dev_out) {
   auto destination = options["destination"].as<net::endpoint>();
   auto destination_address = boost::asio::ip::make_address(destination.host);
   sockaddr_in mcast_out_addr{0};
   mcast_out_addr.sin_family = AF_INET;
   mcast_out_addr.sin_port = htons(destination.port);
   mcast_out_addr.sin_addr.s_addr = htonl(destination_address.to_v4().to_uint());
   //auto tx = tx_channel{out_dev_addr, mcast_out_addr};
   const auto max_chunk_size = std::size_t{1024};
   auto tx = rmaxx::tx_channel{dev_out, mcast_out_addr, max_chunk_size}; // TODO: max_packets per chunk
   auto tx_queue = rmaxx::tx_queue(dev_out, max_chunk_size);

   auto source = options["source"].as<net::endpoint>();
   sockaddr_in mcast;
   mcast.sin_family = AF_INET;
   mcast.sin_addr.s_addr = htonl(boost::asio::ip::make_address(source.host).to_v4().to_ulong());
   mcast.sin_port = htons(source.port);
   rx.join(mcast);
   auto n = options["packets"].as<std::size_t>();
   if (!utils::set_thread_priority(THREAD_PRIORITY_TIME_CRITICAL - 1)) {
      auto error = boost::winapi::GetLastError();
      BOOST_LOG_TRIVIAL(warning) << "unable set receiving thread priority : " << boost::system::error_code(error, boost::system::system_category()).message();
   }
   auto& stop = stop_token();
   auto send_thread = std::thread{[&tx,&tx_queue,&stop] {
      try {
         if (!utils::set_thread_priority(THREAD_PRIORITY_TIME_CRITICAL - 1)) {
            auto error = boost::winapi::GetLastError();
            BOOST_LOG_TRIVIAL(warning) << "unable set sending thread priority : " << boost::system::error_code(error, boost::system::system_category()).message();
         }
         while (!stop) {
            if (tx_queue.empty()) {
               //std::this_thread::yield();
               continue;
            }            
            switch (auto status = tx.commit(tx_queue)) {
               case RMAX_OK:
               case RMAX_ERR_HW_SEND_QUEUE_FULL:
                  break;
               default:
                  throw std::runtime_error{std::string{"rmax_out_commit_chunk failed : "} + rmaxx::to_string(status)};
            }
         }
      } catch (std::exception& e) {
         BOOST_LOG_TRIVIAL(error) << "send thread, error : " << e.what();
      } catch (...) {
         BOOST_LOG_TRIVIAL(error) << "send thread, miserably failed:(";
      }
      stop = true;
      BOOST_LOG_TRIVIAL(info) << "exiting send loop...";
   }};
   {
      auto rx_statistics = rx_stats{n};
      auto tx_statistics = tx_stats{};
      rmax_in_completion completion;
      while (rx.get_next_chunk(completion, 1) && n-- != 0 && !stop) {
         for (auto i = decltype(completion.chunk_size){0}; i != completion.chunk_size; ++i) {
            auto header = static_cast<char const*>(completion.data_ptr) + i * rx.stride_size();
            auto payload = header + completion.packet_info_arr[i].hdr_size;
            auto const data_size = completion.packet_info_arr[i].data_size;
            rx_statistics.mark_processed(data_size);
            for (auto attempt = 3; !tx_queue.push_back(payload, data_size);) {
               if (attempt-- == 0)
                    throw std::runtime_error{"can't queue more data, sent queue is full"};
               tx_statistics.retry();
            }
            tx_statistics.mark_processed(data_size);
         }
      }
      BOOST_LOG_TRIVIAL(error) << "done receiving, shutting down... " ;
      stop = true;
   }
   send_thread.join();
}

void run(boost::program_options::variables_map const& options)
{
   BOOST_LOG_TRIVIAL(info)
             << "################################################\n"
             << "## Rivermax library version:    " << rmax_get_version_string() << "    ##\n"
             << "## Generic sender version:      " << RMAX_API_MAJOR << "." << RMAX_API_MINOR
             << "." << RMAX_RELEASE_VERSION << "." << RMAX_BUILD << "    ##\n"
             << "################################################\n";

   unsigned int api_major;
   unsigned int api_minor;
   unsigned int release;
   unsigned int build;

   /* Verify version mismatch */
   rmax_get_version(&api_major, &api_minor, &release, &build);
   if (api_major != RMAX_API_MAJOR || api_minor < RMAX_API_MINOR)
      throw std::runtime_error{"Incompatible Rivermax version"};

   if (api_minor > RMAX_API_MINOR || release != RMAX_RELEASE_VERSION || build != RMAX_BUILD)
      BOOST_LOG_TRIVIAL(warning) << "\nWarning - Rivermax versions are not aligned";

   auto sig_handler = [](const int /*signal*/) {
      stop_token() = true;
   };
   std::signal(SIGINT, sig_handler);
   std::signal(SIGBREAK, sig_handler);
   [[maybe_unused]] auto sentry = rmaxx::initialize();

   //in_addr in_dev_addr;
   //in_dev_addr.s_addr = htonl(options["if-in"].as<boost::asio::ip::address>().to_v4().to_ulong());
   auto in_dev_addr = nic_address(options["if-in"].as<std::string>());

   //in_addr out_dev_addr;
   //out_dev_addr.s_addr = htonl(options["if-out"].as<boost::asio::ip::address>().to_v4().to_ulong());
   auto out_dev_addr = nic_address(options["if-out"].as<std::string>());

   auto const single_device = in_dev_addr.s_addr == out_dev_addr.s_addr;
   rmax_in_memblock payload = {0};
   payload.max_size = decltype(payload.max_size)(rmaxx::max_payload_size());
   payload.min_size = payload.max_size / 4;

   auto buffer_attr = rmax_in_buffer_attr{0};
   buffer_attr.num_of_elements = (std::numeric_limits<std::uint16_t>::max)() * 2;
   buffer_attr.attr_flags = 0;
   buffer_attr.data = &payload;
   buffer_attr.hdr = nullptr;

   constexpr auto stream_type = RMAX_APP_PROTOCOL_PACKET /*RMAX_APP_PROTOCOL_PAYLOAD*/;

   auto addr = sockaddr_in{0};
   addr.sin_family = AF_INET;
   addr.sin_addr = in_dev_addr;
   std::size_t payload_size = 0;
   std::size_t header_size = 0;
   if (auto status = rmax_in_query_buffer_size(stream_type, &addr, &buffer_attr, &payload_size, &header_size); status != RMAX_OK)
      throw std::runtime_error{std::string{"rmax_in_query_buffer_size failed : "} + rmaxx::to_string(status)};
   assert(header_size == 0);

   static auto const huge_page_size = utils::huge_page_size();
   static auto const page_size = huge_page_size != 0 ? huge_page_size : utils::page_size();

   auto const per_device_size = utils ::size_in_blocks(payload_size, page_size) * page_size;

   
   auto params = utils::allocation_parameters{};
   auto numa_node = options["numa-node"];
   if (!numa_node.empty()) {
      params.numa_node = numa_node.as<unsigned long>();
      //auto cpus = utils::experemental::cpu_set()
      //                | std::views::filter([numa_node = *params.numa_node](auto const& cpu_info) { return cpu_info.NumaNodeIndex == numa_node;} ) 
      //                | std::views::transform([](auto const& cpu_info) -> decltype(cpu_info.CpuSet) const& { return cpu_info.Id; })
      //                | std::ranges::to<std::vector>();
   }

   params.size = single_device ? per_device_size * 2 : per_device_size ;
   params.type = utils::allocation_parameters::allocation_type::relaxed;
   auto huge_region = utils::alloc_huge_region(params);
   if (!huge_region)
      // throw std::bad_alloc{std::to_string(mem_region_size) + "bytes allocation failed" };
      throw boost::system::system_error(boost::winapi::GetLastError(), boost::system::system_category(), std::to_string(params.size) + "bytes allocation failed");

   BOOST_LOG_TRIVIAL(info) << params.size << " bytes " << (params.type == utils::allocation_parameters::allocation_type::huge_pages ? "in huge pages " : " ") << "has been allocated" ; // for device ...
   auto device_in = rmaxx::device{in_dev_addr, std::move(huge_region), params.size, per_device_size};
   auto rx = rmaxx::rx_channel{device_in, payload.stride_size, payload.max_size, payload.min_size};

   if (!single_device) {
      params.size = per_device_size;
      params.type = utils::allocation_parameters::allocation_type::relaxed;
      auto huge_region2 = utils::alloc_huge_region(params);
      if (!huge_region2)
         // throw std::bad_alloc{std::to_string(mem_region_size) + "bytes allocation failed" };
         throw boost::system::system_error(boost::winapi::GetLastError(), boost::system::system_category(), std::to_string(params.size) + "bytes allocation failed");

      BOOST_LOG_TRIVIAL(info) << params.size << " bytes " << (params.type == utils::allocation_parameters::allocation_type::huge_pages ? "in huge pages " : " ") << "has been allocated" ; // for device ...
      auto device_out = rmaxx::device{out_dev_addr, std::move(huge_region2), params.size, per_device_size};
      echo(options, rx, device_out);
   }
   else
      echo(options, rx, device_in);
}

