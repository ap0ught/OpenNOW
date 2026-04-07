#include "opennow/native/app.hpp"

#include <sstream>

#include "opennow/native/platform_info.hpp"
#include "opennow/native/protocol.hpp"

namespace opennow::native {

Application::Application(std::string ipc_host, int ipc_port, std::string session_id)
    : ipc_host_(std::move(ipc_host)), ipc_port_(ipc_port), session_id_(std::move(session_id)) {}

Application::~Application() {
  webrtc_session_.Disconnect();
  media_pipeline_.Shutdown();
  ipc_client_.Disconnect();
#if defined(OPENNOW_HAS_SDL3)
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
#endif
}

bool Application::Initialize(std::string& error) {
#if defined(OPENNOW_HAS_SDL3)
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
    error = SDL_GetError();
    return false;
  }

  window_ = SDL_CreateWindow("OpenNOW Native Streamer", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window_) {
    error = SDL_GetError();
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, nullptr);
  if (!renderer_) {
    error = SDL_GetError();
    return false;
  }

  media_pipeline_.SetLogger([this](const std::string& message) { EmitLog(message); });
  if (!media_pipeline_.Initialize(renderer_, error)) {
    return false;
  }

  ipc_client_.SetStatusHandler([this](const std::string& message) {
    EmitLog(message);
  });
  ipc_client_.SetMessageHandler([this](const std::string& json) {
    HandleIncomingJson(json);
  });
  if (!ipc_client_.Connect(ipc_host_, static_cast<std::uint16_t>(ipc_port_))) {
    error = "Could not connect to Electron native-streamer manager";
    return false;
  }

  webrtc_session_.SetEmitter([this](const std::string& json) {
    ipc_client_.SendJson(json);
  });
  webrtc_session_.SetLogger([this](const std::string& message) {
    EmitLog(message);
  });
  webrtc_session_.SetMediaPipeline(&media_pipeline_);
  webrtc_session_.SetInputReadyCallback([this](int protocol_version) {
    input_bridge_.OnInputReady(protocol_version);
    EmitState("streaming", "Input channel ready", "protocol v" + std::to_string(protocol_version));
  });
  input_bridge_.SetSendPacket([this](InputPacket packet) {
    EmitInput(std::move(packet));
  });

  ipc_client_.SendJson(BuildEnvelope("hello", std::string("{\"version\":1,\"sessionId\":\"") + EscapeJson(session_id_) + "\"}"));
  EmitState("launching", "Native streamer window created", DescribePlatformTarget() + std::string(" · ") + media_pipeline_.DescribeCapabilities());
  running_ = true;
  return true;
#else
  error = "SDL3 development headers were not found when building OpenNOW Native Streamer.";
  return false;
#endif
}

int Application::Run() {
#if defined(OPENNOW_HAS_SDL3)
  while (running_) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running_ = false;
        break;
      }
      input_bridge_.HandleEvent(event);
    }

    input_bridge_.Tick();

    SDL_SetRenderDrawColor(renderer_, 8, 10, 18, 255);
    SDL_RenderClear(renderer_);
    media_pipeline_.RenderFrame();
    SDL_RenderPresent(renderer_);
    SDL_Delay(8);
  }
  return 0;
#else
  return 1;
#endif
}

void Application::HandleIncomingJson(const std::string& json) {
  const auto type = FindJsonString(json, "type");
  if (!type) {
    return;
  }

  if (*type == "session-config") {
    std::string error;
    if (webrtc_session_.ConfigureFromSession(json, error)) {
      ipc_client_.SendJson(BuildEnvelope("session-config-ack", "{\"ok\":true}"));
      EmitState("ready", "Session configuration applied", media_pipeline_.DescribeCapabilities());
    } else {
      EmitState("failed", "Invalid session configuration", error);
    }
    return;
  }

  if (*type == "signaling-connected") {
    EmitState("connecting", "Signaling connected");
    return;
  }

  if (*type == "signaling-offer") {
    if (const auto sdp = FindJsonString(json, "sdp")) {
      std::string error;
      if (!webrtc_session_.HandleOffer(*sdp, error)) {
        EmitState("failed", "Offer handling failed", error);
      }
    }
    return;
  }

  if (*type == "signaling-remote-ice") {
    webrtc_session_.AddRemoteIce(json);
    return;
  }

  if (*type == "disconnect") {
    running_ = false;
    webrtc_session_.Disconnect();
    EmitState("exited", "Disconnect requested by Electron shell");
  }
}

void Application::EmitState(const std::string& state, const std::string& message, const std::string& detail) {
  std::ostringstream payload;
  payload << "{\"state\":\"" << EscapeJson(state) << "\",\"message\":\"" << EscapeJson(message) << "\"";
  if (!detail.empty()) {
    payload << ",\"detail\":\"" << EscapeJson(detail) << "\"";
  }
  payload << "}";
  ipc_client_.SendJson(BuildEnvelope("state", payload.str()));
}

void Application::EmitLog(const std::string& message) {
  ipc_client_.SendJson(BuildEnvelope("log", std::string("{\"message\":\"") + EscapeJson(message) + "\"}"));
}

void Application::EmitInput(InputPacket packet) {
  if (!webrtc_session_.SendInputPacket(packet)) {
    EmitLog("Input packet dropped because the data channel is not open yet");
  }
}

}  // namespace opennow::native
