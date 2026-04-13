import { describe, expect, it } from "vitest";
import type { SessionErrorInfo } from "./sessionError";
import {
  SESSION_ERROR_TRANSPORT_KIND,
  isSerializedSessionError,
  parseSerializedSessionErrorTransport,
  serializeSessionErrorTransport,
  toSerializedSessionError,
} from "./sessionError";

const sampleInfo: SessionErrorInfo = {
  httpStatus: 503,
  statusCode: 3,
  statusDescription: "timeout",
  unifiedErrorCode: undefined,
  sessionErrorCode: undefined,
  gfnErrorCode: 3237093635,
  title: "Server timeout",
  description: "The session server did not respond in time.",
};

describe("sessionError transport (local)", () => {
  it("toSerializedSessionError adds transport shape", () => {
    const s = toSerializedSessionError(sampleInfo);
    expect(s.kind).toBe(SESSION_ERROR_TRANSPORT_KIND);
    expect(s.name).toBe("SessionError");
    expect(s.message).toBe(sampleInfo.description);
    expect(s.title).toBe(sampleInfo.title);
  });

  it("serialize + parse round-trips when marker is embedded with only a prefix before it", () => {
    const wire = serializeSessionErrorTransport(sampleInfo);
    const wrapped = `upstream said: ${wire}`;
    const parsed = parseSerializedSessionErrorTransport(wrapped);
    expect(parsed).not.toBeNull();
    expect(parsed?.gfnErrorCode).toBe(sampleInfo.gfnErrorCode);
    expect(parsed?.description).toBe(sampleInfo.description);
  });

  it("parseSerializedSessionErrorTransport returns null without marker", () => {
    expect(parseSerializedSessionErrorTransport("plain text")).toBeNull();
  });

  it("parseSerializedSessionErrorTransport returns null for invalid JSON payload", () => {
    const prefix = "__OPENNOW_SESSION_ERROR__:";
    expect(parseSerializedSessionErrorTransport(`${prefix}{not json`)).toBeNull();
  });

  it("isSerializedSessionError rejects incomplete objects", () => {
    expect(isSerializedSessionError(null)).toBe(false);
    expect(isSerializedSessionError({ kind: SESSION_ERROR_TRANSPORT_KIND })).toBe(false);
  });
});
