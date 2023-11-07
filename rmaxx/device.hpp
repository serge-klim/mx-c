#pragma once
#include "rmaxx_utils.hpp"
#include "circular_memory.hpp"
#include "utils/huge_pages.hpp"
#include <boost/pool/simple_segregated_storage.hpp>
#include "winsock2.h"
#include <memory>
#include <mutex>
#include <cstdint>

namespace rmaxx {

constexpr auto max_payload_size() noexcept { return std::size_t{1408}; }

class device
{
  using memory_pool = boost::simple_segregated_storage<std::size_t>;
  struct release_block
  {
     constexpr release_block(device* dev) noexcept : device_{dev} {}
     void operator()(void* mem) noexcept {
        assert(!!device_);
        device_->free_mem_block(*this, mem);
     }
   private:
     device* device_;
  };
public:
  device(struct in_addr dev_addr, utils::memory_block memory_block, std::size_t block_size, std::size_t partition_size);
  constexpr struct in_addr const& address() noexcept { return addr_; }
  using registered_mem_block = std::unique_ptr<void, release_block>;
  registered_mem_block acquire_mem_block();
  constexpr rmax_mkey_id key_id(registered_mem_block const&) const noexcept { return memory_key_id(mem_); }
  constexpr std::size_t size(registered_mem_block const&) const noexcept { return partition_size_; }
  void free_mem_block(release_block const&, void* mem) noexcept;

private:
   struct in_addr addr_;
   std::mutex guard_;
   std::size_t partition_size_;
   rmaxx::registered_memory<utils::huge_region> mem_;
   memory_pool mem_pool_;
};

}

