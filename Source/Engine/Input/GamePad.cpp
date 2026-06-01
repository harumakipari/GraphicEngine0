#include "pch.h"
#include "GamePad.h"
#include <math.h>
#include <algorithm>
#include <assert.h>

static float ApplyLinearDeadzone(float value, float max_value, float deadzone_size)
{
	if (value < -deadzone_size)
	{
		// Increase negative values to remove the deadzone discontinuity.
		value += deadzone_size;
	}
	else if (value > deadzone_size)
	{
		// Decrease positive values to remove the deadzone discontinuity.
		value -= deadzone_size;
	}
	else
	{
		// Values inside the deadzone come out zero.
		return 0;
	}

	// Scale into 0-1 range.
	float scaled_value = value / (max_value - deadzone_size);
	return std::max<float>(-1.f, std::min<float>(scaled_value, 1.f));
}

void GamePad::ApplyStickDeadzone(float x, float y, DeadzoneMode deadzone_mode, float max_value, float deadzone_size, _Out_ float& result_x, _Out_ float& result_y)
{
	switch (deadzone_mode)
	{
	case DeadzoneMode::independentAxes:
		result_x = ApplyLinearDeadzone(x, max_value, deadzone_size);
		result_y = ApplyLinearDeadzone(y, max_value, deadzone_size);
		break;
	case DeadzoneMode::circular:
	{
		float dist = sqrtf(x * x + y * y);
		float wanted = ApplyLinearDeadzone(dist, max_value, deadzone_size);

		float scale = (wanted > 0.f) ? (wanted / dist) : 0.f;

		result_x = std::max <float>(-1.f, std::min<float>(x * scale, 1.f));
		result_y = std::max<float>(-1.f, std::min<float>(y * scale, 1.f));
		break;
	}
	default: //deadzone_mode::none
		result_x = ApplyLinearDeadzone(x, max_value, 0);
		result_y = ApplyLinearDeadzone(y, max_value, 0);
		break;
	}
}

