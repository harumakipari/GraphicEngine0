#include "pch.h"
#include "InputSystem.h"
#include <Windows.h>
#include "Graphics/Core/Graphics.h"

// 仮想的な左スティック方向のキーコード
#define GAMEPAD_L_UP     0
#define GAMEPAD_L_LEFT   1
#define GAMEPAD_L_DOWN   2
#define GAMEPAD_L_RIGHT  3

// 仮想的な右スティック方向のキーコード
#define GAMEPAD_R_UP     4
#define GAMEPAD_R_LEFT   5
#define GAMEPAD_R_DOWN   6
#define GAMEPAD_R_RIGHT  7

// 斜めの入力を正規化
static float ApplyLinearDeadzone(float value, float maxValue, float deadZoneSize)
{
    if (value < -deadZoneSize)
    {
        value += deadZoneSize;
    }
    else if (value > deadZoneSize)
    {
        value -= deadZoneSize;
    }
    else
    {
        return 0;
    }

    // 0 ~ 1 にスケーリング
    float scaledValue = value / (maxValue - deadZoneSize);
    return std::max<float>(-1.0f, std::min<float>(scaledValue, 1.0f));
}

static void ApplyStickDeadzone(float x, float y, DeadZoneMode deadZoneMode, float maxValue, float deadZoneSize, _Out_ float& resultX, _Out_ float& resultY)
{
    switch (deadZoneMode)
    {
    case DeadZoneMode::IndependentAxes:
        resultX = ApplyLinearDeadzone(x, maxValue, deadZoneSize);
        resultY = ApplyLinearDeadzone(y, maxValue, deadZoneSize);
        break;
    case  DeadZoneMode::Circular:
    {
        float dist = sqrtf(x * x + y * y);
        float wanted = ApplyLinearDeadzone(dist, maxValue, deadZoneSize);
        float scale = (wanted > 0.0f) ? (wanted / dist) : 0.0f;
        resultX = std::max<float>(-1.0f, std::min<float>(x * scale, 1.0f));
        resultY = std::max<float>(-1.0f, std::min<float>(y * scale, 1.0f));
        break;
    }
    default: //DeadZoneMode::None:
        //resultX
        break;
    }
}

// スティック取得関数を作成する
static DirectX::XMFLOAT2 GetStick(const XINPUT_GAMEPAD& pad, GamePadKeyType type)
{
    float x = 0.0f;
    float y = 0.0f;

    if (type == GamePadKeyType::LeftStick)
    {
        x = pad.sThumbLX / 32767.0f;
        y = pad.sThumbLY / 32767.0f;
    }
    else
    {
        x = pad.sThumbRX / 32767.0f;
        y = pad.sThumbRY / 32767.0f;
    }

    float dead = 0.25f;

    if (fabs(x) < dead) x = 0.0f;
    if (fabs(y) < dead) y = 0.0f;

    return { x, y };
}

void InputKey::Update(float deltaTime)
{
    oldPressTime_ = pressTime_;
    pressTime_ = (static_cast<USHORT>(GetAsyncKeyState(vkey_)) & 0x8000) ? pressTime_ + deltaTime : 0.0f;
}

void Gamepad::Update(float deltaTime)
{
    oldPressTime_ = pressTime_;

    const XINPUT_STATE& state = InputSystem::GetXInputState();
    const XINPUT_GAMEPAD& pad = state.Gamepad;

    bool pressed = false;

    switch (keyType)
    {
    case GamePadKeyType::Button:
    {
        pressed = (pad.wButtons & vkey_) != 0;
        break;
    }

    case GamePadKeyType::LeftTrigger:
    {
        pressed = pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        break;
    }

    case GamePadKeyType::RightTrigger:
    {
        pressed = pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        break;
    }

    case GamePadKeyType::LeftStick:
    case GamePadKeyType::RightStick:
    {
        auto stick = GetStick(pad, keyType);

        // vkey_ は方向を表す
        switch (vkey_)
        {
        case GAMEPAD_L_UP:
        case GAMEPAD_R_UP:
            pressed = stick.y > 0.0f;
            break;

        case GAMEPAD_L_DOWN:
        case GAMEPAD_R_DOWN:
            pressed = stick.y < 0.0f;
            break;

        case GAMEPAD_L_LEFT:
        case GAMEPAD_R_LEFT:
            pressed = stick.x < 0.0f;
            break;

        case GAMEPAD_L_RIGHT:
        case GAMEPAD_R_RIGHT:
            pressed = stick.x > 0.0f;
            break;
        }

        break;
    }
    }

    pressTime_ = pressed ? pressTime_ + deltaTime : 0.0f;
}

