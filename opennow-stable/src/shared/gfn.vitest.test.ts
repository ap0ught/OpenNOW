import { describe, expect, it } from "vitest";
import {
  colorQualityBitDepth,
  colorQualityChromaFormat,
  colorQualityIs10Bit,
  colorQualityRequiresHevc,
  getDefaultStreamPreferences,
  isSupportedUserFacingCodec,
  keyboardLayoutOptions,
  normalizeStreamPreferences,
  resolveGfnKeyboardLayout,
} from "./gfn";

/** Pure shared helpers — no network, no NVIDIA services. */
describe("gfn helpers", () => {
  it("normalizeStreamPreferences coerces invalid enum-like values", () => {
    const r = normalizeStreamPreferences("not-a-codec" as "H264", "not-a-quality" as "8bit_420");
    expect(r.codec).toBe("H264");
    expect(r.colorQuality).toBe("8bit_420");
    expect(r.migrated).toBe(true);
  });

  it("resolveGfnKeyboardLayout maps mac layout when requested", () => {
    expect(resolveGfnKeyboardLayout("en-US", "darwin")).toBe("m-us");
    expect(resolveGfnKeyboardLayout("en-US", "win32")).toBe("en-US");
  });

  it("colorQualityBitDepth and chromaFormat", () => {
    expect(colorQualityBitDepth("8bit_420")).toBe(0);
    expect(colorQualityBitDepth("10bit_420")).toBe(10);
    expect(colorQualityChromaFormat("8bit_444")).toBe(2);
    expect(colorQualityChromaFormat("8bit_420")).toBe(0);
  });

  it("colorQualityRequiresHevc and colorQualityIs10Bit", () => {
    expect(colorQualityRequiresHevc("8bit_420")).toBe(false);
    expect(colorQualityRequiresHevc("10bit_420")).toBe(true);
    expect(colorQualityIs10Bit("10bit_444")).toBe(true);
    expect(colorQualityIs10Bit("8bit_444")).toBe(false);
  });

  it("isSupportedUserFacingCodec accepts shipped codecs only", () => {
    expect(isSupportedUserFacingCodec("H264")).toBe(true);
    expect(isSupportedUserFacingCodec("bogus" as "H264")).toBe(false);
  });

  it("getDefaultStreamPreferences returns normalized defaults", () => {
    const p = getDefaultStreamPreferences();
    expect(p.codec).toMatch(/^(H264|H265|AV1)$/);
    expect(p.colorQuality).toMatch(/^(8bit|10bit)_(420|444)$/);
  });

  it("keyboardLayoutOptions covers common layouts", () => {
    expect(keyboardLayoutOptions.some((o) => o.value === "en-US")).toBe(true);
    expect(keyboardLayoutOptions.length).toBeGreaterThanOrEqual(10);
  });
});
