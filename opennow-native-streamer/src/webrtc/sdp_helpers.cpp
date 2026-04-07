#include "opennow/native/sdp_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opennow::native {

namespace {

std::string DetectLineEnding(std::string_view sdp) {
  return std::string(sdp).find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::vector<std::string> SplitLines(std::string_view sdp) {
  std::stringstream stream{std::string(sdp)};
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  return lines;
}

std::string JoinLines(const std::vector<std::string>& lines, const std::string& eol) {
  std::ostringstream out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    out << lines[i];
    if (i + 1 < lines.size()) {
      out << eol;
    }
  }
  return out.str();
}

std::string NormalizeCodec(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  if (value == "HEVC") {
    return "H265";
  }
  return value;
}

}  // namespace

std::optional<std::string> ExtractPublicIp(std::string_view host_or_ip) {
  const std::string value(host_or_ip);
  static const std::regex dotted(R"(^\d{1,3}(?:\.\d{1,3}){3}$)");
  if (std::regex_match(value, dotted)) {
    return value;
  }

  const auto first_dot = value.find('.');
  const std::string first_label = value.substr(0, first_dot);
  std::stringstream label_stream(first_label);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(label_stream, part, '-')) {
    parts.push_back(part);
  }
  if (parts.size() != 4) {
    return std::nullopt;
  }
  for (const auto& candidate : parts) {
    if (!std::regex_match(candidate, std::regex(R"(^\d{1,3}$)"))) {
      return std::nullopt;
    }
  }
  return parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
}

std::string FixServerIp(std::string_view sdp, std::string_view server_ip) {
  const auto public_ip = ExtractPublicIp(server_ip);
  if (!public_ip) {
    return std::string(sdp);
  }

  std::string fixed(sdp);
  fixed = std::regex_replace(fixed, std::regex(R"(c=IN IP4 0\.0\.0\.0)"), "c=IN IP4 " + *public_ip);
  fixed = std::regex_replace(
      fixed,
      std::regex(R"((a=candidate:\S+\s+\d+\s+\w+\s+\d+\s+)0\.0\.0\.0(\s+))"),
      "$1" + *public_ip + "$2");
  return fixed;
}

