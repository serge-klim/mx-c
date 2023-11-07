#pragma once
//#include "WinSock2.h"
#include "socket.hpp"
#include "ws2ipdef.h"
#include <boost/system/system_error.hpp>

namespace rio { inline namespace v2 {

namespace detail {
template <int Level>
struct option
{
   constexpr int level() const noexcept { return Level; }
};

} // namespace detail

namespace options {
class loopback : public detail::option<IPPROTO_IP>
{
 public:
   constexpr loopback(bool enable = true) noexcept : enable_{static_cast<decltype(enable_)>(enable)} {}
   constexpr int name() const noexcept { return IP_MULTICAST_LOOP; }
   /*constexpr*/ char const* value() const noexcept { return reinterpret_cast<char const*>(&enable_); }
   constexpr int value_size() const noexcept { return sizeof(enable_); }

 private:
   int enable_;
};

constexpr char const* name(loopback const&) noexcept { return "loopback"; }

namespace multicast {

class hops : public detail::option<IPPROTO_IP>
{
 public:
   constexpr hops(unsigned short value) noexcept : value_{static_cast<decltype(value_)>(value)} {}
   constexpr int name() const noexcept { return IP_MULTICAST_TTL; }
   /*constexpr*/ char const* value() const noexcept { return reinterpret_cast<char const*>(&value_); }
   constexpr int value_size() const noexcept { return sizeof(value_); }
 private:
   unsigned char value_;
};

constexpr char const* name(hops const&) noexcept { return "hops"; }

class join_group : public detail::option<IPPROTO_IP>
{
 public:
   constexpr join_group(ip_mreq mreq) noexcept : mreq_{std::move(mreq)} {}
   constexpr join_group(struct in_addr imr_multiaddr, struct in_addr imr_interface = {INADDR_ANY}) noexcept : mreq_{imr_multiaddr, imr_interface} {}
   constexpr int name() const noexcept { return IP_ADD_MEMBERSHIP; }
   /*constexpr*/ char const* value() const noexcept { return reinterpret_cast<char const*>(&mreq_); }
   constexpr int value_size() const noexcept { return sizeof(mreq_); }

 private:
   ip_mreq mreq_ /*= {INADDR_ANY, INADDR_ANY}*/;
};

constexpr char const* name(join_group const&) noexcept { return "multicast::join_group"; }

class outbound_interface : public detail::option<IPPROTO_IP>
{
 public:
   constexpr outbound_interface(unsigned long if_addr) noexcept { if_addr_.s_addr = if_addr; }
   constexpr outbound_interface(struct in_addr if_addr) noexcept : if_addr_{if_addr} {}
   constexpr int name() const noexcept { return IP_MULTICAST_IF; }
   /*constexpr*/ char const* value() const noexcept { return reinterpret_cast<char const*>(&if_addr_); }
   constexpr int value_size() const noexcept { return sizeof(if_addr_); }

 private:
   in_addr if_addr_ /*= {INADDR_ANY, INADDR_ANY}*/;
};
constexpr char const* name(outbound_interface const&) noexcept { return "multicast::outbound_interface"; }
} // namespace multicast

} // namespace options

namespace detail {

template <typename Options>
void set_options(scoped_socket const& socket, Options&& options)
{
    if (setsockopt(handle(socket), options.level(), options.name(), options.value(), options.value_size()) != 0)
       throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), std::string{"setsockopt "} + name(options) + " filed"};
}

template <typename... Options>
void set_options(scoped_socket const& socket, Options&&... options)
{
   set_options(socket, std::forward<Options>(options)...);
}
} // namespace detail

} // namespace v2
} // rio

