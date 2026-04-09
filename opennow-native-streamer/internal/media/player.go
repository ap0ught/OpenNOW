package media

import (
	"context"
	"sync"

	"github.com/OpenCloudGaming/OpenNOW/opennow-native-streamer/internal/platform"
)

type Config struct {
	WindowTitle string
	Width       int
	Height      int
	Codec       string
	Platform    platform.Capabilities
}

type Player interface {
	Start(context.Context, Config) error
	PushVideoRTP([]byte) error
	PushAudioRTP([]byte) error
	SetStatus(string)
	Close() error
}

type Factory func() Player

var (
	factoryMu sync.RWMutex
	factory   Factory = func() Player { return &NullPlayer{} }
)

func RegisterFactory(next Factory) {
	factoryMu.Lock()
	defer factoryMu.Unlock()
	factory = next
}

func New() Player {
	factoryMu.RLock()
	defer factoryMu.RUnlock()
	return factory()
}
