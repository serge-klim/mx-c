#include "sockperf/sockperf.hpp"
#include "utils/program_options/net.hpp"

#include <boost/program_options.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include <vector>
#include <array>
#include <thread>

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

boost::asio::awaitable<std::size_t> coro_sockperf_echo(boost::asio::ip::udp::socket in, boost::asio::ip::udp::socket out)
{
  auto packets = std::size_t{0};
  try
  {
    auto data = std::array<char, 10240>{};
    auto source = boost::asio::ip::udp::endpoint{};
    for (;;)
    {     
       auto n = std::size_t{0};
       do {
          n += co_await in.async_receive_from(boost::asio::buffer(data.data() + n, data.size() - n), source, boost::asio::use_awaitable);
       } while (n < sockperf::header_size() || n < sockperf::message_size(data.data(), n));

       if (sockperf::message_type(data.data(), n) == sockperf::type::Ping) {
          sockperf::message_type(sockperf::type::Pong, data.data(), n);
          co_await out.async_send_to(boost::asio::buffer(data, n), source, boost::asio::use_awaitable);
       }
       ++packets;
    }
  }
  catch (std::exception& e)
  {
    BOOST_LOG_TRIVIAL(error)  << "echo error " << e.what();
  } 
  catch (...) {
    BOOST_LOG_TRIVIAL(error) << "unexpected error...";
  }
  co_return packets;
}

boost::asio::awaitable<std::size_t> coro_echo(boost::asio::ip::udp::socket in, boost::asio::ip::udp::socket out, boost::asio::ip::udp::endpoint destination)
{
  auto packets = std::size_t{0};
  try {
    char data[1024];
    auto source = boost::asio::ip::udp::endpoint{};
    for (;;) {
       std::size_t n = co_await in.async_receive_from(boost::asio::buffer(data), source, boost::asio::use_awaitable);
       co_await out.async_send_to(boost::asio::buffer(data, n), destination, boost::asio::use_awaitable);
       ++packets;
    }
  } catch (std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "echo error " << e.what();
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "unexpected error...";
  }
  co_return packets;
}

void run(boost::asio::io_context& ioctx, boost::program_options::variables_map const& options)
{
  auto source = options["source"].as<net::endpoint>();
  auto source_address = boost::asio::ip::make_address(source.host);
  auto source_endpoint = boost::asio::ip::udp::endpoint{source_address, source.port};
  auto socket_in = boost::asio::ip::udp::socket{ioctx, source_endpoint.protocol()};
  socket_in.bind(boost::asio::ip::udp::endpoint{boost::asio::ip::address_v4::any(), source.port});
  if (auto nic = options["if-in"]; nic.empty())
    socket_in.set_option(
        boost::asio::ip::multicast::join_group{source_address});
  else
    socket_in.set_option(
        boost::asio::ip::multicast::join_group{source_address.to_v4(), nic.as<boost::asio::ip::address>().to_v4()});

   auto destination = options["destination"]/*.as<net::endpoint>()*/;
  //auto destination_endpoint = boost::asio::ip::udp::endpoint{boost::asio::ip::make_address(destination.host), destination.port};

  auto socket_out = boost::asio::ip::udp::socket{ioctx, source_endpoint/*destination_endpoint*/.protocol()};
  if (auto nic = options["if-out"]; !nic.empty())
    socket_out.set_option(boost::asio::ip::multicast::outbound_interface{nic.as<boost::asio::ip::address>().to_v4()});
  if (options.count("ttl") != 0)
    socket_out.set_option(boost::asio::ip::multicast::hops{options["ttl"].as<int>()});
  socket_out.set_option(boost::asio::ip::multicast::enable_loopback{true/*options["loopback"].as<bool>()*/});

  if (options["sockperf"].as<bool>()) {
      BOOST_LOG_TRIVIAL(info) << "waiting for client on " << source_address << ':' << source.port
          << "\n\n\t!!!!!   please run client as (win)sockperf.exe pp -i " << source_address << " -p " << source.port << "[ -n 100000[000] -m 128]  !!!!!!\n";

    boost::asio::co_spawn(ioctx, coro_sockperf_echo(std::move(socket_in), std::move(socket_out)), [&](std::exception_ptr, std::size_t processed) {
       BOOST_LOG_TRIVIAL(info) << processed << " packets has been transmitted ";
    });
  } else {
    if (destination.empty())
       throw boost::program_options::validation_error{boost::program_options::validation_error::at_least_one_value_required, "destination required"};
    auto destination_ep = destination.as<net::endpoint>();
    auto destination_endpoint = boost::asio::ip::udp::endpoint{boost::asio::ip::make_address(destination_ep.host), destination_ep.port};
    boost::asio::co_spawn(ioctx, coro_echo(std::move(socket_in), std::move(socket_out), std::move(destination_endpoint)), [&](std::exception_ptr, std::size_t processed) {
       BOOST_LOG_TRIVIAL(info) << processed << " packets has been transmitted ";
    });
  }
}

void run_service(boost::asio::io_context& ioctx, boost::asio::io_context::count_type (boost::asio::io_context::*run)()/* = &boost::asio::io_context::run*/)
{
  while (!ioctx.stopped()) {
    try {
       (ioctx.*run)();
    } catch (std::exception& e) {
       BOOST_LOG_TRIVIAL(error) << "service thread failed : " << e.what();
    } catch (...) {
       BOOST_LOG_TRIVIAL(error) << "service thread unexpected exception";
    }
  }
}

void run(boost::program_options::variables_map const& options) {
  auto thread_n = options["threads"].as<std::size_t>();
  if (thread_n == 0)
    thread_n = std::thread::hardware_concurrency();

  boost::asio::io_context ioctx{static_cast<int>(thread_n)};
  boost::asio::signal_set signals{ioctx};
  signals.add(SIGINT);
  signals.add(SIGTERM);
#if defined(SIGQUIT)
  signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals.async_wait([&ioctx](boost::system::error_code error, int signo) {
     if (!error)
        BOOST_LOG_TRIVIAL(info) << "signal : " << signo << " received , shutting down ...";
     else
        BOOST_LOG_TRIVIAL(error) << "signal wait failed " << error.message() << " shutting down... ";
     ioctx.stop();
  });
  run(ioctx, options);
  assert(thread_n != 0);
  BOOST_LOG_TRIVIAL(info) << "starting up thread pool on " << thread_n << " threads ...";
  boost::asio::io_context::count_type (boost::asio::io_context::*run)() = &boost::asio::io_context::run;
  if (auto poll = options["poll"]; !poll.empty() && poll.as<bool>())
    run = &boost::asio::io_context::poll;
  auto threads = std::vector<std::thread>{};
  threads.reserve(--thread_n);
  while (thread_n--)
  threads.emplace_back(&run_service, std::ref(ioctx), run);
  
  run_service(ioctx, run);
}

