#pragma once
#include "utils/huge_pages.hpp"
#include "WinSock2.h"
#include "MSWSock.h"
#include <boost/pool/simple_segregated_storage.hpp>
#include <boost/compressed_pair.hpp>
#include <utility>
#include <memory>
#include <cassert>

namespace rio {
inline namespace v3 {

namespace detail {

RIO_BUFFERID register_buffer(void* memory, std::size_t size);
void deregister_buffer(RIO_BUFFERID) noexcept;

template <typename BaseDx>
class memory_registration
{
 public:
   /*constexpr*/ memory_registration(RIO_BUFFERID id, BaseDx const& dx) noexcept : data_{id, dx} {}
   constexpr auto id() const noexcept { return data_.first(); }
   void operator()(void* ptr) const noexcept
   {
      assert(ptr != nullptr);
      deregister_buffer(data_.first());
      (data_.second())(ptr);
   }

 private:
   boost::compressed_pair<RIO_BUFFERID, BaseDx> data_;
};

template <typename T, typename Dx>
auto register_memory(std::unique_ptr<T, Dx>&& mem, std::size_t size)
{
   auto id = register_buffer(mem.get(), size);
   auto deleter = detail::memory_registration<Dx>{id, mem.get_deleter()};
   return std::unique_ptr<T, detail::memory_registration<Dx>>{mem.release(), std::move(deleter)};
}

template <typename T>
struct registered_memory
{};

template <typename T, typename Dx>
struct registered_memory<std::unique_ptr<T, Dx>>
{
   using type = std::unique_ptr<T, memory_registration<Dx>>;
};

} // namespace detail

template <typename T>
using registered_memory = typename detail::registered_memory<T>::type;

template <typename T, typename BaseDx>
constexpr auto memory_id(std::unique_ptr<T, detail::memory_registration<BaseDx>> const& mem_reg) noexcept { return mem_reg.get_deleter().id();}

class buffer_pool
{
 public:
   buffer_pool(utils::huge_region mem_region, std::size_t mem_region_size, std::size_t chunk_size);
   RIO_BUF allocate();
   void free(RIO_BUF const& buffer);
   constexpr auto block_size() const noexcept { return block_size_; }
   constexpr bool empty() const noexcept { return n_ == 0; }
   constexpr auto available_blocks() const noexcept { return n_; }
   RIO_BUF make_buffer(void* buffer) const noexcept { return make_buffer(buffer, block_size()); }
   void* offset_to_pointer(decltype(RIO_BUF::Offset) offset) const noexcept { return static_cast<char*>(registered_memory_.get()) + offset; }
 private:
   RIO_BUF make_buffer(void* buffer, std::size_t chunk_size) const noexcept;   
 private:
   std::size_t n_;
   std::size_t block_size_;
   std::size_t memory_size_;
   registered_memory<utils::huge_region> registered_memory_;
   boost::simple_segregated_storage<std::size_t> storage_;
};

} //namespace v3

} // namespace rio
