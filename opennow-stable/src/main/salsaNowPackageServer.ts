import { randomBytes } from "node:crypto";
import { createReadStream, statSync } from "node:fs";
import http from "node:http";
import os from "node:os";
import { basename } from "node:path";

/**
 * HTTP path prefix before the secret token. Kept short and generic so the URL does not
 * advertise what is being served (logs, history, screen shares).
 */
const LOCAL_FILE_SERVE_PREFIX = "/x/";

export type SalsaNowServeStartResult =
  | {
      ok: true;
      port: number;
      token: string;
      fileName: string;
      localUrl: string;
      lanUrls: string[];
    }
  | { ok: false; error: string };

let server: http.Server | null = null;

export function isPackageServerActive(): boolean {
  return server !== null;
}

export function stopPackageServer(): void {
  if (server) {
    server.close();
    server = null;
  }
}

function listLanIPv4(): string[] {
  const out: string[] = [];
  const ifaces = os.networkInterfaces();
  for (const name of Object.keys(ifaces)) {
    for (const addr of ifaces[name] ?? []) {
      if (addr.family === "IPv4" && !addr.internal) {
        out.push(addr.address);
      }
    }
  }
  return out;
}

/**
 * Serves a single file over HTTP on 0.0.0.0 (random port). Path includes an unguessable token.
 * Intended for short-lived sharing (e.g. copy URL into another machine on the same LAN).
 */
export function startPackageServer(absoluteFilePath: string): Promise<SalsaNowServeStartResult> {
  stopPackageServer();

  let st: ReturnType<typeof statSync>;
  try {
    st = statSync(absoluteFilePath);
  } catch {
    return Promise.resolve({ ok: false, error: "Could not read file." });
  }
  if (!st.isFile()) {
    return Promise.resolve({ ok: false, error: "Path is not a file." });
  }

  const token = randomBytes(18).toString("hex");
  const fileName = basename(absoluteFilePath);
  const safeName = fileName.replace(/"/g, "");

  return new Promise((resolve) => {
    const handler = (req: http.IncomingMessage, res: http.ServerResponse): void => {
      if (req.method === "OPTIONS") {
        res.writeHead(204, {
          "Access-Control-Allow-Origin": "*",
          "Access-Control-Allow-Methods": "GET, HEAD, OPTIONS",
        });
        res.end();
        return;
      }
      if (req.method !== "GET" && req.method !== "HEAD") {
        res.statusCode = 405;
        res.end();
        return;
      }
      const urlPath = (req.url ?? "").split("?")[0] ?? "";
      const expectedPath = `${LOCAL_FILE_SERVE_PREFIX}${token}`;
      if (urlPath !== expectedPath) {
        res.statusCode = 404;
        res.end();
        return;
      }

      res.writeHead(200, {
        "Content-Type": "application/octet-stream",
        "Content-Disposition": `attachment; filename="${safeName}"`,
        "Content-Length": st.size,
        "Access-Control-Allow-Origin": "*",
      });

      if (req.method === "HEAD") {
        res.end();
        return;
      }

      const stream = createReadStream(absoluteFilePath);
      stream.on("error", () => {
        if (!res.writableEnded) {
          res.destroy();
        }
      });
      stream.pipe(res);
    };

    const srv = http.createServer(handler);
    srv.once("error", (err: NodeJS.ErrnoException) => {
      if (server === srv) {
        server = null;
      }
      resolve({ ok: false, error: err.message || "Failed to bind HTTP server." });
    });
    srv.listen(0, "0.0.0.0", () => {
      const addr = srv.address();
      if (!addr || typeof addr === "string") {
        srv.close();
        resolve({ ok: false, error: "Could not allocate a listening port." });
        return;
      }
      server = srv;
      const port = addr.port;
      const path = `${LOCAL_FILE_SERVE_PREFIX}${token}`;
      const localUrl = `http://127.0.0.1:${port}${path}`;
      const lanUrls = listLanIPv4().map((ip) => `http://${ip}:${port}${path}`);
      resolve({ ok: true, port, token, fileName, localUrl, lanUrls });
    });
  });
}
