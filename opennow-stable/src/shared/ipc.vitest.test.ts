import { describe, expect, it } from "vitest";
import { IPC_CHANNELS } from "./ipc";

describe("IPC_CHANNELS", () => {
  it("uses unique string values for every channel", () => {
    const values = Object.values(IPC_CHANNELS);
    expect(new Set(values).size).toBe(values.length);
  });

  it("keeps settings-related channels stable", () => {
    expect(IPC_CHANNELS.SETTINGS_GET).toBe("settings:get");
    expect(IPC_CHANNELS.SETTINGS_SET).toBe("settings:set");
    expect(IPC_CHANNELS.SETTINGS_RESET).toBe("settings:reset");
  });
});
