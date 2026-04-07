#include "opennow/native/input_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace opennow::native {

namespace {
constexpr std::uint32_t INPUT_HEARTBEAT = 2;
constexpr std::uint32_t INPUT_KEY_DOWN = 3;
constexpr std::uint32_t INPUT_KEY_UP = 4;
constexpr std::uint32_t INPUT_MOUSE_REL = 7;
constexpr std::uint32_t INPUT_MOUSE_BUTTON_DOWN = 8;
constexpr std::uint32_t INPUT_MOUSE_BUTTON_UP = 9;
constexpr std::uint32_t INPUT_MOUSE_WHEEL = 10;
constexpr std::uint32_t INPUT_GAMEPAD = 12;
constexpr std::size_t GAMEPAD_PACKET_SIZE = 38;

void WriteUint16Le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
  out[offset] = static_cast<std::uint8_t>(value & 0xff);
  out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void WriteUint32Le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
  out[offset] = static_cast<std::uint8_t>(value & 0xff);
  out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
  out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
  out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

void WriteUint16Be(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
  out[offset] = static_cast<std::uint8_t>((value >> 8) & 0xff);
  out[offset + 1] = static_cast<std::uint8_t>(value & 0xff);
}

void WriteInt16Be(std::vector<std::uint8_t>& out, std::size_t offset, std::int16_t value) {
  WriteUint16Be(out, offset, static_cast<std::uint16_t>(value));
}

void WriteInt16Le(std::vector<std::uint8_t>& out, std::size_t offset, std::int16_t value) {
  WriteUint16Le(out, offset, static_cast<std::uint16_t>(value));
}

void WriteUint64Be(std::vector<std::uint8_t>& out, std::size_t offset, std::uint64_t value) {
  for (int index = 0; index < 8; ++index) {
    out[offset + index] = static_cast<std::uint8_t>((value >> ((7 - index) * 8)) & 0xff);
  }
}

void WriteUint64Le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint64_t value) {
  for (int index = 0; index < 8; ++index) {
    out[offset + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xff);
  }
}

void WriteWrappedTimestamp(std::vector<std::uint8_t>& out, std::size_t offset) {
  const auto ts_us = TimestampUs();
  const auto hi = static_cast<std::uint32_t>((ts_us >> 32) & 0xffffffffULL);
  const auto lo = static_cast<std::uint32_t>(ts_us & 0xffffffffULL);
  WriteUint32Le(out, offset + 4, 0);
  out[offset] = static_cast<std::uint8_t>((hi >> 24) & 0xff);
  out[offset + 1] = static_cast<std::uint8_t>((hi >> 16) & 0xff);
  out[offset + 2] = static_cast<std::uint8_t>((hi >> 8) & 0xff);
  out[offset + 3] = static_cast<std::uint8_t>(hi & 0xff);
  out[offset + 4] = static_cast<std::uint8_t>((lo >> 24) & 0xff);
  out[offset + 5] = static_cast<std::uint8_t>((lo >> 16) & 0xff);
  out[offset + 6] = static_cast<std::uint8_t>((lo >> 8) & 0xff);
  out[offset + 7] = static_cast<std::uint8_t>(lo & 0xff);
}

std::vector<std::uint8_t> WrapSingleEvent(const std::vector<std::uint8_t>& payload, int protocol_version) {
  if (protocol_version <= 2) {
    return payload;
  }
  std::vector<std::uint8_t> wrapped(10 + payload.size());
  wrapped[0] = 0x23;
  WriteWrappedTimestamp(wrapped, 1);
  wrapped[9] = 0x22;
  std::copy(payload.begin(), payload.end(), wrapped.begin() + 10);
  return wrapped;
}

std::vector<std::uint8_t> WrapBatchedEvent(const std::vector<std::uint8_t>& payload, int protocol_version) {
  if (protocol_version <= 2) {
    return payload;
  }
  std::vector<std::uint8_t> wrapped(12 + payload.size());
  wrapped[0] = 0x23;
  WriteWrappedTimestamp(wrapped, 1);
  wrapped[9] = 0x21;
  WriteUint16Be(wrapped, 10, static_cast<std::uint16_t>(payload.size()));
  std::copy(payload.begin(), payload.end(), wrapped.begin() + 12);
  return wrapped;
}

