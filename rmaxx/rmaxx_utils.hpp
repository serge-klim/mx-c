#pragma once
#include "rivermax_api.h"
#include <boost/compressed_pair.hpp>
#include <string>
#include <memory>
#include <stdexcept>
#include <cassert>

#include <system_error>

namespace rmaxx {

constexpr char const* to_string(rmax_status_t status) noexcept
{
   switch (status) {

      case RMAX_OK: return "RMAX_OK";

      /* Failure codes */
      case RMAX_ERR_NO_HW_RESOURCES: return "RMAX_ERR_NO_HW_RESOURCES";
      case RMAX_ERR_NO_FREE_CHUNK: return "RMAX_ERR_NO_FREE_CHUNK";
      case RMAX_ERR_NO_CHUNK_TO_SEND: return "RMAX_ERR_NO_CHUNK_TO_SEND";
      case RMAX_ERR_HW_SEND_QUEUE_FULL: return "RMAX_ERR_HW_SEND_QUEUE_FULL";
      case RMAX_ERR_NO_MEMORY: return "RMAX_ERR_NO_MEMORY";
      case RMAX_ERR_NOT_INITIALAZED: return "RMAX_ERR_NOT_INITIALAZED";
      case RMAX_ERR_NOT_IMPLEMENTED: return "RMAX_ERR_NOT_IMPLEMENTED";
      case RMAX_ERR_NO_DEVICE: return "RMAX_ERR_NO_DEVICE";
      case RMAX_ERR_BUSY: return "RMAX_ERR_BUSY";
      case RMAX_ERR_CANCELLED: return "RMAX_ERR_CANCELLED";
      case RMAX_ERR_HW_COMPLETION_ISSUE: return "RMAX_ERR_HW_COMPLETION_ISSUE";
      case RMAX_ERR_LICENSE_ISSUE: return "RMAX_ERR_LICENSE_ISSUE";
      case RMAX_ERR_UNKNOWN_ISSUE: return "RMAX_ERR_UNKNOWN_ISSUE";
      case RMAX_ERR_NO_ATTACH: return "RMAX_ERR_NO_ATTACH";
      case RMAX_ERR_STEERING_ISSUE: return "RMAX_ERR_STEERING_ISSUE";
      case RMAX_ERR_METHOD_NOT_SUPPORTED_BY_STREAM: return "RMAX_ERR_METHOD_NOT_SUPPORTED_BY_STREAM";
      case RMAX_ERR_CHECKSUM_ISSUE: return "RMAX_ERR_CHECKSUM_ISSUE";
      case RMAX_ERR_DESTINATION_UNREACHABLE: return "RMAX_ERR_DESTINATION_UNREACHABLE";
      case RMAX_ERR_MEMORY_REGISTRATION: return "RMAX_ERR_MEMORY_REGISTRATION";
      /* missing driver or underlying application */
      case RMAX_ERR_NO_DEPENDENCY: return "RMAX_ERR_NO_DEPENDENCY";
      /* for example exceeds link rate limit */
      case RMAX_ERR_EXCEEDS_LIMIT: return "RMAX_ERR_EXCEEDS_LIMIT";
      /* not supported by Rivermax */
      case RMAX_ERR_UNSUPPORTED: return "RMAX_ERR_UNSUPPORTED";
      /* clock type not supported by the device in use */
      case RMAX_ERR_CLOCK_TYPE_NOT_SUPPORTED: return "RMAX_ERR_CLOCK_TYPE_NOT_SUPPORTED";

      /* Failure due to invalid function parameter */
      case RMAX_INVALID_PARAMETER_MIX: return "RMAX_INVALID_PARAMETER_MIX";
      case RMAX_ERR_INVALID_PARAM_1: return "RMAX_ERR_INVALID_PARAM_1";
      case RMAX_ERR_INVALID_PARAM_2: return "RMAX_ERR_INVALID_PARAM_2";
      case RMAX_ERR_INVALID_PARAM_3: return "RMAX_ERR_INVALID_PARAM_3";
      case RMAX_ERR_INVALID_PARAM_4: return "RMAX_ERR_INVALID_PARAM_4";
      case RMAX_ERR_INVALID_PARAM_5: return "RMAX_ERR_INVALID_PARAM_5";
      case RMAX_ERR_INVALID_PARAM_6: return "RMAX_ERR_INVALID_PARAM_6";
      case RMAX_ERR_INVALID_PARAM_7: return "RMAX_ERR_INVALID_PARAM_7";
      case RMAX_ERR_INVALID_PARAM_8: return "RMAX_ERR_INVALID_PARAM_8";
      case RMAX_ERR_INVALID_PARAM_9: return "RMAX_ERR_INVALID_PARAM_9";
      case RMAX_ERR_INVALID_PARAM_10: return "RMAX_ERR_INVALID_PARAM_10";
      /* Ctrl-C was captured by Rivermax */
      case RMAX_SIGNAL: return "RMAX_SIGNAL";
      /* Device doesn't support PTP real time clock */
      case RMAX_ERR_UNSUPPORTED_PTP_RT_CLOCK_DEVICE: return "RMAX_ERR_UNSUPPORTED_PTP_RT_CLOCK_DEVICE";
      case RMAX_ERR_LAST: return "RMAX_ERR_LAST";
      default:break;
   }
   return "unknow rmax_status_t";
}


struct rmax_sentry
{
   rmax_sentry() = default;
   rmax_sentry(rmax_sentry const&) = delete;
   rmax_sentry& operator=(rmax_sentry const&) = delete;
   constexpr rmax_sentry(rmax_sentry&& other) noexcept : clean{std::exchange(other.clean, false)} {}
   constexpr rmax_sentry& operator=(rmax_sentry&& other) noexcept
   {
      clean = std::exchange(other.clean, false);
      return *this;
   }
   ~rmax_sentry() noexcept { 
       if (clean)
        rmax_cleanup(); 
   }
 private:
   bool clean = true;
};


struct error_category : std::error_category
{ 
   [[nodiscard]] char const* name() const noexcept override { return "rivermax"; }
   [[nodiscard]] std::string message(int error) const override {return to_string(static_cast<rmax_status_t>(error)); }
};

[[nodiscard]] inline std::error_code make_error_code(rmax_status_t error) noexcept
{
   static auto category = error_category{};
   return {static_cast<int>(error), category};
}


[[nodiscard]] rmax_sentry initialize();
[[nodiscard]] rmax_sentry initialize(rmax_init_config* init_config);

namespace detail {

template <typename BaseDx>
class memory_registration
{
 public:
   /*constexpr*/ memory_registration(rmax_mkey_id mkey_id, struct in_addr dev_addr, BaseDx const& dx) noexcept : mkey_id_{mkey_id}, extra_{std::move(dev_addr), dx} {}
   constexpr auto mkey_id() const noexcept { return mkey_id_; }
   void operator()(void* ptr) const noexcept {
      assert(ptr != nullptr);
      [[maybe_unused]] auto status = rmax_deregister_memory(mkey_id_, extra_.first());
      assert(status == RMAX_OK && "rmax_deregister_memory failed!");
      (extra_.second())(ptr);
   }
 private:
   rmax_mkey_id mkey_id_;
   boost::compressed_pair<struct in_addr, BaseDx> extra_;
};

template <typename T>
struct registered_memory
{};

template <typename T, typename Dx>
struct registered_memory<std::unique_ptr<T, Dx>>
{
   using type = std::unique_ptr<T, memory_registration<Dx>>;
};

} // namespace detail

template<typename T>
using registered_memory = typename detail::registered_memory<T>::type;

template <typename T, typename BaseDx>
constexpr auto memory_key_id(std::unique_ptr<T, detail::memory_registration<BaseDx>> const& mem_reg) noexcept
{
   return mem_reg.get_deleter().mkey_id();
}


template<typename T, typename Dx>
auto register_memory(struct in_addr dev_addr, std::unique_ptr<T, Dx>&& mem, std::size_t size)
{
   rmax_mkey_id mkey_id;
   if (auto status = rmax_register_memory(mem.get(), size, dev_addr, &mkey_id); status != RMAX_OK)
      throw std::runtime_error{std::string{"rmax_register_memory failed : "} + rmaxx::to_string(status)};
   auto deleter = detail::memory_registration<Dx>{mkey_id, dev_addr, mem.get_deleter()}; 
   return std::unique_ptr<T, detail::memory_registration<Dx>>{mem.release(), std::move(deleter)};
}

//auto create

} // namespace rmaxx

