import { randomBytes } from "node:crypto";

import WebSocket from "ws";

import type {
  IceCandidatePayload,
  MainToRendererSignalingEvent,
  SendAnswerRequest,
} from "@shared/gfn";

const USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/131.0.0.0 Safari/537.36";

interface SignalingMessage {
  ackid?: number;
  ack?: number;
  hb?: number;
  peer_info?: {
    id: number;
  };
  peer_msg?: {
    from: number;
    to: number;
    msg: string;
  };
}

export class GfnSignalingClient {
  private ws: WebSocket | null = null;
  private peerId = 2;
  private peerName = `peer-${Math.floor(Math.random() * 10_000_000_000)}`;
  private ackCounter = 0;
  private maxReceivedAckId = 0;
  private heartbeatTimer: NodeJS.Timeout | null = null;
  private reconnectTimer: NodeJS.Timeout | null = null;
  private reconnectAttempts = 0;
  private manualDisconnect = false;
  private listeners = new Set<(event: MainToRendererSignalingEvent) => void>();
  private static readonly MAX_RECONNECT_ATTEMPTS = 6;
  private static readonly RECONNECT_BASE_DELAY_MS = 750;

  constructor(
    private readonly signalingServer: string,
    private readonly sessionId: string,
    private readonly signalingUrl?: string,
  ) {}

  private buildSignInUrl(reconnect = false): string {
    // Match Rust behavior: extract host:port from signalingUrl if available,
    // since the signalingUrl contains the real server address (which may differ
    // from signalingServer when the resource path was an rtsps:// URL)
    let serverWithPort: string;

    if (this.signalingUrl) {
      // Extract host:port from wss://host:port/path
      const withoutScheme = this.signalingUrl.replace(/^wss?:\/\//, "");
      const hostPort = withoutScheme.split("/")[0];
      serverWithPort = hostPort && hostPort.length > 0
        ? (hostPort.includes(":") ? hostPort : `${hostPort}:443`)
        : (this.signalingServer.includes(":") ? this.signalingServer : `${this.signalingServer}:443`);
    } else {
      serverWithPort = this.signalingServer.includes(":")
        ? this.signalingServer
        : `${this.signalingServer}:443`;
    }

    const url = `wss://${serverWithPort}/nvst/sign_in?peer_id=${this.peerName}&version=2&peer_role=1${reconnect ? "&reconnect=1" : ""}`;
    console.log("[Signaling] URL:", url, "(server:", this.signalingServer, ", signalingUrl:", this.signalingUrl, ")");
    return url;
  }

  onEvent(listener: (event: MainToRendererSignalingEvent) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private emit(event: MainToRendererSignalingEvent): void {
    for (const listener of this.listeners) {
      listener(event);
    }
  }

  private nextAckId(): number {
    this.ackCounter += 1;
    return this.ackCounter;
  }

  private sendJson(payload: unknown): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return;
    }
    this.ws.send(JSON.stringify(payload));
  }

  private setupHeartbeat(): void {
    this.clearHeartbeat();
    // Official client does not proactively send signaling hb packets.
  }

