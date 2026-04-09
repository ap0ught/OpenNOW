package webrtc

import (
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type IceCredentials struct {
	Ufrag       string
	Password    string
	Fingerprint string
}

type NvstParams struct {
	Width                    int
	Height                   int
	ClientViewportWidth      int
	ClientViewportHeight     int
	FPS                      int
	MaxBitrateKbps           int
	PartialReliableThreshold int
	Codec                    string
	ColorQuality             string
	Credentials              IceCredentials
}

func ExtractPublicIP(hostOrIP string) string {
	if matched, _ := regexp.MatchString(`^\d{1,3}(?:\.\d{1,3}){3}$`, hostOrIP); matched {
		return hostOrIP
	}
	first := strings.Split(hostOrIP, ".")[0]
	parts := strings.Split(first, "-")
	if len(parts) != 4 {
		return ""
	}
	for _, part := range parts {
		if _, err := strconv.Atoi(part); err != nil {
			return ""
		}
	}
	return strings.Join(parts, ".")
}

func FixServerIP(sdp, serverIP string) string {
	ip := ExtractPublicIP(serverIP)
	if ip == "" {
		return sdp
	}
	replaced := strings.ReplaceAll(sdp, "c=IN IP4 0.0.0.0", "c=IN IP4 "+ip)
	candidateRe := regexp.MustCompile(`(a=candidate:\S+\s+\d+\s+\w+\s+\d+\s+)0\.0\.0\.0(\s+)`)
	return candidateRe.ReplaceAllString(replaced, `${1}`+ip+`${2}`)
}

func ExtractIceCredentials(sdp string) IceCredentials {
	creds := IceCredentials{}
	for _, line := range strings.Split(sdp, "\n") {
		line = strings.TrimSpace(line)
		switch {
		case strings.HasPrefix(line, "a=ice-ufrag:"):
			creds.Ufrag = strings.TrimPrefix(line, "a=ice-ufrag:")
		case strings.HasPrefix(line, "a=ice-pwd:"):
			creds.Password = strings.TrimPrefix(line, "a=ice-pwd:")
		case strings.HasPrefix(line, "a=fingerprint:sha-256 "):
			creds.Fingerprint = strings.TrimPrefix(line, "a=fingerprint:sha-256 ")
		}
	}
	return creds
}

func ExtractIceUfragFromOffer(sdp string) string {
	for _, line := range strings.Split(sdp, "\n") {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "a=ice-ufrag:") {
			return strings.TrimPrefix(line, "a=ice-ufrag:")
		}
	}
	return ""
}

func ParsePartialReliableThresholdMs(sdp string) int {
	re := regexp.MustCompile(`a=ri\.partialReliableThresholdMs:(\d+)`)
	match := re.FindStringSubmatch(sdp)
	if len(match) < 2 {
		return 120
	}
	value, err := strconv.Atoi(match[1])
	if err != nil || value <= 0 {
		return 120
	}
	if value > 5000 {
		return 5000
	}
	return value
}

func MungeAnswerSDP(sdp string, maxBitrateKbps int) string {
	lines := strings.Split(strings.ReplaceAll(sdp, "\r\n", "\n"), "\n")
	result := make([]string, 0, len(lines)+8)
	for i, line := range lines {
		result = append(result, line)
		if strings.HasPrefix(line, "m=video") || strings.HasPrefix(line, "m=audio") {
			next := ""
			if i+1 < len(lines) {
				next = lines[i+1]
			}
			if !strings.HasPrefix(next, "b=") {
				bitrate := maxBitrateKbps
				if strings.HasPrefix(line, "m=audio") {
					bitrate = 128
				}
				result = append(result, fmt.Sprintf("b=AS:%d", bitrate))
			}
		}
		if strings.HasPrefix(line, "a=fmtp:") && strings.Contains(line, "minptime=") && !strings.Contains(line, "stereo=1") {
			result[len(result)-1] = line + ";stereo=1"
		}
	}
	return strings.Join(result, "\r\n")
}

