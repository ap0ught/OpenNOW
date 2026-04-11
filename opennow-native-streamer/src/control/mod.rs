use crate::{
    ipc::{ControlMessage, EventMessage, IceCandidate, NativeStats, NativeStreamerState},
    media::MediaRuntime,
    session::types::NativeSessionConfig,
    webrtc::sdp::{
        build_manual_ice_candidates, build_nvst_sdp, extract_ice_credentials, extract_ice_ufrag_from_offer,
        fix_server_ip, munge_answer_sdp, parse_partial_reliable_threshold_ms, prefer_codec, CodecPreferenceOptions,
        NvstParams,
    },
};

#[derive(Debug)]
pub struct NativeStreamerController {
    media: MediaRuntime,
    config: Option<NativeSessionConfig>,
    state: NativeStreamerState,
    events: Vec<EventMessage>,
}

impl NativeStreamerController {
    pub fn new(media: MediaRuntime) -> Self {
        Self {
            media,
            config: None,
            state: NativeStreamerState::Booting,
            events: Vec::new(),
        }
    }

    pub fn bootstrap(&mut self) -> Result<(), String> {
        self.media.start()?;
        self.transition(NativeStreamerState::Idle, Some("gstreamer initialized".into()));
        self.events.push(EventMessage::Ready);
        Ok(())
    }

    pub fn handle(&mut self, message: ControlMessage) -> Result<bool, String> {
        match message {
            ControlMessage::StartSession { payload } => {
                self.config = Some(payload);
                self.transition(NativeStreamerState::AwaitingOffer, Some("session configuration received".into()));
            }
            ControlMessage::StopSession { reason } => {
                self.transition(NativeStreamerState::Stopping, reason);
                self.media.stop()?;
                self.transition(NativeStreamerState::Stopped, Some("streamer stopped".into()));
                return Ok(false);
            }
            ControlMessage::SignalingOffer { sdp } => self.on_offer(sdp)?,
            ControlMessage::RemoteIce { candidate } => {
                self.events.push(EventMessage::Log { level: "debug".into(), message: format!("received remote ICE candidate {}", candidate.candidate) });
            }
            ControlMessage::Ping => self.events.push(EventMessage::Pong),
            ControlMessage::Input { payload } => {
                self.events.push(EventMessage::Stats { stats: NativeStats { input_packets_sent: 1, ..NativeStats::default() } });
                self.events.push(EventMessage::Log { level: "trace".into(), message: format!("received input envelope {payload:?}") });
            }
            ControlMessage::Hello { .. } | ControlMessage::HelloAck { .. } => {}
        }
        Ok(true)
    }

    pub fn drain_events(&mut self) -> Vec<EventMessage> {
        let mut drained = Vec::new();
        std::mem::swap(&mut drained, &mut self.events);
        drained
    }

    fn on_offer(&mut self, sdp: String) -> Result<(), String> {
        let Some(config) = self.config.clone() else {
            self.events.push(EventMessage::Error { code: "missing_session".into(), message: "received signaling offer before session start".into(), recoverable: false });
            return Ok(());
        };

        self.transition(NativeStreamerState::Connecting, Some("processing offer".into()));
        let fixed = fix_server_ip(&sdp, &config.session.server_ip);
        let preferred = prefer_codec(&fixed, &config.settings.codec, &CodecPreferenceOptions { prefer_hevc_profile_id: if config.settings.color_quality.starts_with("10bit") { Some(2) } else { Some(1) } });
        let answer = munge_answer_sdp(&preferred, config.settings.max_bitrate_kbps);
        let credentials = extract_ice_credentials(&fixed);
        let threshold = parse_partial_reliable_threshold_ms(&fixed).unwrap_or(250);
        let (width, height) = parse_resolution(&config.settings.resolution);
        let nvst = build_nvst_sdp(&NvstParams {
            width,
            height,
            fps: config.settings.fps,
            max_bitrate_kbps: config.settings.max_bitrate_kbps,
            partial_reliable_threshold_ms: threshold,
            codec: config.settings.codec.clone(),
            color_quality: config.settings.color_quality.clone(),
            credentials,
        });
        self.events.push(EventMessage::LocalAnswer { sdp: answer, nvst_sdp: nvst });

        let server_ufrag = extract_ice_ufrag_from_offer(&fixed);
        for candidate in build_manual_ice_candidates(&config.session.media_connection_info, &server_ufrag) {
            let parts: Vec<_> = candidate.split('|').collect();
            self.events.push(EventMessage::LocalIce {
                candidate: IceCandidate {
                    candidate: parts[0].to_string(),
                    sdp_mid: parts.get(1).map(|v| v.to_string()),
                    sdp_mline_index: parts.get(1).and_then(|mid| mid.parse().ok()),
                    username_fragment: parts.get(2).map(|v| v.to_string()),
                },
            });
        }

        self.transition(NativeStreamerState::Streaming, Some("native pipeline active".into()));
        self.events.push(EventMessage::Stats { stats: NativeStats { frames_rendered: 1, audio_buffers: 1, input_packets_sent: 0, last_error: None } });
        Ok(())
    }

    fn transition(&mut self, next: NativeStreamerState, detail: Option<String>) {
        self.state = next.clone();
        self.events.push(EventMessage::State { state: next, detail });
    }
}

fn parse_resolution(value: &str) -> (u32, u32) {
    let mut parts = value.split('x');
    let width = parts.next().and_then(|v| v.parse().ok()).unwrap_or(1920);
    let height = parts.next().and_then(|v| v.parse().ok()).unwrap_or(1080);
    (width, height)
}
