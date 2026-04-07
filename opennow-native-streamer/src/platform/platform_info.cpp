#include "opennow/native/platform_info.hpp"

namespace opennow::native {

std::string DescribePlatformTarget() {
#if defined(_WIN32)
  return "Windows";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm__))
  return "Linux ARM / Raspberry Pi";
#elif defined(__linux__)
  return "Linux";
#else
  return "Unknown platform";
#endif
}

}  // namespace opennow::native