//  初期化
void InputSystem::Initialize()
{
    directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Up)] = std::make_unique<Keyboard>('W');
    directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Left)] = std::make_unique<Keyboard>('A');
    directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Down)] = std::make_unique<Keyboard>('S');
    directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Right)] = std::make_unique<Keyboard>('D');

    directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Up)] = std::make_unique<Keyboard>(VK_UP);
    directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Left)] = std::make_unique<Keyboard>(VK_LEFT);
    directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Down)] = std::make_unique<Keyboard>(VK_DOWN);
    directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Right)] = std::make_unique<Keyboard>(VK_RIGHT);


    inputKeys.clear();

    inputKeys["MouseRight"].emplace_back(std::make_unique<Mouse>(VK_RBUTTON));
    inputKeys["MouseLeft"].emplace_back(std::make_unique<Mouse>(VK_LBUTTON));

    inputKeys["F1"].emplace_back(std::make_unique<Keyboard>(VK_F1));
    inputKeys["F5"].emplace_back(std::make_unique<Keyboard>(VK_F5));
    inputKeys["F6"].emplace_back(std::make_unique<Keyboard>(VK_F6));
    inputKeys["F7"].emplace_back(std::make_unique<Keyboard>(VK_F7));
    inputKeys["F8"].emplace_back(std::make_unique<Keyboard>(VK_F8));
    inputKeys["Alt"].emplace_back(std::make_unique<Keyboard>(VK_MENU));
    inputKeys["Enter"].emplace_back(std::make_unique<Keyboard>(VK_RETURN));
    inputKeys["Shift"].emplace_back(std::make_unique<Keyboard>(VK_SHIFT));

    inputKeys["Ctrl"].emplace_back(std::make_unique<Keyboard>(VK_CONTROL));

    inputKeys["1"].emplace_back(std::make_unique<Keyboard>('1'));
    inputKeys["2"].emplace_back(std::make_unique<Keyboard>('2'));


    inputKeys["Space"].emplace_back(std::make_unique<Keyboard>(VK_SPACE));
    //inputKeys["Space"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_X));

    inputKeys["Up"].emplace_back(std::make_unique<Keyboard>(VK_UP));
    inputKeys["W"].emplace_back(std::make_unique<Keyboard>('W'));
    inputKeys["Left"].emplace_back(std::make_unique<Keyboard>(VK_LEFT));
    inputKeys["A"].emplace_back(std::make_unique<Keyboard>('A'));
    inputKeys["Down"].emplace_back(std::make_unique<Keyboard>(VK_DOWN));
    inputKeys["S"].emplace_back(std::make_unique<Keyboard>('S'));
    inputKeys["Right"].emplace_back(std::make_unique<Keyboard>(VK_RIGHT));
    inputKeys["D"].emplace_back(std::make_unique<Keyboard>('D'));


    inputKeys["W"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_L_UP));     // スティック上
    inputKeys["A"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_L_LEFT));   // スティック左
    inputKeys["S"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_L_DOWN));   // スティック下
    inputKeys["D"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_L_RIGHT));  // スティック右

    inputKeys["Up"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_R_UP));     // スティック上
    inputKeys["Left"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_R_LEFT));   // スティック左
    inputKeys["Down"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_R_DOWN));   // スティック下
    inputKeys["Right"].emplace_back(std::make_unique<Gamepad>(GAMEPAD_R_RIGHT));  // スティック右

    inputKeys["E"].emplace_back(std::make_unique<Keyboard>('E'));
    inputKeys["Q"].emplace_back(std::make_unique<Keyboard>('Q'));
    inputKeys["Z"].emplace_back(std::make_unique<Keyboard>('Z'));
    inputKeys["K"].emplace_back(std::make_unique<Keyboard>('K'));
    inputKeys["R"].emplace_back(std::make_unique<Keyboard>('R'));
    inputKeys["X"].emplace_back(std::make_unique<Keyboard>('X'));
    inputKeys["T"].emplace_back(std::make_unique<Keyboard>('T'));
    inputKeys["Y"].emplace_back(std::make_unique<Keyboard>('Y'));
    inputKeys["Backspace"].emplace_back(std::make_unique<Keyboard>(VK_BACK));
    inputKeys["Backspace"].emplace_back(std::make_unique<Keyboard>(XINPUT_GAMEPAD_B));
    inputKeys["Enter"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));
    inputKeys["1"].emplace_back(std::make_unique<Keyboard>('1'));

    //inputKeys["ok"].emplace_back(std::make_unique<Mouse>(VK_LBUTTON)); // マウス左ボタン
    inputKeys["ok"].emplace_back(std::make_unique<Keyboard>(VK_RETURN));    // Enterキー
    inputKeys["ok"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));
    inputKeys["ok"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_B));


    inputKeys["LockOn"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_THUMB));    // 右スティック押し込み

    inputKeys["Attack"].emplace_back(std::make_unique<Keyboard>('Z'));
    inputKeys["Attack"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_SHOULDER));     // 右

    inputKeys["Dodge"].emplace_back(std::make_unique<Keyboard>(VK_RETURN)); // Enterキー
    inputKeys["Dodge"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_B));     // Bボタン

    //inputKeys["Z"].emplace_back(std::make_unique<Keyboard>('Z'));
    //inputKeys["RB"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_SHOULDER));     // 右
    //inputKeys["RT"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::RightTrigger));
    //inputKeys["LB"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_LEFT_SHOULDER));     // 左
    //inputKeys["LT"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::LeftTrigger));

    inputKeys["BookRight"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_SHOULDER));     // 右
    inputKeys["BookRight"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::RightTrigger));
    inputKeys["BookLeft"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_LEFT_SHOULDER));     // 左
    inputKeys["BookLeft"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::LeftTrigger));

    inputKeys["GamePadA"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));     // 左
    inputKeys["GamePadA"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_B));     // 左

    // 今回のゲームで使用する入力キーをここでまとめて登録しておく
    inputKeys["ScissorsAttack"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::RightTrigger));
    inputKeys["ScissorsAttack"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_SHOULDER));     // 右
    inputKeys["ScissorsAttack"].emplace_back(std::make_unique<Mouse>(VK_RBUTTON)); // マウス右ボタン

    inputKeys["ScissorsAction"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));
    inputKeys["ScissorsAction"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_B));
    inputKeys["ScissorsAction"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_X));
    inputKeys["ScissorsAction"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_Y));
    inputKeys["ScissorsAction"].emplace_back(std::make_unique<Mouse>(VK_LBUTTON)); // マウス左ボタン

    inputKeys["TutorialOk"].emplace_back(std::make_unique<Mouse>(VK_LBUTTON));  // 左ボタン
    inputKeys["TutorialOk"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));     // A
    inputKeys["TutorialOk"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_B));     // A

    inputKeys["DashCancel"].emplace_back(std::make_unique<Mouse>(VK_RBUTTON));  // 右クリック
    inputKeys["DashCancel"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::RightTrigger));
    inputKeys["DashCancel"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_RIGHT_SHOULDER));     // 右
    inputKeys["DashCancel"].emplace_back(std::make_unique<Gamepad>(0, GamePadKeyType::LeftTrigger));
    inputKeys["DashCancel"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_LEFT_SHOULDER));     // 左


    // コントローラーとキーボード対応させる
    inputKeys["UIUp"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_DPAD_UP));
    inputKeys["UIDown"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_DPAD_DOWN));

    // コントローラーとキーボード対応させる
    inputKeys["UILeft"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_DPAD_LEFT));
    inputKeys["UIRight"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_DPAD_RIGHT));


    inputKeys["UIUp"].emplace_back(std::make_unique<Keyboard>(VK_UP));
    inputKeys["UIDown"].emplace_back(std::make_unique<Keyboard>(VK_DOWN));

    inputKeys["UISubmit"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_A));
    inputKeys["UISubmit"].emplace_back(std::make_unique<Keyboard>(VK_RETURN));

    // ポーズ画面
    inputKeys["Pause"].emplace_back(std::make_unique<Keyboard>(VK_TAB));
    inputKeys["Pause"].emplace_back(std::make_unique<Gamepad>(XINPUT_GAMEPAD_START));
}



