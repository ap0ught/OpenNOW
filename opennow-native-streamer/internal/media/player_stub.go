package media

import (
	"context"
	"sync/atomic"
)

type NullPlayer struct {
	closed atomic.Bool
}

func (p *NullPlayer) Start(_ context.Context, _ Config) error { return nil }
func (p *NullPlayer) PushVideoRTP(_ []byte) error             { return nil }
func (p *NullPlayer) PushAudioRTP(_ []byte) error             { return nil }
func (p *NullPlayer) SetStatus(_ string)                      {}
func (p *NullPlayer) Close() error {
	p.closed.Store(true)
	return nil
}