func PreferCodec(sdp, codec string) string {
	lines := strings.Split(strings.ReplaceAll(sdp, "\r\n", "\n"), "\n")
	codec = strings.ToUpper(codec)
	payloads := map[string]string{}
	rtxAPT := map[string]string{}
	inVideo := false
	for _, line := range lines {
		if strings.HasPrefix(line, "m=video") {
			inVideo = true
			continue
		}
		if strings.HasPrefix(line, "m=") && inVideo {
			inVideo = false
		}
		if !inVideo || !strings.HasPrefix(line, "a=rtpmap:") {
			continue
		}
		rest := strings.TrimPrefix(line, "a=rtpmap:")
		parts := strings.Fields(rest)
		if len(parts) < 2 {
			continue
		}
		name := strings.ToUpper(strings.Split(parts[1], "/")[0])
		if name == "HEVC" {
			name = "H265"
		}
		payloads[parts[0]] = name
	}
	inVideo = false
	for _, line := range lines {
		if strings.HasPrefix(line, "m=video") {
			inVideo = true
			continue
		}
		if strings.HasPrefix(line, "m=") && inVideo {
			inVideo = false
		}
		if !inVideo || !strings.HasPrefix(line, "a=fmtp:") {
			continue
		}
		rest := strings.TrimPrefix(line, "a=fmtp:")
		parts := strings.Fields(rest)
		if len(parts) < 2 {
			continue
		}
		aptMatch := regexp.MustCompile(`(?:^|;)\s*apt=(\d+)`).FindStringSubmatch(parts[1])
		if len(aptMatch) == 2 {
			rtxAPT[parts[0]] = aptMatch[1]
		}
	}
	allowed := map[string]bool{}
	ordered := make([]string, 0)
	for pt, name := range payloads {
		if name == codec {
			allowed[pt] = true
			ordered = append(ordered, pt)
		}
	}
	sort.Strings(ordered)
	for rtx, apt := range rtxAPT {
		if allowed[apt] && payloads[rtx] == "RTX" {
			allowed[rtx] = true
		}
	}
	if len(ordered) == 0 {
		return sdp
	}
	out := make([]string, 0, len(lines))
	inVideo = false
	for _, line := range lines {
		if strings.HasPrefix(line, "m=video") {
			inVideo = true
			parts := strings.Fields(line)
			head := append([]string{}, parts[:3]...)
			pts := make([]string, 0, len(parts)-3)
			for _, pt := range parts[3:] {
				if allowed[pt] {
					pts = append(pts, pt)
				}
			}
			out = append(out, strings.Join(append(head, pts...), " "))
			continue
		}
		if strings.HasPrefix(line, "m=") && inVideo {
			inVideo = false
		}
		if inVideo && (strings.HasPrefix(line, "a=rtpmap:") || strings.HasPrefix(line, "a=fmtp:") || strings.HasPrefix(line, "a=rtcp-fb:")) {
			rest := strings.SplitN(line, ":", 2)
			if len(rest) == 2 {
				pt := strings.Fields(rest[1])
				if len(pt) > 0 && !allowed[pt[0]] {
					continue
				}
			}
		}
		out = append(out, line)
	}
	return strings.Join(out, "\r\n")
}

func BuildNvstSDP(params NvstParams) string {
	minBitrate := max(5000, int(float64(params.MaxBitrateKbps)*0.35))
	initialBitrate := max(minBitrate, int(float64(params.MaxBitrateKbps)*0.7))
	bitDepth := 8
	if strings.HasPrefix(params.ColorQuality, "10bit") && strings.ToUpper(params.Codec) != "H264" {
		bitDepth = 10
	}
	lines := []string{
		"v=0",
		"o=SdpTest test_id_13 14 IN IPv4 127.0.0.1",
		"s=-",
		"t=0 0",
		"a=general.featureFlags:3",
		"a=general.enableRecoveryMode:0",
		fmt.Sprintf("a=general.icePassword:%s", params.Credentials.Password),
		fmt.Sprintf("a=general.iceUserNameFragment:%s", params.Credentials.Ufrag),
		fmt.Sprintf("a=general.dtlsFingerprint:%s", params.Credentials.Fingerprint),
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
		fmt.Sprintf("a=ri.partialReliableThresholdMs:%d", params.PartialReliableThreshold),
		fmt.Sprintf("a=video.width:%d", params.Width),
		fmt.Sprintf("a=video.height:%d", params.Height),
		fmt.Sprintf("a=video.maxFPS:%d", params.FPS),
		fmt.Sprintf("a=video.clientViewportWd:%d", params.ClientViewportWidth),
		fmt.Sprintf("a=video.clientViewportHt:%d", params.ClientViewportHeight),
		fmt.Sprintf("a=video.maxBitrateKbps:%d", params.MaxBitrateKbps),
		fmt.Sprintf("a=video.initialBitrateKbps:%d", initialBitrate),
		fmt.Sprintf("a=video.minBitrateKbps:%d", minBitrate),
		fmt.Sprintf("a=video.codec:%s", strings.ToUpper(params.Codec)),
		fmt.Sprintf("a=video.bitDepth:%d", bitDepth),
	}
	if params.FPS >= 90 {
		lines = append(lines,
			"a=bwe.iirFilterFactor:8",
			"a=video.encoderFeatureSetting:47",
			"a=video.encoderPreset:6",
		)
	}
	return strings.Join(lines, "\r\n")
}
