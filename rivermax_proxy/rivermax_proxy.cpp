#include "loggers.hpp"
#include "utility.hpp"
#include "jump_table.hpp"
#include <boost/system/system_error.hpp>
#include <boost/winapi/dll.hpp>
#include <boost/winapi/get_last_error.hpp>
#include <boost/winapi/get_proc_address.hpp>
#include <boost/program_options.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/adapted/struct/detail/extension.hpp>
#include <boost/fusion/include/size.hpp>
#include <boost/fusion/include/value_at.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/spirit/home/x3.hpp>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <array>
#include <memory>
#include <type_traits>
#include <cassert>


void wait_for_debbuger()
{
   BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "waiting for debugger to attach....";
   boost::log::core::get()->flush();
   while (!IsDebuggerPresent())
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
   DebugBreak();
}


//struct rmax_jump_table
//{
//	decltype(&::rmax_cleanup                  ) cleanup                  ;
//    decltype(&::rmax_get_version_string       ) get_version_string       ;
//	decltype(&::rmax_get_version              ) get_version              ;
//	decltype(&::rmax_init_version             ) init_version             ;
//	decltype(&::rmax_in_create_stream         ) in_create_stream         ;
//	decltype(&::rmax_in_destroy_stream        ) in_destroy_stream        ;
//	decltype(&::rmax_in_attach_flow           ) in_attach_flow           ;
//	decltype(&::rmax_in_detach_flow           ) in_detach_flow           ;
//	decltype(&::rmax_in_get_next_chunk        ) in_get_next_chunk        ;
//	decltype(&::rmax_out_create_gen_stream    ) out_create_gen_stream    ;
//	decltype(&::rmax_out_destroy_stream       ) out_destroy_stream       ;
//	decltype(&::rmax_register_memory          ) register_memory          ;
//	decltype(&::rmax_deregister_memory        ) deregister_memory        ;
//	decltype(&::rmax_out_commit_chunk         ) out_commit_chunk         ;
//	decltype(&::rmax_out_commit_chunk_to      ) out_commit_chunk_to      ;
//	decltype(&::rmax_in_query_buffer_size     ) in_query_buffer_size     ;
//	decltype(&::rmax_out_cancel_unsent_chunks) out_cancel_unsent_chunks  ;
//
//   ////rmax_status_t (*rmax_cleanup)(void);
//   //decltype(&::rmax_cleanup) rmax_cleanup;
//   //const char*   (*rmax_get_version_string)();
//   //rmax_status_t (*rmax_get_version)(unsigned* major_version, unsigned* minor_version, unsigned* release_number, unsigned* build);
//   //rmax_status_t (*rmax_init_version)(unsigned major_version, unsigned minor_version, struct rmax_init_config* init_config);
//   //rmax_status_t (*rmax_in_create_stream)(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, rmax_in_timestamp_format timestamp_format, rmax_in_flags flags, rmax_stream_id* stream_id);
//   //rmax_status_t (*rmax_in_destroy_stream)(rmax_stream_id id);
//   //rmax_status_t (*rmax_in_attach_flow)(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr);
//   //rmax_status_t (*rmax_in_detach_flow)(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr);
//   //rmax_status_t (*rmax_in_get_next_chunk)(rmax_stream_id id, size_t min_chunk_size_in_strides, size_t max_chunk_size_in_strides, int timeout, int flags, struct rmax_in_completion* rx_completion);
//   //rmax_status_t (*rmax_out_create_gen_stream)(struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id);
//   //rmax_status_t (*rmax_out_destroy_stream)(rmax_stream_id id);
//   //rmax_status_t (*rmax_register_memory)(void* addr, size_t length, struct in_addr dev_addr, rmax_mkey_id* id);
//   //rmax_status_t (*rmax_deregister_memory)(rmax_mkey_id id, struct in_addr dev_addr);
//   //rmax_status_t (*rmax_out_commit_chunk)(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags);
//   //rmax_status_t (*rmax_out_commit_chunk_to)(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags, struct sockaddr* to_addr);
//   //rmax_status_t (*rmax_in_query_buffer_size)(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, size_t* payload_size, size_t* header_size);
//   //rmax_status_t (*rmax_out_cancel_unsent_chunks)(rmax_stream_id id);
//
//	//FARPROC rmax_cleanup                  ;
//	//FARPROC rmax_get_version              ;
//	//FARPROC rmax_init_version             ;
//	//FARPROC rmax_in_create_stream         ;
//	//FARPROC rmax_in_destroy_stream        ;
//	//FARPROC rmax_in_attach_flow           ;
//	//FARPROC rmax_in_detach_flow           ;
//	//FARPROC rmax_in_get_next_chunk        ;
//	//FARPROC rmax_out_create_gen_stream    ;
//	//FARPROC rmax_out_destroy_stream       ;
//	//FARPROC rmax_register_memory          ;
//	//FARPROC rmax_deregister_memory        ;
//	//FARPROC rmax_out_commit_chunk         ;
//	//FARPROC rmax_out_commit_chunk_to      ;
//	//FARPROC rmax_in_query_buffer_size     ;
//	//FARPROC rmax_out_cancel_unsent_chunks ;
//};

