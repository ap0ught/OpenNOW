package protocol

import "encoding/json"

const Version = 1
const WindowTitle = "OpenNOW Native Streamer"

type Envelope struct {
	Type    string          `json:"type"`
	Version int             `json:"version,omitempty"`
	Payload json.RawMessage `json:"payload,omitempty"`
}

type Hello struct {
	ProcessID int      `json:"processId"`
	Platform  string   `json:"platform"`
	Arch      string   `json:"arch"`
	Features  []string `json:"features"`
}

type HelloAck struct {
	Accepted bool   `json:"accepted"`
	Version  int    `json:"version"`
	Message  string `json:"message,omitempty"`
}

type IceServer struct {
	URLs       []string `json:"urls"`
	Username   string   `json:"username,omitempty"`
	Credential string   `json:"credential,omitempty"`
}

type MediaConnectionInfo struct {
	IP   string `json:"ip"`
	Port int    `json:"port"`
}

type NegotiatedStreamProfile struct {
	Resolution string `json:"resolution,omitempty"`
	FPS        int    `json:"fps,omitempty"`
	Codec      string `json:"codec,omitempty"`
}

type SessionInfo struct {
	SessionID               string                   `json:"sessionId"`
	ServerIP                string                   `json:"serverIp"`
	SignalingServer         string                   `json:"signalingServer"`
	SignalingURL            string                   `json:"signalingUrl,omitempty"`
	StreamingBaseURL        string                   `json:"streamingBaseUrl,omitempty"`
	IceServers              []IceServer              `json:"iceServers"`
	MediaConnectionInfo     *MediaConnectionInfo     `json:"mediaConnectionInfo,omitempty"`
	NegotiatedStreamProfile *NegotiatedStreamProfile `json:"negotiatedStreamProfile,omitempty"`
	GPUType                 string                   `json:"gpuType,omitempty"`
}

type LaunchSettings struct {
	Resolution        string  `json:"resolution"`
	FPS               int     `json:"fps"`
	MaxBitrateMbps    float64 `json:"maxBitrateMbps"`
	Codec             string  `json:"codec"`
	ColorQuality      string  `json:"colorQuality"`
	MouseSensitivity  float64 `json:"mouseSensitivity"`
	MouseAcceleration float64 `json:"mouseAcceleration"`
}

type StartSessionRequest struct {
	Session  SessionInfo    `json:"session"`
	Settings LaunchSettings `json:"settings"`
	Window   WindowSettings `json:"window"`
}

type WindowSettings struct {
	Title           string `json:"title"`
	Width           int    `json:"width"`
	Height          int    `json:"height"`
	DisplayScalePPM int    `json:"displayScalePpm,omitempty"`
}

type SignalOffer struct {
	SDP string `json:"sdp"`
}

type IceCandidate struct {
	Candidate        string  `json:"candidate"`
	SDPMid           string  `json:"sdpMid,omitempty"`
	SDPMLineIndex    *uint16 `json:"sdpMLineIndex,omitempty"`
	UsernameFragment string  `json:"usernameFragment,omitempty"`
}

type LocalAnswer struct {
	SDP     string `json:"sdp"`
	NvstSDP string `json:"nvstSdp,omitempty"`
}

type NativeState struct {
	Status  string `json:"status"`
	Message string `json:"message,omitempty"`
}

type NativeError struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	Fatal   bool   `json:"fatal"`
}

type NativeStats struct {
	ConnectionState string  `json:"connectionState"`
	VideoCodec      string  `json:"videoCodec,omitempty"`
	AudioCodec      string  `json:"audioCodec,omitempty"`
	BitrateKbps     int     `json:"bitrateKbps"`
	PacketsLost     uint32  `json:"packetsLost"`
	PacketsReceived uint32  `json:"packetsReceived"`
	RTTMs           float64 `json:"rttMs"`
}

type KeyframeRequest struct {
	Reason        string `json:"reason"`
	BacklogFrames int    `json:"backlogFrames"`
	Attempt       int    `json:"attempt"`
}

type InputMessage struct {
	Kind    string          `json:"kind"`
	Payload json.RawMessage `json:"payload"`
}

type KeyboardInput struct {
	Keycode   uint16 `json:"keycode"`
	Scancode  uint16 `json:"scancode"`
	Modifiers uint16 `json:"modifiers"`
	Down      bool   `json:"down"`
}

type MouseMoveInput struct {
	DX int16 `json:"dx"`
	DY int16 `json:"dy"`
}

type MouseButtonInput struct {
	Button uint8 `json:"button"`
	Down   bool  `json:"down"`
}

type MouseWheelInput struct {
	Delta int16 `json:"delta"`
}

type GamepadInput struct {
	ControllerID uint8  `json:"controllerId"`
	Buttons      uint16 `json:"buttons"`
	LeftTrigger  uint8  `json:"leftTrigger"`
	RightTrigger uint8  `json:"rightTrigger"`
	LeftStickX   int16  `json:"leftStickX"`
	LeftStickY   int16  `json:"leftStickY"`
	RightStickX  int16  `json:"rightStickX"`
	RightStickY  int16  `json:"rightStickY"`
	Connected    bool   `json:"connected"`
}
