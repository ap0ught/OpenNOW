#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace opennow::native {

enum class InputRoute {
  Reliable,
  PartiallyReliable,
};

struct InputPacket {
  std::vector<std::uint8_t> bytes;
  InputRoute route = InputRoute::Reliable;
};

struct KeyboardPacket {
  std::uint16_t keycode = 0;
  std::uint16_t scancode = 0;
  std::uint16_t modifiers = 0;
  std::uint64_t timestamp_us = 0;
};

struct MouseMovePacket {
  std::int16_t dx = 0;
  std::int16_t dy = 0;
  std::uint64_t timestamp_us = 0;
};

struct MouseButtonPacket {
  std::uint8_t button = 0;
  std::uint64_t timestamp_us = 0;
};

struct MouseWheelPacket {
  std::int16_t delta = 0;
  std::uint64_t timestamp_us = 0;
};

struct GamepadStatePacket {
  int controller_id = 0;
  std::uint16_t buttons = 0;
  std::uint8_t left_trigger = 0;
  std::uint8_t right_trigger = 0;
  std::int16_t left_stick_x = 0;
  std::int16_t left_stick_y = 0;
  std::int16_t right_stick_x = 0;
  std::int16_t right_stick_y = 0;
  bool connected = false;
  std::uint64_t timestamp_us = 0;
};

struct KeyboardMapping {
  std::uint16_t vk = 0;
  std::uint16_t scancode = 0;
};

class InputEncoder {
 public:
  void SetProtocolVersion(int version);
  int protocol_version() const { return protocol_version_; }

  std::vector<std::uint8_t> EncodeHeartbeat() const;
  std::vector<std::uint8_t> EncodeKeyDown(const KeyboardPacket& packet) const;
  std::vector<std::uint8_t> EncodeKeyUp(const KeyboardPacket& packet) const;
  std::vector<std::uint8_t> EncodeMouseMove(const MouseMovePacket& packet) const;
  std::vector<std::uint8_t> EncodeMouseButtonDown(const MouseButtonPacket& packet) const;
  std::vector<std::uint8_t> EncodeMouseButtonUp(const MouseButtonPacket& packet) const;
  std::vector<std::uint8_t> EncodeMouseWheel(const MouseWheelPacket& packet) const;
  std::vector<std::uint8_t> EncodeGamepadState(const GamepadStatePacket& packet, std::uint16_t bitmap, bool use_partially_reliable);

  void ResetGamepadSequenceNumbers();

 private:
  std::vector<std::uint8_t> EncodeKey(std::uint32_t type, const KeyboardPacket& packet) const;
  std::vector<std::uint8_t> EncodeMouseButton(std::uint32_t type, const MouseButtonPacket& packet) const;
  std::uint16_t NextGamepadSequence(int controller_id);

  int protocol_version_ = 2;
  std::unordered_map<int, std::uint16_t> gamepad_sequence_;
};

std::uint64_t TimestampUs(std::optional<std::uint64_t> source_timestamp_ms = std::nullopt);
std::uint16_t ModifierFlags(bool shift, bool ctrl, bool alt, bool meta, bool caps_lock, bool num_lock);
std::int16_t NormalizeToInt16(float value);
std::uint8_t NormalizeToUint8(float value);
float ApplyAxisDeadzone(float value, float deadzone = 0.15f);
KeyboardMapping MapKeyName(const std::string& key_name);

constexpr std::uint16_t GAMEPAD_DPAD_UP = 0x0001;
constexpr std::uint16_t GAMEPAD_DPAD_DOWN = 0x0002;
constexpr std::uint16_t GAMEPAD_DPAD_LEFT = 0x0004;
constexpr std::uint16_t GAMEPAD_DPAD_RIGHT = 0x0008;
constexpr std::uint16_t GAMEPAD_START = 0x0010;
constexpr std::uint16_t GAMEPAD_BACK = 0x0020;
constexpr std::uint16_t GAMEPAD_LS = 0x0040;
constexpr std::uint16_t GAMEPAD_RS = 0x0080;
constexpr std::uint16_t GAMEPAD_LB = 0x0100;
constexpr std::uint16_t GAMEPAD_RB = 0x0200;
constexpr std::uint16_t GAMEPAD_GUIDE = 0x0400;
constexpr std::uint16_t GAMEPAD_A = 0x1000;
constexpr std::uint16_t GAMEPAD_B = 0x2000;
constexpr std::uint16_t GAMEPAD_X = 0x4000;
constexpr std::uint16_t GAMEPAD_Y = 0x8000;

}  // namespace opennow::native