BOOST_FUSION_ADAPT_STRUCT(rmax_jump_table,
                          cleanup,
                          get_version_string,
                          get_version,
                          init_version,
                          init,
                          in_create_stream,
                          in_destroy_stream,
                          in_attach_flow,
                          in_detach_flow,
                          in_get_next_chunk,
                          out_create_gen_stream,
                          out_destroy_stream,
                          register_memory,
                          deregister_memory,
                          out_commit_chunk,
                          out_commit_chunk_to,
                          in_query_buffer_size,
                          out_cancel_unsent_chunks
	)

//extern "C" void jmp_to(FARPROC);
//extern "C" void proxy_rmax_get_version_string();

namespace {
std::array<rmax_jump_table, 5 /*depth*/> jump_table{0};
//rmax_jump_table jump_table;
thread_local std::size_t jump_table_ix = 0;

struct
{
   struct
   {
       unsigned major = 0;
       unsigned minor = 0;
       unsigned release_number = 0;
       unsigned build = 0;
   } original, imposed;
} rivermax_version;
}

rmax_jump_table const& enty_jump_table() noexcept {
   jump_table_ix = 0;
   return jump_table[0];
}

rmax_jump_table const& pop_jump_table() noexcept
{
   assert(jump_table_ix + 1 < jump_table.size());
   return jump_table[++jump_table_ix];
}

template <typename F>
F& original_call_slot(F rmax_jump_table::*f) noexcept
{
   auto begin = std::begin(jump_table);
   auto entry = std::find_if(begin, end(jump_table), [f](auto const& entry) {
      return (entry.*f) == nullptr;
   });
   assert(entry != begin);
   --entry;
   return (*entry).*f;
}


