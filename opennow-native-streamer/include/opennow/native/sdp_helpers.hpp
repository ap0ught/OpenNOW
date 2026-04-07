#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace opennow::native {

struct IceCredentials {
  std::string ufrag;
  std::string password;
  std::string fingerprint;
};

std::optional<std::string> ExtractPublicIp(std::string_view host_or_ip);
std::string FixServerIp(std::string_view sdp, std::string_view server_ip);
std::string PreferCodec(std::string_view sdp, std::string_view codec);
std::string RewriteH265TierFlag(std::string_view sdp, int tier_flag);
std::string RewriteH265LevelIdByProfile(std::string_view sdp, int max_level_main, int max_level_main10);
std::string ExtractIceUfragFromOffer(std::string_view sdp);
std::string MungeAnswerSdp(std::string_view sdp, int max_bitrate_kbps);
std::optional<int> ParsePartialReliableThresholdMs(std::string_view sdp);
IceCredentials ExtractIceCredentials(std::string_view sdp);
std::string BuildNvstSdp(
    int width,
    int height,
    int client_viewport_width,
    int client_viewport_height,
    int fps,
    int max_bitrate_kbps,
    std::string_view codec,
    std::string_view color_quality,
    int partial_reliable_threshold_ms,
    const IceCredentials& credentials);

}  // namespace opennow::native
