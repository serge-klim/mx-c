#include "rmaxx_utils.hpp"


rmaxx::rmax_sentry rmaxx::initialize(rmax_init_config* config)
{
   if (auto status = rmax_init(config); status != RMAX_OK)
      throw std::runtime_error{std::string{"rmax_init failed : "} + rmaxx::to_string(status)};

   return {};
}

rmaxx::rmax_sentry rmaxx::initialize()
{
   auto init_config = rmax_init_config{0};
   init_config.flags = /*RIVERMAX_CPU_MASK |*/ RIVERMAX_HANDLE_SIGNAL;
   return initialize(&init_config);
}
