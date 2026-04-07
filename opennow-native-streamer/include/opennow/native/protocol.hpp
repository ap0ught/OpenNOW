#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opennow::native {

struct ProtocolMessage {
  std::string type;
  std::string payload;
};

std::vector<std::uint8_t> FrameJson(std::string_view json);
std::string EscapeJson(std::string_view input);
std::string BuildEnvelope(std::string_view type, std::string_view payloadJson = "{}");
std::optional<std::string> FindJsonString(std::string_view json, std::string_view key);
std::optional<int> FindJsonInt(std::string_view json, std::string_view key);
std::optional<bool> FindJsonBool(std::string_view json, std::string_view key);

}  // namespace opennow::native
