#define BOOST_TEST_MODULE rio_tests
#include "rio/socket.hpp"
#include "rio/channels.hpp"
//#include "utils/sysinfo.hpp"
//#include "utils/memory.hpp"
//#include "utils/huge_pages.hpp"
#include <boost/winapi/get_last_error.hpp>
#include <boost/system/system_error.hpp>
//#include <algorithm>
//#include <cassert>

#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE(rio_utils_test_suite)

RIO_EXTENSION_FUNCTION_TABLE init_rio_extensions(rio::detail::scoped_socket const& socket)
{
   GUID functionTableId = WSAID_MULTIPLE_RIO;
   RIO_EXTENSION_FUNCTION_TABLE rio;
   DWORD bytes;
   if (WSAIoctl(handle(socket) /**socket.get()*/, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &functionTableId, sizeof(GUID), &rio, sizeof(rio), &bytes, nullptr, nullptr) != 0)
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER filed"};
   return rio;
}

BOOST_AUTO_TEST_CASE(extension_test)
{
   // WSADATA data;
   // if (auto error = WSAStartup(0x202, &data))
   //    throw boost::system::system_error{boost::system::error_code{error, boost::system::system_category()}, "WSAStartup filed"};
   ////WSACleanup()
   RIO_EXTENSION_FUNCTION_TABLE extension1;
   {
        auto rio_sentry = rio::initialize();
        auto s1 = rio::wsa_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
        extension1 = init_rio_extensions(s1);
   }
   RIO_EXTENSION_FUNCTION_TABLE extension2;
   {
        auto rio_sentry = rio::initialize();
        auto s2 = rio::wsa_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
        extension2 = init_rio_extensions(s2);
   }

   BOOST_CHECK(std::memcmp(&extension1, &extension2, sizeof(extension2))==0);
}


BOOST_AUTO_TEST_SUITE_END()
