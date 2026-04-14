# OpenNOW — AI Context Document

This document provides structured context for AI coding assistants working on the OpenNOW repository.

## Repository Overview

OpenNOW is an open-source Electron desktop client for GeForce NOW (NVIDIA's cloud gaming service).
It lets users browse the GFN game catalog, tune stream quality settings, and launch cloud gaming sessions.

The active client lives entirely in `opennow-stable/`. The repository root only contains a workspace
shim `package.json` that proxies common scripts into `opennow-stable/`.

## Project Stack

- **Runtime**: Electron 41 (Chromium renderer + Node.js main)
- **Frontend**: React 19 + TypeScript
- **Bundler**: electron-vite (Vite-based)
- **Packaging**: electron-builder (NSIS, portable, dmg, AppImage, deb)
- **Key deps**: `ws` (WebSocket signaling), `discord-rpc` (Rich Presence), `lucide-react` (icons)

## Directory Layout

```
.
├── opennow-stable/            Active Electron app workspace
│   ├── src/
│   │   ├── main/              Electron main process (Node.js)
│   │   │   ├── index.ts       Entry point — app init, BrowserWindow, all IPC handlers
│   │   │   ├── settings.ts    Persistent user settings (userData/settings.json)
│   │   │   ├── discordRpc.ts  Discord Rich Presence integration
│   │   │   ├── gfn/           GFN-specific services
│   │   │   │   ├── auth.ts    OAuth2 PKCE login, token refresh, session persistence
│   │   │   │   ├── cloudmatch.ts  Session create/poll/claim/stop
│   │   │   │   ├── errorCodes.ts  SessionError + GFN error codes
│   │   │   │   ├── games.ts   Game catalog fetching + cache facade
│   │   │   │   ├── signaling.ts  WebSocket signaling client (GfnSignalingClient)
│   │   │   │   ├── subscription.ts  Subscription + dynamic regions
│   │   │   │   └── types.ts   Internal GFN API response types
│   │   │   └── services/
│   │   │       ├── cacheManager.ts    Disk-backed catalog cache
│   │   │       ├── cacheEventBus.ts   EventEmitter for cache lifecycle events
│   │   │       └── refreshScheduler.ts  Periodic background cache refresh
│   │   ├── preload/
│   │   │   └── index.ts       contextBridge — exposes window.openNow API
│   │   ├── renderer/src/      React application
│   │   │   ├── App.tsx        Top-level state machine and stream lifecycle
│   │   │   ├── main.tsx       React root mount
│   │   │   ├── gfn/           WebRTC client and input protocol
│   │   │   ├── components/    All UI screens and panels
│   │   │   ├── shortcuts.ts   Keyboard shortcut handling
│   │   │   ├── controllerNavigation.ts  Gamepad navigation
│   │   │   ├── utils/         Diagnostics, formatting helpers
│   │   │   └── vite-env.d.ts  Declares window.openNow type
│   │   └── shared/
│   │       ├── gfn.ts         Shared request/response types + OpenNowApi interface
│   │       ├── ipc.ts         IPC_CHANNELS constant map
│   │       ├── logger.ts      Log capture and export
│   │       └── sessionError.ts  SessionError serialization for IPC transport
│   ├── electron.vite.config.ts
│   ├── package.json
│   └── tsconfig*.json
├── docs/
│   ├── development.md         Developer setup guide
│   └── streamer-investigation.md
├── .github/
│   ├── workflows/
│   │   ├── auto-build.yml     CI build on PR / push to main+dev
│   │   ├── release.yml        Full cross-platform release pipeline
│   │   └── codeql.yml         CodeQL security scanning (weekly + on push)
│   ├── ISSUE_TEMPLATE/
│   └── CONTRIBUTING.md
└── README.md
```

## Architecture — Three-Process Boundary

```
┌─────────────────────────────────────┐
│  Renderer (Chromium, unsandboxed)   │
│  React UI · WebRTC · window.openNow │
└────────────────┬────────────────────┘
                 │ contextBridge (IPC)
┌────────────────▼────────────────────┐
│  Preload (index.ts)                 │
│  Exposes window.openNow via         │
│  contextBridge.exposeInMainWorld    │
└────────────────┬────────────────────┘
                 │ ipcRenderer.invoke / ipcMain.handle
┌────────────────▼────────────────────┐
│  Main Process (Node.js)             │
│  Auth · CloudMatch · Signaling      │
│  Settings · Media · Cache           │
└─────────────────────────────────────┘
```

**Key rules:**
- Renderer must only call `window.openNow` — never import Electron or Node APIs directly.
- All IPC channels are declared in `src/shared/ipc.ts` (`IPC_CHANNELS` const map).
- Shared data shapes live in `src/shared/gfn.ts` (`OpenNowApi` interface + all request/response types).
- Always update `gfn.ts` and `ipc.ts` first when adding a new feature, then wire preload and main.

## IPC Contract Pattern

```
shared/ipc.ts        — canonical channel name string constant
shared/gfn.ts        — request/response TypeScript types + OpenNowApi interface
preload/index.ts     — ipcRenderer.invoke call exposed on window.openNow
main/index.ts        — ipcMain.handle handler (delegates to gfn/* or services/*)
```

## Auth / Token Flow

1. `AuthService.login()` opens the GFN OAuth2 PKCE login URL in the system browser via `shell.openExternal`.
2. A short-lived HTTP server on `localhost` catches the authorization code redirect.
3. Token exchange and refresh happen against `https://login.nvidia.com`.
4. Session state (tokens, selected provider) is persisted in `userData/auth-state.json`.
5. `AuthService.resolveJwtToken()` / `ensureValidSessionWithStatus()` handle proactive token refresh before expiry.
6. All authenticated IPC handlers call `resolveJwt(payload.token)` to get a fresh token.

## Streaming Flow (Big Picture)

1. Renderer calls `window.openNow.createSession(...)` → IPC → `CREATE_SESSION` handler.
2. Main tries to resume an existing session (active session check + RESUME claim).
3. If none, calls `cloudmatch.createSession(...)` to start a new GFN session.
4. Returns `SessionInfo` (includes `signalingServer`, `iceServers`, `sessionId`).
5. Renderer calls `window.openNow.connectSignaling(...)` → Main opens WebSocket to signaling server.
6. `GfnSignalingClient` relays `offer`, `ice_candidate`, etc. back to renderer via `SIGNALING_EVENT` IPC.
7. Renderer `WebRTC` client (in `renderer/src/gfn/`) answers SDP, negotiates ICE, and plays the stream.

## Error Handling Pattern

- GFN API errors are normalized to `SessionError` in `src/main/gfn/errorCodes.ts`.
- IPC handlers call `rethrowSerializedSessionError(error)` which serializes `SessionError` into the
  error message string before it crosses the IPC boundary (Electron only forwards `Error.message`).
- Preload `invokeSessionChannel` calls `parseSerializedSessionErrorTransport` to reconstruct the
  typed `SessionError` on the renderer side.

## Settings

Settings are stored as a flat JSON object in `userData/settings.json`.
The `SettingsManager` class in `main/settings.ts` handles typed get/set/reset.
The `Settings` type in `shared/gfn.ts` defines all setting keys.
Some settings (e.g., `decoderPreference`, `encoderPreference`) are read at app start before
Chromium command-line flags are set (`loadBootstrapVideoPreferences()` in main/index.ts).

## Import Aliases

The `@shared/*` alias resolves to `src/shared/` and is configured in:
- `electron.vite.config.ts` (for all three build targets)
- `tsconfig.json` (renderer)
- `tsconfig.node.json` (main + preload)

## Build & Dev Commands

From the repository root (or from `opennow-stable/`):

```bash
npm run dev          # Start Electron in development (hot-reload)
npm run typecheck    # TypeScript type-check only (no emit)
npm run build        # Compile/bundle for production
npm run dist         # Build + package installers (unsigned)
npm run dist:signed  # Build + package with code signing
```

There is currently **no test runner or linter** configured. PR validation relies on `typecheck` and `build` passing.

## CI / Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| `auto-build.yml` | PR, push to `main`/`dev` | Compile + package all platforms, upload artifacts |
| `release.yml` | Push `v*.*.*` tag or `workflow_dispatch` | Full release: typecheck → build → package → GitHub Release → version bump commit |
| `codeql.yml` | Push to `main`/`dev`, weekly schedule | CodeQL static security analysis |

### Release Pipeline Details

The `release.yml` workflow:
1. **preflight** — validates semver, typechecks, resolves `version`, `prerelease`, `release_tag`, `make_latest`.
2. **build** — cross-platform matrix (Windows NSIS+portable, macOS dmg+zip x64+arm64, Linux AppImage+deb x64+arm64).
3. **release** — downloads all artifacts and publishes a GitHub Release via `softprops/action-gh-release@v2`.
4. **finalize** — commits the version bump back to the release branch (`dev` by default).

Custom runners used: `blacksmith-4vcpu-*` (Windows, Linux), `macos-latest` (macOS).

## Security Model

- `contextIsolation: true`, `nodeIntegration: false` — renderer has no direct Node.js access.
- `sandbox: false` — required so the preload script can use Electron APIs; kept minimal.
- `setWindowOpenHandler` is configured to deny all popup window creation; external `http(s)` links
  are forwarded to the system browser via `shell.openExternal`.
- `will-navigate` event guard prevents the renderer from navigating away from the bundled app.
- `WebRtcHideLocalIpsWithMdns` is disabled intentionally for lower-latency ICE connectivity.
- Auth tokens are stored in plain JSON on disk (`userData/auth-state.json`) — industry-standard for
  desktop OAuth clients; no additional encryption is applied.
- File access in IPC handlers (`MEDIA_THUMBNAIL`, `MEDIA_SHOW_IN_FOLDER`) uses `realpath` +
  `relative()` path confinement to prevent directory traversal.
- Screenshot and recording IDs are validated to contain no path separators or `..` sequences.

## Conventions and Style

- Prefer `async/await` and typed fetch wrappers. Avoid raw callbacks.
- All new IPC channels follow the naming convention `namespace:verb` (e.g., `recording:begin`).
- Keep the shared contract (`gfn.ts`, `ipc.ts`) as the source of truth; don't add ad-hoc IPC.
- Do not log tokens or full response bodies in production code paths.
- Use `@shared/*` alias for cross-boundary imports; do not use relative paths like `../../shared`.
- The `OpenNowApi` interface in `shared/gfn.ts` is the single source of truth for what the renderer
  can call. Add new methods there first, then implement in preload + main.
