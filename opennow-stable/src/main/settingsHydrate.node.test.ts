import assert from "node:assert/strict";
import { describe, it } from "node:test";
import type { Settings } from "@shared/gfn";
import { DEFAULT_SETTINGS } from "./settingsDefaults";
import { applyStreamCompatibility, freshDefaultSettings, hydrateSettingsFromPartial } from "./settingsHydrate";

describe("hydrateSettingsFromPartial (local-only, no NVIDIA)", () => {
  it("fills missing keys from defaults", () => {
    const { settings, migrated } = hydrateSettingsFromPartial({ resolution: "1280x720" });
    assert.equal(settings.resolution, "1280x720");
    assert.equal(settings.fps, DEFAULT_SETTINGS.fps);
    assert.equal(migrated, false);
  });

  it("migrates legacy stop-stream shortcut", () => {
    const { settings, migrated } = hydrateSettingsFromPartial({
      shortcutStopStream: "Meta+Shift+Q",
    } as Partial<Settings>);
    assert.equal(settings.shortcutStopStream, "Ctrl+Shift+Q");
    assert.equal(migrated, true);
  });

  it("migrates boolean mouseAcceleration to percentage", () => {
    const { settings, migrated } = hydrateSettingsFromPartial({
      mouseAcceleration: true,
    } as unknown as Partial<Settings>);
    assert.equal(settings.mouseAcceleration, 100);
    assert.equal(migrated, true);
  });

  it("clamps mouseAcceleration to 1–150", () => {
    const { settings } = hydrateSettingsFromPartial({ mouseAcceleration: 999 });
    assert.equal(settings.mouseAcceleration, 150);
  });
});

describe("freshDefaultSettings", () => {
  it("returns a full settings object", () => {
    const s = freshDefaultSettings();
    assert.equal(typeof s.resolution, "string");
    assert.ok(Array.isArray(s.favoriteGameIds));
  });
});

describe("applyStreamCompatibility", () => {
  it("normalizes invalid persisted codec/color strings", () => {
    const s: Settings = {
      ...DEFAULT_SETTINGS,
      codec: "bogus" as Settings["codec"],
      colorQuality: "bogus" as Settings["colorQuality"],
    };
    const migrated = applyStreamCompatibility(s);
    assert.equal(migrated, true);
    assert.equal(s.codec, "H264");
    assert.equal(s.colorQuality, "8bit_420");
  });
});
