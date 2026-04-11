#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IceServer {
    pub urls: Vec<String>,
    pub username: Option<String>,
    pub credential: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MediaConnectionInfo {
    pub ip: String,
    pub port: u16,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SessionInfo {
    pub session_id: String,
    pub server_ip: String,
    pub signaling_server: String,
    pub signaling_url: String,
    pub zone: String,
    pub streaming_base_url: Option<String>,
    pub ice_servers: Vec<IceServer>,
    pub media_connection_info: Option<MediaConnectionInfo>,
    pub gpu_type: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StreamSettings {
    pub resolution: String,
    pub fps: u32,
    pub max_bitrate_kbps: u32,
    pub codec: String,
    pub color_quality: String,
    pub decoder_preference: Option<String>,
    pub mouse_sensitivity_milli: u32,
    pub mouse_acceleration: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NativeSessionConfig {
    pub session: SessionInfo,
    pub settings: StreamSettings,
    pub window_title: String,
}
