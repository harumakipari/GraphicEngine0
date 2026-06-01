#ifndef _SAMPLER_STATE
#define _SAMPLER_STATE

SamplerState samplerStates[8] : register(s0);

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
#define LINEAR_BORDER_BLACK 3
#define LINEAR_BORDER_WHITE 4
#define LINEAR_CLAMP 5
#define LINEAR_MIRROR 6
#define MOTION_BLUR 7

SamplerComparisonState comparisionSamplerState : register(s8);

#endif