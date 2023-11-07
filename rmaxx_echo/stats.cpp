#include "stats.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>



void statistics::dump(std::ostream& out, char const* type /*= "processed"*/) const
{
    out << '[' << boost::posix_time::microsec_clock::local_time() << "] ...done.\n " << processed_ << " packets has been " << type <<'\n';
    if (size_.max != 0) {
       out << "\tmax packet size : " << size_.max << '\n';
       out << "\taverage packet size : " << size_.total / processed_ << '\n';
    }
}

std::size_t statistics::mark_processed(std::size_t packet_size) noexcept
{
    update_packet_size(packet_size);
    return mark_processed();
}

void statistics::update_packet_size(std::size_t size) noexcept
{
    size_.total += size;
    if (size_.max < size)
       size_.max = size;
}

rx_stats::rx_stats(std::size_t n)
{
    std::clog << '[' << boost::posix_time::microsec_clock::local_time() << "] processing " << n << " packets... " << std::endl;
}

rx_stats::~rx_stats()
{
    statistics::dump(std::clog, "received");
    std::clog.flush();
}

tx_stats::~tx_stats()
{
    statistics::dump(std::clog, "sent");
    if (retries_.total != 0)
       std::clog << "\tmax per packet retries  : " << retries_.max << " , total : " << (retries_.total == 0 ? 1 : retries_.total) << '\n';
    std::clog.flush();
}

std::size_t tx_stats::retry() noexcept
{
    if (retries_.processed == processed()) {
       if (retries_.max < ++retries_.current)
          retries_.max = retries_.current;
    } else {
       retries_.processed = processed();
       retries_.current = 1;
    }
    ++retries_.total;
    return retries_.current;
}