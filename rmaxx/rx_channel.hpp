#pragma once
#include "device.hpp"

namespace rmaxx {

class rx_channel 
{
 public:
   rx_channel(device& dev, std::uint16_t stride_size, std::uint16_t max_payload_size, std::uint16_t min_payload_size);
   ~rx_channel() noexcept;
   constexpr auto stride_size() const noexcept { return stride_size_; }
   void join(struct sockaddr_in const& local_addr);
   void join(struct sockaddr_in const& local_addr, struct in_addr remote_addr);
   bool get_next_chunk(rmax_in_completion& completion, std::size_t min_packets = 0, size_t max_packets = 5000, int timeout = 0 /*usecs, 0 - wait forever*/);

 private:
   //rmax_in_memblock payload_ = {0};
   std::uint16_t stride_size_;
   device::registered_mem_block mem_block_;
   rmax_stream_id stream_id_;
};

} // namespace rmaxx