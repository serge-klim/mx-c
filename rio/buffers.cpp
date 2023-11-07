#include "buffers.hpp"
#include "channels.hpp"
#include "utils/sysinfo.hpp"
#include <boost/system/system_error.hpp>
#include <boost/winapi/get_last_error.hpp>
#include <cassert>

RIO_BUFFERID rio::v3::detail::register_buffer(void* memory, std::size_t size) {
   auto id = (*extentsions().RIORegisterBuffer)(static_cast<char*>(memory), static_cast<DWORD>(size));
   if (id == RIO_INVALID_BUFFERID)
      throw boost::system::system_error{static_cast<int>(boost::winapi::GetLastError()), boost::system::system_category(), "RIORegisterBuffer filed"};
   return id;
}

void rio::v3::detail::deregister_buffer(RIO_BUFFERID id) noexcept {
   (*extentsions().RIODeregisterBuffer)(id);
}

rio::v3::buffer_pool::buffer_pool(utils::huge_region mem_region, std::size_t mem_region_size, std::size_t chunk_size)
    : n_{mem_region_size / chunk_size}, block_size_{chunk_size}, memory_size_{mem_region_size}, registered_memory_{detail::register_memory(std::move(mem_region), mem_region_size)}
{
  storage_.add_block(registered_memory_.get(), memory_size_, chunk_size);
}

RIO_BUF rio::v3::buffer_pool::make_buffer(void* buffer, std::size_t chunk_size) const noexcept
{
    assert(static_cast<char const*>(buffer) >= static_cast<char const*>(registered_memory_.get()));
    assert(static_cast<char const*>(buffer) - static_cast<char const*>(registered_memory_.get()) < static_cast<std::ptrdiff_t>(memory_size_));
    RIO_BUF res;
    res.BufferId = memory_id(registered_memory_);
    res.Offset = static_cast<decltype(res.Offset)>(static_cast<char const*>(buffer) - static_cast<char const*>(registered_memory_.get()));
    res.Length = static_cast<decltype(res.Length)>(chunk_size);
    return res;
}

RIO_BUF rio::v3::buffer_pool::allocate()
{
    assert(!storage_.empty());
    assert(n_ != 0);
    --n_;
    return make_buffer(storage_.malloc(), block_size());
}

void rio::v3::buffer_pool::free(RIO_BUF const& buffer)
{
    storage_.free(offset_to_pointer(buffer.Offset));
    ++n_;
    assert(!storage_.empty());
}

