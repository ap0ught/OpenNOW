# Development Guide

This guide covers the active Electron-based OpenNOW client in [`opennow-stable/`](../opennow-stable).

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

## Architecture

### Main process

The main process handles platform and system responsibilities:

- OAuth and session bootstrap
- Game catalog fetches and cache refresh
- CloudMatch session creation, polling, claiming, and stopping
- Signaling and low-level Electron integration
- Local media management for screenshots and recordings
- Persistent settings storage

Key entry point:

- [`opennow-stable/src/main/index.ts`](../opennow-stable/src/main/index.ts)

#### Main process lifecycle

Electron does not use a custom game-style “main loop” file; behavior is driven by **Node’s event loop** plus **Electron `app` / `BrowserWindow` / `ipcMain` events**. In OpenNOW the flow in [`index.ts`](../opennow-stable/src/main/index.ts) is roughly:

1. **Module load (before `app.whenReady()`)** — Reads basic video acceleration preferences from `userData/settings.json` (if present) so Chromium **command-line switches** (WebRTC, GPU decode/encode, background throttling, and related flags) can be applied before the GPU stack initializes.
2. **`app.whenReady()`** — Starts core services in order: log capture, cache manager, auth state on disk, settings manager, optional Discord Rich Presence, default-session **permission** handlers for media/fullscreen/pointer lock, **`registerIpcHandlers()`** (all `ipcMain.handle` / `on` registrations), cache refresh scheduler (with event-bus → renderer notifications), then **`createMainWindow()`** (loads the UI and attaches window helpers such as persisting window size).
3. **Runtime** — The UI and WebRTC live mostly in the **renderer**; the main process keeps running to service **IPC**, **signaling** (when connected), **CloudMatch** calls triggered from IPC, filesystem work, and background refresh timers.
4. **`app.on("activate")` (macOS)** — If every window was closed but the app stays alive (macOS convention), opening the app again recreates the main window.
5. **`window-all-closed`** — On Windows and Linux, closing all windows calls `app.quit()`. On macOS the app typically stays running until the user quits from the menu.
6. **`before-quit`** — Stops the refresh scheduler, disconnects any active signaling client, and tears down Discord RPC so work does not continue after shutdown.

Until the process exits, **IPC handlers registered in `registerIpcHandlers()` remain live** for the renderer and preload bridge.

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
- Handling controller input, shortcuts, stats overlay, screenshots, recordings, and settings UI

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

### Run automated tests (local-only)

Tests are **offline** by design: they exercise pure logic and small main-process helpers that do not call GeForce NOW, NVIDIA APIs, or the network.

- **Vitest** (`src/**/*.vitest.test.ts`) — shared helpers and other renderer-adjacent pure code, with `@shared` path aliases aligned to Vite.
- **Node.js built-in test runner** (`src/**/*.node.test.ts`, via `tsx`) — Electron-free modules such as `settingsHydrate` (no `electron` import), for maximum compatibility with stock Node tooling.

From the repository root:

```bash
npm test
```

Or inside `opennow-stable/`:

```bash
npm run test:vitest
npm run test:node
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

| Target      | Output                              |
| ----------- | ----------------------------------- |
| Windows     | NSIS installer, portable executable |
| macOS x64   | `dmg`, `zip`                        |
| macOS arm64 | `dmg`, `zip`                        |
| Linux x64   | `AppImage`, `deb`                   |
| Linux ARM64 | `AppImage`, `deb`                   |

## Notes For Contributors

- The active app is the Electron client. If you see older references to previous implementations, prefer `opennow-stable/`.
- Root-level npm scripts are convenience wrappers around the `opennow-stable` workspace.
- Before opening a PR, run `npm test`, `npm run typecheck`, and `npm run build`.

For contribution workflow details, see [`.github/CONTRIBUTING.md`](../.github/CONTRIBUTING.md).
