import { Clapperboard, Search, RefreshCw, Clock3, Cpu, X } from "lucide-react";
import type { JSX } from "react";
import { useEffect, useMemo, useRef, useState } from "react";

import type { ClipRecord } from "@shared/gfn";

export interface ClipsPageProps {
  clips: ClipRecord[];
  searchQuery: string;
  onSearchChange: (query: string) => void;
  onRefresh: () => void;
}

function formatTimestamp(timestampMs: number): string {
  const date = new Date(timestampMs);
  if (Number.isNaN(date.getTime())) return "Unknown time";
  return date.toLocaleString();
}

function clipTypeLabel(type: ClipRecord["clipType"]): string {
  if (type === "instant-replay") return "Instant Replay";
  if (type === "manual-recording") return "Recording";
  return "Screenshot";
}

export function ClipsPage({ clips, searchQuery, onSearchChange, onRefresh }: ClipsPageProps): JSX.Element {
  const [previewClipId, setPreviewClipId] = useState<string | null>(null);
  const [localAssetUrlById, setLocalAssetUrlById] = useState<Record<string, string>>({});
  const loadingAssetIdsRef = useRef<Set<string>>(new Set());
  const objectUrlsRef = useRef<Set<string>>(new Set());
  const query = searchQuery.trim().toLowerCase();
  const filtered = query
    ? clips.filter((clip) => {
        const haystack = `${clip.gameTitle} ${clip.machineLabel ?? ""} ${clip.codec ?? ""} ${clipTypeLabel(clip.clipType)}`.toLowerCase();
        return haystack.includes(query);
      })
    : clips;

  const needsLocalAsset = (clip: ClipRecord): boolean =>
    Boolean(clip.filePath) && (!clip.fileUrl || clip.fileUrl.startsWith("file:"));

  const resolveClipSource = (clip: ClipRecord): string | null => {
    if (localAssetUrlById[clip.id]) {
      return localAssetUrlById[clip.id]!;
    }
    if (clip.fileUrl && !clip.fileUrl.startsWith("file:")) {
      return clip.fileUrl;
    }
    return null;
  };

  const previewClip = useMemo(
    () => filtered.find((clip) => clip.id === previewClipId) ?? clips.find((clip) => clip.id === previewClipId) ?? null,
    [clips, filtered, previewClipId],
  );
  const previewSource = previewClip ? resolveClipSource(previewClip) : null;

  const isPreviewVideo = previewClip ? previewClip.clipType !== "screenshot" : false;

  useEffect(() => {
    return () => {
      for (const url of objectUrlsRef.current) {
        URL.revokeObjectURL(url);
      }
      objectUrlsRef.current.clear();
    };
  }, []);

  useEffect(() => {
    const candidates = previewClip
      ? [...filtered, previewClip]
      : filtered;

    for (const clip of candidates) {
      if (!needsLocalAsset(clip) || !clip.filePath) {
        continue;
      }
      if (localAssetUrlById[clip.id] || loadingAssetIdsRef.current.has(clip.id)) {
        continue;
      }

      loadingAssetIdsRef.current.add(clip.id);
      void window.openNow.readCaptureAsset(clip.filePath)
        .then((asset) => {
          const blob = new Blob([new Uint8Array(asset.bytes)], { type: asset.mimeType });
          const url = URL.createObjectURL(blob);
          objectUrlsRef.current.add(url);
          setLocalAssetUrlById((prev) => ({ ...prev, [clip.id]: url }));
        })
        .catch((error) => {
          console.warn("Failed to load clip asset:", error);
        })
        .finally(() => {
          loadingAssetIdsRef.current.delete(clip.id);
        });
    }
  }, [filtered, localAssetUrlById, previewClip]);

  return (
    <div className="clips-page">
      <header className="clips-toolbar">
        <div className="clips-title">
          <Clapperboard className="clips-title-icon" size={22} />
          <h1>Clips</h1>
        </div>

        <div className="clips-search">
          <Search className="clips-search-icon" size={16} />
          <input
            type="text"
            value={searchQuery}
            onChange={(event) => onSearchChange(event.target.value)}
            placeholder="Search clips..."
            className="clips-search-input"
          />
        </div>

        <button type="button" className="clips-refresh-btn" onClick={onRefresh} title="Refresh clips">
          <RefreshCw size={14} />
          Refresh
        </button>
      </header>

      <div className="clips-grid-area">
        {filtered.length === 0 ? (
          <div className="clips-empty-state">
            <Clapperboard className="clips-empty-icon" size={44} />
            <h3>No clips yet</h3>
            <p>
              {query
                ? "No clips match your search."
                : "Use your capture shortcuts during a session to save instant replays, recordings, and screenshots."}
            </p>
          </div>
        ) : (
          <div className="clips-grid">
            {filtered.map((clip) => (
              <article key={clip.id} className="clip-card">
                {(() => {
                  const source = resolveClipSource(clip);
                  const canPreview = Boolean(source);
                  return (
                <button
                  type="button"
                  className={`clip-card-banner clip-card-banner-btn${canPreview ? "" : " is-disabled"}`}
                  onClick={() => {
                    if (canPreview) {
                      setPreviewClipId(clip.id);
                    }
                  }}
                  disabled={!canPreview}
                >
                  {source ? (
                    clip.clipType === "screenshot" ? (
                      <img src={source} alt={clip.gameTitle} className="clip-card-image" loading="lazy" />
                    ) : (
                      <video src={source} className="clip-card-image" muted playsInline preload="metadata" />
                    )
                  ) : clip.gameBannerUrl ? (
                    <img src={clip.gameBannerUrl} alt={clip.gameTitle} className="clip-card-image" loading="lazy" />
                  ) : (
                    <div className="clip-card-fallback">
                      <Clapperboard size={20} />
                    </div>
                  )}
                  <span className={`clip-card-status clip-card-status--${clip.status}`}>
                    {clip.status}
                  </span>
                </button>
                  );
                })()}

                <div className="clip-card-body">
                  <h3 className="clip-card-title">{clip.gameTitle}</h3>
                  <div className="clip-card-meta">
                    <span className="clip-card-chip">{clipTypeLabel(clip.clipType)}</span>
                    {clip.codec && <span className="clip-card-chip">{clip.codec}</span>}
                  </div>

                  <div className="clip-card-row">
                    <Cpu size={12} />
                    <span>{clip.machineLabel || "Unknown machine"}</span>
                  </div>
                  <div className="clip-card-row">
                    <Clock3 size={12} />
                    <span>{formatTimestamp(clip.timestampMs)}</span>
                  </div>

                  {resolveClipSource(clip) && (
                    <button type="button" className="clip-card-link" onClick={() => setPreviewClipId(clip.id)}>
                      Preview
                    </button>
                  )}
                </div>
              </article>
            ))}
          </div>
        )}
      </div>

      {previewClip && previewSource && (
        <div className="clips-preview-backdrop" onClick={() => setPreviewClipId(null)}>
          <div
            className="clips-preview-modal"
            role="dialog"
            aria-modal="true"
            aria-label="Clip preview"
            onClick={(event) => event.stopPropagation()}
          >
            <div className="clips-preview-head">
              <strong>{previewClip.gameTitle}</strong>
              <button type="button" className="clips-preview-close" onClick={() => setPreviewClipId(null)}>
                <X size={16} />
              </button>
            </div>
            <div className="clips-preview-content">
              {isPreviewVideo ? (
                <video src={previewSource} controls autoPlay playsInline className="clips-preview-media" />
              ) : (
                <img src={previewSource} alt={previewClip.gameTitle} className="clips-preview-media" />
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
