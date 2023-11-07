#pragma once
#include "device.hpp"
#include <mutex>
#include <atomic>
#include <new>
#include <cstdint>

namespace rmaxx {

class tx_queue
{
   tx_queue(device& dev, std::size_t packets_n, std::size_t packets_area_size);
 public:
   tx_queue(device& dev, std::size_t packets_n_hint = 32);
   tx_queue(tx_queue const&) = delete;
   tx_queue& operator=(tx_queue const&) = delete;
   //tx_queue(tx_queue&&) = default;
   //tx_queue& operator=(tx_queue&&) = default;

   using size_type = circular_memory::size_type;
   bool push_back(void const* payload, size_type size);
   rmax_chunk const& front() /*const*/ noexcept;
   void pop();
   bool empty() const noexcept { return packets_.size - packets_.free == 0; }
   constexpr std::size_t capacity() const noexcept { return packets_.size; }
 private:
   constexpr auto packets_extra() const noexcept { return packets_.size - 1; }
   constexpr rmax_packet* packets_begin() const noexcept { return packets_.data; }
   bool validate() const noexcept;
 private:
   device::registered_mem_block mem_block_;
   circular_memory buffer_;
   rmax_chunk chunk_;
   struct
   {
      rmax_packet* data;
      std::uint_fast16_t /*const*/ size;
      std::uint_fast16_t tail = 0;
      alignas(std::hardware_destructive_interference_size) std::atomic<std::uint_fast16_t> free;
   } packets_;
};

class tx_channel
{
 public:
   tx_channel(device& dev, sockaddr_in const& mcast_out_addr, std::size_t max_chunk_size);
   tx_channel(device& dev, sockaddr_in const& mcast_out_addr, tx_queue const& q) : tx_channel{dev, mcast_out_addr, q.capacity()} {}
   tx_channel(tx_channel const&) = delete;
   tx_channel& operator = (tx_channel const&) = delete;
   tx_channel(tx_channel &&) = default;
   tx_channel& operator=(tx_channel&&) = default;
   ~tx_channel() noexcept;
   rmax_status_t commit(tx_queue& queue, rmax_commit_flags_t flags = static_cast<rmax_commit_flags_t>(0));
 private:
   bool shutdown() noexcept;
 private:
   // device& dev_;
   rmax_stream_id stream_id_;
};

} // namespace rmax

#include <mutex>

namespace rmaxx {

namespace concurrent {
class tx_channel
{
 public:
   tx_channel(device& dev, sockaddr_in const& mcast_out_addr, std::size_t max_chunk_size_hint = 32);
   rmax_status_t send(void const* payload, tx_queue::size_type size);
 private:
   std::mutex queue_guard_;
   bool sending_ = false;
   tx_queue queue_;
   rmaxx::tx_channel channel_;
};
}

} // namespace rmaxx