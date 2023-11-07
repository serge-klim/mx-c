#pragma once
//#include "options.hpp"
#include "WinSock2.h"
#include "MSWSock.h"
#include <boost/winapi/basic_types.hpp>
#include <memory>
#include <utility>
#include <type_traits>

namespace rio { inline namespace v2 {

struct rio_sentry : WSADATA
{
   rio_sentry() = default;
   rio_sentry(rio_sentry const&) = delete;
   rio_sentry& operator=(rio_sentry const&) = delete;
   constexpr rio_sentry(rio_sentry&& other) noexcept : clean{std::exchange(other.clean, false)} {}
   constexpr rio_sentry& operator=(rio_sentry&& other) noexcept
   {
      clean = std::exchange(other.clean, false);
      return *this;
   }
   ~rio_sentry() noexcept; 
 private:
   bool clean = true;
 };


rio_sentry initialize(unsigned short version = 0x202);
RIO_EXTENSION_FUNCTION_TABLE const& extentsions() noexcept;

namespace detail {

struct proxy_socket_type;
struct close_socket
{
   void operator()(proxy_socket_type* socket) noexcept;
};

using scoped_socket = std::unique_ptr<detail::proxy_socket_type, detail::close_socket>;
inline /*constexpr*/ SOCKET handle(scoped_socket const& socket) noexcept { return reinterpret_cast<SOCKET>(socket.get()); }

} // namespace detail


using detail::scoped_socket;
scoped_socket wsa_socket(int af, int type, int protocol, LPWSAPROTOCOL_INFOW protocol_info, GROUP g, boost::winapi::DWORD_ flags);

}} // namespace rio::v2
