#include "loggers.hpp"
#include "jump_table.hpp"
#include "ws2tcpip.h"
#include "utility.hpp"
#include <boost/program_options.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/winapi/thread.hpp>
#include <boost/mp11/algorithm.hpp>
#include <bitset>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>
#include <utility>
#include <sstream>
#include <string>
#include <array>
#include <cstdint>

boost::program_options::variables_map& config() noexcept;
void wait_for_debbuger();
void wait_for_debbuger_if_configured();

namespace {

constexpr boost::log::trivial::severity_level severity_level(rmax_status_t status, boost::log::trivial::severity_level default_level = boost::log::trivial::trace) noexcept
{
   return status == RMAX_OK ? default_level : boost::log::trivial::error;
}

}

rmax_status_t log_rmax_cleanup(void) {
    auto res = (*pop_jump_table().cleanup)();
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_cleanup() -> " << rmaxx::to_string(res);
    return res;    
}

//namespace {
//std::string to_string(struct in_addr const& address)
//{
//    auto buffer = std::array<char, INET_ADDRSTRLEN>{};
//    inet_ntop(AF_INET, &address, buffer.data(), buffer.size());
//    return buffer.data();
//}
//
//std::string to_string(struct sockaddr_in const& address)
//{
//    return to_string(address.sin_addr) + ':' + std::to_string(address.sin_port);
//}
//
//std::string to_string(struct in6_addr const& address)
//{
//    auto buffer = std::array<char, INET6_ADDRSTRLEN>{};
//    inet_ntop(AF_INET, &address, buffer.data(), buffer.size());
//    return buffer.data();
//}
//
//std::string to_string(struct sockaddr_in6 const& address)
//{
//    return to_string(address.sin6_addr) + ':' + std::to_string(address.sin6_port);
//}
//
//
//std::string to_string(struct sockaddr const& address) {
//    return address.sa_family == AF_INET
//        ? to_string(*reinterpret_cast<struct sockaddr_in const*>(address.sa_data))
//        : to_string(*reinterpret_cast<struct sockaddr_in6 const*>(address.sa_data));
//}
//
//} // namespace

rmax_status_t log_rmax_get_version(unsigned* major_version, unsigned* minor_version, unsigned* release_number, unsigned* build) {
    auto res = (*pop_jump_table().get_version)(major_version, minor_version, release_number, build);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_get_version(" << *major_version << ',' << *minor_version << ',' << *release_number << ',' << *build << ") -> " << rmaxx::to_string(res);
    return res;
}

const char* log_rmax_get_version_string() {   
    auto res = (*pop_jump_table().get_version_string)();
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "rmax_get_version_string() -> " << res;
    return res;
}

rmax_status_t log_rmax_init_version(unsigned major_version, unsigned minor_version, struct rmax_init_config* init_config) {
    auto res = (*pop_jump_table().init_version)(major_version, minor_version, init_config);

    static_assert(sizeof(init_config->cpu_mask.rmax_bits[0]) <= sizeof(unsigned long long), "cpu dumping needs to be updated!");
    auto cpu_mask = std::bitset<sizeof(init_config->cpu_mask.rmax_bits) / sizeof(std::uint8_t) * 8>{};
    boost::mp11::mp_for_each<boost::mp11::mp_iota_c<sizeof(init_config->cpu_mask.rmax_bits)/sizeof(init_config->cpu_mask.rmax_bits[0])>>([&](auto I) {
       cpu_mask |= (decltype(cpu_mask){init_config->cpu_mask.rmax_bits[I]} << I * sizeof(sizeof(init_config->cpu_mask.rmax_bits[0]) / sizeof(std::uint8_t) * 8));
    });

    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_init_version(" << major_version << ',' << minor_version << ',' << cpu_mask.to_string() << ',' << init_config->flags << "}) -> " << rmaxx::to_string(res);
    return res;
}

