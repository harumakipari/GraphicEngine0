#pragma once

#include <d3d11_1.h>
#include <wrl.h>

#include <unordered_map>
#include <string>
#include <future>

#include <functional>

#include "SceneSetting.h"
#include "Graphics/Resource/Texture.h"


#include "Core/ActorManager.h"
#include "Engine/Camera/CameraManager.h"
#include "UI/UIManager.h"
#include "UI/Game/SceneTransitionManager.h"

//シーンの状態を表す列挙型
enum class SCENE_STATE
{
    awaiting,			// 待機中
    initializing,		// 初期化中
    initialized,		// 初期化完了
    active,				// アクティブ
    uninitializing,		// 終了処理中
    uninitialized		// 未初期化
};

// シーンクラスの定義
class Scene
{
public:
    Scene() = default;
    virtual ~Scene()=default;
    Scene(const Scene&) = delete;
    Scene& operator =(const Scene&) = delete;
    Scene(Scene&&) noexcept = delete;
    Scene& operator =(Scene&&) noexcept = delete;

    // シーンの現在の状態を取得
    SCENE_STATE State() const
    {
        return state_;
    }

    // Scene* scene = Scene::GetCurrrentScene()

    // 現在のシーンへのアクセス関数
    static Scene* GetCurrentScene()
    {
        return _current_scene.get();
    }

    // ActorManager の取得
    const ActorManager* GetActorManager() const { return actorManager_.get(); }
    ActorManager* GetActorManager() { return actorManager_.get(); }
    const UIManager* GetUIManager() const { return uiManager.get(); }
    UIManager* GetUIManager() { return uiManager.get(); }

    CameraManager* GetCameraManager() const { return cameraManager.get(); }
    const CameraManager* GetCameraManager() { return cameraManager.get(); }

    void ClearActorManager() const
    {
        actorManager_->ClearAll();
    }

    bool IsPaused() const { return paused; }

    void SetPaused(const bool p) { paused = p; }

    void SetActiveCamera(const std::shared_ptr<Camera>& camera)
    {
        activeCamera = camera;
    }

    Camera* GetActiveCamera()const
    {
        if (activeCamera.expired())
        {
            Logger::Error(Logger::LogCategory::System, U8("シーンのアクティブカメラが nullptr を返しています！"));
            return nullptr;
        }
        return activeCamera.lock().get();
    }
    //std::unique_ptr<ActorManager>& GetActorManager() { return actorManager_; }
private:
    // 純粋仮想関数：シーンの初期化
    virtual bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) = 0;
    //シーンが始まったとき
    virtual void Start() {};
    // 純粋仮想関数：シーンの更新
    virtual void Update(float delta_time) = 0;
    // 純粋仮想関数：シーンの描画
    virtual void Render(ID3D11DeviceContext* immediate_context, float delta_time) = 0;
    // 仮想関数：シーンの終了処理
    virtual bool Uninitialize(ID3D11Device* device)
    {
        ReleaseAllTextures();
        bool completelyRegenerate = true;
        return completelyRegenerate;
    }
    // 仮想関数：ウィンドウサイズ変更時の処理
    virtual bool OnSizeChanged(ID3D11Device* device, UINT64 width, UINT height)
    {
        return true;
    }
    // シーンの状態を管理するための変数
    std::atomic<SCENE_STATE> state_ = SCENE_STATE::awaiting;
    // シーンの状態を設定
    void State(SCENE_STATE state)
    {
        state_ = state;
    }


    virtual void DrawGui() {}

protected:
    virtual void SetUpActors() {}
