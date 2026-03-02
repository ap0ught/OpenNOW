import { Clapperboard, Search, RefreshCw, Clock3, Cpu } from "lucide-react";
import type { JSX } from "react";

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
  const query = searchQuery.trim().toLowerCase();
  const filtered = query
    ? clips.filter((clip) => {
        const haystack = `${clip.gameTitle} ${clip.machineLabel ?? ""} ${clip.codec ?? ""} ${clipTypeLabel(clip.clipType)}`.toLowerCase();
        return haystack.includes(query);
      })
    : clips;

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
                <div className="clip-card-banner">
                  {clip.gameBannerUrl ? (
                    <img src={clip.gameBannerUrl} alt={clip.gameTitle} className="clip-card-image" loading="lazy" />
                  ) : (
                    <div className="clip-card-fallback">
                      <Clapperboard size={20} />
                    </div>
                  )}
                  <span className={`clip-card-status clip-card-status--${clip.status}`}>
                    {clip.status}
                  </span>
                </div>

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

                  {clip.fileUrl && (
                    <a href={clip.fileUrl} target="_blank" rel="noreferrer" className="clip-card-link">
                      Open Clip
                    </a>
                  )}
                </div>
              </article>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