void configure_environment_variables() {
   auto setenv = [](const char* name, const char* value) noexcept {
       if (!SetEnvironmentVariable(name, value)) {
          auto error = boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category());
          BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "unable to set environment variable" << name << '=' << value << ':' << error.message();
      }
   };
   if (auto const& rivermax_log_file = config()["rivermax_log_file"]; !rivermax_log_file.empty())
      //_putenv(("RIVERMAX_LOG_FILE =" + rivermax_log_file.as<std::string>()).c_str());
      setenv("RIVERMAX_LOG_FILE", rivermax_log_file.as<std::string>().c_str());
   if (config()["verbose"].as<bool>()) {
      //_putenv("RIVERMAX_LOG_LEVEL=6");
      //_putenv("RIVERMAX_DISABLE_STDOUT_LOG=1");
      setenv("RIVERMAX_LOG_LEVEL", "6");
      setenv("RIVERMAX_DISABLE_STDOUT_LOG", "1");
      if (auto environment = std::unique_ptr<char, decltype(&FreeEnvironmentStringsA)>{GetEnvironmentStringsA(), &FreeEnvironmentStringsA}) {
         auto variable = environment.get();
         do {
            auto view = std::string_view{variable};
            if (boost::algorithm::icontains(view, "RIVERMAX"))
               BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << view;
            variable += view.length() + 1;
         } while (*variable != '\0');
      }
   }
}

 boost::system::error_code initialize(HMODULE module) {
   configure_environment_variables();
   auto res = boost::system::error_code{};
   auto const rivermax_path = config()["rivermax_dll"].as<std::string>();
   auto rivermax = boost::winapi::load_library_ex(rivermax_path.c_str(), 0, LOAD_LIBRARY_SEARCH_SYSTEM32 /*| LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR*/);
   if (!rivermax) {
      res = boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category());
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::fatal) << "can't load rivermax.dll [" << rivermax_path << "] : " << res.message();
      return res;
   }

   jump_table[0].get_version = &diversion_rmax_get_version;
   jump_table[0].init_version = &diversion_rmax_init_version;
   jump_table[0].init = &diversion_rmax_init;


   auto const trace = config()["enable-tracing"].as<bool>();
   auto const rio = config()["rio"].as<bool>();
   boost::mp11::mp_for_each<boost::mp11::mp_iota_c<boost::fusion::result_of::size<rmax_jump_table /*decltype(jump_table)*/>::value>>([&res, module, rivermax, trace, rio](auto Ix) {      
      static auto const prefix = std::string{"rmax_"};
      auto const name = prefix + boost::fusion::extension::struct_member_name<rmax_jump_table /*decltype(jump_table)*/, Ix>::call();
      {
         if(auto f = boost::winapi::get_proc_address(rivermax, name.c_str()))
            boost::fusion::at_c<Ix>(jump_table[0]) = reinterpret_cast<typename boost::fusion::result_of::value_at_c<rmax_jump_table /*decltype(jump_table)*/, Ix>::type>(f);
         if(!boost::fusion::at_c<Ix>(jump_table[0]))
            res = boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category());
      }
      auto redirect_level = std::size_t{0};
      if (trace) {
         if (auto tracer = reinterpret_cast<typename boost::fusion::result_of::value_at_c<rmax_jump_table /*decltype(jump_table)*/, Ix>::type>(boost::winapi::get_proc_address(module, ("log_" + name).c_str()))) {
            assert(redirect_level + 1 < jump_table.size());
            std::swap(boost::fusion::at_c<Ix>(jump_table[redirect_level]), tracer);
            boost::fusion::at_c<Ix>(jump_table[++redirect_level]) = tracer;
         } else 
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "unable to find tracer for \"" << name << '"';
      }
      if (rio) {
         if (auto f = reinterpret_cast<typename boost::fusion::result_of::value_at_c<rmax_jump_table /*decltype(jump_table)*/, Ix>::type>(boost::winapi::get_proc_address(module, ("rio_" + name).c_str()))) {
            assert(redirect_level + 1 < jump_table.size());
            std::swap(boost::fusion::at_c<Ix>(jump_table[redirect_level]), f);
            boost::fusion::at_c<Ix>(jump_table[++redirect_level]) = f;
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << '"' << name << "\" will be replaced by \"rio_" << name << "\"...";
         }
      }
      if (auto override = config()["override." + name]; !override.empty()) {
         auto const& override_name = override.template as<std::string>();
         if(auto f = reinterpret_cast<typename boost::fusion::result_of::value_at_c<rmax_jump_table /*decltype(jump_table)*/, Ix>::type>(boost::winapi::get_proc_address(module, override_name.c_str())))
         {
            assert(redirect_level + 1 < jump_table.size());
            std::swap(boost::fusion::at_c<Ix>(jump_table[redirect_level]), f);
            boost::fusion::at_c<Ix>(jump_table[++redirect_level]) = f;
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << '"' << name << "\" will be replaced by \"" << override_name << "\"...";
         } else {
            auto error = boost::system::error_code(boost::winapi::GetLastError(), boost::system::system_category());
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "unable to find \"" << override_name << "\" : " << error.message();
         }
      }
   });

   if (!res) {
      auto& get_version = original_call_slot(&rmax_jump_table::get_version);

      /* Verify version mismatch */
      (*get_version)(&rivermax_version.original.major, &rivermax_version.original.minor, &rivermax_version.original.release_number, &rivermax_version.original.build);
      assert(rivermax_version.original.major != 0);
      if (rivermax_version.original.major != RMAX_API_MAJOR || rivermax_version.original.minor < RMAX_API_MINOR) {
         if (!config()["ignore-version-mismatch"].as<bool>()) {
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "Rivermax version mismatch , this proxy compiled for " << RMAX_API_MAJOR << '.' << RMAX_API_MINOR << '.' << RMAX_RELEASE_VERSION << '.' << RMAX_BUILD
                                                                     << "\n but \"" << rivermax_path << "\" is " << rivermax_version.original.major << '.' << rivermax_version.original.minor << '.' 
                                                                     << rivermax_version.original.release_number << '.' << rivermax_version.original.build
                                                                     << "\n please set ignore-version-mismatch to true in the configuration file : \"" << rivermax_path << ".config\" to ignore versions mismatch ";

            return make_error_code(boost::system::errc::not_supported);
         }
         BOOST_LOG_SEV(logger::get(), boost::log::trivial::warning) << "WARNING! Rivermax version mismatch , this proxy compiled for " << RMAX_API_MAJOR << '.' << RMAX_API_MINOR << '.' << RMAX_RELEASE_VERSION << '.' << RMAX_BUILD
                                                                    << "\n but \"" << rivermax_path << "\" is " << rivermax_version.original.major << '.' << rivermax_version.original.minor << '.'
                                                                    << rivermax_version.original.release_number << '.' << rivermax_version.original.build;
      }
      rivermax_version.imposed = rivermax_version.original;
      if (auto const version = config()["impose-version"]; !version.empty()) {
         auto version_string = version.as<std::string>();
         auto begin = cbegin(version_string);
         auto end = cend(version_string);
         auto ver = std::vector<unsigned int>{};
         if (boost::spirit::x3::parse(begin, end, boost::spirit::x3::uint_ % '.', ver) && begin == end && ver.size() > 1) {
            ver.resize(4);
            rivermax_version.imposed.major = ver[0];
            rivermax_version.imposed.minor = ver[1];
            rivermax_version.imposed.release_number = ver[2];
            rivermax_version.imposed.build = ver[3];
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "imposing rmax_get_version ("
                                                                       << rivermax_version.imposed.major << '.'
                                                                       << rivermax_version.imposed.minor << '.'
                                                                       << rivermax_version.imposed.release_number << '.'
                                                                       << rivermax_version.imposed.build << ')';

            get_version = &diversion_rmax_get_version;

         } else {
            BOOST_LOG_SEV(logger::get(), boost::log::trivial::error) << "unable to parse configured version \"" << version_string << " ignoring it...";
         }
      }

   }
   return res;
}

