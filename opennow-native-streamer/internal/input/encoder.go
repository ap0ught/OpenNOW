package input

import (
	"encoding/binary"
	"time"
)

const (
	InputHeartbeat       = 2
	InputKeyDown         = 3
	InputKeyUp           = 4
	InputMouseRelative   = 7
	InputMouseButtonDown = 8
	InputMouseButtonUp   = 9
	InputMouseWheel      = 10
	InputGamepad         = 12
)

type Encoder struct {
	gamepadSequence [4]uint32
}

type KeyboardPayload struct {
	Keycode   uint16
	Scancode  uint16
	Modifiers uint16
	Down      bool
}

type MouseMovePayload struct{ DX, DY int16 }
type MouseButtonPayload struct {
	Button uint8
	Down   bool
}
type MouseWheelPayload struct{ Delta int16 }

type GamepadPayload struct {
	ControllerID uint8
	Buttons      uint16
	LeftTrigger  uint8
	RightTrigger uint8
	LeftStickX   int16
	LeftStickY   int16
	RightStickX  int16
	RightStickY  int16
	Connected    bool
}

func timestampMicros() uint64 { return uint64(time.Now().UnixMicro()) }

func (e *Encoder) EncodeKeyboard(v KeyboardPayload) []byte {
	buf := make([]byte, 16)
	if v.Down {
		buf[0] = InputKeyDown
	} else {
		buf[0] = InputKeyUp
	}
	binary.LittleEndian.PutUint16(buf[2:], v.Keycode)
	binary.LittleEndian.PutUint16(buf[4:], v.Scancode)
	binary.LittleEndian.PutUint16(buf[6:], v.Modifiers)
	binary.LittleEndian.PutUint64(buf[8:], timestampMicros())
	return buf
}

func (e *Encoder) EncodeMouseMove(v MouseMovePayload) []byte {
	buf := make([]byte, 13)
	buf[0] = InputMouseRelative
	binary.LittleEndian.PutUint16(buf[1:], uint16(v.DX))
	binary.LittleEndian.PutUint16(buf[3:], uint16(v.DY))
	binary.LittleEndian.PutUint64(buf[5:], timestampMicros())
	return buf
}

func (e *Encoder) EncodeMouseButton(v MouseButtonPayload) []byte {
	buf := make([]byte, 10)
	if v.Down {
		buf[0] = InputMouseButtonDown
	} else {
		buf[0] = InputMouseButtonUp
	}
	buf[1] = v.Button
	binary.LittleEndian.PutUint64(buf[2:], timestampMicros())
	return buf
}

func (e *Encoder) EncodeMouseWheel(v MouseWheelPayload) []byte {
	buf := make([]byte, 11)
	buf[0] = InputMouseWheel
	binary.LittleEndian.PutUint16(buf[1:], uint16(v.Delta))
	binary.LittleEndian.PutUint64(buf[3:], timestampMicros())
	return buf
}

func (e *Encoder) EncodeGamepad(v GamepadPayload) []byte {
	buf := make([]byte, 38)
	buf[0] = InputGamepad
	buf[1] = v.ControllerID
	if v.Connected {
		buf[2] = 1
	}
	binary.LittleEndian.PutUint32(buf[4:], e.gamepadSequence[v.ControllerID])
	e.gamepadSequence[v.ControllerID]++
	binary.LittleEndian.PutUint16(buf[8:], v.Buttons)
	buf[10] = v.LeftTrigger
	buf[11] = v.RightTrigger
	binary.LittleEndian.PutUint16(buf[12:], uint16(v.LeftStickX))
	binary.LittleEndian.PutUint16(buf[14:], uint16(v.LeftStickY))
	binary.LittleEndian.PutUint16(buf[16:], uint16(v.RightStickX))
	binary.LittleEndian.PutUint16(buf[18:], uint16(v.RightStickY))
	binary.LittleEndian.PutUint64(buf[24:], timestampMicros())
	return buf
}
