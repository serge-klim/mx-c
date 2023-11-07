#pragma once
////#include "rivermax_api.h"
#include "rmaxx/rmaxx_utils.hpp"
#include "diversion.hpp"


struct rmax_jump_table
{
   decltype(&::rmax_cleanup) cleanup;
   decltype(&::rmax_get_version_string) get_version_string;
   //decltype(&::rmax_get_version) get_version;
   //decltype(&::rmax_init_version) init_version;
   //decltype(&::rmax_init) init;
   decltype(&diversion_rmax_get_version) get_version;
   decltype(&diversion_rmax_init_version) init_version;
   decltype(&diversion_rmax_init) init;
   decltype(&::rmax_in_create_stream) in_create_stream;
   decltype(&::rmax_in_destroy_stream) in_destroy_stream;
   decltype(&::rmax_in_attach_flow) in_attach_flow;
   decltype(&::rmax_in_detach_flow) in_detach_flow;
   decltype(&::rmax_in_get_next_chunk) in_get_next_chunk;
   decltype(&::rmax_out_create_gen_stream) out_create_gen_stream;
   decltype(&::rmax_out_destroy_stream) out_destroy_stream;
   decltype(&::rmax_register_memory) register_memory;
   decltype(&::rmax_deregister_memory) deregister_memory;
   decltype(&::rmax_out_commit_chunk) out_commit_chunk;
   decltype(&::rmax_out_commit_chunk_to) out_commit_chunk_to;
   decltype(&::rmax_in_query_buffer_size) in_query_buffer_size;
   decltype(&::rmax_out_cancel_unsent_chunks) out_cancel_unsent_chunks;

   //decltype(&::rmax_cleanup) cleanup = nullptr;
   //decltype(&::rmax_get_version_string) get_version_string = nullptr;
   //decltype(&diversion_rmax_get_version) get_version = &diversion_rmax_get_version;
   //decltype(&diversion_rmax_init_version) init_version = &diversion_rmax_init_version;
   //decltype(&diversion_rmax_init) init = &diversion_rmax_init;
   //decltype(&::rmax_in_create_stream) in_create_stream = nullptr;
   //decltype(&::rmax_in_destroy_stream) in_destroy_stream = nullptr;
   //decltype(&::rmax_in_attach_flow) in_attach_flow = nullptr;
   //decltype(&::rmax_in_detach_flow) in_detach_flow = nullptr;
   //decltype(&::rmax_in_get_next_chunk) in_get_next_chunk = nullptr;
   //decltype(&::rmax_out_create_gen_stream) out_create_gen_stream = nullptr;
   //decltype(&::rmax_out_destroy_stream) out_destroy_stream = nullptr;
   //decltype(&::rmax_register_memory) register_memory = nullptr;
   //decltype(&::rmax_deregister_memory) deregister_memory = nullptr;
   //decltype(&::rmax_out_commit_chunk) out_commit_chunk = nullptr;
   //decltype(&::rmax_out_commit_chunk_to) out_commit_chunk_to = nullptr;
   //decltype(&::rmax_in_query_buffer_size) in_query_buffer_size = nullptr;
   //decltype(&::rmax_out_cancel_unsent_chunks) out_cancel_unsent_chunks = nullptr;
};


rmax_jump_table const& pop_jump_table() noexcept;