////////////////////////////////////////////////////////////////////////////////////


rmax_status_t diversion_rmax_init(struct rmax_init_config* init_config)
{
   auto init_version = original_call_slot(&rmax_jump_table::init_version);
   return (*init_version)(rivermax_version.original.major, rivermax_version.original.minor, init_config);
}

rmax_status_t diversion_rmax_init_version(unsigned /*api_major_version*/,
                                          unsigned /*api_minor_version*/,
                                          struct rmax_init_config* init_config)
{
   auto init_version = original_call_slot(&rmax_jump_table::init);
   return (*init_version)(init_config);
}

rmax_status_t diversion_rmax_get_version(unsigned* major_version, unsigned* minor_version,
                                         unsigned* release_number, unsigned* /*build*/)
{
   *major_version = rivermax_version.imposed.major;
   *minor_version = rivermax_version.imposed.minor;
   *release_number = rivermax_version.imposed.release_number;

   return RMAX_OK;
}

rmax_status_t diversion_rmax_get_version_long(unsigned* major_version, unsigned* minor_version,
                                              unsigned* release_number, unsigned* build)
{
   *major_version = rivermax_version.imposed.major;
   *minor_version = rivermax_version.imposed.minor;
   *release_number = rivermax_version.imposed.release_number;
   *build = rivermax_version.imposed.build; 
   return RMAX_OK;
}

////////////////////////////////////////////////////////////////////////////////////
const char* proxy_rmax_get_version_string()
{
   configure_environment_variables();
   return (*enty_jump_table().get_version_string)();
   
   //rmax_jump_table jump_table;
   //jump_table.rmax_get_version = (FARPROC)0xa000000f000000e;
   //    //   __asm { jmp jump_table.rmax_get_version }
   //jmp_to(jump_table.rmax_get_version);
   //return nullptr;
}

rmax_status_t proxy_rmax_cleanup() 
{ 
	return (*enty_jump_table().cleanup)(); 
}

