#include "utility.hpp"
#include "Windows.h"
#include <string>
#include <fstream>


BOOL APIENTRY DllMain(HMODULE module, DWORD reason_for_call, LPVOID)
{
   switch (reason_for_call) {
      case DLL_PROCESS_ATTACH: {
         auto description = boost::program_options::options_description{};
         // clang-format off
            description.add_options()
                ("wait-for-debugger", boost::program_options::value<bool>()->default_value(false), "\twait for debugger")
                ("rivermax_dll", boost::program_options::value<std::string>()->default_value("C:\\Windows\\System32\\rivermax.dll"), "\tpath to rivermax.dll to load")
                ("verbose", boost::program_options::value<bool>()->default_value(false), "\tverbose")
                ("ignore-version-mismatch", boost::program_options::value<bool>()->default_value(false), "\tignore version mismatch")
                ("enable-tracing", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\ttrace rivermax calls")
                ("cpu-mask", boost::program_options::value<std::string>(), "\tcpu mask")
                ("min-max_chunk_size", boost::program_options::value<std::size_t>()->default_value(0), "\tmax number of packets per chunk")
                ("min-max_iovec_num", boost::program_options::value<std::size_t>()->default_value(0), "\tmax io vector entries number for a single packet, default is 2 entries")
                ("min-size_in_chunks", boost::program_options::value<std::size_t>()->default_value(0), "\tStream size in chunks. Default is: (32k / @max_chunk_size)")
                ("max-streams", boost::program_options::value<std::size_t>()->default_value(3), "\tStream size in chunks. Default is: (32k / @max_chunk_size)")
                ("max-payload-size", boost::program_options::value<std::size_t>()->default_value(1408), "\tmax payload size")

                ("rio", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\tenable rio layer: send via RIO sockets")
                ("rio.max_n_out_gen_streams", boost::program_options::value<std::size_t>()->default_value(10),"\tmax out stream number")
                ("rio.numa_node", boost::program_options::value<unsigned long>(),"\tnuma node")
                ("rio.prefer_huge_pages", boost::program_options::value<bool>()->default_value(true),"\tuse huge pages")
                ("rio.loopback", boost::program_options::value<bool>()->default_value(false),"\tenable loopback")
                ("rio.ttl", boost::program_options::value<unsigned short>(),"\ttl")
                ;
         // clang-format on			
         auto& vm = config();
         char const* argv[] = {"rivermax.dll"};
         store(boost::program_options::command_line_parser(
                 sizeof(argv)/sizeof(argv[0]), argv)
                 .options(description) /*.positional(positional)*/.run(),
             vm);
         
          auto config_filename = std::string(std::string::size_type{MAX_PATH}, '\0');
          if (auto size = GetModuleFileName(module, config_filename.data(), static_cast<DWORD>(config_filename.size())); size != 0 && size < config_filename.size()) {
             config_filename.resize(size);
             config_filename += ".config";
             if (auto ifs = std::fstream{ config_filename }) {
                boost::program_options::options_description config_file_options;
                config_file_options.add(description);
                auto parsed = parse_config_file(ifs, config_file_options, true);
                store(parsed, vm);
                void init_log_from_unrecognized_program_options(boost::program_options::basic_parsed_options<char> const&, boost::program_options::variables_map&);
                init_log_from_unrecognized_program_options(parsed, config());
             }
          }
          if (vm["wait-for-debugger"].as<bool>())
             wait_for_debbuger();
          auto const error = initialize(module);
          auto res = !error;
          if(!res)
              SetLastError(static_cast<DWORD>(error.value()));
           return res;
      }
      case DLL_THREAD_ATTACH:
         break;
      case DLL_THREAD_DETACH:
         break;
      case DLL_PROCESS_DETACH:
         break;
   }
   return TRUE;
}
