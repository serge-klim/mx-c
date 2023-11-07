#pragma once
#include "rmaxx_utils.hpp"
#include "utils/circular_memory.hpp"
#include <cstdint>

namespace rmaxx {

class circular_memory
{
 public:
   using size_type = std::uint_fast32_t;
   circular_memory(rmax_mkey_id mid, char* buffer, size_type size) noexcept
       : mid_{mid}, mem_{buffer, size} {}
   bool empty() const noexcept { return mem_.empty(); }
   rmax_iov* allocate_iov(size_type size) noexcept;
   void release(rmax_iov* blocks, std::size_t n = 1) noexcept;

   // debug only
   auto free() const noexcept { return mem_.free(); }
   constexpr auto size() const noexcept { return mem_.size(); }
   size_type used_memory(rmax_iov const* iovec) const noexcept { return mem_.used_memory(iovec); }
  private:
   rmax_mkey_id mid_;
   utils::circular_memory<size_type> mem_;
};

}


