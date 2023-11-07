#include "utils/program_options/validators/net.hpp"
#include "utils/program_options/validators/chrono.hpp"
#include "utils/program_options/log.hpp"
//#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

constexpr auto receiver_default_args() noexcept { return " -r receiver ..."; }

void setup_common_options(boost::program_options::options_description& description)
{
   // clang-format off
   description.add_options()
       ("threads,t", boost::program_options::value<std::size_t>()->default_value(0), "\tthread number")
       ("poll", boost::program_options::value<bool>()->default_value(false), "\t use boost::asio::io_service::poll instead of boost::asio::io_service::run")
       ("source,s", boost::program_options::value<net::endpoint>()->required(), "\taddress to listen to ip:port")
       ("destination,d", boost::program_options::value<net::endpoint>()/*->required()*/, "\taddress to multicast to ip:port")
       ("if-out", boost::program_options::value<boost::asio::ip::address>(), "interface to multicast on from")
       ("if-in", boost::program_options::value<boost::asio::ip::address>(), "interface to listen to on")
       ("ttl", boost::program_options::value<int>(), "\tmulticast ttl")
       ("buffer-size", boost::program_options::value<std::size_t>()->default_value(1024), "\tbuffer size")
       ("loopback", boost::program_options::value<bool>()->default_value(true), "\tenable loopback")
       ("sockperf", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\tsockperf echo")
       ;
   // clang-format on

}


std::ostream& help(std::ostream& out, boost::program_options::options_description& description)
{
   // clang-format off
   out << description << "\neg:\n"
                           "echo --poll=true "
                           "--threads=1 "
                           "--source=239.255.0.1:30001 "
                           "--destination=239.255.0.1:30003 "
                           "--if-out=192.168.8.166 "
                           "--if-in=192.168.8.166 "
                           "--ttl=16";
   // clang-format on
   return out;
}

bool parse_config(boost::program_options::variables_map& vm)
{
   auto res = false;
   auto config_filename = vm.count("config") == 0 ? std::make_pair(false, std::string{"config.ini"}) : std::make_pair(true, vm["config"].as<std::string>());
   if (auto ifs = std::ifstream{config_filename.second}) {
      auto description = boost::program_options::options_description{"configuration"};
      setup_common_options(description);

      boost::program_options::options_description config_file_options;
      config_file_options.add(description);
      auto parsed = parse_config_file(ifs, config_file_options, true);
      store(parsed, vm);

      auto const& additional = collect_unrecognized(parsed.options, boost::program_options::include_positional);
      init_log_from_unrecognized_program_options(additional);

      notify(vm); // check config file options sanity
      res = true;
   } else if (config_filename.first)
      throw std::runtime_error{str(boost::format("can't open configuration file \"%1%\"") % config_filename.second)};
   return res;
}

int main(int argc, char* argv[])
{
   auto description = boost::program_options::options_description{"options"};
   try {
      // clang-format off
        description.add_options()
            ("help,h", "\tprint usage message")
            ("config,c", boost::program_options::value<std::string>())
            ;
      // clang-format on
      setup_common_options(description);
      //boost::program_options::positional_options_description positional;
      //positional.add("config", -1);

      boost::program_options::variables_map vm;
      store(boost::program_options::command_line_parser(
                argc, argv)
                .options(description) /*.positional(positional)*/.run(),
            vm);

      if (vm.count("help") != 0) {
         help(std::cout, description);
         return 0;
      }
      if (!parse_config(vm))
          notify(vm); // check cmd-line option sanity
      void run(boost::program_options::variables_map const& options);
      run(vm);

   } catch (boost::program_options::error& e) {
      std::cerr << "error : " << e.what() << "\n\n";
      help(std::cerr, description);
   } catch (std::exception& e) {
      std::cerr << "error : " << e.what() << std::endl;
   } catch (...) {
      std::cerr << "miserably failed:(" << std::endl;
   }

   return 0;
}
