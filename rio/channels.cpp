#include "channels.hpp"
#include "utils/sysinfo.hpp"
#include <boost/system/system_error.hpp>
#include <boost/winapi/basic_types.hpp>

//std::pair<rio::registered_memory<char>, std::size_t> rio::v2::detail::alloc_buffer(std::size_t n_packets, std::size_t max_packet_size)
//{
//   auto const size_hint = n_packets * max_packet_size;
//   auto allocated_mem_size = std::size_t{0};
//   auto mem_region = utils::huge_region{};
//   if (auto const huge_page_size = utils::huge_page_size()) {
//      allocated_mem_size = utils::size_in_blocks(size_hint, huge_page_size) * huge_page_size;
//      mem_region = utils::alloc_huge_pages(allocated_mem_size);
//   }
//
//   if (!mem_region) {
//      auto const page_size = utils::page_size();
//      allocated_mem_size = utils::size_in_blocks(size_hint, page_size) * page_size;
//      mem_region = utils::alloc_mem(allocated_mem_size);
//      if (!mem_region) {
//         // throw std::bad_alloc{std::to_string(mem_region_size) + "bytes allocation failed" };
//         throw boost::system::system_error(boost::winapi::GetLastError(), boost::system::system_category(), std::to_string(allocated_mem_size) + "bytes allocation failed");
//      }
//   }
//
//   auto const cache_line_size = utils::cache_line_size();
//   auto const aligned_packet_size = utils::size_in_blocks(max_packet_size, cache_line_size) * cache_line_size;
//   /*auto */ n_packets = allocated_mem_size / aligned_packet_size;
//   assert(n_packets != 0);
//   return std::make_pair(rio::detail::register_memory(extentsions(), std::move(mem_region), allocated_mem_size), allocated_mem_size);
//}


rio::v2::cq rio::v2::create_cq(std::size_t queue_size, RIO_NOTIFICATION_COMPLETION* completion_type /*= nullptr*/)
{
   // RIO_NOTIFICATION_COMPLETION completion_type;
   // completion_type.Type = RIO_IOCP_COMPLETION;
   // completion_type.Iocp.IocpHandle = iocp;
   // completion_type.Iocp.CompletionKey = reinterpret_cast<decltype(completion_type.Iocp.CompletionKey)>(RioIOCQEvent);
   // completion_type.Iocp.Overlapped = &overlapped;

   /// creating RIO CQ, which is bigger than (or equal to) RQ size
   auto raw_riocp = (*extentsions().RIOCreateCompletionQueue)(static_cast<boost::winapi::DWORD_>(queue_size), completion_type);
   if (raw_riocp == RIO_INVALID_CQ)
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOCreateCompletionQueue filed"};
   return {raw_riocp, extentsions().RIOCloseCompletionQueue};
}


rio::v2::channel::channel(SOCKET socket, RIO_CQ cq, std::size_t in_queue_size, std::size_t out_queue_size, void* context /*= nullptr*/) 
   : rq_{(*extentsions().RIOCreateRequestQueue)(socket, static_cast<unsigned long>(in_queue_size), 1, static_cast<unsigned long>(out_queue_size), 1, cq, cq, context)}
{
   if (rq_ == RIO_INVALID_RQ)
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOCreateRequestQueue filed"};
}

void rio::v2::channel::receive_from(RIO_BUF* buffer, RIO_BUF* address, void* context /*= nullptr*/, unsigned long flags /*= 0*/)
{
   assert(rq_ != RIO_INVALID_RQ);
   if (!(*extentsions().RIOReceiveEx)(rq_, buffer, 1, nullptr, address, nullptr, nullptr, flags, context))
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOReceiveEx filed"};
}

void rio::v2::channel::receive(RIO_BUF* buffer, void* context /*= nullptr*/, unsigned long flags /*= 0*/)
{
   assert(rq_ != RIO_INVALID_RQ);
   if (!(*extentsions().RIOReceive)(rq_, buffer, 1, flags, context))
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOReceive filed"};
}
void rio::v2::channel::receive_commit() 
{
   assert(rq_ != RIO_INVALID_RQ);
   if (!(*extentsions().RIOReceive)(rq_, nullptr, 0, RIO_MSG_COMMIT_ONLY, nullptr))
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOReceive(..,,RIO_MSG_COMMIT_ONLY,...) filed"};
}

void rio::v2::channel::send(RIO_BUF* buffer, void* context /*= nullptr*/, unsigned long flags /*= 0*/)
{
   assert(rq_ != RIO_INVALID_RQ);
   if (!(*extentsions().RIOSend)(rq_, buffer, 1, flags, context))
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOSend filed"};
}

void rio::v2::channel::send_commit()
{
   assert(rq_ != RIO_INVALID_RQ);
   if (!(*extentsions().RIOSend)(rq_, nullptr, 0, RIO_MSG_COMMIT_ONLY, nullptr))
      throw boost::system::system_error{WSAGetLastError(), boost::system::system_category(), "RIOSend(..,,RIO_MSG_COMMIT_ONLY,...) filed"};
}


