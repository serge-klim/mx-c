#pragma once
#include <boost/program_options.hpp>
#include <boost/system/system_error.hpp>
#include <boost/winapi/dll.hpp>
#include <string>

boost::system::error_code initialize(boost::winapi::HMODULE_ module);
boost::program_options::variables_map& config() noexcept;
void wait_for_debbuger();

std::string to_string(struct in_addr const& address);
std::string to_string(struct sockaddr_in const& address);
std::string to_string(struct in6_addr const& address);
std::string to_string(struct sockaddr_in6 const& address);
std::string to_string(struct sockaddr const& address);
