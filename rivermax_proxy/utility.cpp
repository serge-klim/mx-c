#include "utility.hpp"
#include "WS2tcpip.h"

boost::program_options::variables_map& config() noexcept
{
   static boost::program_options::variables_map cfg;
   return cfg;
}

void wait_for_debbuger_if_configured()
{
   if (config()["wait-for-debugger-on-crash"].as<bool>())
      wait_for_debbuger();
}

std::string to_string(struct in_addr const& address)
{
   auto buffer = std::array<char, INET_ADDRSTRLEN>{};
   inet_ntop(AF_INET, &address, buffer.data(), buffer.size());
   return buffer.data();
}

std::string to_string(struct sockaddr_in const& address)
{
   return to_string(address.sin_addr) + ':' + std::to_string(address.sin_port);
}

std::string to_string(struct in6_addr const& address)
{
   auto buffer = std::array<char, INET6_ADDRSTRLEN>{};
   inet_ntop(AF_INET, &address, buffer.data(), buffer.size());
   return buffer.data();
}

std::string to_string(struct sockaddr_in6 const& address)
{
   return to_string(address.sin6_addr) + ':' + std::to_string(address.sin6_port);
}

std::string to_string(struct sockaddr const& address)
{
   return address.sa_family == AF_INET
              ? to_string(*reinterpret_cast<struct sockaddr_in const*>(&address/*.sa_data*/))
              : to_string(*reinterpret_cast<struct sockaddr_in6 const*>(&address/*.sa_data*/));
}
