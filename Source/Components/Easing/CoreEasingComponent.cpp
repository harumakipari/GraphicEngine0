#include "pch.h"
#include "CoreEasingComponent.h"

#include "Core/Actor.h"
#include "Engine/Utility/Time.h"

void CoreEasingComponent::Tick(float deltaTime)
{
    for (auto& [accessor, handler] : handlers)
    {
        if (!handler.IsCompleted())
        {
            float value = accessor.getter ? accessor.getter() : 0.0f;
            handler.Update(value, useUnscaledTime ? Time::UnscaledDeltaTime() : Time::DeltaTime());
            if (accessor.setter) {
                accessor.setter(value);
            }
        }
    }
    if (!handlers.empty())
    {
        handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
            [&](const auto& handler) {
                return handler.second.IsCompleted();
            }),
            handlers.end());
    }
}

void CoreEasingComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
#if 0
    if (ImGui::Button("Dump"))
    {
        DumpObject(this);// ないから
    }
#endif // 0

    auto owner = owner_.lock();
    if (!owner)
    {
        Logger::Warning(u8"イージングコンポーネントのownerがnullです！");
    }

    ImGui::Checkbox("useUnscaledTime", &useUnscaledTime);

    static int valueType = 0;
    const char* valueTypes[] = { "positionX", "positionY","positionZ", "scaleX","scaleY","scaleZ", "rotationX", "rotationY", "rotationZ" };

    const char* typeNames[] = { "InQuad", "OutQuad", "InOutQuad", "InCubic", "OutCubic", "InOutCubic", "InQuart", "OutQuart", "InOutQuart", "InQuint", "OutQuint", "InOutQuint", "InSine", "OutSine",
        "InOutSine", "InExp", "OutExp", "InOutExp", "InCirc", "OutCirc", "InOutCirc", "InBounce", "OutBounce", "InOutBounce", "InBack", "OutBack", "InOutBack", "Linear", "None" };

    ImGui::Combo("valueType", &valueType, valueTypes, IM_ARRAYSIZE(valueTypes));

    for (int i = 0; i < easeItems.size(); i++)
    {
        ImGui::PushID(i);

        if (ImGui::TreeNodeEx(("ease" + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Combo("function", &easeItems[i].first, typeNames, IM_ARRAYSIZE(typeNames));
            TestEasingHandler::ToEasingFunction(static_cast<TestEaseType>(easeItems[i].first), easeItems[i].second.function, easeItems[i].second.backFunction);

            if (easeItems[i].first != IM_ARRAYSIZE(typeNames) - 1)
            {
                ImGui::DragFloat("start", &easeItems[i].second.easeData.startValue, 0.1f);
                ImGui::DragFloat("end", &easeItems[i].second.easeData.endValue, 0.1f);
                if (easeItems[i].second.backFunction)
                {
                    ImGui::DragFloat("back", &easeItems[i].second.easeData.backValue, 0.1f);
                }
            }
            ImGui::DragFloat("time", &easeItems[i].second.easeData.totalTime, 0.1f);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
    float size = 20.0f;

    if (ImGui::Button("+", ImVec2(size, size)) && easeItems.size() < easeItems.max_size())
    {
        easeItems.resize(easeItems.size() + 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(size, size)) && easeItems.size() > 0)
    {
        easeItems.resize(easeItems.size() - 1);
    }

    if (ImGui::Button("Start") && easeItems.size() > 0)
    {
        TestEasingHandler handler;
        for (auto& easeItem : easeItems)
        {
            handler.AddEasing(easeItem.second);
        }
        //Propertyアクセス設定
        PropertyAccessor<float> accessor;
        DirectX::XMFLOAT3 ownerPosition = owner->GetPosition();
        DirectX::XMFLOAT3 ownerScale = owner->GetScale();
        DirectX::XMFLOAT3 ownerAngle = owner->GetEulerRotation();
        switch (valueType)
        {
        case 0:
        {
            accessor.getter = [&]()-> float {
                return ownerPosition.x;
                };
            accessor.setter = [&](float value) {
                ownerPosition.x = value;
                owner->SetPosition(ownerPosition);
                };
            break;
        }
        case 1:
        {
            accessor.getter = [&]()-> float {
                return ownerPosition.y;
                };
            accessor.setter = [&](float value) {
                ownerPosition.y = value;
                owner->SetPosition(ownerPosition);
                };
            break;
        }
        case 2:
        {
            accessor.getter = [&]()-> float {
                return ownerPosition.z;
                };
            accessor.setter = [&](float value) {
                ownerPosition.z = value;
                owner->SetPosition(ownerPosition);
                };
            break;
        }
        case 3:
        {
            accessor.getter = [&]()-> float {
                return ownerScale.x;
                };
            accessor.setter = [&](float value) {
                ownerScale.x = value;
                owner->SetScale(ownerScale);
                };
            break;
        }
        case 4:
        {
            accessor.getter = [&]()-> float {
                return ownerScale.y;
                };
            accessor.setter = [&](float value) {
                ownerScale.y = value;
                owner->SetScale(ownerScale);
                };
            break;
        }
        case 5:
        {
            accessor.getter = [&]()-> float {
                return ownerScale.z;
                };
            accessor.setter = [&](float value) {
                ownerScale.z = value;
                owner->SetScale(ownerScale);
                };
            break;
        }
        case 6:
        {
            accessor.getter = [&]()-> float {
                return ownerAngle.x;
                };
            accessor.setter = [&](float value) {
                ownerAngle.x = value;
                owner->SetEulerRotation(ownerAngle);
                };
            break;
        }
        case 7:
        {
            accessor.getter = [&]()-> float {
                return ownerAngle.y;
                };
            accessor.setter = [&](float value) {
                ownerAngle.y = value;
                owner->SetEulerRotation(ownerAngle);
                };
            break;
        }
        case 8:
        {
            accessor.getter = [&]()-> float {
                return ownerAngle.z;
                };
            accessor.setter = [&](float value) {
                ownerAngle.z = value;
                owner->SetEulerRotation(ownerAngle);
                };
            break;
        }
        default:
            break;
        }

        StartHandler(handler, accessor);
    }
#endif // USE_IMGUI
}
