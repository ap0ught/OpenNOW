#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "opennow/native/input_protocol.hpp"

#if defined(OPENNOW_HAS_SDL3)
#include <SDL3/SDL.h>
#else
struct SDL_Event {
  int type;
};
#endif

namespace opennow::native {

class InputBridge {
 public:
  using SendPacket = std::function<void(InputPacket)>;

  void SetSendPacket(SendPacket send_packet);
  void HandleEvent(const SDL_Event& event);
  void Tick();
  void OnInputReady(int protocol_version);
  void Reset();

 private:
#if defined(OPENNOW_HAS_SDL3)
  struct GamepadState {
    GamepadStatePacket packet{};
    std::uint64_t last_sent_us = 0;
  };

  void HandleKeyboardEvent(const SDL_Event& event, bool pressed);
  void HandleMouseMotion(const SDL_Event& event);
  void HandleMouseButton(const SDL_Event& event, bool pressed);
  void HandleMouseWheel(const SDL_Event& event);
  void HandleGamepadAxis(const SDL_Event& event);
  void HandleGamepadButton(const SDL_Event& event, bool pressed);
  void HandleGamepadAdded(const SDL_Event& event);
  void HandleGamepadRemoved(const SDL_Event& event);
  void SendGamepadState(int controller_id, bool force);
  void UpdateGamepadBitmap();
#endif
  void Send(const InputPacket& packet);

  SendPacket send_packet_;
  InputEncoder encoder_;
  bool input_ready_ = false;
  std::uint64_t last_heartbeat_us_ = 0;
  std::uint16_t gamepad_bitmap_ = 0;
#if defined(OPENNOW_HAS_SDL3)
  std::unordered_map<int, GamepadState> gamepads_;
#endif
};

}  // namespace opennow::native
