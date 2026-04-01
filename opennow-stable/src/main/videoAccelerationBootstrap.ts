import { execSync } from "node:child_process";
import type { VideoAccelerationPreference } from "@shared/gfn";

/**
 * On Windows, Chromium's D3D11 + Media Foundation WebRTC decode path is unreliable on
 * some Intel-only (iGPU) systems: streams can stay black with 0 Mbps while ICE is fine.
 * When we detect no discrete NVIDIA/AMD GPU, force software decode/encode so WebRTC uses
 * the CPU path (OpenH264 / dav1d) instead.
 *
 * Returns null if GPU list could not be read (keep "auto").
 */
function getWindowsVideoControllerNames(): string[] | null {
  try {
    const out = execSync("wmic path win32_VideoController get Name /format:list", {
      encoding: "utf8",
      timeout: 5000,
      windowsHide: true,
      maxBuffer: 512 * 1024,
    });
    const names = out
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.startsWith("Name="))
      .map((line) => line.slice("Name=".length).trim())
      .filter(Boolean);
    if (names.length > 0) {
      return names;
    }
  } catch {
    // WMIC may be missing or fail on some installs
  }

  try {
    const out = execSync(
      "powershell -NoProfile -NonInteractive -Command \"Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name\"",
      {
        encoding: "utf8",
        timeout: 8000,
        windowsHide: true,
        maxBuffer: 512 * 1024,
      },
    );
    const names = out
      .split(/\r?\n/)
      .map((s) => s.trim())
      .filter(Boolean);
    return names.length > 0 ? names : null;
  } catch {
    return null;
  }
}

/**
 * True when the machine appears to use only Intel graphics (typical iGPU-only laptops).
 * False when any NVIDIA or AMD discrete adapter is present.
 */
function isWindowsIntelOnlySystem(): boolean | null {
  const names = getWindowsVideoControllerNames();
  if (names === null) {
    return null;
  }
  if (names.length === 0) {
    return null;
  }

  const hasDiscreteNvidiaOrAmd = names.some((n) => {
    const lower = n.toLowerCase();
    return lower.includes("nvidia") || /\bamd\b/.test(lower) || lower.includes("radeon");
  });
  if (hasDiscreteNvidiaOrAmd) {
    return false;
  }

  const hasIntel = names.some((n) => /\bintel\b/i.test(n));
  if (hasIntel) {
    return true;
  }

  return null;
}

export interface BootstrapVideoPrefs {
  decoderPreference: VideoAccelerationPreference;
  encoderPreference: VideoAccelerationPreference;
}

/**
 * Chromium command-line video prefs applied before app.ready.
 * Intel-only Windows: prefer software paths to avoid broken HW WebRTC decode on some iGPUs.
 */
export function getBootstrapVideoPrefs(): BootstrapVideoPrefs {
  if (process.platform !== "win32") {
    return { decoderPreference: "auto", encoderPreference: "auto" };
  }

  const intelOnly = isWindowsIntelOnlySystem();
  if (intelOnly === true) {
    console.log(
      "[Main] Windows: Intel-only GPU — using software video decode/encode (WebRTC iGPU workaround)",
    );
    return { decoderPreference: "software", encoderPreference: "software" };
  }
  if (intelOnly === null) {
    console.log("[Main] Windows: GPU list unavailable; video acceleration remains auto");
  }
  return { decoderPreference: "auto", encoderPreference: "auto" };
}
