# Development Guide

This guide covers the active Electron-based OpenNOW client in [`opennow-stable/`](../opennow-stable).
The repository also contains the separate Go native streamer foundation in [`opennow-native-streamer/`](../opennow-native-streamer).

## Prerequisites

- Node.js 22 or newer
- npm
- A GeForce NOW account for end-to-end testing

## Getting Started

Install app dependencies inside the Electron workspace:

```bash
cd opennow-stable
npm install
```

You can then work either from the app directory or from the repository root.

From the repository root:

```bash
npm run dev
npm run typecheck
npm run build
npm run dist
```

Directly inside `opennow-stable/`:

```bash
npm run dev
npm run preview
npm run typecheck
npm run build
npm run dist
npm run dist:signed
```

## Workspace Layout

```text
opennow-stable/
├── src/
│   ├── main/           Electron main process
│   │   ├── gfn/        Auth, game catalogs, subscriptions, CloudMatch, signaling
│   │   └── services/   Cache and refresh helpers
│   ├── preload/        Safe API exposed to the renderer
│   ├── renderer/src/   React application
│   │   ├── components/ Screens, stream UI, settings, library, navigation
│   │   ├── gfn/        WebRTC client and input protocol
│   │   └── utils/      Diagnostics and UI helpers
│   └── shared/         Shared types, IPC channels, logging helpers
├── electron.vite.config.ts
├── package.json
└── tsconfig*.json
```

```text
opennow-native-streamer/
├── cmd/opennow-native-streamer/  Native process entrypoint
├── internal/ipc/                Versioned main↔native control protocol
├── internal/webrtc/             SDP/NVST helpers and Pion endpoint
├── internal/media/              GStreamer-backed playback abstraction
├── internal/input/              GFN-compatible input encoding
├── internal/platform/           Platform and decoder probing
└── pkg/protocol/                Shared IPC message contracts
```

## Architecture

### Main process

The main process handles platform and system responsibilities:

- OAuth and session bootstrap
- Game catalog fetches and cache refresh
- CloudMatch session creation, polling, claiming, and stopping
- Signaling, native streamer supervision, and low-level Electron integration
- Local media management for screenshots and recordings
- Persistent settings storage

Key entry point:

- [`opennow-stable/src/main/index.ts`](../opennow-stable/src/main/index.ts)

### Preload

The preload layer exposes a narrow IPC surface to the renderer with `contextBridge`.

Key entry point:

- [`opennow-stable/src/preload/index.ts`](../opennow-stable/src/preload/index.ts)

### Renderer

The renderer is a React app responsible for:

- Login and provider selection
- Browsing the catalog and public listings
- Managing stream launch state and session recovery
- Rendering the WebRTC stream
- Switching between Chromium/WebRTC and the native streamer backend
- Handling controller input, shortcuts, stats overlay, screenshots, recordings, and settings UI

### Native streamer foundation

The native streamer is a separate process and window, not an embedded browser surface. In the current foundation:

- Electron main continues to own session lifecycle and the signaling WebSocket
- Electron main spawns the Go process and communicates over a local versioned socket protocol
- The Go process handles SDP/NVST munging, Pion `PeerConnection` setup, native input encoding, and the media abstraction
- GStreamer is the intended media/rendering backbone; non-`gstreamer` builds keep the control plane compilable for CI and development environments that do not have GStreamer installed
- Linux ARM / Raspberry Pi are first-class in the module layout through platform probing and decoder fallback structure

Key entry points:

- [`opennow-stable/src/renderer/src/App.tsx`](../opennow-stable/src/renderer/src/App.tsx)
- [`opennow-stable/src/renderer/src/components/StreamView.tsx`](../opennow-stable/src/renderer/src/components/StreamView.tsx)
- [`opennow-stable/src/renderer/src/components/SettingsPage.tsx`](../opennow-stable/src/renderer/src/components/SettingsPage.tsx)

## Common Tasks

### Start the app in development

```bash
cd opennow-stable
npm run dev
```

### Run type checks

```bash
cd opennow-stable
npm run typecheck
```

### Build production bundles

```bash
cd opennow-stable
npm run build
```

### Package release artifacts locally

Unsigned packages:

```bash
cd opennow-stable
npm run dist
```

Signed packages, if your environment is configured for signing:

```bash
cd opennow-stable
npm run dist:signed
```

## CI And Releases

The repository includes two main GitHub Actions workflows:

- [`auto-build.yml`](../.github/workflows/auto-build.yml) builds pull requests and pushes to `main` and `dev`
- [`release.yml`](../.github/workflows/release.yml) packages and publishes tagged or manually-triggered releases

Current build matrix:

| Target | Output |
| --- | --- |
| Windows | NSIS installer, portable executable |
| macOS x64 | `dmg`, `zip` |
| macOS arm64 | `dmg`, `zip` |
| Linux x64 | `AppImage`, `deb` |
| Linux ARM64 | `AppImage`, `deb` |

## Notes For Contributors

- The active app is the Electron client. If you see older references to previous implementations, prefer `opennow-stable/`.
- Root-level npm scripts are convenience wrappers around the `opennow-stable` workspace.
- Before opening a PR, run `npm run typecheck` and `npm run build`.

For contribution workflow details, see [`.github/CONTRIBUTING.md`](../.github/CONTRIBUTING.md).
