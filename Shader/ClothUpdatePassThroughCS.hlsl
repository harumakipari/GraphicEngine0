// Diagnostic-only compute pass. It verifies the StructuredBuffer/UAV and
// CopyResource path without executing the cloth integration or constraints.
struct ClothVertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
    float3 velocity;
    float3 oldPosition;
    float3 oldVelocity;
    float4 rotation;
    uint isPinned;
};

StructuredBuffer<ClothVertex> inVertices : register(t0);
RWStructuredBuffer<ClothVertex> outVertices : register(u0);

cbuffer CLOTH_SIMULATE_CBUFFER : register(b10)
{
    float g;
    uint vertexCount;
    float windPhaseOffset;
    float windBase;
    float3 windEmitPosition;
};

[numthreads(16, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= vertexCount)
        return;

    outVertices[id.x] = inVertices[id.x];
}
