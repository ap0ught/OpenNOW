# OpenNOW Native Streamer

`OpenNOW Native Streamer` is a separate Go process for the OpenNOW Electron app. It owns the native streaming window, the WebRTC endpoint, input encoding, and the media pipeline abstraction while Electron main continues to own CloudMatch session lifecycle and the NVST signaling WebSocket.

## Why Go + GStreamer

- Go keeps the control plane, IPC, and process lifecycle readable.
- GStreamer is the long-term media/rendering backbone for cross-platform decode, audio output, and native-window presentation.
- The Electron app keeps the Chromium/WebRTC path intact as the fallback when the native toggle is disabled.
- Linux ARM and Raspberry Pi remain explicit targets through platform probing and isolated media/platform modules instead of desktop-only assumptions.

## Ownership split

- Electron main
  - session create/poll/claim/stop
  - signaling WebSocket
  - spawning and supervising the native process
  - forwarding offer/ICE and relaying answer/ICE back to signaling
- Native process
  - native window lifecycle
  - Pion `PeerConnection` and `DataChannel` endpoint
  - protocol-sensitive SDP munging and NVST SDP generation
  - GFN input packet encoding
  - media sink abstraction for GStreamer-backed playback

## IPC protocol

Electron main and the native process communicate over a versioned local socket using JSON lines.

Message classes:

- `hello` / `hello-ack`
- `start-session`
- `signaling-offer`
- `remote-ice`
- `local-answer`
- `local-ice`
- `input`
- `request-keyframe`
- `stats`
- `state`
- `error`
- `stop`

## Build

Prerequisites for the real media backend:

- Go 1.25+
- GStreamer 1.22+
- SDL2 development libraries

Development / CI can build the control-plane foundation without native media dependencies:

```bash
go build ./...
```

To build the native window + GStreamer backend:

```bash
go build -tags gstreamer ./cmd/opennow-native-streamer
```

## Platform notes

- Windows: intended decoder path is Media Foundation / D3D11-backed GStreamer plugins when available.
- macOS: intended decoder path is VideoToolbox-backed GStreamer plugins.
- Linux x64: intended decoder path is VA-API/NVDEC depending on host.
- Linux ARM / Raspberry Pi: keep decoder selection capability-based. V4L2, VA-API, and software fallback must remain valid options.

## Current scope

Implemented foundation in this change:

- versioned main↔native IPC
- native process lifecycle and separate window abstraction
- protocol-sensitive SDP and NVST SDP helpers ported to Go
- Pion-based WebRTC endpoint with input/control channels
- GFN-compatible mouse, keyboard, and controller packet encoding foundation
- Electron integration behind a settings toggle

Deferred for follow-up:

- microphone parity beyond scaffolding
- screenshot / recording migration
- HDR / 10-bit output polish
- overlay parity with the browser path
- full production GStreamer decode path validation on each target platform
