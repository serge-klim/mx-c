#include "device.hpp"

rmaxx::device::device(struct in_addr dev_addr, utils::memory_block memory_block, std::size_t block_size, std::size_t partition_size) 
	: addr_{dev_addr} 
	, partition_size_{partition_size}
	, mem_{rmaxx::register_memory(dev_addr, std::move(memory_block), block_size)}
{
   mem_pool_.add_block(mem_.get(), block_size, partition_size_);
}   

rmaxx::device::registered_mem_block rmaxx::device::acquire_mem_block() /*noexcept*/
{
   auto lock = std::lock_guard{guard_};
   auto ptr = !mem_pool_.empty() ? mem_pool_.malloc() : static_cast<void*>(nullptr);
   return {ptr, release_block{this}};
}

void rmaxx::device::free_mem_block(release_block const&, void* mem) noexcept
{
   auto lock = std::lock_guard{guard_};
   assert(!!mem);
   mem_pool_.free(mem);
}
