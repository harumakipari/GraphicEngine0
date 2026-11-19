#pragma once

#include <Windows.h>

#include <Xinput.h>
#pragma comment(lib, "xinput.lib")

#include <DirectXMath.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

enum class InputDeviceType:uint8_t
{
    Keyboard, Mouse, Gamepad
};

class InputKey
{
protected:
    int vkey;
    float pressTime = 0.0f;
    float oldPressTime = 0.0f;
    InputDeviceType deviceType;

public:
    InputKey(const int vKey, const InputDeviceType deviceType) : vkey(vKey), pressTime(0), oldPressTime(0), deviceType(deviceType) {}
    virtual ~InputKey() = default;
    virtual void Update(float deltaTime);
    virtual bool IsPressed() const { return pressTime > 0; }
    virtual bool IsTrigger() const { return (oldPressTime == 0 && pressTime > 0); }
    virtual bool IsRelease() const { return (pressTime == 0 && oldPressTime > 0); }

    virtual InputDeviceType GetDeviceType() const { return deviceType; }
};

class Keyboard :public InputKey
{
public:
    Keyboard(const int vkey) :InputKey(vkey, InputDeviceType::Keyboard) {}
    virtual ~Keyboard() override = default;
    Keyboard(Keyboard&) = delete;
    Keyboard& operator=(Keyboard&) = delete;
};

class Mouse :public InputKey
{
public:
    Mouse(const int vkey) :InputKey(vkey, InputDeviceType::Mouse) {}
    virtual ~Mouse() override = default;
    Mouse(Mouse&) = delete;
    Mouse& operator=(Mouse&) = delete;
};


enum class GamePadKeyType :uint8_t
{
    Button,
    LeftTrigger,
    RightTrigger,
    LeftStick,
    RightStick
};
enum class Side:uint8_t
{
    Left, Right
};

enum class Axis:uint8_t
{
    X, Y
};

class Gamepad :public InputKey
{
private:
    GamePadKeyType keyType;
public:
    Gamepad(const int vkey, const GamePadKeyType type = GamePadKeyType::Button) :InputKey(vkey, InputDeviceType::Gamepad), keyType(type) {}
    virtual ~Gamepad() override = default;
    Gamepad(Gamepad&) = delete;
    Gamepad& operator=(Gamepad&) = delete;

    void Update(float deltaTime) override;
};

enum class DeadZoneMode :uint8_t
{
    IndependentAxes, Circular, None
};
enum class DeviceFlags :uint8_t
{
    All, KeyboardOnly, MouseOnly, GamePadOnly, KeyboardAndMouse, KeyboardAndGamePad, MouseAndGamePad
};
enum class InputStateMask :uint8_t
{
    Press, Trigger, Release
};
enum class Direction :uint8_t
{
    Up, Left, Down, Right, None
};


class InputSystem
{
private:
    static inline std::unordered_map<std::string, std::vector<std::unique_ptr<InputKey>>> inputKeys;
    static inline std::unique_ptr<InputKey> directionKeys[2][4];

private:
    InputSystem() = default;
    virtual ~InputSystem() = default;

public:
    //  初期化
    static void Initialize();

    //終了化
    static void Finalize() {}

    // 更新処理
    static void Update(float deltaTime);

    // 入力状態の取得
    static bool GetInputState(const std::string& action, InputStateMask state = InputStateMask::Press, DeviceFlags flag = DeviceFlags::All);

    // 方向スティック状態の取得
    static float GetAxis(Side side, Axis axis) { return mAxis[static_cast<size_t>(side)][static_cast<size_t>(axis)]; }
    static int GetAxisRaw(const Side side, const Axis axis) { return static_cast<int>(round(GetAxis(side, axis))); }

    static DirectX::XMFLOAT2 GetLeftStick()
    {
        return {
            GetAxis(Side::Left, Axis::X),
            GetAxis(Side::Left, Axis::Y)
        };
    }

    static DirectX::XMFLOAT2 GetRightStick()
    {
        return {
            GetAxis(Side::Right, Axis::X),
            GetAxis(Side::Right, Axis::Y)
        };
    }

    // マウスカーソルの移動量取得
    static void GetMouseDelta(int& x, int& y) { (x = mousePositionX[0] - mousePositionX[1], y = mousePositionY[0] - mousePositionY[1]); }

    // マウスカーソルの位置を取得
    static DirectX::XMFLOAT2 GetMousePositionScreen()
    {
        DirectX::XMFLOAT2 mousePosition;
        mousePosition.x = static_cast<float>(mousePositionX[0]);
        mousePosition.y = static_cast<float>(mousePositionY[0]);
        return mousePosition;
    }