rmax_status_t proxy_rmax_get_version(unsigned* major_version, unsigned* minor_version, unsigned* release_number, unsigned* build) 
{  
    configure_environment_variables();
	return (*enty_jump_table().get_version)(major_version,minor_version, release_number, build);
}

rmax_status_t proxy_rmax_init_version(unsigned major_version, unsigned minor_version, struct rmax_init_config* init_config)
{
   configure_environment_variables();
   if (rivermax_version.imposed.major != 0) {
      BOOST_LOG_SEV(logger::get(), boost::log::trivial::info) << "rmax_init_version(" << major_version << ',' << minor_version << "...) -> (" << rivermax_version.imposed.major << ',' << rivermax_version.imposed.minor << "...)";
      major_version = rivermax_version.original.major;
      minor_version = rivermax_version.original.minor;
   }
   return (*enty_jump_table().init_version)(major_version, minor_version, init_config);
}

rmax_status_t proxy_rmax_init(struct rmax_init_config* init_config)
{
   configure_environment_variables();
   return (*enty_jump_table().init)(init_config);
}

rmax_status_t proxy_rmax_in_destroy_stream(rmax_stream_id id) 
{ 
   return (*enty_jump_table().in_destroy_stream)(id);
}
rmax_status_t proxy_rmax_in_create_stream(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, rmax_in_timestamp_format timestamp_format, rmax_in_flags flags, rmax_stream_id* stream_id) 
{
   return (*enty_jump_table().in_create_stream)(rx_type, local_nic_addr, buffer_attr, timestamp_format, flags, stream_id);
}
rmax_status_t proxy_rmax_in_attach_flow(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr) 
{
   return (*enty_jump_table().in_attach_flow)(id, flow_attr);
}
rmax_status_t proxy_rmax_in_detach_flow(rmax_stream_id id, struct rmax_in_flow_attr* flow_attr)
{
   return (*enty_jump_table().in_detach_flow)(id, flow_attr);
}
rmax_status_t proxy_rmax_in_get_next_chunk(rmax_stream_id id, size_t min_chunk_size_in_strides, size_t max_chunk_size_in_strides, int timeout, int flags, struct rmax_in_completion* rx_completion)
{
   return (*enty_jump_table().in_get_next_chunk)(id, min_chunk_size_in_strides, max_chunk_size_in_strides, timeout, flags, rx_completion);
}
rmax_status_t proxy_rmax_out_create_gen_stream(struct rmax_out_gen_stream_params* params, rmax_stream_id* stream_id) 
{ 
	return (*enty_jump_table().out_create_gen_stream)(params, stream_id);
}
rmax_status_t proxy_rmax_out_destroy_stream(rmax_stream_id id)
{
   return (*enty_jump_table().out_destroy_stream)(id);
}
rmax_status_t proxy_rmax_register_memory(void* addr, size_t length, struct in_addr dev_addr, rmax_mkey_id* id)
{
   return (*enty_jump_table().register_memory)(addr, length, dev_addr, id);
}
rmax_status_t proxy_rmax_deregister_memory(rmax_mkey_id id, struct in_addr dev_addr)
{
   return (*enty_jump_table().deregister_memory)(id, dev_addr);
}
rmax_status_t proxy_rmax_out_commit_chunk(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags)
{
   return (*enty_jump_table().out_commit_chunk)(id, time, chunk, flags);
}
rmax_status_t proxy_rmax_out_commit_chunk_to(rmax_stream_id id, uint64_t time, struct rmax_chunk* chunk, rmax_commit_flags_t flags, struct sockaddr* to_addr)
{
   return (*enty_jump_table().out_commit_chunk_to)(id, time, chunk, flags, to_addr);
}
rmax_status_t proxy_rmax_in_query_buffer_size(rmax_in_stream_type rx_type, struct sockaddr_in* local_nic_addr, struct rmax_in_buffer_attr* buffer_attr, size_t* payload_size, size_t* header_size)
{
   return (*enty_jump_table().in_query_buffer_size)(rx_type, local_nic_addr, buffer_attr, payload_size, header_size);
}
rmax_status_t proxy_rmax_out_cancel_unsent_chunks(rmax_stream_id id)
{
   return (*enty_jump_table().out_cancel_unsent_chunks)(id);
}