std::vector<std::uint8_t> WrapGamepadPartiallyReliable(
    const std::vector<std::uint8_t>& payload,
    int protocol_version,
    int controller_id,
    std::uint16_t sequence_number) {
  if (protocol_version <= 2) {
    return payload;
  }
  std::vector<std::uint8_t> wrapped(16 + payload.size());
  wrapped[0] = 0x23;
  WriteWrappedTimestamp(wrapped, 1);
  wrapped[9] = 0x26;
  wrapped[10] = static_cast<std::uint8_t>(controller_id & 0xff);
  WriteUint16Be(wrapped, 11, sequence_number);
  wrapped[13] = 0x21;
  WriteUint16Be(wrapped, 14, static_cast<std::uint16_t>(payload.size()));
  std::copy(payload.begin(), payload.end(), wrapped.begin() + 16);
  return wrapped;
}

}  // namespace

void InputEncoder::SetProtocolVersion(int version) {
  protocol_version_ = std::max(1, version);
}

std::vector<std::uint8_t> InputEncoder::EncodeHeartbeat() const {
  std::vector<std::uint8_t> payload(4);
  WriteUint32Le(payload, 0, INPUT_HEARTBEAT);
  return payload;
}

std::vector<std::uint8_t> InputEncoder::EncodeKeyDown(const KeyboardPacket& packet) const {
  return EncodeKey(INPUT_KEY_DOWN, packet);
}

std::vector<std::uint8_t> InputEncoder::EncodeKeyUp(const KeyboardPacket& packet) const {
  return EncodeKey(INPUT_KEY_UP, packet);
}

std::vector<std::uint8_t> InputEncoder::EncodeMouseMove(const MouseMovePacket& packet) const {
  std::vector<std::uint8_t> payload(22);
  WriteUint32Le(payload, 0, INPUT_MOUSE_REL);
  WriteInt16Be(payload, 4, packet.dx);
  WriteInt16Be(payload, 6, packet.dy);
  WriteUint16Be(payload, 8, 0);
  payload[10] = 0;
  payload[11] = 0;
  payload[12] = 0;
  payload[13] = 0;
  WriteUint64Be(payload, 14, packet.timestamp_us);
  return WrapBatchedEvent(payload, protocol_version_);
}

std::vector<std::uint8_t> InputEncoder::EncodeMouseButtonDown(const MouseButtonPacket& packet) const {
  return EncodeMouseButton(INPUT_MOUSE_BUTTON_DOWN, packet);
}

std::vector<std::uint8_t> InputEncoder::EncodeMouseButtonUp(const MouseButtonPacket& packet) const {
  return EncodeMouseButton(INPUT_MOUSE_BUTTON_UP, packet);
}

std::vector<std::uint8_t> InputEncoder::EncodeMouseWheel(const MouseWheelPacket& packet) const {
  std::vector<std::uint8_t> payload(22);
  WriteUint32Le(payload, 0, INPUT_MOUSE_WHEEL);
  WriteInt16Be(payload, 4, 0);
  WriteInt16Be(payload, 6, packet.delta);
  WriteUint16Be(payload, 8, 0);
  payload[10] = 0;
  payload[11] = 0;
  payload[12] = 0;
  payload[13] = 0;
  WriteUint64Be(payload, 14, packet.timestamp_us);
  return WrapSingleEvent(payload, protocol_version_);
}

std::vector<std::uint8_t> InputEncoder::EncodeGamepadState(
    const GamepadStatePacket& packet,
    std::uint16_t bitmap,
    bool use_partially_reliable) {
  std::vector<std::uint8_t> payload(GAMEPAD_PACKET_SIZE);
  WriteUint32Le(payload, 0, INPUT_GAMEPAD);
  WriteUint16Le(payload, 4, 26);
  WriteUint16Le(payload, 6, static_cast<std::uint16_t>(packet.controller_id & 0x03));
  WriteUint16Le(payload, 8, bitmap);
  WriteUint16Le(payload, 10, 20);
  WriteUint16Le(payload, 12, packet.buttons);
  WriteUint16Le(payload, 14, static_cast<std::uint16_t>(packet.left_trigger | (packet.right_trigger << 8)));
  WriteInt16Le(payload, 16, packet.left_stick_x);
  WriteInt16Le(payload, 18, packet.left_stick_y);
  WriteInt16Le(payload, 20, packet.right_stick_x);
  WriteInt16Le(payload, 22, packet.right_stick_y);
  WriteUint16Le(payload, 24, 0);
  WriteUint16Le(payload, 26, 85);
  WriteUint16Le(payload, 28, 0);
  WriteUint64Le(payload, 30, packet.timestamp_us);
  if (use_partially_reliable) {
    return WrapGamepadPartiallyReliable(payload, protocol_version_, packet.controller_id, NextGamepadSequence(packet.controller_id));
  }
  return WrapBatchedEvent(payload, protocol_version_);
}

