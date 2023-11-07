#include "loggers.hpp"
#include "jump_table.hpp"
#include "utility.hpp"
#include <atomic>
#include <bitset>
#include <system_error>

namespace {

std::atomic<unsigned int> global_shutdow = 0;

}

rmax_status_t rmax_init_version_ex(unsigned major_version, unsigned minor_version, struct rmax_init_config* init_config)
{
   auto shadow_init_config = *init_config;
   if (auto value = config()["cpu_mask"]; !value.empty()) {
      auto cpu_mask = std::bitset<sizeof(init_config->cpu_mask.rmax_bits) / sizeof(std::uint8_t) * 8>{value.as<std::string>()};
      if (!cpu_mask.none() && !cpu_mask.all()) {
         BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_init_version(" << major_version << ',' << minor_version << "overriding cpu mask" << cpu_mask.to_string() << init_config->flags << "})";
         boost::mp11::mp_for_each<boost::mp11::mp_iota_c<sizeof(init_config->cpu_mask.rmax_bits) / sizeof(init_config->cpu_mask.rmax_bits[0])>>([&](auto I) {
            cpu_mask >>= I * sizeof(sizeof(init_config->cpu_mask.rmax_bits[0]) / sizeof(std::uint8_t) * 8);
            //init_config->cpu_mask.rmax_bits[I] = cpu_mask
         });
         init_config = &shadow_init_config;
      }
   }
   auto res = (*pop_jump_table().init_version)(major_version, minor_version, init_config);
   //if (res != RMAX_OK)
   //  BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_init_version(" << major_version << ',' << minor_version << ", init_config->cpu_mask << ," << init_config->flags << "}) -> " << rmaxx::to_string(res);
   return res;
}

unsigned int shutdown_hack(/*struct sockaddr_in**/ /*local_nic_addr*/)
{
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "shutdown hack [" << global_shutdow << "]...";
   return ++global_shutdow;
}


rmax_status_t rmax_in_get_next_chunk_shutdown_hack(rmax_stream_id id, size_t min_chunk_size_in_strides, size_t max_chunk_size_in_strides, int timeout, int flags, struct rmax_in_completion* rx_completion)
{
   auto const f = pop_jump_table().in_get_next_chunk;
   auto res = RMAX_OK;
   auto shutdown = global_shutdow.load();
   while ((res = (*f)(id, min_chunk_size_in_strides, max_chunk_size_in_strides, timeout == 0 ? 500000 : timeout, flags, rx_completion)) == RMAX_OK && timeout == 0 && rx_completion->chunk_size == 0) {
      if (global_shutdow != shutdown) {
         auto error_code = make_error_code(std::errc::interrupted);
         BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "rmax_in_get_next_chunk_shutdown_hack : " << error_code.message();
         throw std::system_error{error_code};
      }
   }
   return res;
}


rmax_status_t rmax_out_create_gen_stream_extended(struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id)
{
   rmax_out_gen_stream_params shadow_params = *params;
   if (auto max_chunk_size = config()["min-max_chunk_size"].as<std::size_t>(); params->max_chunk_size < max_chunk_size) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "overriding requested max_chunk_size : " << params->max_chunk_size << " with " << max_chunk_size;
      shadow_params.max_chunk_size = max_chunk_size;
   }
   if (auto max_iovec_num = config()["min-max_iovec_num"].as<std::size_t>(); params->max_iovec_num < max_iovec_num) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "overriding requested max_iovec_num : " << params->max_iovec_num << " with " << max_iovec_num;
      shadow_params.max_iovec_num = max_iovec_num;
   }
   if (auto size_in_chunks = config()["min-size_in_chunks"].as<std::size_t>(); params->size_in_chunks < size_in_chunks) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "overriding requested size_in_chunks : " << params->size_in_chunks << " with " << size_in_chunks;
      shadow_params.size_in_chunks = size_in_chunks;
   }

   return (*pop_jump_table().out_create_gen_stream)(&shadow_params, stream_id);
}

rmax_status_t rmax_in_attach_flow_ignore_port0(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr) {
    if (flow_attr->local_addr.sin_port == 0) {
        BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "ignoring rmax_in_attach_flow(" << id << ',' << to_string(flow_attr->local_addr.sin_addr) << " ... )  due to no port specified ";
        return RMAX_OK;
    }
    return (*pop_jump_table().in_attach_flow)(id, flow_attr);
}
