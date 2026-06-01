#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <Windows.h>

#include <Xinput.h>
#pragma comment(lib, "xinput.lib")

#include <DirectXMath.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>


enum class InputDeviceType { Keyboard, Mouse, Gamepad };

class InputKey
{
protected:
    int vkey_;
    float pressTime_ = 0.0f;
    float oldPressTime_ = 0.0f;
    InputDeviceType deviceType_;

public:
    InputKey(int vKey, InputDeviceType deviceType) : vkey_(vKey), pressTime_(0), oldPressTime_(0), deviceType_(deviceType) {}
    virtual ~InputKey() = default;
    virtual void Update(float deltaTime);
    virtual bool IsPressed() const { return pressTime_ > 0; }
    virtual bool IsTrigger() const { return (oldPressTime_ == 0 && pressTime_ > 0); }
    virtual bool IsRelease() const { return (pressTime_ == 0 && oldPressTime_ > 0); }

    virtual InputDeviceType GetDeviceType() const { return deviceType_; }
};

class Keyboard :public InputKey
{
public:
    Keyboard(int vkey) :InputKey(vkey, InputDeviceType::Keyboard) {}
    virtual ~Keyboard() override = default;
    Keyboard(Keyboard&) = delete;
    Keyboard& operator=(Keyboard&) = delete;
};

class Mouse :public InputKey
{
public:
    Mouse(int vkey) :InputKey(vkey, InputDeviceType::Mouse) {}
    virtual ~Mouse() override = default;
    Mouse(Mouse&) = delete;
    Mouse& operator=(Mouse&) = delete;
};


///------------  Gamepad ----------///
enum class GamepadKey
{
    A = XINPUT_GAMEPAD_A,
    B = XINPUT_GAMEPAD_B,
    X = XINPUT_GAMEPAD_X,
    Y = XINPUT_GAMEPAD_Y,

    LB = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RB = XINPUT_GAMEPAD_RIGHT_SHOULDER,

    Start = XINPUT_GAMEPAD_START,
    Back = XINPUT_GAMEPAD_BACK,

    DPadUp = XINPUT_GAMEPAD_DPAD_UP,
    DPadDown = XINPUT_GAMEPAD_DPAD_DOWN,
    DPadLeft = XINPUT_GAMEPAD_DPAD_LEFT,
    DPadRight = XINPUT_GAMEPAD_DPAD_RIGHT
};
enum class GamePadKeyType :uint8_t
{
    Button,
    LeftTrigger,
    RightTrigger,
    LeftStick,
    RightStick
};
enum class Side { Left, Right };
enum class Axis { X, Y };

class Gamepad :public InputKey
{
private:
    GamePadKeyType keyType;
public:
    Gamepad(int vkey, GamePadKeyType type = GamePadKeyType::Button) :InputKey(vkey, InputDeviceType::Gamepad), keyType(type) {}
    virtual ~Gamepad() override = default;
    Gamepad(Gamepad&) = delete;
    Gamepad& operator=(Gamepad&) = delete;

    void Update(float deltaTime) override;
};



