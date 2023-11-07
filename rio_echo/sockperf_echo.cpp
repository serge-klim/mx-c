#include "sockperf/sockperf.hpp"
#include "rio/socket.hpp"
#include "rio/options.hpp"
#include "rio/channels.hpp"
#include "rio/buffers.hpp"

#include "utils/sysinfo.hpp"
#include "utils/nic_addresses.hpp"
#include "utils/program_options/net.hpp"
#include "utils/thread.hpp"
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include <iostream>

#include <chrono>
#include <bitset>
#include <thread>
#include <array>
//#include <limits>
//#include <csignal>
//#include <cstdint>
#include <cstring>
#include <cassert>


//rio_sockperf_echo.exe -s 239.255.0.1:30001 --if-in=Wi-Fi --loopback=true
//WinSockperf.exe pp -i 239.255.0.1 -p 30001
//WinSockperf.exe pp -i 239.255.0.1 -p 30001 -n 100000000

namespace {

struct in_addr nic_address(std::string const& /*boost::asio::ip::address*/ str)
{
   struct in_addr res;
   if (inet_pton(AF_INET, str.c_str(), &res) == 1)
      return res;
   auto addrs = utils::net::get_nic_addresses<in_addr>(str);
   switch (addrs.size())
   {
       case 0:
          throw std::runtime_error{"can't find adapter \"" + str + "\" address"};
       case 1:
          return addrs.front();
       default:
          throw std::runtime_error{"more than one adapter \"" + str + "\" address found"};
    }
}

std::string to_string(in_addr const& address)
{
    auto buffer = std::array<char, INET_ADDRSTRLEN>{};
    inet_ntop(AF_INET, &address, buffer.data(), buffer.size());
    return buffer.data();
 }

std::string to_string(in_addr6 const& address)
{
    auto buffer = std::array<char, INET6_ADDRSTRLEN>{};
    inet_ntop(AF_INET6, &address, buffer.data(), buffer.size());
    return buffer.data();
}

std::string to_string(sockaddr_in const& address)
{
    auto res = to_string(address.sin_addr);
    res += ':';
    res += std::to_string(address.sin_port);
    return res;
}

std::string to_string(sockaddr_in6 const& address)
{
    auto res = to_string(address.sin6_addr);
    res += ':';
    res += std::to_string(address.sin6_port);
    return res;
}

std::string to_string(SOCKADDR_INET const& address)
{
    return address.si_family == AF_INET 
                ? to_string(reinterpret_cast<sockaddr_in const&>(address.Ipv4))
               : to_string(reinterpret_cast<sockaddr_in6 const&>(address.Ipv6))
        ;
}

bool same_address(SOCKADDR_INET const& left, SOCKADDR_INET const& right) noexcept
{
    auto is_same_ip4 = [](sockaddr_in const& left, sockaddr_in const& right){
       return left.sin_port == right.sin_port
           && std::memcmp(&left.sin_addr, &right.sin_addr, sizeof(left.sin_addr)) == 0
           ;
    };
    auto is_same_ip6 = [](sockaddr_in6 const& left, sockaddr_in6 const& right)
    {
       return left.sin6_port == right.sin6_port 
           && std::memcmp(&left.sin6_addr, &right.sin6_addr, sizeof(left.sin6_addr)) == 0
           ;
    };
    return left.si_family == right.si_family
               ? is_same_ip4(reinterpret_cast<sockaddr_in const&>(left.Ipv4), reinterpret_cast<sockaddr_in const&>(right.Ipv4))
               : is_same_ip6(reinterpret_cast<sockaddr_in6 const&>(left.Ipv6), reinterpret_cast<sockaddr_in6 const&>(right.Ipv6))
        ;
}

} // namespace

namespace {
//auto& stop_token() noexcept {
//   static auto stop = std::atomic<bool>{false};
//   return stop;
//}

enum op
{
    sent = 101,
    received
};

}

constexpr auto address_size = utils::size_in_blocks(sizeof(SOCKADDR_INET), std::hardware_destructive_interference_size) * std::hardware_destructive_interference_size;

