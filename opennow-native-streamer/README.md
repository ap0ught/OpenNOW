# OpenNOW Native Streamer

`OpenNOW Native Streamer` is a standalone Rust project that provides the foundation for a separate native streaming process and window for OpenNOW.

## Why Rust + GStreamer

- Rust keeps the control plane explicit, typed, and maintainable across Windows, macOS, Linux, and Linux ARM.
- GStreamer remains the intended media/rendering backbone for decode, audio playback, and future platform-specific acceleration.
- The process split preserves the existing Electron/Chromium streaming path as a fallback while enabling a native transport and window architecture.

## Ownership split

- Electron main process keeps CloudMatch session creation, polling, claiming, stopping, and the NVST signaling WebSocket.
- This Rust process owns native process lifecycle, protocol-sensitive SDP handling, native window/media runtime bootstrapping, and input packet encoding.
- Electron main and the native process communicate over a versioned framed JSON IPC channel over a local Unix socket or named pipe.

## Current foundation scope

Implemented in this project:

- versioned local IPC framing and message codec
- SDP helper port for `extractPublicIp()`, `fixServerIp()`, codec filtering, H.265 normalization, answer munging, NVST SDP generation, and partial-reliability parsing
- input packet encoder covering keyboard, mouse, wheel, and controller/gamepad packet layout
- controller that receives offers/ICE via IPC, produces local answer + manual ICE, and emits state/stats/log/error events
- media runtime abstraction with an optional GStreamer-backed implementation (`--features gstreamer-backend`) and a CI-safe stub backend when GStreamer development libraries are unavailable

Not yet migrated in this task foundation:

- real decoder/render sink hookup to the negotiated RTP stream
- production controller/window toolkit integration
- microphone parity beyond architecture scaffolding
- screenshots/recording migration
- HDR and 10-bit output polish

## Platform notes

- Windows: architecture expects D3D11/Media Foundation style decode probing.
- macOS: architecture expects VideoToolbox-backed decode probing.
- Linux x64: architecture expects VA-API/NVDEC/software probing.
- Linux ARM / Raspberry Pi: architecture keeps ARM-specific decode logic isolated so V4L2/stateless/software probing can be added without rewriting the control plane.

## Development

Run tests:

```bash
cargo test --manifest-path opennow-native-streamer/Cargo.toml
```

Run the native process directly against a controller-created socket:

```bash
cargo run --manifest-path opennow-native-streamer/Cargo.toml -- --ipc-endpoint /tmp/opennow-native-streamer.sock
```

Enable the GStreamer-backed runtime when the machine has GStreamer development libraries installed:

```bash
cargo test --manifest-path opennow-native-streamer/Cargo.toml --features gstreamer-backend
```
