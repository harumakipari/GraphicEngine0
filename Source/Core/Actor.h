#pragma once

// C++ 標準ライブラリ
#include <string>
#include <DirectXMath.h>
#include <memory>
#include<assert.h>

#include "Components/Base/Component.h"
#include "Components/Base/SceneComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Transform/Transform.h"
#include "Engine/Utility/Win32Utils.h"
#include "Math/MathHelper.h"

class Scene;

class Actor :public std::enable_shared_from_this <Actor>
{
public:
	using HitCallback = std::function<void(std::pair<CollisionComponent*, CollisionComponent*>)>;

public:
	Actor() = default;
	Actor(const std::string& actorName) :actorName(actorName) {}
	virtual ~Actor() = default;

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;
	Actor(Actor&&) noexcept = delete;
	Actor& operator=(Actor&&) noexcept = delete;

	// Lifecycle
	virtual void Initialize(const Transform& transform) {}

	virtual void Update(float deltaTime) {}

	virtual void Finalize() {}

	// 全Componentを破棄してActorを終了状態にする。Manager一覧からの除去はManager側が行う。
	void DestroyActor();

	// ActorManager::Update() がこの Actor の削除予約を次に確認した時点で破棄する。
	// 同じ Update() 内でも、この Actor の処理開始前ならその回で破棄される。
	void MarkPendingKill() { isPendingKill = true; }

	// Component Management
	template <class T>
	std::shared_ptr<T> AddComponent(const std::string& name, const std::string& parentName = "")
	{
		// 自分自身が shared_ptr で管理されている前提で、それを渡す
		std::shared_ptr<Actor> sharedThis = shared_from_this(); // Actorは std::enable_shared_from_this 継承が必要
		// 自分自身の確認
		_ASSERT_EXPR(sharedThis != nullptr, "shared_from_this() returned nullptr!");

		std::string finalName = name;
		int suffix = 1;

		auto nameExists = [&](const std::string& checkName)
			{
				return std::any_of(
					ownedComponents.begin(),
					ownedComponents.end(),
					[&](const std::shared_ptr<Component>& comp)
					{
						return comp->GetName() == checkName;
					});
			};

		while (nameExists(finalName))
		{
			finalName = name + "_" + std::to_string(suffix++);
		}

		std::shared_ptr<T> newComponent = std::make_shared<T>(finalName, sharedThis);
		_ASSERT_EXPR(newComponent != nullptr, "Failed to create new SceneComponent!");

		if constexpr ((std::is_base_of<SceneComponent, T>::value))
		{
			std::shared_ptr<SceneComponent> sceneComponent = std::dynamic_pointer_cast<SceneComponent>(newComponent);
			_ASSERT_EXPR(sceneComponent != nullptr, "Dynamic cast to SceneComponent failed!");
			if (parentName.empty())
			{
				if (rootComponent)
				{
					sceneComponent->AttachTo(rootComponent);
				}
				else
				{
					SetRootComponent(sceneComponent);
				}
			}
			else
			{
				std::shared_ptr<SceneComponent> parent = std::dynamic_pointer_cast<SceneComponent>(FindComponentByName(parentName));
				sceneComponent->AttachTo(parent);
			}
		}
		ownedComponents.push_back(std::static_pointer_cast<Component>(newComponent));
		// 初期化する
		newComponent->OnRegister();
		return newComponent;
	}


	// 名前が空ならRootComponentを返す。見つからない場合はnullptr。
	std::shared_ptr<Component> FindComponentByName(const std::string& name);

	void DestroyComponentByName(const std::string& name);

	template<typename T>
	T* GetComponent()
	{
		// SceneComponent から探す
		for (const std::shared_ptr<Component>& compent : ownedComponents)
		{
			T* casted = dynamic_cast<T*>(compent.get());
			if (casted != nullptr)
			{
				return casted;
			}
		}
		_ASSERT(L"Actor の GetComponent が nullptr を返しています。");
		return nullptr;
	}