void echo(boost::program_options::variables_map const& options, rio::v3::buffer_pool& pool) {

   auto socket_in = rio::wsa_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
   // int yes = 1;
   // if (setsockopt(handle(socket_in), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes)) == SOCKET_ERROR)
   //    throw boost::system::system_error(WSAGetLastError(), boost::system::system_category(), "setsockopt SO_REUSEADDR filed");

   auto src = options["source"].as<net::endpoint>();
   auto src_addr = sockaddr_in{0};
   src_addr.sin_family = AF_INET;
   src_addr.sin_addr.s_addr = INADDR_ANY;
   src_addr.sin_port = htons(src.port);
   if (bind(handle(socket_in), reinterpret_cast<SOCKADDR*>(&src_addr), sizeof(src_addr)) == SOCKET_ERROR)
       throw boost::system::system_error(WSAGetLastError(), boost::system::system_category(), "bind filed");

   ip_mreq mreq = {0, 0};
   mreq.imr_multiaddr.s_addr = htonl(boost::asio::ip::make_address(src.host).to_v4().to_uint());
   mreq.imr_interface.s_addr = INADDR_ANY; // host_to_network_long
   if (auto const& if_in = options["if-in"]; !if_in.empty()) {
       auto name = if_in.as<std::string>();
       mreq.imr_interface = nic_address(name);
       BOOST_LOG_TRIVIAL(info) << "listening on if : " << name << " [" << to_string(mreq.imr_interface) << ']';
   }
   // if (setsockopt(handle(socket_in), IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
   //    throw boost::system::system_error(WSAGetLastError(), boost::system::system_category(), "setsockopt IP_ADD_MEMBERSHIP filed");
   set_options(socket_in, rio::options::multicast::join_group{mreq});

   struct tx
   {
       SOCKADDR_INET addr = {};
       rio::scoped_socket socket;
       rio::tx_channel channel;
       std::size_t pong_sent = 0;
       RIO_BUF pending = {0};
   };
   auto txs = std::array<tx, 2>{};

   auto const queue_size = options["queue-size"].as<std::size_t>();
   auto cq = rio::create_cq(queue_size * (1 /*rx*/ + txs.size()));
   auto rx = rio::v2::rx_channel{handle(socket_in), cq.get(), queue_size};

    assert(queue_size != 0);
    for (std::size_t i = 0; i != queue_size; ++i) {
        assert(!pool.empty());
        auto address = pool.allocate();
        auto buffer = address;
        address.Length = address_size;
        buffer.Offset += address.Length;
        buffer.Length -= address.Length;
        rx.receive_from(&buffer, &address, pool.offset_to_pointer(address.Offset), RIO_MSG_DONT_NOTIFY | RIO_MSG_DEFER);
    }
    rx.commit();
    BOOST_LOG_TRIVIAL(info) << "waiting for client on " << src.host << ':' << src.port
                            << "\n\tplease run client as (win)sockperf.exe pp -i " << src.host << " -p " << src.port << "[ -n 100000[000] -m 128]"
                            << "  ...";


   // void* current_buffer = nullptr;
   // auto current_buffer_size = std::size_t{0};
    auto active_tx = decltype(txs)::size_type{0};
    auto results = std::vector<RIORESULT>(queue_size * txs.size());
    for (;;) {
        switch (auto n = (*rio::v2::extentsions().RIODequeueCompletion)(cq.get(), results.data(), static_cast<unsigned long>(results.size()))) {
          case RIO_CORRUPT_CQ:
             throw std::runtime_error{"RIO_CORRUPT_CQ"};
          case 0:
             std::this_thread::yield();
             break;
          default: {
             auto commit_tx = false;
             auto commit_rx = false;
             for (auto i = decltype(n){0}; i != n; ++i) {
                if (results[i].Status != 0)
                   throw boost::system::system_error(results[i].Status, boost::system::system_category(), "RIODequeueCompletion invalid result");
                //data received
                if (results[i].SocketContext == reinterpret_cast<decltype(results[i].SocketContext)>(&rx)) {
                   SOCKADDR_INET* addr = reinterpret_cast<SOCKADDR_INET*>(results[i].RequestContext);
                   auto payload = reinterpret_cast<char*>(results[i].RequestContext) + address_size;
                   auto const payload_size = results[i].BytesTransferred;
                   if (!same_address(txs[active_tx].addr, *addr)) {
                      if (++active_tx == txs.size())
                         active_tx = 0;
                      if (!same_address(txs[active_tx].addr, *addr)) {                         
                         if (txs[active_tx].socket) {
                            BOOST_LOG_TRIVIAL(info) << "disconnecting from : " << to_string(txs[active_tx].addr) << " pong sent " << txs[active_tx].pong_sent << "...";
                            txs[active_tx].socket.reset();
                            txs[active_tx].pong_sent = 0;
                         }
                         std::memcpy(&txs[active_tx].addr, addr, sizeof(txs[active_tx].addr));
                      }
                      // else //seems we are still connected to this client....
                   }

                   // enough data to decide what to do with it
                   if (payload_size >= sockperf::header_size()) {
                      auto message_size = sockperf::message_size(payload, payload_size);
                      if (message_size >= payload_size) {
                         // enough data to decide what to do with it
                         if (!txs[active_tx].socket) {
                            BOOST_LOG_TRIVIAL(info) << "client request received : " << message_size << " bytes from : " << to_string(*addr)
                                                    << " [" << sockperf::seqn(payload, payload_size) << ':' << static_cast<int>(sockperf::message_type(payload, payload_size)) << "]";
                            // connecting...
                            auto socket = rio::wsa_socket(addr->si_family, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);

                            if (auto const& if_out = options["if-out"]; !if_out.empty()) {
                                auto name = if_out.as<std::string>();
                                BOOST_LOG_TRIVIAL(info) << "sending responding multicasting via if : " << name << " [" << to_string(mreq.imr_interface) << ']';
                                set_options(socket, rio::options::multicast::outbound_interface{nic_address(name)});
                            }
                            if (options["loopback"].as<bool>())
                                set_options(socket, rio::options::loopback{});

                            if (auto const& ttl = options["ttl"]; !ttl.empty()) {
                                auto value = ttl.as<unsigned short>();
                                // BOOST_LOG_TRIVIAL(info) << "multicasting on if : " << name << " [" << to_string(mreq.imr_interface) << ']';
                                set_options(socket, rio::options::multicast::hops{value});
                            }
                            if (connect(handle(socket),
                                        reinterpret_cast<sockaddr const*>(&addr->Ipv4),
                                        addr->si_family == AF_INET ? sizeof(addr->Ipv4) : sizeof(addr->Ipv6)) != 0)
                                throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "connect filed"};

                            txs[active_tx].channel = rio::v2::tx_channel{handle(socket), cq.get(), queue_size};
                            txs[active_tx].socket = std::move(socket);
                         }
                         if (sockperf::message_type(payload, message_size) == sockperf::type::Ping) {
                            // seems ping received, sending pong...
                            sockperf::message_type(sockperf::type::Pong, payload, message_size);
                            auto buffer = pool.make_buffer(payload);
                            buffer.Length = message_size;
                            txs[active_tx].channel.send(&buffer, payload, RIO_MSG_DONT_NOTIFY | RIO_MSG_DEFER);
                            ++txs[active_tx].pong_sent;
                            commit_tx = true;
                            continue;
                         }
                         assert(message_size + sockperf::header_size() != payload_size && "Oops! deal with leftovers");
                      }
                   }
                   //receive more data
                   results[i].RequestContext = reinterpret_cast<decltype(results[i].RequestContext)>(payload);
                }
                auto buffer = pool.make_buffer(reinterpret_cast<void*>(results[i].RequestContext));
                auto address = buffer;
                address.Offset -= address_size;
                address.Length = address_size;
                buffer.Length -= address_size;
                rx.receive_from(&buffer, &address, pool.offset_to_pointer(address.Offset), RIO_MSG_DONT_NOTIFY | RIO_MSG_DEFER);
                commit_rx = true;
             }
             if (commit_tx)
                txs[active_tx].channel.commit();
             if (commit_rx)
                rx.commit();
          }
        }
    }
}

