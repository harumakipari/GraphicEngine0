#ifndef VIEW_CONSTANTS_INCLUDE
#define VIEW_CONSTANTS_INCLUDE

cbuffer VIEW_CONSTANTS_BUFFER : register(b4)
{
    row_major float4x4 viewProjection;
    float4 cameraPosition;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 inverseViewProjection;
    row_major float4x4 invView;
    float4 cameraClipDistance;
    row_major float4x4 previousViewProjection;
    row_major float4x4 lightViewProjection;
}

#endif