	template<class T>
	void GetComponents(std::vector<T*>& components)
	{
		components.clear();
		for (auto component : ownedComponents)
		{
			if (T* downCastComponent = dynamic_cast<T*>(component.get()))
			{
				components.push_back(downCastComponent);
			}
		}

	}

	// Actor が所有する全 Component を取得する（読み取り専用）
	const std::vector<std::shared_ptr<Component>>& GetComponents() const
	{
		return ownedComponents;
	}

	// PhysXのsimulate中にShapeを破棄しないよう、削除を後段へ予約する。 
	void RequestDestroyComponent(const std::string& name)
	{
		pendingDestroyComponentNames.push_back(name);
	}

	// Update後に、予約されたComponentの削除を実行する。 
	void PostDestroyComponents()
	{
		for (const auto& name : pendingDestroyComponentNames)
		{
			DestroyComponentByName(name);
		}
		pendingDestroyComponentNames.clear();
	}

	void SetRootComponent(const std::shared_ptr<SceneComponent>& root)
	{
		rootComponent = root;
	}

	std::shared_ptr<SceneComponent> GetRootComponent()
	{
		return rootComponent;
	}


	DirectX::XMFLOAT4X4 GetWorldTransform() const
	{
		DirectX::XMFLOAT4X4 worldTransform;
		DirectX::XMMATRIX M = rootComponent ? rootComponent->GetFinalWorldTransform().ToMatrix() : DirectX::XMMatrixIdentity();
		DirectX::XMStoreFloat4x4(&worldTransform, M);
		return worldTransform;
	}

	DirectX::XMFLOAT3 GetPosition() const
	{
		if (rootComponent)
		{
			return rootComponent->GetRelativeLocation();
		}
		return { 0.0f,0.0f,0.0f };
	}

	void SetPosition(const DirectX::XMFLOAT3& position) const
	{
		this->rootComponent->SetRelativeLocationDirect(position);
		UpdateAllComponentTransforms();
	}

	const DirectX::XMFLOAT4& GetQuaternionRotation() const { return rootComponent->GetRelativeRotation(); }

	void SetQuaternionRotation(const DirectX::XMFLOAT4& rotation) const
	{
		// 明示的に検査を追加
		_ASSERT_EXPR(MathHelper::IsValidQuaternion(rotation), L"SetQuaternionRotation: Invalid quaternion");
		this->rootComponent->SetRelativeRotationDirect(rotation);
	}

	// degrees で返す
	DirectX::XMFLOAT3 GetEulerRotation() const
	{
		return rootComponent->GetRelativeEulerRotation();
	}

	void SetEulerRotation(const DirectX::XMFLOAT3& eulerRotation) const
	{
		this->rootComponent->SetRelativeEulerRotationDirect(eulerRotation);
	}

	DirectX::XMFLOAT3 GetScale() const { return rootComponent->GetRelativeScale(); }

	void SetScale(const DirectX::XMFLOAT3& scale) const
	{
		this->rootComponent->SetRelativeScaleDirect(scale);
	}

