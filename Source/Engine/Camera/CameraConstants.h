#ifndef CAMERA_CONSTANTS_H
#define CAMERA_CONSTANTS_H

#include <DirectXMath.h>

struct ViewConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4 cameraPosition;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 invProjection;
    DirectX::XMFLOAT4X4 invViewProjection;
    DirectX::XMFLOAT4X4 invView;
    DirectX::XMFLOAT4  cameraClipDistance;
};


#endif // !CAMERA_CONSTANTS_H
