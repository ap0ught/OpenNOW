use std::collections::VecDeque;

use crate::{input::protocol::InputPacketEnvelope, session::types::NativeSessionConfig};

pub const PROTOCOL_VERSION: u32 = 1;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ControlMessage {
    Hello { protocol_version: u32, process_id: u32 },
    HelloAck { protocol_version: u32, instance_id: String },
    StartSession { payload: NativeSessionConfig },
    StopSession { reason: Option<String> },
    SignalingOffer { sdp: String },
    RemoteIce { candidate: IceCandidate },
    Input { payload: InputPacketEnvelope },
    Ping,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EventMessage {
    Ready,
    State { state: NativeStreamerState, detail: Option<String> },
    LocalAnswer { sdp: String, nvst_sdp: String },
    LocalIce { candidate: IceCandidate },
    Stats { stats: NativeStats },
    Log { level: String, message: String },
    Error { code: String, message: String, recoverable: bool },
    Pong,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IceCandidate {
    pub candidate: String,
    pub sdp_mid: Option<String>,
    pub sdp_mline_index: Option<u32>,
    pub username_fragment: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NativeStreamerState {
    Booting,
    Idle,
    Starting,
    AwaitingOffer,
    Connecting,
    Streaming,
    Stopping,
    Stopped,
    Failed,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct NativeStats {
    pub frames_rendered: u64,
    pub audio_buffers: u64,
    pub input_packets_sent: u64,
    pub last_error: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FrameCodecError {
    FrameTooLarge,
    InvalidLength(u32),
    Truncated,
    InvalidJson(String),
}

pub trait WireMessage: Sized {
    fn encode_message(&self) -> String;
    fn decode_message(input: &str) -> Result<Self, FrameCodecError>;
}

pub fn encode_frame<T: WireMessage>(message: &T) -> Result<Vec<u8>, FrameCodecError> {
    let payload = message.encode_message().into_bytes();
    let len: u32 = payload.len().try_into().map_err(|_| FrameCodecError::FrameTooLarge)?;
    let mut frame = Vec::with_capacity(payload.len() + 4);
    frame.extend_from_slice(&len.to_be_bytes());
    frame.extend_from_slice(&payload);
    Ok(frame)
}

#[derive(Default)]
pub struct FrameDecoder {
    buffer: VecDeque<u8>,
}

impl FrameDecoder {
    pub fn push(&mut self, chunk: &[u8]) {
        self.buffer.extend(chunk.iter().copied());
    }

    pub fn try_next<T: WireMessage>(&mut self) -> Result<Option<T>, FrameCodecError> {
        if self.buffer.len() < 4 {
            return Ok(None);
        }
        let header = [self.buffer[0], self.buffer[1], self.buffer[2], self.buffer[3]];
        let len = u32::from_be_bytes(header);
        if len > 8 * 1024 * 1024 {
            return Err(FrameCodecError::InvalidLength(len));
        }
        let needed = 4usize + len as usize;
        if self.buffer.len() < needed {
            return Ok(None);
        }
        for _ in 0..4 {
            self.buffer.pop_front();
        }
        let mut payload = vec![0u8; len as usize];
        for byte in &mut payload {
            *byte = self.buffer.pop_front().ok_or(FrameCodecError::Truncated)?;
        }
        let text = String::from_utf8(payload).map_err(|error| FrameCodecError::InvalidJson(error.to_string()))?;
        Ok(Some(T::decode_message(&text)?))
    }
}

fn parse_field<'a>(input: &'a str, key: &str) -> Option<&'a str> {
    input.split(';').find_map(|part| part.strip_prefix(key))
}

impl WireMessage for ControlMessage {
    fn encode_message(&self) -> String {
        match self {
            Self::Ping => "type=ping".to_string(),
            Self::StopSession { reason } => format!("type=stop_session;reason={}", reason.clone().unwrap_or_default()),
            Self::Hello { protocol_version, process_id } => format!("type=hello;protocol_version={protocol_version};process_id={process_id}"),
            Self::HelloAck { protocol_version, instance_id } => format!("type=hello_ack;protocol_version={protocol_version};instance_id={instance_id}"),
            Self::SignalingOffer { sdp } => format!("type=signaling_offer;sdp={}", sdp.replace(';', "%3B")),
            Self::RemoteIce { candidate } => format!(
                "type=remote_ice;candidate={};sdp_mid={};sdp_mline_index={};username_fragment={}",
                candidate.candidate.replace(';', "%3B"),
                candidate.sdp_mid.clone().unwrap_or_default(),
                candidate.sdp_mline_index.map(|v| v.to_string()).unwrap_or_default(),
                candidate.username_fragment.clone().unwrap_or_default()
            ),
            Self::StartSession { payload } => format!("type=start_session;window_title={};session_id={};server_ip={};signaling_server={};signaling_url={};zone={};resolution={};fps={};max_bitrate_kbps={};codec={};color_quality={};mouse_sensitivity_milli={};mouse_acceleration={}", payload.window_title.replace(';', "%3B"), payload.session.session_id, payload.session.server_ip, payload.session.signaling_server, payload.session.signaling_url.replace(';', "%3B"), payload.session.zone, payload.settings.resolution, payload.settings.fps, payload.settings.max_bitrate_kbps, payload.settings.codec, payload.settings.color_quality, payload.settings.mouse_sensitivity_milli, payload.settings.mouse_acceleration),
            Self::Input { payload } => format!("type=input;payload={payload:?}"),
        }
    }

    fn decode_message(input: &str) -> Result<Self, FrameCodecError> {
        match parse_field(input, "type=") {
            Some("ping") => Ok(Self::Ping),
            Some("stop_session") => Ok(Self::StopSession { reason: parse_field(input, "reason=").map(|value| value.to_string()).filter(|value| !value.is_empty()) }),
            Some("hello") => Ok(Self::Hello { protocol_version: parse_field(input, "protocol_version=").and_then(|value| value.parse().ok()).unwrap_or_default(), process_id: parse_field(input, "process_id=").and_then(|value| value.parse().ok()).unwrap_or_default() }),
            Some("hello_ack") => Ok(Self::HelloAck { protocol_version: parse_field(input, "protocol_version=").and_then(|value| value.parse().ok()).unwrap_or_default(), instance_id: parse_field(input, "instance_id=").unwrap_or_default().to_string() }),
            Some(other) => Err(FrameCodecError::InvalidJson(format!("unsupported control message {other}"))),
            None => Err(FrameCodecError::InvalidJson("missing type".into())),
        }
    }
}

impl WireMessage for EventMessage {
    fn encode_message(&self) -> String {
        match self {
            Self::Ready => "type=ready".to_string(),
            Self::Pong => "type=pong".to_string(),
            Self::State { state, detail } => format!("type=state;state={state:?};detail={}", detail.clone().unwrap_or_default()),
            Self::LocalAnswer { sdp, nvst_sdp } => format!("type=local_answer;sdp={};nvst_sdp={}", sdp.replace(';', "%3B"), nvst_sdp.replace(';', "%3B")),
            Self::LocalIce { candidate } => format!("type=local_ice;candidate={};sdp_mid={};sdp_mline_index={};username_fragment={}", candidate.candidate.replace(';', "%3B"), candidate.sdp_mid.clone().unwrap_or_default(), candidate.sdp_mline_index.map(|v| v.to_string()).unwrap_or_default(), candidate.username_fragment.clone().unwrap_or_default()),
            Self::Stats { stats } => format!("type=stats;frames_rendered={};audio_buffers={};input_packets_sent={};last_error={}", stats.frames_rendered, stats.audio_buffers, stats.input_packets_sent, stats.last_error.clone().unwrap_or_default()),
            Self::Log { level, message } => format!("type=log;level={level};message={}", message.replace(';', "%3B")),
            Self::Error { code, message, recoverable } => format!("type=error;code={code};message={};recoverable={recoverable}", message.replace(';', "%3B")),
        }
    }

    fn decode_message(input: &str) -> Result<Self, FrameCodecError> {
        match parse_field(input, "type=") {
            Some("ready") => Ok(Self::Ready),
            Some("pong") => Ok(Self::Pong),
            Some("state") => Ok(Self::State { state: NativeStreamerState::Idle, detail: parse_field(input, "detail=").map(|value| value.to_string()) }),
            Some(other) => Err(FrameCodecError::InvalidJson(format!("unsupported event message {other}"))),
            None => Err(FrameCodecError::InvalidJson("missing type".into())),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_control_frame() {
        let message = ControlMessage::Ping;
        let frame = encode_frame(&message).unwrap();
        let mut decoder = FrameDecoder::default();
        decoder.push(&frame[..2]);
        assert_eq!(decoder.try_next::<ControlMessage>().unwrap(), None);
        decoder.push(&frame[2..]);
        assert_eq!(decoder.try_next::<ControlMessage>().unwrap(), Some(message));
    }

    #[test]
    fn rejects_oversized_frame() {
        let mut decoder = FrameDecoder::default();
        decoder.push(&(9_999_999u32.to_be_bytes()));
        let err = decoder.try_next::<ControlMessage>().unwrap_err();
        assert!(matches!(err, FrameCodecError::InvalidLength(_)));
    }
}