std::string PreferCodec(std::string_view sdp, std::string_view codec) {
  const auto eol = DetectLineEnding(sdp);
  auto lines = SplitLines(sdp);
  const auto normalized_codec = NormalizeCodec(std::string(codec));

  bool in_video = false;
  std::unordered_map<std::string, std::string> codec_by_payload;
  std::unordered_map<std::string, std::string> fmtp_by_payload;
  std::unordered_map<std::string, std::string> rtx_apt_by_payload;
  std::vector<std::string> preferred_payloads;

  for (const auto& line : lines) {
    if (line.rfind("m=video", 0) == 0) {
      in_video = true;
      continue;
    }
    if (line.rfind("m=", 0) == 0 && in_video) {
      in_video = false;
    }
    if (!in_video || line.rfind("a=rtpmap:", 0) != 0) {
      continue;
    }
    const auto colon = line.find(':');
    const auto space = line.find(' ', colon + 1);
    if (colon == std::string::npos || space == std::string::npos) {
      continue;
    }
    const auto pt = line.substr(colon + 1, space - colon - 1);
    auto codec_name = line.substr(space + 1);
    const auto slash = codec_name.find('/');
    codec_name = NormalizeCodec(codec_name.substr(0, slash));
    codec_by_payload[pt] = codec_name;
    if (codec_name == normalized_codec) {
      preferred_payloads.push_back(pt);
    }
  }

  in_video = false;
  for (const auto& line : lines) {
    if (line.rfind("m=video", 0) == 0) {
      in_video = true;
      continue;
    }
    if (line.rfind("m=", 0) == 0 && in_video) {
      in_video = false;
    }
    if (!in_video || line.rfind("a=fmtp:", 0) != 0) {
      continue;
    }
    const auto colon = line.find(':');
    const auto space = line.find(' ', colon + 1);
    if (colon == std::string::npos || space == std::string::npos) {
      continue;
    }
    const auto pt = line.substr(colon + 1, space - colon - 1);
    const auto params = line.substr(space + 1);
    fmtp_by_payload[pt] = params;
    std::smatch match;
    if (std::regex_search(params, match, std::regex(R"((?:^|;)\s*apt=(\d+))", std::regex::icase))) {
      rtx_apt_by_payload[pt] = match[1].str();
    }
  }

  if (preferred_payloads.empty()) {
    return std::string(sdp);
  }

  if (normalized_codec == "H265") {
    std::stable_sort(preferred_payloads.begin(), preferred_payloads.end(), [&](const std::string& a, const std::string& b) {
      const auto pa = fmtp_by_payload[a];
      const auto pb = fmtp_by_payload[b];
      const bool a_main10 = pa.find("profile-id=2") != std::string::npos;
      const bool b_main10 = pb.find("profile-id=2") != std::string::npos;
      return !a_main10 && b_main10;
    });
  }

  std::unordered_set<std::string> allowed(preferred_payloads.begin(), preferred_payloads.end());
  for (const auto& [rtx_pt, apt] : rtx_apt_by_payload) {
    if (allowed.contains(apt) && codec_by_payload[rtx_pt] == "RTX") {
      allowed.insert(rtx_pt);
    }
  }

  std::vector<std::string> filtered;
  in_video = false;
  for (const auto& line : lines) {
    if (line.rfind("m=video", 0) == 0) {
      in_video = true;
      std::stringstream stream(line);
      std::vector<std::string> tokens;
      std::string token;
      while (stream >> token) {
        tokens.push_back(token);
      }
      std::vector<std::string> rewritten(tokens.begin(), tokens.begin() + std::min<std::size_t>(3, tokens.size()));
      for (const auto& pt : preferred_payloads) {
        if (std::find(tokens.begin() + std::min<std::size_t>(3, tokens.size()), tokens.end(), pt) != tokens.end()) {
          rewritten.push_back(pt);
        }
      }
      for (std::size_t i = 3; i < tokens.size(); ++i) {
        if (allowed.contains(tokens[i]) && std::find(rewritten.begin(), rewritten.end(), tokens[i]) == rewritten.end()) {
          rewritten.push_back(tokens[i]);
        }
      }
      std::ostringstream out;
      for (std::size_t i = 0; i < rewritten.size(); ++i) {
        if (i > 0) out << ' ';
        out << rewritten[i];
      }
      filtered.push_back(out.str());
      continue;
    }
    if (line.rfind("m=", 0) == 0 && in_video) {
      in_video = false;
    }
    if (in_video && (line.rfind("a=rtpmap:", 0) == 0 || line.rfind("a=fmtp:", 0) == 0 || line.rfind("a=rtcp-fb:", 0) == 0)) {
      const auto colon = line.find(':');
      const auto space = line.find(' ', colon + 1);
      const auto pt = line.substr(colon + 1, (space == std::string::npos ? line.size() : space) - colon - 1);
      if (!allowed.contains(pt)) {
        continue;
      }
    }
    filtered.push_back(line);
  }

  return JoinLines(filtered, eol);
}

std::string RewriteH265TierFlag(std::string_view sdp, int tier_flag) {
  auto eol = DetectLineEnding(sdp);
  auto lines = SplitLines(sdp);
  bool in_video = false;
  std::unordered_set<std::string> h265_pts;
  for (const auto& line : lines) {
    if (line.rfind("m=video", 0) == 0) {
      in_video = true;
      continue;
    }
    if (line.rfind("m=", 0) == 0 && in_video) {
      in_video = false;
    }
    if (in_video && line.rfind("a=rtpmap:", 0) == 0 && NormalizeCodec(line).find("H265") != std::string::npos) {
      const auto colon = line.find(':');
      const auto space = line.find(' ', colon + 1);
      h265_pts.insert(line.substr(colon + 1, space - colon - 1));
    }
  }
  for (auto& line : lines) {
    if (line.rfind("a=fmtp:", 0) != 0) continue;
    const auto colon = line.find(':');
    const auto space = line.find(' ', colon + 1);
    const auto pt = line.substr(colon + 1, space - colon - 1);
    if (!h265_pts.contains(pt)) continue;
    line = std::regex_replace(line, std::regex(R"(tier-flag=1)", std::regex::icase), "tier-flag=" + std::to_string(tier_flag));
  }
  return JoinLines(lines, eol);
}

