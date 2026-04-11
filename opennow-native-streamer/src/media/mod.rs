#[derive(Debug, Default)]
pub struct MediaRuntime {
    started: bool,
}

impl MediaRuntime {
    pub fn new() -> Result<Self, String> {
        Ok(Self { started: false })
    }

    pub fn start(&mut self) -> Result<(), String> {
        self.started = true;
        Ok(())
    }

    pub fn stop(&mut self) -> Result<(), String> {
        self.started = false;
        Ok(())
    }

    pub fn is_started(&self) -> bool {
        self.started
    }
}