// 更新処理
void InputSystem::Update(float deltaTime)
{
    // UIがマウスを使用しているかどうかのフラグをリセット
    InputSystem::isUIUsingMouse = false;


    DWORD xinputResult = XInputGetState(static_cast<DWORD>(slot), &state);
    isGamePadConnected = (xinputResult == ERROR_SUCCESS);

    //入力情報の更新
    {
        if (isGamePadConnected)
        {
            //ゲームパッドのAxisLeft更新
            ApplyStickDeadzone(static_cast<float>(state.Gamepad.sThumbLX), static_cast<float>(state.Gamepad.sThumbLY),
                deadZoneMode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),
                mAxis[static_cast<size_t>(Side::Left)][static_cast<size_t>(Axis::X)], mAxis[static_cast<size_t>(Side::Left)][static_cast<size_t>(Axis::Y)]);
            //ゲームパッドのAxisRight更新
            ApplyStickDeadzone(static_cast<float>(state.Gamepad.sThumbRX), static_cast<float>(state.Gamepad.sThumbRY),
                deadZoneMode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),
                mAxis[static_cast<size_t>(Side::Right)][static_cast<size_t>(Axis::X)], mAxis[static_cast<size_t>(Side::Right)][static_cast<size_t>(Axis::Y)]);

            // 振動の更新
            if (vibrationTimer > 0.0f)
            {
                vibrationTimer -= deltaTime;

                WORD motor = static_cast<WORD>(vibrationPower * 65535.0f);

                XINPUT_VIBRATION vibration = {};
                vibration.wLeftMotorSpeed = motor;   // 低周波（重い）
                vibration.wRightMotorSpeed = motor;  // 高周波（軽い）

                XInputSetState(slot, &vibration);
            }
            else
            {
                // 停止
                XINPUT_VIBRATION vibration = {};
                vibration.wLeftMotorSpeed = 0;
                vibration.wRightMotorSpeed = 0;

                XInputSetState(slot, &vibration);
            }
        }
        else
        {
            //移動キー更新処理
            for (auto& keys : directionKeys) {
                for (auto& key : keys) {
                    key->Update(deltaTime);
                }
            }

            //値更新
            ApplyStickDeadzone(
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Right)]->IsPressed()) -
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Left)]->IsPressed()),
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Up)]->IsPressed()) -
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Left)][static_cast<size_t>(Direction::Down)]->IsPressed()),
                deadZoneMode, 1.f, 0.f,
                mAxis[static_cast<size_t>(Side::Left)][static_cast<size_t>(Axis::X)], mAxis[static_cast<size_t>(Side::Left)][static_cast<size_t>(Axis::Y)]);

            ApplyStickDeadzone(
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Right)]->IsPressed()) -
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Left)]->IsPressed()),
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Up)]->IsPressed()) -
                static_cast<float>(directionKeys[static_cast<size_t>(Side::Right)][static_cast<size_t>(Direction::Down)]->IsPressed()),
                deadZoneMode, 1.f, 0.f,
                mAxis[static_cast<size_t>(Side::Right)][static_cast<size_t>(Axis::X)], mAxis[static_cast<size_t>(Side::Right)][static_cast<size_t>(Axis::Y)]);
        }
        //ボタンの入力更新処理
        for (auto& actionKeys : inputKeys) {
            for (auto& key : actionKeys.second) {
                key->Update(deltaTime);
            }
        }

    }
    // カーソル位置の取得
    POINT cursor;
    ::GetCursorPos(&cursor);
    ScreenToClient(Graphics::GetWindowHandle(), &cursor);

    // マウス座標更新
    mousePositionX[1] = mousePositionX[0];
    mousePositionY[1] = mousePositionY[0];
    mousePositionX[0] = (LONG)cursor.x;
    mousePositionY[0] = (LONG)cursor.y;

    //アクティブデバイス判定

