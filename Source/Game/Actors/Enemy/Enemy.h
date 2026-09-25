#pragma once
#include "Game/Actors/Base/Character.h"

struct ParticleSystem;

enum class MessageType :int
{
    MsgCallHelp,
    MsgChangeAttackRight,
    MsgGiveAttackRight,
    MsgDontGiveAttackRight,
    MsgAskAttackRight,
};

class Telegram
{
public:
    int sender;
    int receiver;
    MessageType msg;

    //コピーコンストラクタとコピー代入演算子を禁止にする
    Telegram(const Telegram&) = delete;
    Telegram& operator=(const Telegram&) = delete;

    Telegram(int sender, int receiver, MessageType msg) :sender(sender), receiver(receiver), msg(msg)
    {
    }
};

class Enemy :public Character
{
public:
    Enemy() = default;
    ~Enemy() override {}

    Enemy(const std::string& modelName) :Character(modelName)
    {
    }

    //コピーコンストラクタとコピー代入演算子を禁止にする
    Enemy(const Enemy&) = delete;
    Enemy& operator=(const Enemy&) = delete;

    //void Update(float elapsedTime)override;
    //virtual void Update(float elapsedTime, DirectX::XMFLOAT3 playerPosition) = 0;

    virtual void UpdateParticle(ID3D11DeviceContext* immediate_context, float deltaTime, ParticleSystem* p) {};

    // Normal enemies opt in to Player damage without inheriting boss-specific
    // phase, UI, or cinematic behavior.
    virtual bool TakeDamageFromPlayer(int damage) { (void)damage; return false; }
    virtual bool IsDefeated() const { return !IsAlive() || IsPendingKill(); }
    virtual void SpawnPlayerHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition)
    {
        (void)hitPosition; (void)hitNormal; (void)playerPosition;
    }
    virtual void SpawnPlayerRushHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition,
        bool hasHitPosition, bool hasHitNormal)
    {
        (void)hitPosition; (void)hitNormal; (void)playerPosition;
        (void)hasHitPosition; (void)hasHitNormal;
    }

    virtual bool OnMessage(const Telegram& msg) { return false; }
};