	//進行方向の単位ベクトルを取得する
	DirectX::XMFLOAT3 GetForward() const
	{
		DirectX::XMFLOAT3  front;
		// Z軸方向の単位方向ベクトル　デフォルト
		DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0, 0, 1, 0);
		//playerの回転値によって作られる回転行列
		DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&GetQuaternionRotation()));
		// デフォルトのベクトルに回転行列を適応する
		DirectX::XMVECTOR TransformedForward = DirectX::XMVector3TransformNormal(DefaultForward, RotationMatrix);
		TransformedForward = DirectX::XMVector3Normalize(TransformedForward);
		DirectX::XMStoreFloat3(&front, TransformedForward);
		return front;
	}


	// State
	void SetActive(bool isActive)
	{
		this->isActive = isActive;
	}

	bool IsActive() const
	{
		return isActive;
	}

	bool IsAlive() const
	{
		return this->isAlive;
	}

	bool IsPendingKill() const
	{
		return this->isPendingKill;
	}

	// Collision / Event
	//　collisionComponent　が Dynamic の物と当たった時に通る
	virtual void NotifyHit(Actor* otherActor, const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& impulse) {}
	//　collisionComponent　が Dynamic の物と当たった時に通る
	virtual void NotifyHit(CollisionComponent* selfComp, CollisionComponent* otherComp, Actor* otherActor, const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& impulse) {}


	// firstは自身のCollisionComponent、secondは相手側。
	void DispatchHitEvent(std::pair<CollisionComponent*, CollisionComponent*> hitPair) const
	{
		for (auto& callback : hitCallbacks)
		{
			callback(hitPair);
		}
	}

	//キネマティック同士の衝突時に呼ばれる仮想関数
	//派生クラスでオーバーライドして使用する。
	virtual void OnHit(std::pair<CollisionComponent*, CollisionComponent*> hitPair) {}

	void AddHitCallback(const HitCallback& callback)
	{
		hitCallbacks.push_back(callback);
	}

	void RemoveAllHitCallBacks()
	{
		hitCallbacks.clear();
	}

	// Debug
	void DrawImGuiInspector();

	// 継承押したサブクラスの専用GUI
	virtual void DrawImGuiDetails() {};

	// Scene
	void SetOwnerScene(Scene* scene) { ownerScene = scene; }
	Scene* GetOwnerScene() const { return ownerScene; }
	const Scene* GetOwnerConstScene() const { return ownerScene; }


	void MakeRootComponent()
	{
		rootComponent = AddComponent<SceneComponent>(rootComponentName);
	}

	std::string& GetName() { return actorName; }

	// 初期化後に呼び、所有ComponentのWorld Transformを更新する。
	void UpdateAllComponentTransforms() const
	{
		if (rootComponent)
		{
			rootComponent->UpdateComponentToWorld();
		}

		for (const auto& comp : ownedComponents)
		{
			if (comp == rootComponent)
				continue;

			comp->UpdateComponentToWorld();
		}
	}

	// タグを追加する
	void AddTag(const std::string& tag)
	{
		tags.push_back(tag);
	}
	// タグを持っているか
	bool HasTag(const std::string& tag) const
	{
		return std::find(tags.begin(), tags.end(), tag) != tags.end();
	}

	// タイムスケールを取得する
	float GetTimeScale() const { return timeScale; }

	// タイムスケールをリセットする
	void ResetTimeScale() { timeScale = 1.0f; }

	// タイムスケールを設定する
	void SetTimeScale(const float timeScale) { this->timeScale = timeScale; }

	// ルートコンポーネントの名前を取得する
	const std::string& GetRootComponentName()const { return rootComponentName; }
protected:
	Scene* ownerScene = nullptr;   // 自分が属しているScene
	std::vector<std::string> tags;

private:
	// Internal Data
	std::string actorName;
	std::string rootComponentName = "RootComponent";
	bool isActive = true;	// アクターがアクティブかどうか
	bool isAlive = true;	// アクターが存在しているかどうか
	bool isPendingKill = false;		// アクターの削除予約
	std::vector<HitCallback> hitCallbacks;
	// rootComponent (Transform) 系
	std::shared_ptr<SceneComponent> rootComponent;
	// Component
	std::vector<std::shared_ptr<Component>> ownedComponents;
	// 名前とコンポーネントをキャッシュしておく
	std::unordered_map<std::string, std::shared_ptr<Component>> componentsByName;
	// 削除予約用リスト　
	std::vector<std::string> pendingDestroyComponentNames;
	float timeScale = 1.0f;		// アクターごとのタイムスケール
};

static inline bool operator==(const std::shared_ptr<Actor>& actor, const std::string& name)
{
	return actor->GetName() == name;
}

static inline bool operator==(const std::shared_ptr<SceneComponent>& component, const std::string& name)
{
	return component->GetName() == name;
}

static inline bool operator==(const std::shared_ptr<Component>& component, const std::string& name)
{
	return component->GetName() == name;
}