rmax_status_t log_rmax_init(struct rmax_init_config* init_config) {
    auto res = (*pop_jump_table().init)(init_config);

    static_assert(sizeof(init_config->cpu_mask.rmax_bits[0]) <= sizeof(unsigned long long), "cpu dumping needs to be updated!");
    auto cpu_mask = std::bitset<sizeof(init_config->cpu_mask.rmax_bits) / sizeof(std::uint8_t) * 8>{};
    boost::mp11::mp_for_each<boost::mp11::mp_iota_c<sizeof(init_config->cpu_mask.rmax_bits)/sizeof(init_config->cpu_mask.rmax_bits[0])>>([&](auto I) {
       cpu_mask |= (decltype(cpu_mask){init_config->cpu_mask.rmax_bits[I]} << I * sizeof(sizeof(init_config->cpu_mask.rmax_bits[0]) / sizeof(std::uint8_t) * 8));
    });

    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_init(" << cpu_mask.to_string() << ',' << init_config->flags << "}) -> " << rmaxx::to_string(res);
    return res;
}

namespace {
std::string to_string(rmax_in_flow_attr const& in_flow_attr) {
    std::ostringstream out;
    out << "\tlocal_addr : " << to_string(in_flow_attr.local_addr)
     << "\n\tremote_addr : " << to_string(in_flow_attr.remote_addr)
     << "\n\tflow_id : " << in_flow_attr.flow_id;
    return out.str();
}
} // namespace

