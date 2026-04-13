import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
  extractIceCredentials,
  extractIceUfragFromOffer,
  extractPublicIp,
  fixServerIp,
} from "./sdp";

beforeEach(() => {
  vi.spyOn(console, "log").mockImplementation(() => {});
});

afterEach(() => {
  vi.restoreAllMocks();
});

describe("extractPublicIp", () => {
  it("returns null for empty input", () => {
    expect(extractPublicIp("")).toBeNull();
  });

  it("returns dotted IPv4 unchanged", () => {
    expect(extractPublicIp("80.250.97.40")).toBe("80.250.97.40");
  });

  it("parses dash-first-label NVIDIA-style hostnames", () => {
    const host = "80-250-97-40.cloudmatchbeta.nvidiagrid.net";
    expect(extractPublicIp(host)).toBe("80.250.97.40");
  });

  it("returns null when first label is not four numeric octets", () => {
    expect(extractPublicIp("example.com")).toBeNull();
  });
});

describe("fixServerIp", () => {
  it("replaces c= and candidate 0.0.0.0 when IP can be derived", () => {
    const sdp = [
      "v=0",
      "m=video 9 UDP/TLS/RTP/SAVPF 96",
      "c=IN IP4 0.0.0.0",
      "a=candidate:1 1 UDP 2130706431 0.0.0.0 9 typ host",
      "",
    ].join("\r\n");
    const fixed = fixServerIp(sdp, "80-250-97-40.example.net");
    expect(fixed).toContain("c=IN IP4 80.250.97.40");
    expect(fixed).toContain(" 80.250.97.40 ");
    expect(fixed).not.toMatch(/c=IN IP4 0\.0\.0\.0/);
  });

  it("returns original SDP when server host yields no IP", () => {
    const sdp = "c=IN IP4 0.0.0.0\r\n";
    expect(fixServerIp(sdp, "no-ip-here.invalid")).toBe(sdp);
  });
});

describe("ICE helpers", () => {
  const sample = [
    "a=ice-ufrag:abc123",
    "a=ice-pwd:secretpwd",
    "a=fingerprint:sha-256 AA:BB:CC",
  ].join("\r\n");

  it("extractIceUfragFromOffer reads a=ice-ufrag", () => {
    expect(extractIceUfragFromOffer(sample)).toBe("abc123");
  });

  it("extractIceCredentials returns ufrag, pwd, fingerprint", () => {
    const c = extractIceCredentials(sample);
    expect(c.ufrag).toBe("abc123");
    expect(c.pwd).toBe("secretpwd");
    expect(c.fingerprint).toBe("AA:BB:CC");
  });
});
