#include "loggers.hpp"
#include "jump_table.hpp"
#include "utility.hpp"
//#include "rio/socket.hpp"
#include "rio/channels.hpp"
#include "rio/buffers.hpp"
#include "rio/options.hpp"
#include <vector>
#include <array>
#include <mutex>
#include <memory>
#include <system_error>

namespace {

struct out_gen_stream
{
   std::size_t queue_size = 0;
   std::unique_ptr<rio::v3::buffer_pool> pool;
   rio::cq cq;
   rio::scoped_socket socket;
   rio::tx_channel tx;
};

struct global_context
{
   global_context(std::size_t max_n_out_gen_streams) : rio_sentry{rio::initialize()}, out_gen_streams(max_n_out_gen_streams) {}
   rio::rio_sentry rio_sentry;
   std::mutex out_gen_streams_lock;
   std::vector<std::unique_ptr<out_gen_stream>> out_gen_streams;
};

std::unique_ptr<global_context> context;
} // namespace

rmax_status_t rio_rmax_init_version(unsigned major_version, unsigned minor_version, struct rmax_init_config* init_config)
{
   auto res = (*pop_jump_table().init_version)(major_version, minor_version, init_config);
   if (res != RMAX_OK) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "rmax_init_version(" << major_version << ',' << minor_version << ", ...}) failed : " << rmaxx::to_string(res)
                                                                 << "\nrio is enabled so if it's fine [rmax_init_version=dummy_rmax_init_version] override can be set in the configuration file.\n";
      return res;
   }
   try {
      context = std::make_unique<global_context>(config()["rio.max_n_out_gen_streams"].as<std::size_t>());
   } catch (std::exception& e) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "unable to initialize RIO : " << e.what();
      res = RMAX_ERR_NO_HW_RESOURCES;
   }
   return res;
}

rmax_status_t rio_rmax_cleanup(void)
{
   auto res = (*pop_jump_table().cleanup)();
   try {
      context.reset();
   } catch (std::exception& e) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "error during cleanup : " << e.what();
   }
   return res;
}

rmax_status_t rio_rmax_register_memory(void* addr, size_t length, struct in_addr dev_addr, rmax_mkey_id* id) {
//   *id = reinterpret_cast<decltype(*id)>(addr);
   auto res = (*pop_jump_table().register_memory)(addr, length, dev_addr, id);
   if (res == RMAX_ERR_NOT_INITIALAZED) {
      res = RMAX_OK;
   }
   return res;
}

rmax_status_t rio_rmax_deregister_memory(rmax_mkey_id id, struct in_addr dev_addr)
{
   auto res = (*pop_jump_table().deregister_memory)(id, dev_addr);
   if (res == RMAX_ERR_NOT_INITIALAZED) {
      res = RMAX_OK;
   }
   return res;
}


rmax_status_t rio_rmax_out_create_gen_stream(struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id) try
{
   if (params->local_addr->sa_family != AF_INET || params->remote_addr->sa_family != AF_INET)
      return RMAX_ERR_UNSUPPORTED; 

   auto lock = std::lock_guard{context->out_gen_streams_lock};
   auto begin = std::begin(context->out_gen_streams);
   auto end = std::end(context->out_gen_streams);
   auto i = std::find_if(begin, end, [](auto const& stream) {
      return !stream;
   });
   if (i == end) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "unable to create out_gen_stream, only " << context->out_gen_streams.size() << " out_gen_streams are supported at same time." 
          "\n please set rio.max_n_out_gen_streams configuration parameter to more than " << context->out_gen_streams.size();
      return RMAX_ERR_EXCEEDS_LIMIT;
   }

   auto const size_in_chunks = params->size_in_chunks == 0   
                                    /*Stream size in chunks.Default is : (32k / @max_chunk_size)*/
                                   ? (32 * 1024) / params->max_chunk_size /*max number of packets per chunk*/
                                   : params->size_in_chunks;

   auto const queue_size = size_in_chunks * params->max_chunk_size;

   auto const max_payload_size = config()["max-payload-size"].as<std::size_t>();
   auto mem_allocation_params = utils::allocation_parameters{};
   mem_allocation_params.size = queue_size * max_payload_size;
   mem_allocation_params.type = config()["rio.prefer_huge_pages"].as<bool>()
       ? utils::allocation_parameters::allocation_type::relaxed
       : utils::allocation_parameters::allocation_type::memory;

   if (auto numa_node = config()["rio.numa_node"]; !numa_node.empty())
      mem_allocation_params.numa_node = numa_node.as<unsigned long>();
   //if (auto per_stream_mem_size = config()["rio.per_stream_mem_size"]; !per_stream_mem_size.empty())
   //   mem_allocation_params.size = per_stream_mem_size.as<std::size_t>();

   auto mem_region = utils::alloc_huge_region(mem_allocation_params);
   if (!mem_region)
      return RMAX_ERR_NO_MEMORY;

   BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "RIO : " << mem_allocation_params.size << " bytes "
                                                           << (mem_allocation_params.type == utils::allocation_parameters::allocation_type::huge_pages
                                                                    ?" in huge pages has been allocated"
                                                                    :" has been allocated");

   auto socket = rio::wsa_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
   set_options(socket, rio::options::multicast::outbound_interface{reinterpret_cast<struct sockaddr_in const*>(params->local_addr)->sin_addr});
   if (auto const& enable_loopback = config()["rio.loopback"].as<bool>()) {
      //BOOST_LOG_TRIVIAL(info) << "enabling loopback...";
      set_options(socket, rio::options::loopback{});
   }
   if (auto const& ttl = config()["rio.ttl"]; !ttl.empty()) {
      auto value = ttl.as<unsigned short>();
      // BOOST_LOG_TRIVIAL(info) << "multicasting on if : " << name << " [" << to_string(mreq.imr_interface) << ']';
      set_options(socket, rio::options::multicast::hops{value});
   }
   if (connect(handle(socket),
               params->remote_addr,
               sizeof(sockaddr_in)) != 0) {
      auto error = boost::system::error_code(WSAGetLastError(), boost::system::system_category());
      return RMAX_ERR_NO_HW_RESOURCES;
   }


   auto cq = rio::create_cq(queue_size);
   auto s = handle(socket);
   auto rawcq = cq.get();

   *i = std::make_unique<out_gen_stream>(
       queue_size,
       std::make_unique<rio::v3::buffer_pool>(std::move(mem_region), mem_allocation_params.size, max_payload_size),
       std::move(cq),
       std::move(socket),
       rio::v2::tx_channel{s, rawcq, queue_size}
   );

   *stream_id = static_cast<rmax_stream_id>(std::distance(begin, i));
   return RMAX_OK;
} catch (std::exception const& e) {
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rio_rmax_out_create_gen_stream failed : " << e.what();
   return RMAX_ERR_UNKNOWN_ISSUE;
}