#if 0
    //コントローラー
    {
        auto buttons = state.Gamepad.wButtons;
        auto lx = state.Gamepad.sThumbLX;
        auto ly = state.Gamepad.sThumbLY;
        // ボタンが押された or スティックが動いたらアクティブデバイスを切り替え
        if (buttons != 0 || abs(lx) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || abs(ly) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
            activeDevice = InputDeviceType::Gamepad;
        }
    }
    //キーボード
    {
        for (int vk = 0x08; vk <= 0xFE; ++vk) {
            if (GetAsyncKeyState(vk) & 0x8000) {
                activeDevice = InputDeviceType::Keyboard;
            }
        }
    }
    //マウス
    {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000 ||
            GetAsyncKeyState(VK_RBUTTON) & 0x8000 ||
            mousePositionX[0] != mousePositionX[1] ||
            mousePositionY[0] != mousePositionY[1]) {
            activeDevice = InputDeviceType::Mouse;
        }
    }
#else
    // 1. Gamepad
    {
        auto buttons = state.Gamepad.wButtons;
        auto lx = state.Gamepad.sThumbLX;
        auto ly = state.Gamepad.sThumbLY;

        if (buttons != 0 ||
            abs(lx) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            abs(ly) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
        {
            activeDevice = InputDeviceType::Gamepad;
        }
    }

    // 2. Keyboard（Updateの中で検知済みなのでスキャンしない）

    // ※キー更新中にこれを入れれば良い：
    // if(key->GetDeviceType()==Keyboard && key->IsPressedRaw()) activeDevice = Keyboard;

    // 3. Mouse
    {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000 ||
            GetAsyncKeyState(VK_RBUTTON) & 0x8000 ||
            mousePositionX[0] != mousePositionX[1] ||
            mousePositionY[0] != mousePositionY[1])
        {
            activeDevice = InputDeviceType::Mouse;
        }
    }