enum class DeadZoneMode { IndependentAxes, Circular, None };
enum class DeviceFlags { All, KeyboardOnly, MouseOnly, GamePadOnly, KeyboardAndMouse, KeyboardAndGamePad, MouseAndGamePad };
enum class InputStateMask { Press, Trigger, Release };
enum class Direction { Up, Left, Down, Right, None };


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
    static int GetAxisRaw(Side side, Axis axis) { return static_cast<int>(round(GetAxis(side, axis))); }
    static Direction GetAxisDirection()
    {
        int ax = GetAxisRaw(Side::Left, Axis::X);
        int ay = GetAxisRaw(Side::Left, Axis::Y);
        if (ax == 1) { return Direction::Right; }
        if (ax == -1) { return Direction::Left; }
        if (ay == 1) { return Direction::Up; }
        if (ay == -1) { return Direction::Down; }

        return Direction::None;
    }

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

    static bool IsLeftStick(Direction dir)
    {
        auto stick = GetLeftStick();

        switch (dir)
        {
        case Direction::Up:    return stick.y > 0.5f;
        case Direction::Down:  return stick.y < -0.5f;
        case Direction::Left:  return stick.x < -0.5f;
        case Direction::Right: return stick.x > 0.5f;
        }

        return false;
    }


    static bool IsRightStick(Direction dir)
    {
        auto stick = GetRightStick();

        switch (dir)
        {
        case Direction::Up:    return stick.y > 0.5f;
        case Direction::Down:  return stick.y < -0.5f;
        case Direction::Left:  return stick.x < -0.5f;
        case Direction::Right: return stick.x > 0.5f;
        }

        return false;
    }

    static DirectX::XMFLOAT2 GetAxisDirectionVector()
    {
        int ax = GetAxisRaw(Side::Left, Axis::X);
        int ay = GetAxisRaw(Side::Left, Axis::Y);

        return { static_cast<float>(ax),static_cast<float>(ay) };
    }

    // マウスカーソルの移動量取得
    static void GetMouseDelta(int& x, int& y) { (x = mousePositionX[0] - mousePositionX[1], y = mousePositionY[0] - mousePositionY[1]); }

    // マウスカーソルのX座標を取得
    static int GetMousePositionX() { return mousePositionX[0]; }

    // マウスカーソルのY座標を取得
    static int GetMousePositionY() { return mousePositionY[0]; }

    // マウスカーソルの位置を取得
    static DirectX::XMFLOAT2 GetMousePositionScreen()
    {
        DirectX::XMFLOAT2 mousePosition;
        mousePosition.x = static_cast<float>(mousePositionX[0]);
        mousePosition.y = static_cast<float>(mousePositionY[0]);
        return mousePosition;
    }

    static void SetViewportRect(float x, float y, float w, float h)
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

        if (mx < viewportX || my < viewportY ||
            mx > viewportX + viewportW ||
            my > viewportY + viewportH)
        {
            return false;
        }

        out.x = mx - viewportX;
        out.y = my - viewportY;
        return true;
    }

    // ビューポート内のマウスカーソル位置を取得（ビューポート外なら0,0を返す）
    static DirectX::XMFLOAT2 GetMousePositionInViewportOrZero()
    {
        DirectX::XMFLOAT2 pos = { 0.0f,0.0f };
        GetMousePositionInViewport(pos);
        return pos;
    }

    // 前回のマウスカーソルX座標取得
    static int GetOldMousePositionX() { return mousePositionX[1]; }

    // 前回のマウスカーソルY座標取得
    static int GetOldMousePositionY() { return mousePositionY[1]; }

    // カーソルが表示されているか
    static bool IsCursolVisible() { return cursolVisible; }

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

        // 黒帯補正（今後フルスクリーン対応するなら必須）
        float offsetX = (vw - DESIGN_W * scale) * 0.5f;
        float offsetY = (vh - DESIGN_H * scale) * 0.5f;

        out.x = (vp.x - offsetX) / scale;
        out.y = (vp.y - offsetY) / scale;
        return true;
    }

    // コントローラー振動を開始する
    static void SetVibration(float power, float duration)
    {
        vibrationPower = std::clamp(power, 0.0f, 1.0f);
        vibrationDuration = duration;
        vibrationTimer = duration;
    }

    // 入力を有効化・無効化する
    static void SetInputEnabled(bool enabled)
    {
        inputEnabled = enabled;
        if (inputEnabled)
        {
            Logger::Log(U8("入力を有効化した"));
        }
        else
        {
            Logger::Log(U8("入力を無効化した"));
        }
    }

    // 入力可能かどうか
    static bool IsInputEnabled()
    {
        return inputEnabled;
    }

    // カーソルの表示非表示を変更
    static void SetCursolVisible(bool visible)
    {
        cursolVisible = visible;
        int count = 0;

        do
        {
            count = ShowCursor(visible);
        } while ((visible && count < 0) || (!visible && count >= 0));
    }

private:


public:
    // ゲームパッドが接続されているか
    static bool IsGamepadConnected() { return isGamePadConnected; }

    // アクティブなデバイスを取得
    static InputDeviceType GetActiveDevice() { return activeDevice; }

    static inline bool isUIUsingMouse = false; // UIがマウスを使用しているかどうか

private:
    static inline InputDeviceType activeDevice = InputDeviceType::Keyboard;

    friend class Gamepad;
    static XINPUT_STATE GetXInputState() { return state; }

    static inline float mAxis[2][2];
    static inline XINPUT_STATE state;
    static inline DeadZoneMode deadZoneMode = DeadZoneMode::Circular;
    static inline int slot = 0;

    static inline int mousePositionX[2];
    static inline int mousePositionY[2];

    static inline bool isGamePadConnected = false;

    static inline bool cursolLock = false;
    static inline bool cursolVisible = true;

    static inline float viewportX = 0;
    static inline float viewportY = 0;
    static inline float viewportW = 0;
    static inline float viewportH = 0;

    static inline float vibrationTimer = 0.0f; // 振動の残り時間
    static inline float vibrationDuration = 0.0f; // 振動の総時間
    static inline float vibrationPower = 0.0f; // 振動の強さ（0.0f～1.0f）

    static inline bool inputEnabled = true; // 入力することができるかどうか
};

#endif // INPUT_SYSTEM_H