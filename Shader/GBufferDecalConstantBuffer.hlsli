#ifndef __GBUFFER_DECAL_CONSTANT_BUFFER_H__
#define __GBUFFER_DECAL_CONSTANT_BUFFER_H__

cbuffer GBUFFER_DECAL_CONSTANT_BUFFER : register(b12)
{
    row_major float4x4 decal_inverse_transform;
    float4 decal_direction;
};

#endif  //  __GBUFFER_DECAL_CONSTANT_BUFFER_H__