void run(boost::program_options::variables_map const& options)
{

   // auto sig_handler = [](const int /*signal*/) {
   //   stop_token() = true;
   //};
   //std::signal(SIGINT, sig_handler);
   //std::signal(SIGBREAK, sig_handler);


   auto const queue_size = options["queue-size"].as<std::size_t>();
   auto const payload_size = options["payload-size"].as<std::size_t>();
   auto params = utils::allocation_parameters{};
   params.size = (queue_size * 2 + 1) * (payload_size + address_size);
   params.type = utils::allocation_parameters::allocation_type::relaxed;

   if (auto numa_node = options["numa-node"]; !numa_node.empty()) {
       auto numa = numa_node.as<unsigned long>();
       params.numa_node = numa;
       BOOST_LOG_TRIVIAL(info) << "configuring to run on selected (" << numa << ") node...";

      // auto cpus = utils::cpus_info();


       unsigned long highest_numa_node; 
       if(!GetNumaHighestNodeNumber(&highest_numa_node))
          throw boost::system::system_error(WSAGetLastError(), boost::system::system_category(), "GetNumaHighestNodeNumber");
       if (highest_numa_node<numa)
          throw boost::program_options::validation_error{boost::program_options::validation_error::invalid_option_value, "numa-node"};
////////////////////////////////////////////////////
       auto cores = utils::cores_info();
       auto const& workset = utils::numa_node_workset(numa);
       auto core = cbegin(cores);

       //if (std::any(cbegin(workset), cend(workset), [core](auto const& group) {
       //  return group.group == core->
       //    }) {
       //}

       auto group_affinity = GROUP_AFFINITY{};
       group_affinity.Group = core->groups.front().group;
       group_affinity.Mask = core->groups.front().mask;
       BOOST_LOG_TRIVIAL(warning) << "selecting core : " << group_affinity.Group << ':' << std::bitset<64>{group_affinity.Mask}.to_string() << std::endl;

       //std::erase_if(cores_begin, cores_end, [&workset](auto const& core) {
       //    
       //   return core.groups
       //});

       //auto cpu_set_info = std::vector<char>(sizeof(SYSTEM_CPU_SET_INFORMATION) * 16);
       //unsigned long size = 0;
       //if (!GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(cpu_set_info.data()), cpu_set_info.size(), &size, 0, 0)) {
       //   auto error = boost::winapi::GetLastError();
       //   if (error != ERROR_INSUFFICIENT_BUFFER)
       //      throw boost::system::system_error(error, boost::system::system_category(), "GetSystemCpuSetInformation filed");
       //   cpu_set_info.resize(size);
       //   if (!GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(cpu_set_info.data()), cpu_set_info.size(), &size, 0, 0))
       //      throw boost::system::system_error(boost::winapi::GetLastError(), boost::system::system_category(), "GetSystemCpuSetInformation filed");
       //}

       //struct group
       //{
       //   unsigned long long mask;
       //   //unsigned short numa;
       //   unsigned short max_efficiency_class;
       //   std::vector<unsigned long long> cores;
       //};

       //auto groups = std::vector<group>(1);
       //auto const end = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION const*>(cpu_set_info.data() + size);
       //for (
       //    auto cpu_info = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION const*>(cpu_set_info.data());
       //    cpu_info < end;
       //    cpu_info = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION const*>(reinterpret_cast<char const*>(cpu_info) + cpu_info->Size)) {
       //   if (cpu_info->Type == CPU_SET_INFORMATION_TYPE::CpuSetInformation && cpu_info->CpuSet.NumaNodeIndex == numa) {
       //      if (groups.size() < cpu_info->CpuSet.Group + 1)
       //         groups.resize(cpu_info->CpuSet.Group);
       //      if (groups[cpu_info->CpuSet.Group].cores.size() < cpu_info->CpuSet.CoreIndex + 1)
       //         groups[cpu_info->CpuSet.Group].cores.resize(cpu_info->CpuSet.CoreIndex + 1);
       //      //groups[cpu_info->CpuSet.Group].numa = cpu_info->CpuSet.NumaNodeIndex;
       //      groups[cpu_info->CpuSet.Group].cores[cpu_info->CpuSet.CoreIndex] |= (1ull << cpu_info->CpuSet.LogicalProcessorIndex);
       //      groups[cpu_info->CpuSet.Group].mask |= (1ull << cpu_info->CpuSet.LogicalProcessorIndex);
       //      if (groups[cpu_info->CpuSet.Group].max_efficiency_class < cpu_info->CpuSet.EfficiencyClass)
       //         groups[cpu_info->CpuSet.Group].max_efficiency_class = cpu_info->CpuSet.EfficiencyClass;
       //   }
       //}
       //auto groups_begin = cbegin(groups);
       //auto groups_end = cend(groups);
       //auto group = std::max_element(groups_begin, groups_end, [](auto const& l, auto const& r) {
       //   return l.max_efficiency_class < r.max_efficiency_class || l.cores < r.cores;
       //});
       //assert(group != groups_end);
       //auto cores_begin = begin(group->cores);
       //auto cores_end = cend(group->cores);

       //auto core = std::min_element(cbegin(group->cores), cores_end, [](auto const& l, auto const& r) {
       //   // TODO check which parked
       //   auto ll = l == 0 ? (std::numeric_limits<std::size_t>::max)() : std::bitset<64>{l}.count();
       //   auto rr = r == 0 ? (std::numeric_limits<std::size_t>::max)() : std::bitset<64>{r}.count();
       //   return ll < rr;
       //});
       //BOOST_LOG_TRIVIAL(info) << "\n\nselected core : " << std::distance(cores_begin, core) << " [" << std::bitset<64>{*core}.to_string() << ']' << std::endl;
       //auto group_affinity = GROUP_AFFINITY{};
       //group_affinity.Group = std::distance(groups_begin, group);
       //group_affinity.Mask = *core;


//////////////////////////////////////////////////
       auto process = GetCurrentProcess();
       if (!SetPriorityClass(process, REALTIME_PRIORITY_CLASS))
          BOOST_LOG_TRIVIAL(warning) << "SetThreadGroupAffinity filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

       auto thread = GetCurrentThread();
       if (!SetThreadPriority(thread, /*REALTIME_PRIORITY_CLASS*/ THREAD_PRIORITY_HIGHEST))
          BOOST_LOG_TRIVIAL(warning) << "SetThreadGroupAffinity filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

       if (!SetThreadGroupAffinity(thread, &group_affinity, nullptr))
          BOOST_LOG_TRIVIAL(warning) << "SetThreadGroupAffinity filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

       //auto memory_priority = MEMORY_PRIORITY_INFORMATION{0};
       //memory_priority.MemoryPriority = MEMORY_PRIORITY_NORMAL;
       //if (!SetThreadInformation(thread, ThreadMemoryPriority, &memory_priority, sizeof(memory_priority)))
       //   BOOST_LOG_TRIVIAL(warning) << "SetThreadInformation (ThreadMemoryPriority) filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

       //auto throttling_state = THREAD_POWER_THROTTLING_STATE{0};
       //throttling_state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
       //throttling_state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
       //throttling_state.StateMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;

       //if (!SetThreadInformation(thread, ThreadPowerThrottling, &throttling_state, sizeof(throttling_state)))
       //   BOOST_LOG_TRIVIAL(warning) << "SetThreadInformation (ThreadPowerThrottling) filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

       if (!SetThreadPriorityBoost(thread, FALSE))
          BOOST_LOG_TRIVIAL(warning) << "SetThreadPriorityBoost filed : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;

   }

   auto rio_sentry = rio::initialize();
   if (auto huge_region = utils::alloc_huge_region(params)) {
       auto pool = rio::v3::buffer_pool{std::move(huge_region), params.size, payload_size};
       echo(options, pool);
   } else {
       BOOST_LOG_TRIVIAL(warning) << "unable to allocate  : " << params.size << " bytes : " << boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category()).message() << std::endl;
   }
 }