GamePad::GamePad(int user_id, float deadzonex, float deadzoney, DeadzoneMode deadzone_mode) : userId(user_id), emulationMode(user_id == 0), deadzonex(deadzonex), deadzoney(deadzoney), _deadzone_mode(deadzone_mode)
{
	memset(&currentState, 0, sizeof(State));
	memset(&previousState, 0, sizeof(State));

	if (emulationMode)
	{
		const int button_keymap[_buttonCount] = {
			'Z'/*A*/, 'X'/*B*/, 'C'/*X*/, 'V'/*Y*/,
			VK_HOME/*LEFT_THUMB*/, VK_END/*RIGHT_THUMB*/, VK_LCONTROL/*LEFT_SHOULDER*/, VK_RCONTROL/*RIGHT_SHOULDER*/,
			VK_SPACE/*START*/, VK_BACK/*BACK*/, VK_F1/*UP*/, VK_F2/*DOWN*/, VK_F3/*LEFT*/, VK_F4/*RIGHT*/ };

		for (size_t button_index = 0; button_index < _buttonCount; ++button_index)
		{
			buttonKeys[button_index] = std::make_unique<KeyButton>(button_keymap[button_index]);
		}
#if 0
		thumb_stick_keys[static_cast<size_t>(Side::l)][0] = std::make_unique<keybutton>('D');
		thumb_stick_keys[static_cast<size_t>(Side::l)][1] = std::make_unique<keybutton>('A');
		thumb_stick_keys[static_cast<size_t>(Side::l)][2] = std::make_unique<keybutton>('W');
		thumb_stick_keys[static_cast<size_t>(Side::l)][3] = std::make_unique<keybutton>('S');
		thumb_stick_keys[static_cast<size_t>(Side::r)][0] = std::make_unique<keybutton>(VK_RIGHT);
		thumb_stick_keys[static_cast<size_t>(Side::r)][1] = std::make_unique<keybutton>(VK_LEFT);
		thumb_stick_keys[static_cast<size_t>(Side::r)][2] = std::make_unique<keybutton>(VK_UP);
		thumb_stick_keys[static_cast<size_t>(Side::r)][3] = std::make_unique<keybutton>(VK_DOWN);
#else
		thumbStickKeys[static_cast<size_t>(Side::r)][0] = std::make_unique<KeyButton>('D');
		thumbStickKeys[static_cast<size_t>(Side::r)][1] = std::make_unique<KeyButton>('A');
		thumbStickKeys[static_cast<size_t>(Side::r)][2] = std::make_unique<KeyButton>('W');
		thumbStickKeys[static_cast<size_t>(Side::r)][3] = std::make_unique<KeyButton>('S');
		thumbStickKeys[static_cast<size_t>(Side::l)][0] = std::make_unique<KeyButton>(VK_RIGHT);
		thumbStickKeys[static_cast<size_t>(Side::l)][1] = std::make_unique<KeyButton>(VK_LEFT);
		thumbStickKeys[static_cast<size_t>(Side::l)][2] = std::make_unique<KeyButton>(VK_UP);
		thumbStickKeys[static_cast<size_t>(Side::l)][3] = std::make_unique<KeyButton>(VK_DOWN);
#endif

		triggerKeys[static_cast<size_t>(Side::l)] = std::make_unique<KeyButton>(VK_LSHIFT);
		triggerKeys[static_cast<size_t>(Side::r)] = std::make_unique<KeyButton>(VK_RSHIFT);

	}
}
bool GamePad::Acquire()
{
	XINPUT_STATE state;
	DWORD result = XInputGetState(static_cast<DWORD>(userId), &state);
	if (result == ERROR_DEVICE_NOT_CONNECTED)
	{
		currentState.connected = false;
	}
	else
	{
		previousState = currentState;
		memset(&currentState, 0, sizeof(currentState));
		currentState.connected = true;

		currentState.packet = state.dwPacketNumber;

		WORD button_states = state.Gamepad.wButtons;
		currentState.buttons[static_cast<size_t>(Button::a)] = (button_states & XINPUT_GAMEPAD_A) != 0;
		currentState.buttons[static_cast<size_t>(Button::b)] = (button_states & XINPUT_GAMEPAD_B) != 0;
		currentState.buttons[static_cast<size_t>(Button::x)] = (button_states & XINPUT_GAMEPAD_X) != 0;
		currentState.buttons[static_cast<size_t>(Button::y)] = (button_states & XINPUT_GAMEPAD_Y) != 0;
		currentState.buttons[static_cast<size_t>(Button::leftThumb)] = (button_states & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
		currentState.buttons[static_cast<size_t>(Button::rightThumb)] = (button_states & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
		currentState.buttons[static_cast<size_t>(Button::leftShoulder)] = (button_states & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
		currentState.buttons[static_cast<size_t>(Button::rightShoulder)] = (button_states & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
		currentState.buttons[static_cast<size_t>(Button::back)] = (button_states & XINPUT_GAMEPAD_BACK) != 0;
		currentState.buttons[static_cast<size_t>(Button::start)] = (button_states & XINPUT_GAMEPAD_START) != 0;

		currentState.buttons[static_cast<size_t>(Button::up)] = (button_states & XINPUT_GAMEPAD_DPAD_UP) != 0;
		currentState.buttons[static_cast<size_t>(Button::down)] = (button_states & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
		currentState.buttons[static_cast<size_t>(Button::right)] = (button_states & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
		currentState.buttons[static_cast<size_t>(Button::left)] = (button_states & XINPUT_GAMEPAD_DPAD_LEFT) != 0;

		if (_deadzone_mode == DeadzoneMode::none)
		{
			currentState.triggers[static_cast<size_t>(Side::l)] = ApplyLinearDeadzone(static_cast<float>(state.Gamepad.bLeftTrigger), 255.f, 0.f);
			currentState.triggers[static_cast<size_t>(Side::r)] = ApplyLinearDeadzone(static_cast<float>(state.Gamepad.bRightTrigger), 255.f, 0.f);
		}
		else
		{
			currentState.triggers[static_cast<size_t>(Side::l)] = ApplyLinearDeadzone(static_cast<float>(state.Gamepad.bLeftTrigger), 255.f, static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD));
			currentState.triggers[static_cast<size_t>(Side::r)] = ApplyLinearDeadzone(static_cast<float>(state.Gamepad.bRightTrigger), 255.f, static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD));
		}

		ApplyStickDeadzone(static_cast<float>(state.Gamepad.sThumbLX), static_cast<float>(state.Gamepad.sThumbLY),
			_deadzone_mode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),
			currentState.thumbSticks[static_cast<size_t>(Side::l)][static_cast<size_t>(Axis::x)], currentState.thumbSticks[static_cast<size_t>(Side::l)][static_cast<size_t>(Axis::y)]);

		ApplyStickDeadzone(static_cast<float>(state.Gamepad.sThumbRX), static_cast<float>(state.Gamepad.sThumbRY),
			_deadzone_mode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),
			currentState.thumbSticks[static_cast<size_t>(Side::r)][static_cast<size_t>(Axis::x)], currentState.thumbSticks[static_cast<size_t>(Side::r)][static_cast<size_t>(Axis::y)]);
	}
	return true;
}

bool GamePad::ButtonState(Button button, TriggerMode trigger_mode) const
{
	bool button_state = false;
	if (currentState.connected)
	{
		switch (trigger_mode)
		{
		case TriggerMode::none:
			button_state = currentState.buttons[static_cast<size_t>(button)];
			break;
		case TriggerMode::risingEdge:
			button_state = !previousState.buttons[static_cast<size_t>(button)] && currentState.buttons[static_cast<size_t>(button)];
			break;
		case TriggerMode::fallingEdge:
			button_state = previousState.buttons[static_cast<size_t>(button)] && !currentState.buttons[static_cast<size_t>(button)];
			break;
		}
	}
	if (emulationMode && !button_state)
	{
		button_state = buttonKeys[static_cast<size_t>(button)]->State(trigger_mode);
	}
	return button_state;
}

float GamePad::thumbState(Side side, Axis axis) const
{
	float stick_state = 0.0f;
	if (currentState.connected)
	{
		stick_state = currentState.thumbSticks[static_cast<size_t>(side)][static_cast<size_t>(axis)];
	}
	if (emulationMode && stick_state == 0.0f)
	{
		stick_state = axis == Axis::x ? static_cast<float>(std::min<size_t>(1, thumbStickKeys[static_cast<size_t>(side)][0]->State(TriggerMode::none))) - (std::min<size_t>(1, thumbStickKeys[static_cast<size_t>(side)][1]->State(TriggerMode::none))) :
			static_cast<float>(std::min<size_t>(1, thumbStickKeys[static_cast<size_t>(side)][2]->State(TriggerMode::none))) - (std::min<size_t>(1, thumbStickKeys[static_cast<size_t>(side)][3]->State(TriggerMode::none)));
	}
	return stick_state;
}
float GamePad::triggerState(Side side) const
{
	float trigger_state = 0.0f;
	if (currentState.connected)
	{
		trigger_state = currentState.triggers[static_cast<size_t>(side)];
	}
	if (emulationMode && trigger_state == 0)
	{
		trigger_state = triggerKeys[static_cast<size_t>(side)]->State(TriggerMode::none) ? 1.0f : 0.0f;
	}
	return trigger_state;
}
