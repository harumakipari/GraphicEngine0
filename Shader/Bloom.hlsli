cbuffer BLOOM_CONSTANT_BUFFER : register(b8)
{
    float bloomExtractionThreshold;
    float bloomIntensity;
    float2 something;
};