rmax_status_t rio_rmax_out_destroy_stream(rmax_stream_id id)
{
   assert(!!context);
   if (id < 0 || context->out_gen_streams.size() < id)
      return RMAX_ERR_INVALID_PARAM_1;
   auto lock = std::lock_guard{context->out_gen_streams_lock};
   context->out_gen_streams[id].reset();
   return RMAX_OK;
}

rmax_status_t rio_rmax_out_commit_chunk(rmax_stream_id id, uint64_t /*time*/, struct rmax_chunk* chunk, rmax_commit_flags_t /*flags*/)
{
   assert(!!context);
   if (id < 0 || context->out_gen_streams.size() < id)
      return RMAX_ERR_INVALID_PARAM_1;
   auto& stream = context->out_gen_streams[id];
   if (stream->pool->available_blocks() < chunk->size) {
      auto results = std::vector<RIORESULT>(stream->queue_size);
      auto n = (*rio::v2::extentsions().RIODequeueCompletion)(stream->cq.get(), results.data(), static_cast<unsigned long>(results.size()));
      if (n == RIO_CORRUPT_CQ) {
          BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "RIO_CORRUPT_CQ";
          return RMAX_ERR_HW_COMPLETION_ISSUE;
      }
      for (auto i = decltype(n){0}; i != n; ++i) {
          if (results[i].Status == 0) {
             auto buffer = stream->pool->make_buffer(reinterpret_cast<void*>(results[i].RequestContext));
             stream->pool->free(buffer);
          } else {
             BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "RIODequeueCompletion invalid result" << results[i].Status << '(' << boost::system::error_code(results[i].Status, boost::system::system_category()).message() << ')';
          }
      }
      if (stream->pool->available_blocks() < chunk->size)
            return RMAX_ERR_HW_SEND_QUEUE_FULL;
   }

   for (auto i = decltype(chunk->size){0}; i != chunk->size; ++i) {
      auto buffer = stream->pool->allocate();
      auto const max_size = std::exchange(buffer.Length, 0);
      for (auto j = decltype(chunk->packets[i].count){0}; j != chunk->packets[i].count; ++j) {
          if (buffer.Length + chunk->packets[i].iovec[j].length > max_size)
             return /*RMAX_ERR_NO_MEMORY*/ RMAX_ERR_EXCEEDS_LIMIT;
          auto payload = stream->pool->offset_to_pointer(buffer.Offset);
          std::memcpy(static_cast<char*>(payload) + buffer.Length, reinterpret_cast<void const*>(chunk->packets[i].iovec[j].addr), chunk->packets[i].iovec[j].length);
          buffer.Length += chunk->packets[i].iovec[j].length; 
      }
      stream->tx.send(&buffer, stream->pool->offset_to_pointer(buffer.Offset), RIO_MSG_DONT_NOTIFY | RIO_MSG_DEFER);
   }
   stream->tx.commit();
   return RMAX_OK;
}

rmax_status_t rio_rmax_out_cancel_unsent_chunks(rmax_stream_id /*id*/)
{
   return RMAX_ERR_UNSUPPORTED;
}
