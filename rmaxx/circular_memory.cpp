#include "circular_memory.hpp"
#include <numeric>

rmax_iov* rmaxx::circular_memory::allocate_iov(size_type size) noexcept
{
   auto res = static_cast<rmax_iov*>(mem_.allocate(size + sizeof(rmax_iov), alignof(rmax_iov)));
   if (res) {
      res->mid = mid_;
      res->length = static_cast<decltype(res->length)>(size);
      res->addr = reinterpret_cast<decltype(res->addr)>(res + 1);
   }
   return res;
}

void rmaxx::circular_memory::release(rmax_iov* blocks, std::size_t n /*= 1*/) noexcept
{
   for (auto i = decltype(n){0}; i != n; ++i)
      mem_.release(blocks + i);
}

