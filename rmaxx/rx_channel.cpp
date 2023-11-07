#include "rx_channel.hpp"
#include <boost/winapi/get_last_error.hpp>
#include <boost/system/system_error.hpp>
#include <string>
#include <cassert>

rmaxx::rx_channel::rx_channel(device& dev, std::uint16_t stride_size, std::uint16_t max_payload_size, std::uint16_t min_payload_size)
    : mem_block_{dev.acquire_mem_block()}
{
    auto payload = rmax_in_memblock{0};
    payload.ptr = mem_block_.get();
    payload.max_size = max_payload_size;
    payload.min_size = min_payload_size;
    payload.stride_size = stride_size;

    constexpr auto stream_type = RMAX_APP_PROTOCOL_PACKET /*RMAX_APP_PROTOCOL_PAYLOAD*/;
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr = dev.address();

    auto buffer_attr = rmax_in_buffer_attr{0};
    buffer_attr.num_of_elements = (std::numeric_limits<std::uint16_t>::max)() * 2;
    buffer_attr.attr_flags = 0;
    buffer_attr.data = &payload;
    buffer_attr.hdr = nullptr;
    
    if (auto status = rmax_in_create_stream(stream_type, &addr, &buffer_attr, RMAX_PACKET_TIMESTAMP_RAW_NANO, RMAX_IN_CREATE_STREAM_INFO_PER_PACKET, &stream_id_); status != RMAX_OK)
        throw std::runtime_error{std::string{"rmax_in_create_stream failed : "} + rmaxx::to_string(status)};
}

rmaxx::rx_channel::~rx_channel() noexcept {
   [[maybe_unused]] auto status = rmax_in_destroy_stream(stream_id_);
   assert(status == RMAX_OK && "~rx_channel: rmax_in_destroy_stream failed");
}

void rmaxx::rx_channel::join(struct sockaddr_in const& local_addr)
{
   auto remote_addr = in_addr{};
   remote_addr.s_addr = INADDR_ANY;
   return join(local_addr, remote_addr);
}

void rmaxx::rx_channel::join(struct sockaddr_in const& local_addr, struct in_addr remote_addr)
{
   auto attr = rmax_in_flow_attr{local_addr, {0}, 0};
   attr.remote_addr.sin_addr = remote_addr;
   if (auto status = rmax_in_attach_flow(stream_id_, &attr); status != RMAX_OK)
      throw std::runtime_error{std::string{"rmax_in_attach_flow failed : "} + rmaxx::to_string(status)};
}

bool rmaxx::rx_channel::get_next_chunk(rmax_in_completion& completion, std::size_t min_packets /*= 0*/, size_t max_packets /*= 5000*/, int timeout /*= 0*/ /* usecs wait forever*/)
{
   const int flags = 0;
   auto status = rmax_in_get_next_chunk(stream_id_, min_packets, max_packets, timeout, flags, &completion);
   switch(status)
   {
      case RMAX_OK:
         return true;
      case RMAX_SIGNAL:
        return false;
      default:break;
   }
   throw std::runtime_error{std::string{"rmax_in_get_next_chunk failed : "} + rmaxx::to_string(status)};
}