void InputEncoder::ResetGamepadSequenceNumbers() {
  gamepad_sequence_.clear();
}

std::vector<std::uint8_t> InputEncoder::EncodeKey(std::uint32_t type, const KeyboardPacket& packet) const {
  std::vector<std::uint8_t> payload(18);
  WriteUint32Le(payload, 0, type);
  WriteUint16Be(payload, 4, packet.keycode);
  WriteUint16Be(payload, 6, packet.modifiers);
  WriteUint16Be(payload, 8, packet.scancode);
  WriteUint64Be(payload, 10, packet.timestamp_us);
  return WrapSingleEvent(payload, protocol_version_);
}

std::vector<std::uint8_t> InputEncoder::EncodeMouseButton(std::uint32_t type, const MouseButtonPacket& packet) const {
  std::vector<std::uint8_t> payload(18);
  WriteUint32Le(payload, 0, type);
  payload[4] = packet.button;
  payload[5] = 0;
  payload[6] = 0;
  payload[7] = 0;
  payload[8] = 0;
  payload[9] = 0;
  WriteUint64Be(payload, 10, packet.timestamp_us);
  return WrapSingleEvent(payload, protocol_version_);
}

std::uint16_t InputEncoder::NextGamepadSequence(int controller_id) {
  const auto current = gamepad_sequence_.contains(controller_id) ? gamepad_sequence_[controller_id] : 1;
  gamepad_sequence_[controller_id] = static_cast<std::uint16_t>((current + 1) % 65536);
  return current;
}

