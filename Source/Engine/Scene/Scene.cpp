#include "pch.h"
#include "Scene.h"

#include <chrono>

#include "Engine/Effects/EffectManager.h"
#include "Graphics/Resource/Texture.h"
#include "UI/Game/SceneTransitionManager.h"

bool Scene::_update(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    if (_current_scene->uiManager)
    {
        _current_scene->uiManager->Update(deltaTime);
    }


    // 現在のActorManagerの更新処理
    if (_current_scene->actorManager_)
    {
        _current_scene->actorManager_->Update(deltaTime);
    }
    // 現在のシーンの更新処理
    _current_scene->Update(deltaTime);
    // シーンを切り替える場合にレンダリングをスキップするためのフラグ
    bool skipRendering = false;
    // 次のシーンが設定されている場合、シーンを切り替える
    if (_next_scene)
    {
        // Direct3D11 のデバイスオブジェクトを取得
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        immediateContext->GetDevice(device.GetAddressOf());


        // 現在のシーンを「終了処理中」に変更
        _current_scene->State(SCENE_STATE::uninitializing);

        // actorManager の破棄処理
        if (_current_scene->actorManager_)
        {
            _current_scene->actorManager_->ClearAll();
        }
        // 現在のシーンの後処理
        _current_scene->Uninitialize(device.Get());
        // 現在のシーンを「終了済み」に変更
        _current_scene->State(SCENE_STATE::uninitialized);

        // プリロード済みのシーンがある場合、次のシーンにセット
        if (_preload_scene)
        {
            _next_scene = std::move(_preload_scene);
        }

        // 次のシーンがまだ初期化されていない場合、初期化を行う
        if (_next_scene->State() < SCENE_STATE::initializing)
        {
            // 現在のレンダリングのビューポートサイズを取得
            D3D11_VIEWPORT viewport;
            UINT numViewports{ 1 };
            immediateContext->RSGetViewports(&numViewports, &viewport);

            // シーンの状態を「初期化中」に変更
            _next_scene->State(SCENE_STATE::initializing);
            // ActorManager の生成
            _next_scene->actorManager_ = std::make_unique<ActorManager>();
            _next_scene->actorManager_->SetOwnerScene(_next_scene.get());
            // シーンの初期化処理（デバイス、画面サイズ、プロパティ情報を渡す）
            _next_scene->Initialize(device.Get(), static_cast<UINT64>(viewport.Width), static_cast<UINT64>(viewport.Height), _payload);
            // シーンの状態を「初期化済み」に変更
            _next_scene->State(SCENE_STATE::initialized);
        }
        // シーンを「アクティブ」に設定
        _next_scene->State(SCENE_STATE::active);
        // 現在のシーンを次のシーンに切り替え
        _current_scene = std::move(_next_scene);
        // シーン変更時にペイロードをクリア（次のシーンには影響しない）
        _payload.clear();
        //シーンのスタート
        _current_scene->Start();
        // シーンが切り替わったため、レンダリングをスキップ
        skipRendering = true;
    }
    return skipRendering;
}

bool Scene::_async_preload_scene(ID3D11Device* device, UINT64 width, UINT height, const std::string& name)
{
    // 引数が適切かチェック（空の名前や無効なサイズはエラー）
    _ASSERT_EXPR(name.size() > 0 && width > 0 && height > 0, L"Invalid Argument");

    // すでにプリロード中のシーンがあるか、非同期処理が実行中ならプリロードしない
    if (_preload_scene || _future.valid())
    {
        return false;
    }

    // シーンが登録済みであることを確認（なければエラー）
    _ASSERT_EXPR(_reflections().find(name) != _reflections().end(), L"Not found scene name in _alchemists");

    // 新しいプリロード用のシーンを作成
    _preload_scene = _reflections().at(name)();

    // まだ非同期処理（future）が開始されていない場合
    if (!_future.valid())
    {
        // プリロード用シーンの状態が「待機中（awaiting）」なら非同期でロード開始
        if (_preload_scene->State() == SCENE_STATE::awaiting)
        {
            _future = std::async(std::launch::async, [device, name, width, height]() {
                // ActorManager の生成
                _preload_scene->actorManager_ = std::make_unique<ActorManager>();
                _preload_scene->actorManager_->SetOwnerScene(_preload_scene.get());
                _preload_scene->State(SCENE_STATE::initializing);// 状態を「初期化中」に設定
                bool success = _preload_scene->Initialize(device, width, height, {});
                _preload_scene->State(SCENE_STATE::initialized);// 状態を「初期化完了」に設定
                return success;
                });
        }
    }
    // シーンの状態が「初期化開始」より進んでいれば true
    return _preload_scene->State() > SCENE_STATE::initializing;
}