public:
    // テンプレート関数：シーンの初期化
    //template<class _boot_scene>
    static bool _boot(ID3D11Device* device, std::string name, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
    {
        //シーンが _reflections() に登録されているかチェック
        _ASSERT_EXPR(_reflections().find(name) != _reflections().end(), L"Not found scene name in reflections");
        //登録されているシーンの ファクトリ関数（std::function） を実行して std::unique_ptr<scene> を取得。
        _current_scene = _reflections().at(name)();
        _current_scene->State(SCENE_STATE::initializing);
        // ActorManager の生成
        _current_scene->actorManager_ = std::make_unique<ActorManager>();
        _current_scene->actorManager_->SetOwnerScene(_current_scene.get());

        _current_scene->Initialize(device, width, height, props);
        if (!_current_scene->GetActorManager())
        {
            OutputDebugStringA("actorManager_ is nullptr immediately after creation!\n");
        }
        else
        {
            OutputDebugStringA("actorManager_ is properly created.\n");
        }
        _current_scene->State(SCENE_STATE::initialized);
        _current_scene->State(SCENE_STATE::active);
        _current_scene->Start();

        //SceneTransitionManager::Instance().NotifySceneChanged();

        return true;
    }
    // ウィンドウサイズ変更時の処理
    static bool _on_size_changed(ID3D11Device* device, UINT64 width, UINT height)
    {
        return _current_scene->OnSizeChanged(device, width, height);
    }

    // 参照を返す
    SceneSettings& GetSceneSettings()  { return settings; }

    // シーンのライト設定などを設定する
    void SetSceneSettings(const SceneSettings& settings) { this->settings = settings; }
private:
    // シーンの更新
    static bool _update(ID3D11DeviceContext* immediateContext, float deltaTime);
    // シーンの描画
    static void _render(ID3D11DeviceContext* immediateContext, float deltaTime)
    {
        _current_scene->Render(immediateContext, deltaTime);
    }
    // GUIの描画
    static void _drawGUI()
    {
        //if (_current_scene->actorManager_)
        //{
        //    _current_scene->actorManager_->DrawImGuiAllActors();
        //}
        _current_scene->DrawGui();
    }

    // シーンの終了処理
    static bool _uninitialize(ID3D11Device* device)
    {
        // 非同期処理（future）が有効なら、完了を待つ
        if (_future.valid())
        {
            _future.wait();
        }
        // actorManager の破棄処理
        if (_current_scene->actorManager_)
        {
            _current_scene->actorManager_->ClearAll();
        }
        // 現在のシーンの後処理を実行
        _current_scene->Uninitialize(device);
        // 現在のシーンをリセット（nullptrにする）
        _current_scene.reset();

        return true;
    }
    // テンプレート関数：シーンの登録
    template<class T>
    static std::string _enroll()
    {
        // シーンのクラス名を取得（型情報から文字列に変換）
        std::string className = typeid(T).name();
        // 名前空間などが含まれている場合、最後の部分のみを取得
        className = className.substr(className.find_last_of(" ") + 1, className.length());
        // すでに登録済みのクラスでないことを確認
        _ASSERT_EXPR(_reflections().find(className) == _reflections().end(), L"'reflections' already has a scene with 'className'");
        // シーンの生成関数を登録（ファクトリー関数を保存）
        _reflections().emplace(std::make_pair(className, []() {return std::make_unique<T>(); }));
        return className;//登録したクラス名を返す
    }
    // Framework クラスを friend に指定（Framework から private/protected にアクセスできる）
    friend class Framework;

protected:
    // シーンのプリロードが完了しているか確認
    static bool _has_finished_preloading()
    {
        // プリロード中のシーンがあり、かつ非同期処理（future）が有効ならチェック
        if (_preload_scene && _future.valid())
        {
            // future の状態を非同期で確認（すぐに結果が得られるか）
            if (_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                // プリロードシーンの状態が初期化完了していれば成功
                if (_preload_scene->State() >= SCENE_STATE::initialized)
                {
                    bool success = _future.get();// future の結果を取得（取得しないと future は無効化できない）
                    return true;
                }
            }
            return false;
        }
        // プリロードシーンがない、またはすでに準備完了なら true
        return true;
    }
public:
    // シーンの遷移
    static bool _transition(const std::string& name, const std::unordered_map<std::string, std::string>& props)
    {
        //非同期処理が完了しているか確認（未完了ならシーン遷移しない）
        if (!_async_wait())
        {
            return false;
        }
        // シーン名が登録されているか確認（登録されていないとエラー）
        _ASSERT_EXPR(_reflections().find(name) != _reflections().end(), L"Not found scene name in reflections");
        // 次に遷移するシーンを設定（ファクトリー関数で新しいシーンを作成）
        _next_scene = _reflections().at(name)();
        // シーンに渡すデータ（プロパティ）を設定
        _payload = props;

        return true;// シーン遷移が成功
    }
    // 非同期でシーンをプリロード
    static bool _async_preload_scene(ID3D11Device* device, UINT64 width, UINT height, const std::string& name);


    static std::vector<std::string> GetRegisteredSceneNames()
    {
        std::vector<std::string> names;
        for (auto& [name, _] : _reflections())
        {
            names.push_back(name);
        }
        return names;
    }

    static const std::string& GetCurrentSceneName()
    {
        static std::string empty;
        if (_current_scene)
            return typeid(*_current_scene).name();
        return empty;
    }
private:
    // 非同期処理の完了を待機
    static bool _async_wait()
    {
        // もし非同期処理（future）が有効なら処理を待つ
        if (_future.valid())
        {
            // `wait_for(0秒)` で非同期処理が完了したか確認
            if (_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                bool success = _future.get();// 非同期処理の結果（true/false）を取得（取得しないと future は無効化できない）
                // 初期化が失敗していたらエラーを出す
                _ASSERT_EXPR(success, L"The scene initioalization process using asynchronous processing hasa failed.");
            }
            else
            {
                return false;// 非同期処理がまだ完了していない
            }
        }
        return true;// 非同期処理が終わっている
    }


    //The compiler version must be set to C++20 or higher to use the reserved word 'static inline'.
    // 「シーン名 → シーン生成関数」 のマップにアクセスできる関数
    static inline std::unordered_map<std::string, std::function<std::unique_ptr<Scene>()>>& _reflections()
    {
        static std::unordered_map<std::string, std::function<std::unique_ptr<Scene>()>> reflections;
        return reflections;
    }


    static inline std::unique_ptr<Scene> _next_scene;//次のシーン
    static inline std::unique_ptr<Scene> _current_scene;//現在のシーン
    static inline std::unique_ptr<Scene> _preload_scene;//プリロード済みのシーン
    // 非同期でシーンをロードするための future
    static inline std::future<bool> _future;
    // シーンに渡す追加情報（キーと値のマップ）
    static inline std::unordered_map<std::string, std::string> _payload;

protected:
    // 注意: 静的初期化順序の問題を回避するための仕組み
    //CAUSION: Static Initialization Order Fiasco
    template<class T>
    struct Autoenrollment
    {//Scene::_reflections() に 自動的に登録
        Autoenrollment()
        {
            // コンストラクタで自動的にシーンを登録
            Scene::_enroll<T>();
        }
    };

private:
    std::unique_ptr<ActorManager> actorManager_;
    bool paused = false;
protected:
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<CameraManager> cameraManager;
    std::weak_ptr<Camera> activeCamera;
    SceneSettings settings; // 常に一つ
};


