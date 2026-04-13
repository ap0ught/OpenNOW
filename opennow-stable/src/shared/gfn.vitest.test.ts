import { describe, expect, it } from "vitest";
import {
  colorQualityBitDepth,
  colorQualityChromaFormat,
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
});