    static void SetViewportRect(const float x, const float y, const float w, const float h)
    {
        viewportX = x;
        viewportY = y;
        viewportW = w;
        viewportH = h;
    }

    static void GetViewportRect(float& x, float& y, float& w, float& h)
    {
        x = viewportX;
        y = viewportY;
        w = viewportW;
        h = viewportH;
    }

    // ビューポート内のマウスカーソル位置を取得（ビューポート外ならfalseを返す）
    static bool GetMousePositionInViewport(DirectX::XMFLOAT2& out)
    {
        float mx = static_cast<float>(mousePositionX[0]);
        float my = static_cast<float>(mousePositionY[0]);
        if (mx < viewportX || my < viewportY || mx > viewportX + viewportW || my > viewportY + viewportH)
        {
            return false;
        }
        out.x = mx - viewportX;
        out.y = my - viewportY;
        return true;
    }

    static bool GetMousePositionUI(DirectX::XMFLOAT2& out)
    {
        DirectX::XMFLOAT2 vp;
        if (!GetMousePositionInViewport(vp))
            return false;

        float vx, vy, vw, vh;
        GetViewportRect(vx, vy, vw, vh);

        constexpr float DESIGN_W = 1920.0f;
        constexpr float DESIGN_H = 1080.0f;

        float scaleX = vw / DESIGN_W;
        float scaleY = vh / DESIGN_H;
        float scale = std::min<float>(scaleX, scaleY);

        float offsetX = (vw - DESIGN_W * scale) * 0.5f;
        float offsetY = (vh - DESIGN_H * scale) * 0.5f;

        out.x = (vp.x - offsetX) / scale;
        out.y = (vp.y - offsetY) / scale;
        return true;
    }

    // コントローラー振動を開始する
    static void SetVibration(float power, float duration);
    static void RequestRumble(float leftMotorStrength, float rightMotorStrength, float duration);
    static void StopRumble();

    static bool IsAnyGamepadButtonTriggeredRaw()
    {
        return rawGamepadButtonsTriggered != 0 ||
            (rawGamepadLeftTriggerPressed && !rawGamepadLeftTriggerPressedPrevious) ||
            (rawGamepadRightTriggerPressed && !rawGamepadRightTriggerPressedPrevious);
    }

    static bool IsAnyGamepadButtonPressedRaw()
    {
        return rawGamepadButtonsPrevious != 0 || rawGamepadLeftTriggerPressed || rawGamepadRightTriggerPressed;
    }

    static bool IsGamepadButtonTriggeredRaw(const WORD button)
    {
        return (rawGamepadButtonsTriggered & button) != 0;
    }

    // 入力を有効化・無効化する
    static void SetInputEnabled(const bool enabled)
    {
        inputEnabled = enabled;
    }

    // 入力可能かどうか
    static bool IsInputEnabled()
    {
        return inputEnabled;
    }

    // カーソルの表示非表示を変更
    static void SetCursorVisible(const bool visible)
    {
        int count = 0;
        do
        {
            count = ShowCursor(visible);
        } while ((visible && count < 0) || (!visible && count >= 0));
    }

public:
    // ゲームパッドが接続されているか
    static bool IsGamepadConnected() { return isGamePadConnected; }

    static XINPUT_STATE GetXInputState() { return state; }
private:
    static inline float mAxis[2][2];
    static inline XINPUT_STATE state;
    static inline WORD rawGamepadButtonsTriggered = 0;
    static inline WORD rawGamepadButtonsPrevious = 0;
    static inline bool rawGamepadLeftTriggerPressed = false;
    static inline bool rawGamepadLeftTriggerPressedPrevious = false;
    static inline bool rawGamepadRightTriggerPressed = false;
    static inline bool rawGamepadRightTriggerPressedPrevious = false;
    static inline DeadZoneMode deadZoneMode = DeadZoneMode::Circular;
    static inline int slot = 0;

    static inline int mousePositionX[2];
    static inline int mousePositionY[2];

    static inline bool isGamePadConnected = false;

    static inline float viewportX = 0;
    static inline float viewportY = 0;
    static inline float viewportW = 0;
    static inline float viewportH = 0;

    static inline float rumbleTimer = 0.0f; // 振動の残り時間
    static inline float rumbleLeftMotorStrength = 0.0f; // 振動の総時間
    static inline float rumbleRightMotorStrength = 0.0f; // 振動の強さ（0.0f～1.0f）

    static inline bool inputEnabled = true; // 入力することができるかどうか
};

