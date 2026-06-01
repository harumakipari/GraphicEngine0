#pragma once

#include <Windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

#include <memory>

#include <cstdint>

enum class TriggerMode
{
    none,
    risingEdge,
    fallingEdge
};

class KeyButton
{
public:
    KeyButton(int vkey) : vkey(vkey), currentState(0), previousState(0)
    {

    }
    virtual ~KeyButton() = default;
    KeyButton(KeyButton&) = delete;
    KeyButton& operator=(KeyButton&) = delete;

	bool State(TriggerMode triggerMode)
	{
		previousState = currentState;
		if (static_cast<USHORT>(GetAsyncKeyState(vkey)) & 0x8000)
		{
			currentState++;
		}
		else
		{
			currentState = 0;
		}
		if (triggerMode == TriggerMode::risingEdge)
		{
			return previousState == 0 && currentState > 0;
		}
		else if (triggerMode == TriggerMode::fallingEdge)
		{
			return previousState > 0 && currentState == 0;
		}
		else
		{
			return currentState > 0;
		}
	}
    

private:
    int vkey;
    int currentState;
    int previousState;

};

class GamePad
{

private:
	enum class Side { l, r };
	enum class Axis { x, y };
	enum class DeadzoneMode { independentAxes, circular, none };
public:
	enum class Button { a, b, x, y, leftThumb, rightThumb, leftShoulder, rightShoulder, start, back, up, down, left, right, end };

public:
	GamePad(int user_id = 0, float deadzonex = 0.05f, float deadzoney = 0.02f, DeadzoneMode deadzone_mode = DeadzoneMode::independentAxes);
	virtual ~GamePad() = default;
	GamePad(GamePad const&) = delete;
	GamePad& operator=(GamePad const&) = delete;


	bool Acquire();

	bool ButtonState(Button button, TriggerMode trigger_mode = TriggerMode::risingEdge) const;

	float ThumbStateLx() const { return thumbState(Side::l, Axis::x); }
	float ThumbStateLy() const { return thumbState(Side::l, Axis::y); }
	float ThumbStateRx() const { return thumbState(Side::r, Axis::x); }
	float ThumbStateRy() const { return thumbState(Side::r, Axis::y); }

	float TriggerStateL()const { return triggerState(Side::l); }
	float TriggerStateR()const { return triggerState(Side::r); }

private:
	float thumbState(Side side, Axis axis) const;
	float triggerState(Side side)const;

	float deadzonex = 0.05f;
	float deadzoney = 0.02f;
	DeadzoneMode _deadzone_mode = DeadzoneMode::none;
	void ApplyStickDeadzone(float x, float y, DeadzoneMode deadzone_mode, float max_value, float deadzone_size, _Out_ float& result_x, _Out_ float& result_y);

private:
	static const size_t _buttonCount = static_cast<size_t>(Button::end);

private:
	int userId = 0;

	struct State
	{
		DWORD packet;
		bool connected = false;
		bool buttons[_buttonCount];
		float triggers[2]; //[side]
		float thumbSticks[2][2]; //[side][axis]
	};
	State currentState;
	State previousState;

	// use on emulation_mode
	bool emulationMode = false; //if gamepad is disconnected then keyboard is used but only user_id = 0
	std::unique_ptr<KeyButton> buttonKeys[_buttonCount];
	std::unique_ptr<KeyButton> thumbStickKeys[2][4]; //[side][+x/-x/+y/-y]
	std::unique_ptr<KeyButton> triggerKeys[2]; //[side]
};