std::string RewriteH265LevelIdByProfile(std::string_view sdp, int max_level_main, int max_level_main10) {
  auto eol = DetectLineEnding(sdp);
  auto lines = SplitLines(sdp);
  bool in_video = false;
  std::unordered_set<std::string> h265_pts;
  for (const auto& line : lines) {
    if (line.rfind("m=video", 0) == 0) {
      in_video = true;
      continue;
    }
    if (line.rfind("m=", 0) == 0 && in_video) {
      in_video = false;
    }
    if (in_video && line.rfind("a=rtpmap:", 0) == 0 && NormalizeCodec(line).find("H265") != std::string::npos) {
      const auto colon = line.find(':');
      const auto space = line.find(' ', colon + 1);
      h265_pts.insert(line.substr(colon + 1, space - colon - 1));
    }
  }
  for (auto& line : lines) {
    if (line.rfind("a=fmtp:", 0) != 0) continue;
    const auto colon = line.find(':');
    const auto space = line.find(' ', colon + 1);
    const auto pt = line.substr(colon + 1, space - colon - 1);
    if (!h265_pts.contains(pt)) continue;
    std::smatch profile;
    std::smatch level;
    if (!std::regex_search(line, profile, std::regex(R"((?:^|;)\s*profile-id=(\d+))", std::regex::icase)) || !std::regex_search(line, level, std::regex(R"((?:^|;)\s*level-id=(\d+))", std::regex::icase))) {
      continue;
    }
    const int profile_id = std::stoi(profile[1].str());
    const int offered = std::stoi(level[1].str());
    const int maximum = profile_id == 2 ? max_level_main10 : max_level_main;
    if (offered > maximum) {
      line = std::regex_replace(line, std::regex(R"((level-id=)(\d+))", std::regex::icase), "$1" + std::to_string(maximum));
    }
  }
  return JoinLines(lines, eol);
}

std::string ExtractIceUfragFromOffer(std::string_view sdp) {
  std::smatch match;
  const std::string value(sdp);
  if (std::regex_search(value, match, std::regex(R"(a=ice-ufrag:([^\r\n]+))"))) {
    return match[1].str();
  }
  return {};
}

std::string MungeAnswerSdp(std::string_view sdp, int max_bitrate_kbps) {
  const auto line_ending = DetectLineEnding(sdp);
  auto lines = SplitLines(sdp);
  std::vector<std::string> out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    auto current = lines[i];
    if (current.rfind("a=fmtp:", 0) == 0 && current.find("minptime=") != std::string::npos && current.find("stereo=1") == std::string::npos) {
      current += ";stereo=1";
    }
    out.push_back(current);
    if (current.rfind("m=video", 0) == 0) {
      out.push_back("b=AS:" + std::to_string(max_bitrate_kbps));
    } else if (current.rfind("m=audio", 0) == 0) {
      out.push_back("b=AS:128");
    }
  }
  return JoinLines(out, line_ending);
}

std::optional<int> ParsePartialReliableThresholdMs(std::string_view sdp) {
  std::smatch match;
  const std::string value(sdp);
  if (!std::regex_search(value, match, std::regex(R"(a=ri\.partialReliableThresholdMs:(\d+))", std::regex::icase))) {
    return std::nullopt;
  }
  return std::max(1, std::min(5000, std::stoi(match[1].str())));
}

