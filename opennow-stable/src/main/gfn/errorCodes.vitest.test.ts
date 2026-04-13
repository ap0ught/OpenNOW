import { describe, expect, it } from "vitest";
import { GfnErrorCode, SessionError, isSessionError, parseCloudMatchError } from "./errorCodes";

describe("parseCloudMatchError (fixture JSON only, no network)", () => {
  it("parses statusCode from CloudMatch-shaped JSON", () => {
    const body = JSON.stringify({
      requestStatus: { statusCode: 14, statusDescription: "auth" },
    });
    const err = parseCloudMatchError(403, body);
    expect(err.gfnErrorCode).toBe(GfnErrorCode.AuthFailure);
    expect(err.httpStatus).toBe(403);
    expect(err.statusCode).toBe(14);
    expect(err.title.length).toBeGreaterThan(0);
  });

  it("handles non-JSON body without throwing", () => {
    const err = parseCloudMatchError(500, "not json");
    expect(err).toBeInstanceOf(SessionError);
    expect(err.httpStatus).toBe(500);
  });

  it("isSessionError narrows SessionError instances", () => {
    const err = parseCloudMatchError(400, "{}");
    expect(isSessionError(err)).toBe(true);
    expect(isSessionError(new Error("x"))).toBe(false);
  });
});
