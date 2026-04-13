import { describe, expect, it } from "vitest";
import { LogCapture, createRedactedLogExport, formatLogEntry, redactSensitiveData } from "./logger";
import type { LogEntry } from "./logger";

describe("redactSensitiveData", () => {
  it("redacts email-like substrings", () => {
    expect(redactSensitiveData("contact user@example.com please")).toContain("[Redacted for privacy]");
    expect(redactSensitiveData("contact user@example.com please")).not.toContain("user@example.com");
  });

  it("redacts dotted IPv4 literals", () => {
    expect(redactSensitiveData("peer 10.0.0.1")).toBe("peer [Redacted IP]");
  });
});

describe("formatLogEntry", () => {
  it("formats timestamp, level, prefix, and message", () => {
    const entry: LogEntry = {
      timestamp: Date.UTC(2026, 0, 2, 12, 0, 0),
      level: "warn",
      prefix: "Test",
      message: "hello",
      args: [],
    };
    const line = formatLogEntry(entry);
    expect(line).toContain("WARN");
    expect(line).toContain("[Test]");
    expect(line).toContain("hello");
  });
});

describe("createRedactedLogExport", () => {
  it("joins redacted lines", () => {
    const entries: LogEntry[] = [
      {
        timestamp: 0,
        level: "log",
        prefix: "",
        message: "user@evil.test logged in",
        args: [],
      },
    ];
    const out = createRedactedLogExport(entries);
    expect(out.split("\n")).toHaveLength(1);
    expect(out).not.toContain("user@evil.test");
  });
});

describe("LogCapture", () => {
  it("drops oldest entries after MAX_LOG_ENTRIES", () => {
    const cap = new LogCapture("unit");
    for (let i = 0; i < 5002; i++) {
      cap.addEntry("log", "p", `m${i}`, []);
    }
    expect(cap.getCount()).toBe(5000);
    expect(cap.getEntries()[0]?.message).toBe("m2");
  });
});
