# SalsaNOW integration

This repository includes **[SalsaNOW](https://github.com/ap0ught/SalsaNOW)** as a **git submodule** at [`external/SalsaNOW`](../external/SalsaNOW). OpenNOW does not embed its .NET runtime; you build or obtain `SalsaNOW.exe` separately and optionally point OpenNOW at it.

## What SalsaNOW is

SalsaNOW is a **Windows-only** companion tool (C#, .NET Framework 4.8) described upstream as tooling that **prepares GeForce NOW session environments for local Steam-style workflows** (shortcuts, installs, background tasks, optional NVIDIA RTX toggles via NvAPI, etc.). It expects a **GeForce NOW–style environment** (upstream checks for `C:\Asgard`) and may **download configuration** from third-party hosts configured in that project.

OpenNOW remains an **independent** Electron client; SalsaNOW is **optional**, **not required** to browse or stream with OpenNOW, and **not affiliated with NVIDIA** beyond whatever you already do when using GeForce NOW.

## Security and trust

Review the **upstream source** before running `SalsaNOW.exe`. That codebase performs remote downloads and custom TLS validation; treat it like any other powerful system utility. Only enable the OpenNOW launcher after you understand and accept upstream behavior.

## Submodule checkout

After cloning OpenNOW:

```bash
git submodule update --init --recursive
```

The submodule tracks `https://github.com/ap0ught/SalsaNOW.git`.

## Building SalsaNOW (Windows)

1. Open `external/SalsaNOW/SalsaNOW.sln` in **Visual Studio** (workload: .NET desktop development, .NET Framework 4.8 targeting pack).
2. Restore NuGet packages and build **Release | x64** (per upstream `.csproj` output).
3. Copy `SalsaNOW.exe` from `external/SalsaNOW/SalsaNOW/bin/Release/` (or your output path) to a stable location.

## OpenNOW UI and IPC

- **Settings** (Windows only): set **SalsaNOW companion** to the absolute path of `SalsaNOW.exe`, then use **Launch SalsaNOW**.
- The main process reads **`salsaNowExePath`** from saved settings only; the renderer cannot pass an arbitrary path to the launcher IPC.
- IPC channel: `companion:salsa-now-launch` (see [`opennow-stable/src/shared/ipc.ts`](../opennow-stable/src/shared/ipc.ts)).

### Sharing the install package over HTTP (copy link)

OpenNOW can start a **short-lived HTTP server** on your machine that serves **one file** (your built `SalsaNOW.exe`, a zip, etc.) at a URL containing a random token:

- **Serve configured path** — uses `salsaNowExePath` from settings.
- **Pick file to serve…** — opens a file dialog (`.exe`, `.zip`, `.msi`, or any file).

The UI lists:

- **`http://127.0.0.1:<port>/x/<token>`** — only works **on the same computer** running OpenNOW. (`/x/` is a generic path prefix before the unguessable token.)
- **`http://<LAN-IP>:<port>/x/<token>`** — one line per local IPv4; usable from **another device on the same LAN** (same Wi‑Fi/Ethernet, routing/firewall allowing it).

**GeForce NOW cloud session note:** the Windows VM where your game runs is **not** on your home LAN. Those LAN URLs usually **do not** work inside the GFN browser unless you add something that bridges networks (e.g. **Tailscale** on both sides, **ngrok** / similar tunnel to your PC, or hosting the file on **HTTPS** you control). Treat HTTP sharing as best for **lab / same-network** transfer; plan an explicit tunnel or upload if the target is strictly remote.

IPC: `companion:salsa-now-start-package-server`, `companion:salsa-now-stop-package-server`. The server is stopped when you click **Stop sharing** or when OpenNOW quits. The HTTP path uses a short neutral prefix plus token (implementation: `LOCAL_FILE_SERVE_PREFIX` in `salsaNowPackageServer.ts`).

## Relationship to OpenNOW releases

Electron builds in CI **do not** compile the C# submodule. Release artifacts stay the OpenNOW app; SalsaNOW remains a **separate binary** you maintain or ship alongside if desired.
