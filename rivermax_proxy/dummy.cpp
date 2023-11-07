#include "loggers.hpp"
#include "jump_table.hpp"
#include "utility.hpp"
#include <array>
#include <mutex>
//#include <atomic>
//#include <system_error>


rmax_status_t dummy_rmax_init_version(unsigned /*major_version*/, unsigned /*minor_version*/, struct rmax_init_config* /*init_config*/)
{
   //auto res = (*pop_jump_table().init_version)(major_version, minor_version, init_config);
   //if (res != RMAX_OK)
   // BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_init_version(" << major_version << ',' << minor_version << ", init_config->cpu_mask << ," << init_config->flags << "}) -> " << rmaxx::to_string(res);
   return RMAX_OK;
}


rmax_status_t dummy_rmax_register_memory(void* addr, size_t length, struct in_addr /*dev_addr*/, rmax_mkey_id* id)
{
   *id = reinterpret_cast<decltype(*id)>(addr);
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "dummy_rmax_register_memory(" << addr << ',' << length << ',' /* << to_string(dev_addr)*/ << ") -> [ RMAX_OK" << ',' << *id << ']';
   return RMAX_OK;
}

rmax_status_t dummy_deregister_memory(rmax_mkey_id id, struct in_addr /*dev_addr*/)
{
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "dummy_deregister_memory(" << id << ',' << /*to_string(dev_addr) <<*/ ')';
   return RMAX_OK;
}


namespace {
auto dummy_streams_guard = std::mutex{};
struct dummy_stream
{
   bool free = true;
   std::atomic<std::size_t> sent_n = {0};
   std::atomic<std::size_t> sent_size = {0};
};
auto dummy_streams = std::array<dummy_stream, 5>{};
}

rmax_status_t dummy_rmax_out_create_gen_stream(struct rmax_out_gen_stream_params* /*params*/, rmax_stream_id* stream_id)
{
   auto begin = std::begin(dummy_streams);
   auto end = std::end(dummy_streams);
   auto lock = std::lock_guard{dummy_streams_guard};
   auto i = std::find_if(begin, end, [](auto const& stream) { return stream.free; });
   if (i == end)
      return RMAX_ERR_NO_MEMORY;
   *stream_id = static_cast<rmax_stream_id>(std::distance(begin, i));
   i->free = false;
  // BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "rmax_out_create_gen_stream(" << to_string(*params) << ") -> [" << RMAX_OK << ',' << *stream_id << ']';
   return RMAX_OK;
}

rmax_status_t dummy_rmax_out_destroy_stream(rmax_stream_id id)
{
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "dummy_rmax_out_destroy_stream(" << id << ')'
                                                           << "\n\t" << dummy_streams[id].sent_size.exchange(0) << " bytes sent in " << dummy_streams[id].sent_n.exchange(0) << " packets sent";
   auto lock = std::lock_guard{dummy_streams_guard};
   dummy_streams[id].free = true;
   return RMAX_OK;
}

rmax_status_t dummy_rmax_out_commit_chunk(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags) {

   auto size = std::size_t{0};
   for (auto i = decltype(chunk->size){0}; i != chunk->size; ++i) {
      for (auto j = decltype(chunk->packets[i].count){0}; j != chunk->packets[i].count; ++j)
         size += chunk->packets[i].iovec[j].length;
   }

   auto sent_n = dummy_streams[id].sent_n += chunk->size;
   auto sent_size = dummy_streams[id].sent_size += size;
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "dummy_rmax_out_commit_chunk(" << id << ',' << time << ", {" << chunk->size << "," << size << " }," << flags << ") " << sent_n << "x" << sent_size
       //<< "\t\t\t" << chunks_dump
       ;

   return RMAX_OK;
} 