#endif // 0
}

bool InputSystem::GetInputState(const std::string& action, InputStateMask state, DeviceFlags flag)
{
    if (!inputEnabled)
    {// 入力が無効だったら、
        return false;
    }

    auto it = inputKeys.find(action);
    if (it != inputKeys.end())
    {
        const auto& keys = it->second;
        for (auto& key : keys) {
            switch (flag)
            {
            case DeviceFlags::KeyboardOnly:
                if (key->GetDeviceType() != InputDeviceType::Keyboard) continue;
                break;
            case DeviceFlags::MouseOnly:
                if (key->GetDeviceType() != InputDeviceType::Mouse) continue;
                break;
            case DeviceFlags::GamePadOnly:
                if (key->GetDeviceType() != InputDeviceType::Gamepad) continue;
                break;
            case DeviceFlags::KeyboardAndMouse:
                if (key->GetDeviceType() == InputDeviceType::Gamepad) continue;
                break;
            case DeviceFlags::KeyboardAndGamePad:
                if (key->GetDeviceType() == InputDeviceType::Mouse) continue;
                break;
            case DeviceFlags::MouseAndGamePad:
                if (key->GetDeviceType() == InputDeviceType::Keyboard) continue;
                break;
            }
            switch (state)
            {
            case InputStateMask::Trigger:
                if (key->IsTrigger()) return true;
                break;
            case InputStateMask::Release:
                if (key->IsRelease()) return true;
                break;
            default:
                if (key->IsPressed()) return true;
                break;
            }
        }
    }
    return false;
}