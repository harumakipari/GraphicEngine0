#include "pch.h"
#include "BgmActor.h"

void BgmActor::Initialize(const Transform& transform)
{
    std::string parentName = GetRootComponentName();
    audioComponent = AddComponent<AudioSourceComponent>(parentName);
}