rmax_status_t log_rmax_in_create_stream(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, rmax_in_timestamp_format timestamp_format, rmax_in_flags flags, rmax_stream_id* stream_id)
{
    auto res = (*pop_jump_table().in_create_stream)(rx_type, local_nic_addr, buffer_attr, timestamp_format, flags, stream_id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_create_stream(" << rx_type << ',' << to_string(*local_nic_addr) << "...) -> " << rmaxx::to_string(res);
    return res;
}
rmax_status_t log_rmax_in_destroy_stream(rmax_stream_id id)
{
    auto res = (*pop_jump_table().in_destroy_stream)(id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_destroy_stream(" << id << ") -> " << rmaxx::to_string(res);
    return res;
}

rmax_status_t log_rmax_in_attach_flow(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr) {
    auto res = (*pop_jump_table().in_attach_flow)(id, flow_attr);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_attach_flow(" << id << ',' << to_string(*flow_attr) << ") -> " << rmaxx::to_string(res);
    return res;
}

rmax_status_t log_rmax_in_detach_flow(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr) {
    auto res = (*pop_jump_table().in_detach_flow)(id, flow_attr);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_detach_flow(" << id << ',' << to_string(*flow_attr) << ") -> " << rmaxx::to_string(res);
    return res;
}

rmax_status_t log_rmax_in_get_next_chunk(rmax_stream_id id, size_t min_chunk_size_in_strides, size_t max_chunk_size_in_strides, int timeout, int flags, struct rmax_in_completion* rx_completion) {
    auto res = (*pop_jump_table().in_get_next_chunk)(id, min_chunk_size_in_strides, max_chunk_size_in_strides, timeout, flags, rx_completion);
    if (res == RMAX_OK) {
       auto received = std::string{};
      if (rx_completion->chunk_size != 0) {
         std::ostringstream out;
         out << rx_completion->chunk_size << " - packets received";
         if (rx_completion->chunk_size != 0) {
            auto size = std::size_t{0};
            out << '[';
            for (auto i = decltype(rx_completion->chunk_size){0}; i != rx_completion->chunk_size; ++i) {
               // auto header = static_cast<char const*>(rx_completion->data_ptr) + i * rx.stride_size();
               // auto payload = header + rx_completion->packet_info_arr[i].hdr_size;
               // size_t const data_size = rx_completion->packet_info_arr[i].data_size;
               // stats.mark_processed(data_size);
               out << rx_completion->packet_info_arr[i].hdr_size << ':' << rx_completion->packet_info_arr[i].data_size << ',';
               size += rx_completion->packet_info_arr[i].hdr_size;
               size += rx_completion->packet_info_arr[i].data_size;
            }
            received = out.str();
            received.back() = ']';
            received += " = ";
            received += std::to_string(size);
         }
      }
      BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_get_next_chunk(" << id << ',' << min_chunk_size_in_strides << ',' << max_chunk_size_in_strides << ',' << timeout << ',' << flags << ") -> " << rmaxx::to_string(res)
                                                        << "\t\t\t" << received;
    } 
    else
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_in_get_next_chunk(" << id << ',' << min_chunk_size_in_strides << ',' << max_chunk_size_in_strides << ',' << timeout << ',' << flags << ") -> " << rmaxx::to_string(res);
    return res;
}

namespace {
std::string to_string(rmax_out_gen_stream_params const& params)
{
    std::ostringstream out;
    out << "\n\tlocal_addr : " << (params.local_addr == nullptr ? std::string{"<null>"} : to_string(*params.local_addr))
        << "\n\tmax_chunk_size : " << params.max_chunk_size << "  // max number of packets per chunk"
        << "\n\topt_field_mask : " << params.opt_field_mask
        << "\n\tremote_addr : " << (params.remote_addr == nullptr ? std::string{"<null>"} : to_string(*params.remote_addr))
        << "\n\trate : {...}" /*<< params.rate*/
        << "\n\tqos : {" << static_cast<int>(params.qos.dscp) << ',' << static_cast<int>(params.qos.pcp) << '}'
        << "\n\tecn : " << int(params.ecn) << "  //  Explicit Congestion Notification field value."
        << "\n\tmax_iovec_num : " << params.max_iovec_num << " // max io vector entries number for a single packet, default is 2 entries"
        << "\n\tsize_in_chunks : " << params.size_in_chunks << " // Stream size in chunks. Default is: (32k / @max_chunk_size)"
        << "\n\tcomp_q_prop : {" 
            << "\n\t\trate_bps : " << params.comp_q_prop.rate_bps
            << "\n\t\tmax_burst_in_pkt_num : "<< params.comp_q_prop.max_burst_in_pkt_num
            << "\n\t\ttypical_packet_sz : "<< params.comp_q_prop.typical_packet_sz
            << "\n\t\tmin_packet_sz : "<< params.comp_q_prop.min_packet_sz
            << "\n\t\tmax_packet_sz : "<< params.comp_q_prop.max_packet_sz       
        << "\n}\n\tlags : " << params.flags;
    return out.str();
}

}

rmax_status_t log_rmax_out_create_gen_stream(struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id) {
    auto res = (*pop_jump_table().out_create_gen_stream)(params, stream_id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_out_create_gen_stream(" << to_string(*params) << ") -> [" << rmaxx::to_string(res) << ',' << *stream_id << ']';
    //if (res == RMAX_OK) {
    //  if (!out_create_gen_stream.emplace(std::make_pair(*stream_id, std::make_unique<stream_info>(*params))).second)
    //     BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "out gen stream with id : " << *stream_id << " already in cache!";
    //}
    return res;
}

rmax_status_t log_rmax_out_destroy_stream(rmax_stream_id id) {
    auto res = (*pop_jump_table().out_destroy_stream)(id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_out_destroy_stream(" << id << ") -> " << rmaxx::to_string(res);
    //if (res == RMAX_OK) {
    //   if(out_create_gen_stream.erase(id))
    //      BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "can't find out stream to delete :" << id  ;
    //}
    return res;
}


rmax_status_t log_rmax_register_memory(void* addr, size_t length, struct in_addr dev_addr, rmax_mkey_id* id) {
    auto res = (*pop_jump_table().register_memory)(addr, length, dev_addr, id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_register_memory(" << addr << ',' << length << ',' << to_string(dev_addr) << ") -> [" << rmaxx::to_string(res) << ',' << *id << ']';
    return res;
}

rmax_status_t log_rmax_deregister_memory(rmax_mkey_id id, struct in_addr dev_addr)
{
    auto res = (*pop_jump_table().deregister_memory)(id, dev_addr);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_deregister_memory(" << id << ',' << to_string(dev_addr) << ") -> " << rmaxx::to_string(res);
    return res;
}

namespace {

std::string to_string(rmax_chunk const* chunk, bool dump = false) {
    if (chunk == nullptr)
       return "<null>";
    std::ostringstream out;
    out << chunk->size << " - packets [";
    for (decltype(chunk->size) packet = 0; packet != chunk->size; ++packet) {
        auto size = std::size_t{0};
        for (decltype(chunk->packets[packet].count) io = 0; io != chunk->packets[packet].count; ++io) {
            size += chunk->packets[packet].iovec[io].length;
            if (dump) {
                out << '{';
                auto begin = reinterpret_cast<char const*>(chunk->packets[packet].iovec[io].addr);
                boost::algorithm::hex(begin, begin + chunk->packets[packet].iovec[io].length,
                                      std::ostream_iterator<char>{out, ","});
                out << "} ";
            }
        }
        out << packet << ':' << size << ',';
    }
    auto res = out.str();
    if(res.back() != '[')
        res += ']';
    else
        res.back() = ']';
    return res;
}

}

rmax_status_t log_rmax_out_commit_chunk(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags) try {
    //if (chunk == nullptr) {
    //   BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "rmax_out_commit_chunk " << id << " nullptr !";
    //   wait_for_debbuger_if_configured();
    //}
    //auto sentry = stream_sentry{id};
    //auto guard = std::scoped_lock<stream_sentry>{sentry};
    //auto chunks_dump = to_string(chunk);
    auto res = (*pop_jump_table().out_commit_chunk)(id, time, chunk, flags);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_out_commit_chunk(" << id << ',' << time << ", {" << chunk->size << ".... }," << flags << ") -> " << rmaxx::to_string(res)
                                                         //<< "\t\t\t" << chunks_dump
                                                        ;
    return res;
} catch (...) {
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_out_commit_chunk failed...";
    wait_for_debbuger_if_configured();
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::fatal) << "rmax_out_commit_chunk(" << id << ',' << time << ",...," << flags << ") " << to_string(chunk, true);
    throw;
}

rmax_status_t log_rmax_out_commit_chunk_to(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags, struct sockaddr* to_addr) try {
    //if (chunk == nullptr) {
    //   BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "rmax_out_commit_chunk_to " << id << " nullptr !";
    //   wait_for_debbuger_if_configured();
    //}

    //auto chunks_dump = to_string(chunk);
    auto res = (*pop_jump_table().out_commit_chunk_to)(id, time, chunk, flags, to_addr);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_out_commit_chunk_to(" << id << ',' << time << ", {" << chunk->size << ".... ," << flags << ',' << to_addr << ") -> " << rmaxx::to_string(res)
                                                      //<< "\t\t\t" << chunks_dump
                                                       ;
    return res;
} catch (...) {
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_out_commit_chunk_to failed...";
    wait_for_debbuger_if_configured();
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::fatal) << "rmax_out_commit_chunk_to(" << id << ',' << time << ",...," << flags << ") " << to_string(chunk, true);
    throw;
}


rmax_status_t log_rmax_in_query_buffer_size(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, size_t* payload_size, size_t* header_size)
{
    auto res = (*pop_jump_table().in_query_buffer_size)(rx_type, local_nic_addr, buffer_attr, payload_size, header_size);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_in_query_buffer_size(" << rx_type << ',' << to_string(*local_nic_addr) << "...) -> [" << rmaxx::to_string(res)
                                                            << ',' << *payload_size << ',' << *header_size << ']';
    return res;
}

rmax_status_t log_rmax_out_cancel_unsent_chunks(rmax_stream_id id)
{
    auto res = (*pop_jump_table().out_cancel_unsent_chunks)(id);
    BOOST_LOG_SEV(logger::get(), severity_level(res)) << "rmax_out_cancel_unsent_chunks(" << id << ") -> " << rmaxx::to_string(res);
    return res;
}


rmax_status_t log_error_rmax_out_commit_chunk(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags) try {
    auto res = (*pop_jump_table().out_commit_chunk)(id, time, chunk, flags);
    if (res != RMAX_OK)
        BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_out_commit_chunk(" << id << ',' << time << ", {" << chunk->size << ".... }," << flags << ") -> " << rmaxx::to_string(res)
        //<< "\t\t\t" << chunks_dump
        ;
    return res;
} catch (...) {
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "rmax_out_commit_chunk failed...";
    wait_for_debbuger_if_configured();
    BOOST_LOG_SEV(logger::get(), boost::log::trivial::fatal) << "rmax_out_commit_chunk(" << id << ',' << time << ",...," << flags << ") " << to_string(chunk, true);
    throw;
}
