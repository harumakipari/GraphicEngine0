#pragma once
#include "Components/Base/SceneComponent.h"
#include "Engine/Easing/EasingHandler.h"


template<typename T>
struct PropertyAccessor
{
	std::function<T()> getter;      
	std::function<void(T)> setter;  
};

class EasingComponent :public SceneComponent
{
public:
	EasingComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

	virtual ~EasingComponent() = default;

    // イージングハンドラを開始します。
	void StartHandler(const EasingHandler& handler, PropertyAccessor<float> accessor)
    {
		handlers.push_back(std::make_pair(accessor, handler));
	}

	void Clear()
	{
		handlers.clear();
	}

	bool IsAllCompleted() const
	{
		return handlers.empty();
	}

	void Tick(float deltaTime) override;

	void DrawImGuiInspector() override;

protected:
	// デバッグ/GUI 用のイージング要素（内部管理）
	std::vector<std::pair<int, EasingHandler::EaseItem>> easeItems;

	// 実行中ハンドラの集合（プロパティアクセサとペア）
	std::vector<std::pair<PropertyAccessor<float>, EasingHandler>> handlers;

	bool useUnscaledTime = true;

public:
	bool UsesUnscaledTime() const { return useUnscaledTime; }
};


// Actorにつけなくても使えるeasingComponent
class EasingRunner
{
public:
	void StartHandler(const EasingHandler& handler, PropertyAccessor<float> accessor)
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
	std::vector<std::pair<PropertyAccessor<float>, EasingHandler>> handlers;
};