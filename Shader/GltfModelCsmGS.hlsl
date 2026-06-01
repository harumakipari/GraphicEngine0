// CASCADED_SHADOW_MAPS
#include "GltfModel.hlsli"
// CASCADED_SHADOW_MAPS
struct VS_OUT_CSM
{
    float4 position : SV_POSITION;
    uint instanceId : INSTANCEID;
};

struct GS_OUTPUT_CSM
{
    float4 position : SV_POSITION;
    uint renderTargetArrayIndex : SV_RENDERTARGETARRAYINDEX;
};
[maxvertexcount(3)]
void main(
	triangle VS_OUT_CSM input[3] ,
	inout TriangleStream<GS_OUTPUT_CSM> output)
{
    GS_OUTPUT_CSM element;
    element.renderTargetArrayIndex = input[0].instanceId;
    
    element.position = input[0].position;
    output.Append(element);
    element.position = input[1].position;
    output.Append(element);
    element.position = input[2].position;
    output.Append(element);

    output.RestartStrip();
}