package platform

import "runtime"

type Capabilities struct {
	Platform          string   `json:"platform"`
	Arch              string   `json:"arch"`
	Features          []string `json:"features"`
	PreferredDecoders []string `json:"preferredDecoders"`
	SupportsGStreamer bool     `json:"supportsGStreamer"`
	IsLinuxARM        bool     `json:"isLinuxArm"`
}

func Probe() Capabilities {
	caps := Capabilities{
		Platform: runtime.GOOS,
		Arch:     runtime.GOARCH,
	}
	caps.IsLinuxARM = caps.Platform == "linux" && (caps.Arch == "arm64" || caps.Arch == "arm")
	switch caps.Platform {
	case "windows":
		caps.Features = []string{"window", "audio", "controller", "gstreamer-optional"}
		caps.PreferredDecoders = []string{"d3d11h264dec", "d3d11h265dec", "d3d11av1dec", "avdec_h264", "avdec_h265", "dav1ddec"}
	case "darwin":
		caps.Features = []string{"window", "audio", "controller", "gstreamer-optional"}
		caps.PreferredDecoders = []string{"vtdec_hw", "avdec_h264", "avdec_h265", "dav1ddec"}
	default:
		caps.Features = []string{"window", "audio", "controller", "gstreamer-optional"}
		if caps.IsLinuxARM {
			caps.PreferredDecoders = []string{"v4l2slh264dec", "v4l2slh265dec", "vaapih264dec", "avdec_h264", "avdec_h265", "dav1ddec"}
		} else {
			caps.PreferredDecoders = []string{"nvh264dec", "nvh265dec", "vaapih264dec", "vaapih265dec", "avdec_h264", "avdec_h265", "dav1ddec"}
		}
	}
	return caps
}
