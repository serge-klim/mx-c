#define BOOST_TEST_MODULE rivermax_proxy_tests
#include <boost/test/unit_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(rivermax_proxy_utility_test_suite)

BOOST_AUTO_TEST_CASE(version_parsin_test)
{
   std::string version_string = "1.2.0.0";
   auto begin = cbegin(version_string);
   auto end = cend(version_string);
   auto version = std::vector<unsigned short>{};
   BOOST_CHECK(boost::spirit::x3::parse(begin, end, boost::spirit::x3::ushort_ % '.', version));
   BOOST_CHECK(begin == end);
}

BOOST_AUTO_TEST_SUITE_END()
