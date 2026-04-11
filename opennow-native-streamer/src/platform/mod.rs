#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TargetPlatform {
    Windows,
    Macos,
    Linux,
    LinuxArm,
}

impl TargetPlatform {
    pub fn current() -> Self {
        match (std::env::consts::OS, std::env::consts::ARCH) {
            ("windows", _) => Self::Windows,
            ("macos", _) => Self::Macos,
            ("linux", "aarch64") | ("linux", "arm") => Self::LinuxArm,
            _ => Self::Linux,
        }
    }

    pub fn decoder_notes(self) -> &'static str {
        match self {
            Self::Windows => "Prefer D3D11 or Media Foundation decode paths; fall back to software.",
            Self::Macos => "Prefer VideoToolbox decode; fall back to software.",
            Self::Linux => "Probe VA-API, NVDEC, and software decode paths.",
            Self::LinuxArm => "Probe V4L2/stateless and software decode paths for Raspberry Pi-class devices.",
        }
    }
}
