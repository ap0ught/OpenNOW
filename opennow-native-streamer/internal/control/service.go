package control

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"runtime"

	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/ipc"
	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/platform"
	streamwebrtc "github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/webrtc"
	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/pkg/protocol"
)

type Service struct {
	ipc    *ipc.Client
	webrtc *streamwebrtc.Client
	caps   platform.Capabilities
}

func New(client *ipc.Client) *Service {
	service := &Service{ipc: client, caps: platform.Probe()}
	service.webrtc = streamwebrtc.New(service)
	return service
}

func (s *Service) Handle(ctx context.Context, env protocol.Envelope) error {
	switch env.Type {
	case "hello-ack":
		return nil
	case "start-session":
		var req protocol.StartSessionRequest
		if err := json.Unmarshal(env.Payload, &req); err != nil {
			return err
		}
		return s.webrtc.StartSession(ctx, req)
	case "signaling-offer":
		var req protocol.SignalOffer
		if err := json.Unmarshal(env.Payload, &req); err != nil {
			return err
		}
		return s.webrtc.HandleOffer(req)
	case "remote-ice":
		var req protocol.IceCandidate
		if err := json.Unmarshal(env.Payload, &req); err != nil {
			return err
		}
		return s.webrtc.AddRemoteICE(req)
	case "input":
		var req protocol.InputMessage
		if err := json.Unmarshal(env.Payload, &req); err != nil {
			return err
		}
		return s.webrtc.HandleInput(req)
	case "request-keyframe":
		return s.webrtc.RequestKeyframe()
	case "stop":
		return s.webrtc.Close()
	default:
		return nil
	}
}

func (s *Service) Emit(msgType string, payload any) error {
	return s.ipc.Send(msgType, payload)
}

func (s *Service) Hello(ctx context.Context) error {
	_ = ctx
	return s.ipc.Send("hello", protocol.Hello{ProcessID: os.Getpid(), Platform: runtime.GOOS, Arch: runtime.GOARCH, Features: s.caps.Features})
}

func (s *Service) Close() error {
	if err := s.webrtc.Close(); err != nil {
		return fmt.Errorf("close webrtc: %w", err)
	}
	return s.ipc.Close()
}