IceCredentials ExtractIceCredentials(std::string_view sdp) {
  const std::string value(sdp);
  IceCredentials credentials{};
  std::smatch match;
  if (std::regex_search(value, match, std::regex(R"(a=ice-ufrag:([^\r\n]+))"))) {
    credentials.ufrag = match[1].str();
  }
  if (std::regex_search(value, match, std::regex(R"(a=ice-pwd:([^\r\n]+))"))) {
    credentials.password = match[1].str();
  }
  if (std::regex_search(value, match, std::regex(R"(a=fingerprint:sha-256 ([^\r\n]+))"))) {
    credentials.fingerprint = match[1].str();
  }
  return credentials;
}

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
    const IceCredentials& credentials) {
  const bool is_high_fps = fps >= 90;
  const bool is_120 = fps == 120;
  const bool is_240 = fps >= 240;
  const bool is_av1 = NormalizeCodec(std::string(codec)) == "AV1";
  const bool ten_bit = std::string(color_quality).rfind("10bit", 0) == 0 && NormalizeCodec(std::string(codec)) != "H264";
  const int bit_depth = ten_bit ? 10 : 8;
  const int min_bitrate = std::max(5000, max_bitrate_kbps * 35 / 100);
  const int initial_bitrate = std::max(min_bitrate, max_bitrate_kbps * 70 / 100);

  std::vector<std::string> lines = {
      "v=0",
      "o=SdpTest test_id_13 14 IN IPv4 127.0.0.1",
      "s=-",
      "t=0 0",
      "a=general.icePassword:" + credentials.password,
      "a=general.iceUserNameFragment:" + credentials.ufrag,
      "a=general.dtlsFingerprint:" + credentials.fingerprint,
      "m=video 0 RTP/AVP",
      "a=msid:fbc-video-0",
      "a=vqos.fec.rateDropWindow:10",
      "a=vqos.fec.minRequiredFecPackets:2",
      "a=vqos.fec.repairMinPercent:5",
      "a=vqos.fec.repairPercent:5",
      "a=vqos.fec.repairMaxPercent:35",
      "a=vqos.drc.enable:0",
      "a=vqos.dfc.enable:0",
      "a=video.dx9EnableNv12:1",
      "a=video.dx9EnableHdr:1",
      "a=vqos.qpg.enable:1",
      "a=vqos.resControl.qp.qpg.featureSetting:7",
      "a=bwe.useOwdCongestionControl:1",
      "a=video.enableRtpNack:1",
      "a=vqos.bw.txRxLag.minFeedbackTxDeltaMs:200",
      "a=vqos.drc.bitrateIirFilterFactor:18",
      "a=video.packetSize:1140",
      "a=packetPacing.minNumPacketsPerGroup:15",
  };
  if (is_high_fps) {
    lines.insert(lines.end(), {
        "a=bwe.iirFilterFactor:8",
        "a=video.encoderFeatureSetting:47",
        "a=video.encoderPreset:6",
        "a=vqos.resControl.cpmRtc.badNwSkipFramesCount:600",
        "a=vqos.resControl.cpmRtc.decodeTimeThresholdMs:9",
        std::string("a=video.fbcDynamicFpsGrabTimeoutMs:") + (is_120 ? "6" : "18"),
        std::string("a=vqos.resControl.cpmRtc.serverResolutionUpdateCoolDownCount:") + (is_120 ? "6000" : "12000"),
    });
  }
  if (is_240) {
    lines.insert(lines.end(), {
        "a=video.enableNextCaptureMode:1",
        "a=vqos.maxStreamFpsEstimate:240",
        "a=video.videoSplitEncodeStripsPerFrame:3",
        "a=video.updateSplitEncodeStateDynamically:1",
    });
  }
  lines.insert(lines.end(), {
      "a=vqos.adjustStreamingFpsDuringOutOfFocus:1",
      "a=vqos.resControl.cpmRtc.ignoreOutOfFocusWindowState:1",
      "a=vqos.resControl.perfHistory.rtcIgnoreOutOfFocusWindowState:1",
      "a=vqos.resControl.cpmRtc.featureMask:0",
      "a=vqos.resControl.cpmRtc.enable:0",
      "a=vqos.resControl.cpmRtc.minResolutionPercent:100",
      "a=vqos.resControl.cpmRtc.resolutionChangeHoldonMs:999999",
      std::string("a=packetPacing.numGroups:") + (is_120 ? "3" : "5"),
      "a=packetPacing.maxDelayUs:1000",
      "a=packetPacing.minNumPacketsFrame:10",
      "a=video.rtpNackQueueLength:1024",
      "a=video.rtpNackQueueMaxPackets:512",
      "a=video.rtpNackMaxPacketCount:25",
      "a=vqos.drc.qpMaxResThresholdAdj:4",
      "a=vqos.grc.qpMaxResThresholdAdj:4",
      "a=vqos.drc.iirFilterFactor:100",
  });
  if (is_av1) {
    lines.insert(lines.end(), {
        "a=vqos.drc.minQpHeadroom:20",
        "a=vqos.drc.lowerQpThreshold:100",
        "a=vqos.drc.upperQpThreshold:200",
        "a=vqos.drc.minAdaptiveQpThreshold:180",
        "a=vqos.drc.qpCodecThresholdAdj:0",
        "a=vqos.drc.qpMaxResThresholdAdj:20",
        "a=vqos.dfc.minQpHeadroom:20",
        "a=vqos.dfc.qpLowerLimit:100",
        "a=vqos.dfc.qpMaxUpperLimit:200",
        "a=vqos.dfc.qpMinUpperLimit:180",
        "a=vqos.dfc.qpMaxResThresholdAdj:20",
        "a=vqos.dfc.qpCodecThresholdAdj:0",
        "a=vqos.grc.minQpHeadroom:20",
        "a=vqos.grc.lowerQpThreshold:100",
        "a=vqos.grc.upperQpThreshold:200",
        "a=vqos.grc.minAdaptiveQpThreshold:180",
        "a=vqos.grc.qpMaxResThresholdAdj:20",
        "a=vqos.grc.qpCodecThresholdAdj:0",
        "a=video.minQp:25",
        "a=video.enableAv1RcPrecisionFactor:1",
    });
  }
  lines.insert(lines.end(), {
      "a=video.clientViewportWd:" + std::to_string(client_viewport_width),
      "a=video.clientViewportHt:" + std::to_string(client_viewport_height),
      "a=video.maxFPS:" + std::to_string(fps),
      "a=video.initialBitrateKbps:" + std::to_string(initial_bitrate),
      "a=video.initialPeakBitrateKbps:" + std::to_string(max_bitrate_kbps),
      "a=vqos.bw.maximumBitrateKbps:" + std::to_string(max_bitrate_kbps),
      "a=vqos.bw.minimumBitrateKbps:" + std::to_string(min_bitrate),
      "a=vqos.bw.peakBitrateKbps:" + std::to_string(max_bitrate_kbps),
      "a=vqos.bw.serverPeakBitrateKbps:" + std::to_string(max_bitrate_kbps),
      "a=vqos.bw.enableBandwidthEstimation:1",
      "a=vqos.bw.disableBitrateLimit:0",
      "a=vqos.grc.maximumBitrateKbps:" + std::to_string(max_bitrate_kbps),
      "a=vqos.grc.enable:0",
      "a=video.maxNumReferenceFrames:4",
      "a=video.mapRtpTimestampsToFrames:1",
      "a=video.encoderCscMode:3",
      "a=video.dynamicRangeMode:0",
      "a=video.bitDepth:" + std::to_string(bit_depth),
      std::string("a=video.scalingFeature1:") + (is_av1 ? "1" : "0"),
      "a=video.prefilterParams.prefilterModel:0",
      "m=audio 0 RTP/AVP",
      "a=msid:audio",
      "m=mic 0 RTP/AVP",
      "a=msid:mic",
      "a=rtpmap:0 PCMU/8000",
      "m=application 0 RTP/AVP",
      "a=msid:input_1",
      "a=ri.partialReliableThresholdMs:" + std::to_string(partial_reliable_threshold_ms),
      "a=video.width:" + std::to_string(width),
      "a=video.height:" + std::to_string(height),
      "",
  });

  return JoinLines(lines, "\n");
}

}  // namespace opennow::native
