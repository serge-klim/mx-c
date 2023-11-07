#pragma once
#include "socket.hpp"
#include <memory>
#include <type_traits>

namespace rio { inline namespace v2 {

using cq = std::unique_ptr<std::remove_pointer_t<RIO_CQ>, decltype(RIO_EXTENSION_FUNCTION_TABLE::RIOCloseCompletionQueue)>;
cq create_cq(std::size_t queue_size, RIO_NOTIFICATION_COMPLETION* completion_type = nullptr);


class channel
{
 public:
   channel() = default;
   channel(SOCKET socket, RIO_CQ cq, std::size_t in_queue_size, std::size_t out_queue_size, void* context = nullptr);
   channel(channel const&) = delete;
   channel& operator=(channel const&) = delete;
   channel(channel &&) = default;
   channel& operator=(channel &&) = default;
   //~channel() = default; // seems rq_ no need to be destroyed

   constexpr bool operator!() const noexcept { return rq_ == RIO_INVALID_RQ; }
   void receive_from(RIO_BUF* buffer, RIO_BUF* address, void* context = nullptr, unsigned long flags = 0);
   void receive(RIO_BUF* buffer, void* context = nullptr, unsigned long flags = 0);
   void receive_commit();
   void send(RIO_BUF* buffer, void* context = nullptr, unsigned long flags = 0);
   void send_commit();

 private:
   RIO_RQ rq_ = RIO_INVALID_RQ;
};

class rx_channel : channel
{
 public:
   rx_channel() = default;
   rx_channel(SOCKET socket, RIO_CQ cq, std::size_t queue_size) : channel{socket, cq, queue_size, 0, this} {}

   using channel::operator !;
   using channel::receive;
   using channel::receive_from;
   void receive(std::size_t n = 1, unsigned long flags = RIO_MSG_DONT_NOTIFY);
   void commit() { receive_commit(); }
};

class tx_channel : channel
{
 public:
   tx_channel() = default;
   tx_channel(SOCKET socket, RIO_CQ cq, std::size_t queue_size) : channel{socket, cq, 0, queue_size, this} {}
   tx_channel(tx_channel const&) = delete;
   tx_channel& operator=(tx_channel const&) = delete;
   tx_channel(tx_channel&&) = default;
   tx_channel& operator=(tx_channel&&) = default;

   using channel::operator !;
   using channel::send;
   void commit() { send_commit(); }
};

} // namespace v2

} // namespace rio
