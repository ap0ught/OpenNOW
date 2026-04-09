//go:build gstreamer

package media

import (
	"context"
	"errors"
	"fmt"
	"sync"

	"github.com/go-gst/go-gst/gst"
	"github.com/go-gst/go-gst/gst/app"
	"github.com/veandco/go-sdl2/sdl"
)

type gstreamerPlayer struct {
	mu      sync.Mutex
	pipe    *gst.Pipeline
	videoIn *app.Source
	audioIn *app.Source
	window  *sdl.Window
}

func init() {
	RegisterFactory(func() Player { return &gstreamerPlayer{} })
}

func (p *gstreamerPlayer) Start(_ context.Context, cfg Config) error {
	if err := gst.Init(nil); err != nil {
		return err
	}
	if err := sdl.Init(sdl.INIT_VIDEO | sdl.INIT_GAMECONTROLLER | sdl.INIT_AUDIO); err != nil {
		return err
	}
	window, err := sdl.CreateWindow(cfg.WindowTitle, sdl.WINDOWPOS_CENTERED, sdl.WINDOWPOS_CENTERED, int32(cfg.Width), int32(cfg.Height), sdl.WINDOW_RESIZABLE|sdl.WINDOW_ALLOW_HIGHDPI)
	if err != nil {
		return err
	}
	p.window = window

	launch := "appsrc name=videoIn is-live=true format=time do-timestamp=true ! application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000 ! queue leaky=downstream max-size-buffers=4 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink sync=false appsrc name=audioIn is-live=true format=time do-timestamp=true ! application/x-rtp,media=audio,encoding-name=OPUS,payload=111,clock-rate=48000 ! queue leaky=downstream max-size-buffers=8 ! rtpopusdepay ! opusdec ! audioconvert ! audioresample ! autoaudiosink sync=false"
	obj, err := gst.NewPipelineFromString(launch)
	if err != nil {
		return err
	}
	pipe, ok := obj.(*gst.Pipeline)
	if !ok {
		return errors.New("unexpected pipeline type")
	}
	p.pipe = pipe
	videoElem, err := pipe.GetElementByName("videoIn")
	if err != nil {
		return err
	}
	audioElem, err := pipe.GetElementByName("audioIn")
	if err != nil {
		return err
	}
	p.videoIn = app.SrcFromElement(videoElem)
	p.audioIn = app.SrcFromElement(audioElem)
	_, err = pipe.SetState(gst.StatePlaying)
	return err
}

func (p *gstreamerPlayer) PushVideoRTP(packet []byte) error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.videoIn == nil {
		return nil
	}
	buf := gst.NewBufferFromBytes(packet)
	return p.videoIn.PushBuffer(buf)
}

func (p *gstreamerPlayer) PushAudioRTP(packet []byte) error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.audioIn == nil {
		return nil
	}
	buf := gst.NewBufferFromBytes(packet)
	return p.audioIn.PushBuffer(buf)
}

func (p *gstreamerPlayer) SetStatus(status string) {
	if p.window != nil {
		p.window.SetTitle(fmt.Sprintf("%s — %s", "OpenNOW Native Streamer", status))
	}
}

func (p *gstreamerPlayer) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.pipe != nil {
		p.pipe.SetState(gst.StateNull)
		p.pipe = nil
	}
	if p.window != nil {
		p.window.Destroy()
		p.window = nil
	}
	sdl.Quit()
	return nil
}
