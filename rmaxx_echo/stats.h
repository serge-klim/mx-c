#pragma once
#include <iosfwd>
#include <limits>
#include <cstddef>

struct statistics
{
   constexpr std::size_t processed() const noexcept { return processed_; }
   constexpr std::size_t mark_processed() noexcept {  return ++processed_;  }
   constexpr std::size_t mark_batch_processed(std::size_t processed) noexcept { return processed_ += processed; }
   std::size_t mark_processed(std::size_t packet_size) noexcept;
   void update_packet_size(std::size_t size) noexcept;
 protected:
   void dump(std::ostream& out, char const* type = "processed") const;
 private:
   struct
   {
      std::size_t max = 0;
      std::size_t total = 0;
   } size_;
   std::size_t processed_ = 0;
};

struct rx_stats : statistics
{
   rx_stats(std::size_t n);
   ~rx_stats();
};

struct tx_stats : statistics
{
   ~tx_stats();
   std::size_t retry() noexcept;
   struct
   {
      std::size_t processed = (std::numeric_limits<std::size_t>::max)();
      std::size_t current = 0;
      std::size_t total = 0;
      std::size_t max = 0;
   } retries_;
};