//Windows: https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options
//
//socket_out.set_option(boost::asio::ip::multicast::outbound_interface{nic.as<boost::asio::ip::address>().to_v4()});
//
//  BOOST_ASIO_OS_DEF(IP_MULTICAST_IF),
//  BOOST_ASIO_OS_DEF(IPV6_MULTICAST_IF)> outbound_interface;
//
//  return ::setsockopt(s, level = 0, optname = 9 (IP_MULTICAST_IF),
//      (const char*)optval, (SockLenType)optlen = 4);
//
//
//boost::asio::ip::multicast::join_group{source_address.to_v4(), nic.as<boost::asio::ip::address>().to_v4()});
//
//  BOOST_ASIO_OS_DEF(IP_ADD_MEMBERSHIP),
//  BOOST_ASIO_OS_DEF(IPV6_JOIN_GROUP)> join_group;
//
//  return ::setsockopt(s, level = 0, optname = 12 (IP_ADD_MEMBERSHIP),
//      (const char*)optval, (SockLenType)optlen = 8);
//
//
//  unsigned char ttl = 10;
//  return ::setsockopt(s, level = 0, optname = 10 (IP_MULTICAST_TTL),
//      (const char*)&ttl, (SockLenType)optlen = 1 sizeof(ttl));
//
//  int enable  = 1;
//  return ::setsockopt(s, level = 0, optname = 11 (IP_MULTICAST_LOOP),
//      (const char*)&enable, (SockLenType)optlen = 4 sizeof(enable));
//
//#define IP_OPTIONS                 1 // Set/get IP options.
//#define IP_HDRINCL                 2 // Header is included with data.
//#define IP_TOS                     3 // IP type of service.
//#define IP_TTL                     4 // IP TTL (hop limit).
//#define IP_MULTICAST_IF            9 // IP multicast interface.
//#define IP_MULTICAST_TTL          10 // IP multicast TTL (hop limit).
//#define IP_MULTICAST_LOOP         11 // IP multicast loopback.
//#define IP_ADD_MEMBERSHIP         12 // Add an IP group membership.
//#define IP_DROP_MEMBERSHIP        13 // Drop an IP group membership.
//#define IP_DONTFRAGMENT           14 // Don't fragment IP datagrams.
//#define IP_ADD_SOURCE_MEMBERSHIP  15 // Join IP group/source.
//#define IP_DROP_SOURCE_MEMBERSHIP 16 // Leave IP group/source.
//#define IP_BLOCK_SOURCE           17 // Block IP group/source.
//#define IP_UNBLOCK_SOURCE         18 // Unblock IP group/source.
//#define IP_PKTINFO                19 // Receive packet information.
//#define IP_HOPLIMIT               21 // Receive packet hop limit.
//#define IP_RECVTTL                21 // Receive packet Time To Live (TTL).
//#define IP_RECEIVE_BROADCAST      22 // Allow/block broadcast reception.
//#define IP_RECVIF                 24 // Receive arrival interface.
//#define IP_RECVDSTADDR            25 // Receive destination address.
//#define IP_IFLIST                 28 // Enable/Disable an interface list.
//#define IP_ADD_IFLIST             29 // Add an interface list entry.
//#define IP_DEL_IFLIST             30 // Delete an interface list entry.
//#define IP_UNICAST_IF             31 // IP unicast interface.
//#define IP_RTHDR                  32 // Set/get IPv6 routing header.
//#define IP_GET_IFLIST             33 // Get an interface list.
//#define IP_RECVRTHDR              38 // Receive the routing header.
//#define IP_TCLASS                 39 // Packet traffic class.
//#define IP_RECVTCLASS             40 // Receive packet traffic class.
//#define IP_RECVTOS                40 // Receive packet Type Of Service (TOS).
//#define IP_ORIGINAL_ARRIVAL_IF    47 // Original Arrival Interface Index.
//#define IP_ECN                    50 // IP ECN codepoint.
//#define IP_RECVECN                50 // Receive ECN codepoints in the IP header.
//#define IP_PKTINFO_EX             51 // Receive extended packet information.
//#define IP_WFP_REDIRECT_RECORDS   60 // WFP's Connection Redirect Records.
//#define IP_WFP_REDIRECT_CONTEXT   70 // WFP's Connection Redirect Context.
//#define IP_MTU_DISCOVER           71 // Set/get path MTU discover state.
//#define IP_MTU                    73 // Get path MTU.
//#define IP_NRT_INTERFACE          74 // Set NRT interface constraint (outbound).
//#define IP_RECVERR                75 // Receive ICMP errors.
//#define IP_USER_MTU               76 // Set/get app defined upper bound IP layer MTU.
//
//
//
