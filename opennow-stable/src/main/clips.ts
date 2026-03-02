import { app } from "electron";
import { randomUUID } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";
import { pathToFileURL } from "node:url";

import type {
  ClipRecord,
  ClipRecordInput,
  CaptureAssetSaveRequest,
  CaptureAssetSaveResult,
  CaptureAssetReadResult,
} from "@shared/gfn";

interface ClipStoreFile {
  clips: ClipRecord[];
}

const EMPTY_STORE: ClipStoreFile = { clips: [] };

export class ClipStore {
  private readonly filePath: string;
  private clips: ClipRecord[] = [];

  constructor() {
    this.filePath = join(app.getPath("userData"), "clips.json");
    this.clips = this.load();
  }

  private load(): ClipRecord[] {
    try {
      if (!existsSync(this.filePath)) {
        return [];
      }
      const parsed = JSON.parse(readFileSync(this.filePath, "utf-8")) as Partial<ClipStoreFile>;
      if (!parsed || !Array.isArray(parsed.clips)) {
        return [];
      }
      return parsed.clips;
    } catch (error) {
      console.error("[ClipStore] Failed to load clips:", error);
      return [];
    }
  }

  private persist(): void {
    try {
      const dir = app.getPath("userData");
      if (!existsSync(dir)) {
        mkdirSync(dir, { recursive: true });
      }
      writeFileSync(this.filePath, JSON.stringify({ clips: this.clips }, null, 2), "utf-8");
    } catch (error) {
      console.error("[ClipStore] Failed to save clips:", error);
    }
  }

  public list(): ClipRecord[] {
    return [...this.clips].sort((a, b) => b.timestampMs - a.timestampMs);
  }

  public save(input: ClipRecordInput): ClipRecord {
    const clip: ClipRecord = {
      id: randomUUID(),
      clipType: input.clipType,
      status: input.status,
      timestampMs: input.timestampMs,
      gameTitle: input.gameTitle,
      gameBannerUrl: input.gameBannerUrl,
      machineLabel: input.machineLabel,
      codec: input.codec,
      filePath: input.filePath,
      fileUrl: input.fileUrl,
      durationSeconds: input.durationSeconds,
      source: input.source ?? "server",
    };
    this.clips.unshift(clip);
    this.persist();
    return clip;
  }

  public saveAsset(input: CaptureAssetSaveRequest): CaptureAssetSaveResult {
    const safeTitle = (input.gameTitle || "capture")
      .replace(/[^\w.-]+/g, "_")
      .slice(0, 48);
    const stamp = new Date(input.timestampMs || Date.now())
      .toISOString()
      .replace(/[:.]/g, "-");
    const extension = input.extension === "png" ? "png" : "webm";
    const captureDir = join(app.getPath("videos"), "OpenNOW", "Captures");
    if (!existsSync(captureDir)) {
      mkdirSync(captureDir, { recursive: true });
    }
    const fileName = `${safeTitle}_${input.clipType}_${stamp}.${extension}`;
    const filePath = join(captureDir, fileName);
    writeFileSync(filePath, Buffer.from(input.bytes));
    return {
      filePath,
      fileUrl: pathToFileURL(filePath).toString(),
    };
  }

  public readAsset(filePath: string): CaptureAssetReadResult {
    const bytes = new Uint8Array(readFileSync(filePath));
    const ext = extname(filePath).toLowerCase();
    const mimeType =
      ext === ".png" ? "image/png"
      : ext === ".webm" ? "video/webm"
      : ext === ".mp4" ? "video/mp4"
      : "application/octet-stream";
    return { bytes, mimeType };
  }

  public resetForTests(): void {
    this.clips = [...EMPTY_STORE.clips];
    this.persist();
  }
}
