#include "opennow/native/opus_rtp_depacketizer.hpp"

#include <sstream>

namespace opennow::native {

void OpusRtpDepacketizer::SetLogger(LogFn logger) {
  logger_ = std::move(logger);
}

std::optional<DepacketizedOpusFrame> OpusRtpDepacketizer::PushRtpPacket(const std::vector<std::uint8_t>& packet, int clock_rate) {
  if (packet.size() < 12) {
    Log("Dropping malformed Opus RTP packet shorter than header");
    return std::nullopt;
  }

  const std::uint8_t version = static_cast<std::uint8_t>((packet[0] >> 6) & 0x03u);
  if (version != 2) {
    Log("Dropping Opus RTP packet with unsupported RTP version");
    return std::nullopt;
  }

  const bool padding = (packet[0] & 0x20u) != 0;
  const bool extension = (packet[0] & 0x10u) != 0;
  const std::size_t csrc_count = packet[0] & 0x0Fu;
  std::size_t offset = 12 + csrc_count * 4;
  if (packet.size() < offset) {
    Log("Dropping Opus RTP packet with truncated CSRC list");
    return std::nullopt;
  }

  if (extension) {
    if (packet.size() < offset + 4) {
      Log("Dropping Opus RTP packet with truncated extension header");
      return std::nullopt;
    }
    const std::uint16_t extension_words = static_cast<std::uint16_t>((packet[offset + 2] << 8) | packet[offset + 3]);
    offset += 4 + static_cast<std::size_t>(extension_words) * 4;
    if (packet.size() < offset) {
      Log("Dropping Opus RTP packet with truncated extension payload");
      return std::nullopt;
    }
  }

  std::size_t payload_size = packet.size() - offset;
  if (padding) {
    const std::uint8_t padding_size = packet.back();
    if (padding_size == 0 || padding_size > payload_size) {
      Log("Dropping Opus RTP packet with invalid padding");
      return std::nullopt;
    }
    payload_size -= padding_size;
  }

  if (payload_size == 0) {
    Log("Dropping empty Opus RTP payload");
    return std::nullopt;
  }

  const std::uint32_t rtp_timestamp =
      (static_cast<std::uint32_t>(packet[4]) << 24) | (static_cast<std::uint32_t>(packet[5]) << 16) |
      (static_cast<std::uint32_t>(packet[6]) << 8) | static_cast<std::uint32_t>(packet[7]);

  DepacketizedOpusFrame frame;
  frame.payload.assign(packet.begin() + static_cast<std::ptrdiff_t>(offset),
                       packet.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
  const auto safe_clock_rate = clock_rate > 0 ? clock_rate : 48000;
  frame.timestamp_us = (static_cast<std::uint64_t>(rtp_timestamp) * 1000000ULL) / static_cast<std::uint64_t>(safe_clock_rate);
  return frame;
}

void OpusRtpDepacketizer::Log(const std::string& message) const {
  if (logger_) {
    logger_(message);
  }
}

}  // namespace opennow::native
