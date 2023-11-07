#include "socket.hpp"
#include <boost/system/system_error.hpp>

namespace {
RIO_EXTENSION_FUNCTION_TABLE extension_table = {};
}

rio::v2::rio_sentry::~rio_sentry() noexcept
{
    if (clean)
        WSACleanup(); 
}

rio::rio_sentry rio::v2::initialize(unsigned short version /*= 0x202*/)
{
   rio::rio_sentry res;
   if (auto error = WSAStartup(version, &res))
      throw boost::system::system_error{boost::system::error_code{error, boost::system::system_category()}, "WSAStartup filed"};

   if (extension_table.cbSize == 0) {
      auto socket = wsa_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
      GUID function_table_id = WSAID_MULTIPLE_RIO;
      DWORD bytes;
      if (WSAIoctl(handle(socket) /**socket.get()*/, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &function_table_id, sizeof(GUID), &extension_table, sizeof(extension_table), &bytes, nullptr, nullptr) != 0)
         throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER filed"};
   }
   return res;
}

void rio::v2::detail::close_socket::operator()(proxy_socket_type* socket) noexcept
{
   closesocket(reinterpret_cast<SOCKET>(socket));
}

rio::v2::scoped_socket rio::v2::wsa_socket(int af, int type, int protocol, LPWSAPROTOCOL_INFOW protocol_info, GROUP g, boost::winapi::DWORD_ flags)
{
   auto socket = WSASocketW(af, type, protocol, protocol_info, g, flags);
   if (socket == INVALID_SOCKET)
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "WSASocket filed"};

   static_assert(sizeof(detail::proxy_socket_type*) >= sizeof(socket), "this needs to be modified on current platform");
   return {reinterpret_cast<detail::proxy_socket_type*>(socket), detail::close_socket{}};
}


RIO_EXTENSION_FUNCTION_TABLE const& rio::v2::extentsions() noexcept { return extension_table; }