std::uint64_t TimestampUs(std::optional<std::uint64_t> source_timestamp_ms) {
  if (source_timestamp_ms) {
    return *source_timestamp_ms * 1000ULL;
  }
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::uint16_t ModifierFlags(bool shift, bool ctrl, bool alt, bool meta, bool caps_lock, bool num_lock) {
  std::uint16_t flags = 0;
  if (shift) flags |= 0x01;
  if (ctrl) flags |= 0x02;
  if (alt) flags |= 0x04;
  if (meta) flags |= 0x08;
  if (caps_lock) flags |= 0x10;
  if (num_lock) flags |= 0x20;
  return flags;
}

std::int16_t NormalizeToInt16(float value) {
  const auto clamped = std::clamp(value, -1.0f, 1.0f);
  return static_cast<std::int16_t>(std::lrint(clamped * 32767.0f));
}

std::uint8_t NormalizeToUint8(float value) {
  const auto clamped = std::clamp(value, 0.0f, 1.0f);
  return static_cast<std::uint8_t>(std::lrint(clamped * 255.0f));
}

float ApplyAxisDeadzone(float value, float deadzone) {
  const auto abs_value = std::fabs(value);
  if (abs_value < deadzone) {
    return 0.0f;
  }
  const auto scaled = (abs_value - deadzone) / (1.0f - deadzone);
  return std::copysign(std::min(1.0f, scaled), value);
}

KeyboardMapping MapKeyName(const std::string& key_name) {
  static const std::unordered_map<std::string, KeyboardMapping> map = {
      {"A", {0x41, 0x04}}, {"B", {0x42, 0x05}}, {"C", {0x43, 0x06}}, {"D", {0x44, 0x07}},
      {"E", {0x45, 0x08}}, {"F", {0x46, 0x09}}, {"G", {0x47, 0x0a}}, {"H", {0x48, 0x0b}},
      {"I", {0x49, 0x0c}}, {"J", {0x4a, 0x0d}}, {"K", {0x4b, 0x0e}}, {"L", {0x4c, 0x0f}},
      {"M", {0x4d, 0x10}}, {"N", {0x4e, 0x11}}, {"O", {0x4f, 0x12}}, {"P", {0x50, 0x13}},
      {"Q", {0x51, 0x14}}, {"R", {0x52, 0x15}}, {"S", {0x53, 0x16}}, {"T", {0x54, 0x17}},
      {"U", {0x55, 0x18}}, {"V", {0x56, 0x19}}, {"W", {0x57, 0x1a}}, {"X", {0x58, 0x1b}},
      {"Y", {0x59, 0x1c}}, {"Z", {0x5a, 0x1d}},
      {"1", {0x31, 0x1e}}, {"2", {0x32, 0x1f}}, {"3", {0x33, 0x20}}, {"4", {0x34, 0x21}},
      {"5", {0x35, 0x22}}, {"6", {0x36, 0x23}}, {"7", {0x37, 0x24}}, {"8", {0x38, 0x25}},
      {"9", {0x39, 0x26}}, {"0", {0x30, 0x27}}, {"RETURN", {0x0d, 0x28}}, {"ESCAPE", {0x1b, 0x29}},
      {"BACKSPACE", {0x08, 0x2a}}, {"TAB", {0x09, 0x2b}}, {"SPACE", {0x20, 0x2c}},
      {"-", {0xbd, 0x2d}}, {"=", {0xbb, 0x2e}}, {"[", {0xdb, 0x2f}}, {"]", {0xdd, 0x30}},
      {"\\", {0xdc, 0x31}}, {";", {0xba, 0x33}}, {"'", {0xde, 0x34}}, {"`", {0xc0, 0x35}},
      {",", {0xbc, 0x36}}, {".", {0xbe, 0x37}}, {"/", {0xbf, 0x38}},
      {"CAPSLOCK", {0x14, 0x39}}, {"F1", {0x70, 0x3a}}, {"F2", {0x71, 0x3b}}, {"F3", {0x72, 0x3c}},
      {"F4", {0x73, 0x3d}}, {"F5", {0x74, 0x3e}}, {"F6", {0x75, 0x3f}}, {"F7", {0x76, 0x40}},
      {"F8", {0x77, 0x41}}, {"F9", {0x78, 0x42}}, {"F10", {0x79, 0x43}}, {"F11", {0x7a, 0x44}},
      {"F12", {0x7b, 0x45}}, {"PRINTSCREEN", {0x2c, 0x46}}, {"SCROLLLOCK", {0x91, 0x47}},
      {"PAUSE", {0x13, 0x48}}, {"INSERT", {0x2d, 0x49}}, {"HOME", {0x24, 0x4a}},
      {"PAGEUP", {0x21, 0x4b}}, {"DELETE", {0x2e, 0x4c}}, {"END", {0x23, 0x4d}},
      {"PAGEDOWN", {0x22, 0x4e}}, {"RIGHT", {0x27, 0x4f}}, {"LEFT", {0x25, 0x50}},
      {"DOWN", {0x28, 0x51}}, {"UP", {0x26, 0x52}}, {"NUMLOCKCLEAR", {0x90, 0x53}},
      {"KP_DIVIDE", {0x6f, 0x54}}, {"KP_MULTIPLY", {0x6a, 0x55}}, {"KP_MINUS", {0x6d, 0x56}},
      {"KP_PLUS", {0x6b, 0x57}}, {"KP_ENTER", {0x0d, 0x58}}, {"KP_1", {0x61, 0x59}},
      {"KP_2", {0x62, 0x5a}}, {"KP_3", {0x63, 0x5b}}, {"KP_4", {0x64, 0x5c}}, {"KP_5", {0x65, 0x5d}},
      {"KP_6", {0x66, 0x5e}}, {"KP_7", {0x67, 0x5f}}, {"KP_8", {0x68, 0x60}}, {"KP_9", {0x69, 0x61}},
      {"KP_0", {0x60, 0x62}}, {"KP_PERIOD", {0x6e, 0x63}}, {"F13", {0x7c, 0x64}},
      {"APPLICATION", {0x5d, 0x65}}, {"LCTRL", {0xa2, 0xe0}}, {"LSHIFT", {0xa0, 0xe1}},
      {"LALT", {0xa4, 0xe2}}, {"LGUI", {0x5b, 0xe3}}, {"RCTRL", {0xa3, 0xe4}},
      {"RSHIFT", {0xa1, 0xe5}}, {"RALT", {0xa5, 0xe6}}, {"RGUI", {0x5c, 0xe7}},
  };

  const auto found = map.find(key_name);
  if (found != map.end()) {
    return found->second;
  }
  return {};
}

}  // namespace opennow::native
