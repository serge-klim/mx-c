#include "tx_channel.hpp"
#include <string>
#include <thread>
#include <stdexcept>
#include <cassert>


#include <utils/sysinfo.hpp>
auto aligned_packets_area_size(std::size_t packets_n)
{
   auto const cache_line_size = utils::cache_line_size();
   if (cache_line_size != std::hardware_destructive_interference_size)
      throw std::runtime_error{"preconfigured cache line size : " + std::to_string(std::hardware_destructive_interference_size) + " doesn't match to cache line size of used CPU :" + std::to_string(cache_line_size)};

   auto packets_mem_size = (packets_n * 2 - 1) * sizeof(rmax_packet);
   return utils::size_in_blocks(packets_mem_size, cache_line_size) * cache_line_size;
}


rmaxx::tx_queue::tx_queue(device& dev, std::size_t packets_n, std::size_t packets_area_size)
    : mem_block_{dev.acquire_mem_block()}, buffer_{dev.key_id(mem_block_), static_cast<char*>(mem_block_.get()) + packets_area_size, static_cast<decltype(buffer_)::size_type>(dev.size(mem_block_) - packets_area_size)}
{
   packets_.data = chunk_.packets = static_cast<rmax_packet*>(mem_block_.get());
   packets_.free = packets_.size = static_cast<decltype(packets_.size)>((packets_area_size / sizeof(rmax_packet) + 1) / 2);
   assert(validate());
}

rmaxx::tx_queue::tx_queue(device& dev, std::size_t packets_n_hint /*= 32*/)
    : tx_queue{dev, packets_n_hint, aligned_packets_area_size(packets_n_hint)}
{
}

bool rmaxx::tx_queue::push_back(void const* payload, size_type size) {
   //assert(validate());
   if (packets_.free.load() == 0)
      return false;

   auto iov = buffer_.allocate_iov(size);
   if (!iov)
      return false;
   assert(iov->length == size);
   std::memcpy(reinterpret_cast<void*>(iov->addr), payload, size);

   auto packets = packets_begin();
   // packets[packets_.tail];
   if (packets_.tail++ == packets_.size)
      packets_.tail = 1;
   packets[packets_.tail - 1].iovec = iov;
   packets[packets_.tail - 1].count = 1;
   --packets_.free;
   assert(validate());
   return true;
}

bool rmaxx::tx_queue::validate() const noexcept
{
   auto res = packets_.free <= packets_.size;
   assert(res);
   auto const begin = packets_begin();
   res = res && begin <= chunk_.packets;
   assert(res);
   res = res && std::distance(begin, chunk_.packets) < packets_.size + packets_extra();
   assert(res);
   return res;
}

rmax_chunk const& rmaxx::tx_queue::front() /*const*/ noexcept
{
   chunk_.size = packets_.size - packets_.free;
   if (chunk_.size) {
      if (auto last = std::distance(packets_begin(), chunk_.packets) + chunk_.size; last > packets_.size) {
         assert(packets_extra() >= last - packets_.size);
         std::memcpy(packets_begin() + packets_.size, packets_begin(), (last - packets_.size) * sizeof(chunk_.packets[0]));
      }
   }
   return chunk_;
}

void rmaxx::tx_queue::pop()
{
   for (auto i = decltype(chunk_.size){0}; i != chunk_.size; ++i)
      buffer_.release(chunk_.packets[i].iovec, chunk_.packets[i].count);

   chunk_.packets = packets_begin() + (std::distance(packets_begin(), chunk_.packets) + chunk_.size) % packets_.size;
   packets_.free += static_cast<decltype(packets_.size)>(chunk_.size);
}


rmaxx::tx_channel::tx_channel(device& dev, sockaddr_in const& mcast_out_addr, std::size_t max_chunk_size)
{
   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr = dev.address();

   rmax_out_gen_stream_params out_gen_stream_params;
   out_gen_stream_params.local_addr = reinterpret_cast<struct sockaddr*>(&addr);
   out_gen_stream_params.max_iovec_num = 4;  ////// max_payload_size > page_size ? utils::size_in_blocks(max_payload_size, expected_payload_size) : 1 /*(max_payload_size + mem_block_info.page_size - 1) / mem_block_info.page_size*/;
   out_gen_stream_params.max_chunk_size = static_cast<decltype(out_gen_stream_params.max_chunk_size)>(max_chunk_size); // number of packets in chunk
   out_gen_stream_params.size_in_chunks = 0;
   out_gen_stream_params.opt_field_mask = 0;

   auto remote_addr = sockaddr{0};
   std::memcpy(&remote_addr, &mcast_out_addr, sizeof(mcast_out_addr));
   out_gen_stream_params.remote_addr = &remote_addr;
   out_gen_stream_params.flags = RMAX_OUT_STREAM_REM_ADDR;

   if (auto status = rmax_out_create_gen_stream(&out_gen_stream_params, &stream_id_); status != RMAX_OK)
      throw std::runtime_error{std::string{"rmax_out_create_gen_stream failed : "} + rmaxx::to_string(status)};
}

rmaxx::tx_channel::~tx_channel() noexcept
{
   [[maybe_unused]] auto status = shutdown();
   assert(status == RMAX_OK && "shutdown failed");
   for (;;) {
      auto status = rmax_out_destroy_stream(stream_id_);
      if (status == RMAX_OK)
         break;
      assert(status == RMAX_ERR_BUSY && "tx_channel: rmax_in_destroy_stream failed");
      std::this_thread::yield();
   }
}

bool rmaxx::tx_channel::shutdown() noexcept
{
   auto res = rmax_out_cancel_unsent_chunks(stream_id_) == RMAX_OK;
   //ready_.notify_all(); // TODO: kinda wrong!
   return res;
}

rmax_status_t rmaxx::tx_channel::commit(tx_queue& queue, rmax_commit_flags_t flags /*= static_cast<rmax_commit_flags_t>(0)*/)
{
   auto res = rmax_out_commit_chunk(stream_id_, 0, &const_cast<rmax_chunk&>(queue.front()), flags);
   if (res == RMAX_OK)
      queue.pop();
   return res;
}

rmaxx::concurrent::tx_channel::tx_channel(device& dev, sockaddr_in const& mcast_out_addr, std::size_t max_chunk_size_hint /*= 32*/) 
    : queue_{dev, max_chunk_size_hint}
    , channel_{dev, mcast_out_addr, queue_}
{
}

rmax_status_t rmaxx::concurrent::tx_channel::send(void const* payload, tx_queue::size_type size) {
   auto res = rmax_status_t::RMAX_OK;
   auto skip_commit = true;
   {
      auto lock = std::lock_guard{queue_guard_};
      if (!queue_.push_back(payload, size))
         res = RMAX_ERR_HW_SEND_QUEUE_FULL;
      skip_commit = std::exchange(sending_, true);
   }
   
   while (!skip_commit) {
      assert(!queue_.empty());
      res = channel_.commit(queue_);
      switch (res)
      {
         case rmax_status_t::RMAX_OK: 
             break;
         case RMAX_ERR_HW_SEND_QUEUE_FULL:
             res = RMAX_ERR_NO_FREE_CHUNK; // may be need own return code
             [[fallthrough]];
        default:
             auto lock = std::lock_guard{queue_guard_};
             sending_ = false;
             return res;
      }
      if (queue_guard_.try_lock()) {
          skip_commit = queue_.empty();
          sending_ = !skip_commit;
          queue_guard_.unlock();
      }
   }
   return res;
}