  private clearHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  private scheduleReconnect(closeReason: string): void {
    if (this.manualDisconnect) {
      return;
    }
    if (this.reconnectAttempts >= GfnSignalingClient.MAX_RECONNECT_ATTEMPTS) {
      this.emit({ type: "disconnected", reason: `${closeReason} (reconnect exhausted)` });
      return;
    }
    if (this.reconnectTimer) {
      return;
    }

    this.reconnectAttempts += 1;
    const attempt = this.reconnectAttempts;
    const delayMs = Math.min(
      5000,
      GfnSignalingClient.RECONNECT_BASE_DELAY_MS * Math.pow(2, attempt - 1),
    );
    this.emit({
      type: "log",
      message: `Signaling reconnect attempt ${attempt}/${GfnSignalingClient.MAX_RECONNECT_ATTEMPTS} in ${delayMs}ms`,
    });

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      void this.connectSocket(true).catch((error) => {
        const errorMsg = `Signaling reconnect failed: ${String(error)}`;
        console.error("[Signaling]", errorMsg);
        this.emit({ type: "error", message: errorMsg });
        this.scheduleReconnect(errorMsg);
      });
    }, delayMs);
  }

  private sendPeerInfo(): void {
    this.sendJson({
      ackid: this.nextAckId(),
      peer_info: {
        browser: "Chrome",
        browserVersion: "131",
        connected: true,
        id: this.peerId,
        name: this.peerName,
        peerRole: 0,
        resolution: "1920x1080",
        version: 2,
      },
    });
  }

  async connect(): Promise<void> {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return;
    }
    this.manualDisconnect = false;
    this.clearReconnectTimer();
    this.reconnectAttempts = 0;
    this.maxReceivedAckId = 0;
    await this.connectSocket(false);
  }

  private async connectSocket(reconnect: boolean): Promise<void> {
    const url = this.buildSignInUrl(reconnect);
    const protocol = `x-nv-sessionid.${this.sessionId}`;

    console.log("[Signaling] Connecting to:", url, reconnect ? "(reconnect)" : "");
    console.log("[Signaling] Session ID:", this.sessionId);
    console.log("[Signaling] Protocol:", protocol);

    await new Promise<void>((resolve, reject) => {
      const urlHost = url.replace(/^wss?:\/\//, "").split("/")[0];
      let reconnectStabilized = !reconnect;

      const ws = new WebSocket(url, protocol, {
        rejectUnauthorized: false,
        headers: {
          Host: urlHost,
          Origin: "https://play.geforcenow.com",
          "User-Agent": USER_AGENT,
          "Sec-WebSocket-Key": randomBytes(16).toString("base64"),
        },
      });

      this.ws = ws;
      let opened = false;

      ws.once("error", (error) => {
        if (!opened) {
          this.emit({ type: "error", message: `Signaling connect failed: ${String(error)}` });
          reject(error);
          return;
        }
        const errorMsg = String(error);
        console.error("[Signaling] WebSocket error during session:", errorMsg);
        this.emit({ type: "error", message: `Signaling session error: ${errorMsg}` });
      });

      ws.once("open", () => {
        opened = true;
        this.manualDisconnect = false;
        this.clearReconnectTimer();
        if (!reconnect) {
          this.sendPeerInfo();
          this.setupHeartbeat();
        }
        this.emit({ type: "connected" });
        resolve();
      });

      ws.on("message", (raw) => {
        if (!reconnectStabilized && this.reconnectAttempts > 0) {
          reconnectStabilized = true;
          this.reconnectAttempts = 0;
          this.emit({ type: "log", message: "Signaling reconnect stabilized" });
        }
        const text = typeof raw === "string" ? raw : raw.toString("utf8");
        this.handleMessage(text);
      });

      ws.on("close", (code, reason) => {
        if (this.ws === ws) {
          this.ws = null;
        }
        this.clearHeartbeat();
        const reasonText = typeof reason === "string" ? reason : reason.toString("utf8");
        const closeReason = reasonText || "socket closed";
        console.log(`[Signaling] WebSocket closed - code: ${code}, reason: "${closeReason}"`);

        if (!opened) {
          reject(new Error(`Signaling socket closed before open: ${closeReason} (code: ${code})`));
          return;
        }

        if (this.manualDisconnect) {
          this.emit({ type: "disconnected", reason: `${closeReason} (code: ${code})` });
          return;
        }

        if (code === 1006 || code === 1011 || code === 1001) {
          this.scheduleReconnect(`${closeReason} (code: ${code})`);
          return;
        }

        this.emit({ type: "disconnected", reason: `${closeReason} (code: ${code})` });
      });
    });
  }

  private handleMessage(text: string): void {
    let parsed: SignalingMessage;
    try {
      parsed = JSON.parse(text) as SignalingMessage;
    } catch {
      this.emit({ type: "log", message: `Ignoring non-JSON signaling packet: ${text.slice(0, 120)}` });
      return;
    }

    if (parsed.hb) {
      // Official client ignores signaling hb payloads.
      return;
    }

    let shouldProcessPayload = true;
    if (typeof parsed.ackid === "number") {
      if (parsed.ackid <= this.maxReceivedAckId) {
        shouldProcessPayload = false;
      } else {
        this.maxReceivedAckId = parsed.ackid;
      }
      this.sendJson({ ack: this.maxReceivedAckId });
    }

    if (!shouldProcessPayload) {
      return;
    }

    if (!parsed.peer_msg?.msg) {
      return;
    }

    let peerPayload: Record<string, unknown>;
    try {
      peerPayload = JSON.parse(parsed.peer_msg.msg) as Record<string, unknown>;
    } catch {
      this.emit({ type: "log", message: "Received non-JSON peer payload" });
      return;
    }

    if (peerPayload.type === "offer" && typeof peerPayload.sdp === "string") {
      console.log(`[Signaling] Received OFFER SDP (${peerPayload.sdp.length} chars), first 500 chars:`);
      console.log(peerPayload.sdp.slice(0, 500));
      this.emit({ type: "offer", sdp: peerPayload.sdp });
      return;
    }

    if (typeof peerPayload.candidate === "string") {
      console.log(`[Signaling] Received remote ICE candidate: ${peerPayload.candidate}`);
      this.emit({
        type: "remote-ice",
        candidate: {
          candidate: peerPayload.candidate,
          sdpMid:
            typeof peerPayload.sdpMid === "string" || peerPayload.sdpMid === null
              ? peerPayload.sdpMid
              : undefined,
          sdpMLineIndex:
            typeof peerPayload.sdpMLineIndex === "number" || peerPayload.sdpMLineIndex === null
              ? peerPayload.sdpMLineIndex
              : undefined,
        },
      });
      return;
    }

    // Log any unhandled peer message types for debugging
    console.log("[Signaling] Unhandled peer message keys:", Object.keys(peerPayload));
  }

  async sendAnswer(payload: SendAnswerRequest): Promise<void> {
    console.log(`[Signaling] Sending ANSWER SDP (${payload.sdp.length} chars), first 500 chars:`);
    console.log(payload.sdp.slice(0, 500));
    if (payload.nvstSdp) {
      console.log(`[Signaling] Sending nvstSdp (${payload.nvstSdp.length} chars):`);
      console.log(payload.nvstSdp);
    }
    const answer = {
      type: "answer",
      sdp: payload.sdp,
      ...(payload.nvstSdp ? { nvstSdp: payload.nvstSdp } : {}),
    };

    this.sendJson({
      peer_msg: {
        from: this.peerId,
        to: 1,
        msg: JSON.stringify(answer),
      },
      ackid: this.nextAckId(),
    });
  }

  async sendIceCandidate(candidate: IceCandidatePayload): Promise<void> {
    console.log(`[Signaling] Sending local ICE candidate: ${candidate.candidate} (sdpMid=${candidate.sdpMid})`);
    this.sendJson({
      peer_msg: {
        from: this.peerId,
        to: 1,
        msg: JSON.stringify({
          candidate: candidate.candidate,
          sdpMid: candidate.sdpMid,
          sdpMLineIndex: candidate.sdpMLineIndex,
        }),
      },
      ackid: this.nextAckId(),
    });
  }

  disconnect(): void {
    this.manualDisconnect = true;
    this.clearHeartbeat();
    this.clearReconnectTimer();
    this.reconnectAttempts = 0;
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}
