import type { Settings } from "@shared/gfn";
import { normalizeStreamPreferences } from "@shared/gfn";
import {
  DEFAULT_SETTINGS,
  LEGACY_ANTI_AFK_SHORTCUTS,
  LEGACY_STOP_SHORTCUTS,
  defaultAntiAfkShortcut,
  defaultStopShortcut,
} from "./settingsDefaults";

function migrateLegacyShortcuts(settings: Settings): boolean {
  let migrated = false;
  const normalizeShortcut = (value: string): string => value.replace(/\s+/g, "").toUpperCase();
  const stopShortcut = normalizeShortcut(settings.shortcutStopStream);
  const antiAfkShortcut = normalizeShortcut(settings.shortcutToggleAntiAfk);

  if (LEGACY_STOP_SHORTCUTS.has(stopShortcut)) {
    settings.shortcutStopStream = defaultStopShortcut;
    migrated = true;
  }

  if (LEGACY_ANTI_AFK_SHORTCUTS.has(antiAfkShortcut)) {
    settings.shortcutToggleAntiAfk = defaultAntiAfkShortcut;
    migrated = true;
  }

  return migrated;
}

/** Mutates `settings` when codec/colorQuality pair is unsupported. */
export function applyStreamCompatibility(settings: Settings): boolean {
  const normalized = normalizeStreamPreferences(settings.codec, settings.colorQuality);
  if (!normalized.migrated) {
    return false;
  }

  console.warn(
    `[Settings] Migrating unsupported stream settings codec="${settings.codec}" colorQuality="${settings.colorQuality}" to ${normalized.codec}/${normalized.colorQuality}`,
  );
  settings.codec = normalized.codec;
  settings.colorQuality = normalized.colorQuality;
  return true;
}

/**
 * Merge persisted JSON with defaults and run migrations (no disk, no Electron).
 * Safe for unit tests — does not contact NVIDIA or any network service.
 */
export function hydrateSettingsFromPartial(parsed: Partial<Settings>): { settings: Settings; migrated: boolean } {
  const merged: Settings = {
    ...DEFAULT_SETTINGS,
    ...parsed,
  };

  let migrated = migrateLegacyShortcuts(merged);
  migrated = applyStreamCompatibility(merged) || migrated;

  if (typeof (parsed as { mouseAcceleration?: unknown }).mouseAcceleration === "boolean") {
    merged.mouseAcceleration = (parsed as { mouseAcceleration?: boolean }).mouseAcceleration ? 100 : 1;
    migrated = true;
  }

  merged.mouseAcceleration = Math.max(1, Math.min(150, Math.round(merged.mouseAcceleration)));

  return { settings: merged, migrated };
}

/** Fresh defaults with stream compatibility applied (matches previous load() empty-file path). */
export function freshDefaultSettings(): Settings {
  const defaults: Settings = { ...DEFAULT_SETTINGS };
  applyStreamCompatibility(defaults);
  return defaults;
}
