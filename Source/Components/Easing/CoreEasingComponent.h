#pragma once
#include "Components/Base/SceneComponent.h"
#include "Engine/Easing/TestEasingHandler.h"

/**
 * @file
 * @brief イージングを使って任意プロパティを時間的に補間するコンポーネント。
 * @details `EasingHandler` と `PropertyAccessor` を組み合わせ、任意の float プロパティを
 *          直列シーケンスで補間できます。インスペクタ描画や非スケール時間の使用切替にも対応。
 */

template<typename T>
struct PropertyAccessor
{
	/** @brief 値の取得関数。*/
	std::function<T()> getter;      // 値を読む
	/** @brief 値の設定関数。*/
	std::function<void(T)> setter;  // 値を書き込む
};

class CoreEasingComponent :public SceneComponent
{
public:
	CoreEasingComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

	virtual ~CoreEasingComponent() = default;

	/**
	 * @brief イージングハンドラを開始します。
	 * @param handler 実行する `EasingHandler`。
	 * @param accessor 対象プロパティのゲッター/セッター。
	 */
	void StartHandler(const TestEasingHandler& handler, PropertyAccessor<float> accessor)
    {
		handlers.push_back(std::make_pair(accessor, handler));
	}
	/** @brief 全てのハンドラをクリアします。*/
	void Clear()
	{
		handlers.clear();
	}

	/** @brief 全てのハンドラが完了したかを返します。*/
	bool IsAllCompleted() const
	{
		return handlers.empty();
	}

	/**
	 * @brief 毎フレーム更新。
	 * @param deltaTime 経過時間（秒）。
	 */
	void Tick(float deltaTime) override;

	/** @brief インスペクタ用のプロパティ描画。*/
	void DrawImGuiInspector() override;

protected:
	/** @brief デバッグ/GUI 用のイージング要素（内部管理）。*/
	std::vector<std::pair<int, TestEasingHandler::EaseItem>> easeItems;

	/** @brief 実行中ハンドラの集合（プロパティアクセサとペア）。*/
	std::vector<std::pair<PropertyAccessor<float>, TestEasingHandler>> handlers;

	/** @brief 非スケール時間を使用するか。*/
	bool useUnscaledTime = true;

public:
	/** @brief 非スケール時間を使用するかを返します。*/
	bool UsesUnscaledTime() const { return useUnscaledTime; }
	/** @brief テスト用プロパティ。*/
	std::string test = "test";
};


// Actorにつけなくても使えるeasingComponent
class EasingRunner
{
public:
	void StartHandler(const TestEasingHandler& handler, PropertyAccessor<float> accessor)
	{
		handlers.emplace_back(accessor, handler);
	}

	void Tick(float deltaTime)
	{
		for (auto& [accessor, handler] : handlers)
		{
			if (!handler.IsCompleted())
			{
				float value = accessor.getter ? accessor.getter() : 0.0f;
				handler.Update(value, deltaTime);
				if (accessor.setter)
					accessor.setter(value);
			}
		}

		handlers.erase(
			std::remove_if(handlers.begin(), handlers.end(),
				[](auto& h) { return h.second.IsCompleted(); }),
			handlers.end());
	}

	void Clear()
	{
		handlers.clear();
	}

	bool IsFinished() const { return handlers.empty(); }

private:
	std::vector<std::pair<PropertyAccessor<float>, TestEasingHandler>> handlers;
};