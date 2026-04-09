#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace opennow::native {

struct DepacketizedOpusFrame {
  std::vector<std::uint8_t> payload;
  std::uint64_t timestamp_us = 0;
};

class OpusRtpDepacketizer {
 public:
  using LogFn = std::function<void(const std::string&)>;

  void SetLogger(LogFn logger);
  std::optional<DepacketizedOpusFrame> PushRtpPacket(const std::vector<std::uint8_t>& packet, int clock_rate);

 private:
  void Log(const std::string& message) const;

  LogFn logger_;
};

}  // namespace opennow::native
