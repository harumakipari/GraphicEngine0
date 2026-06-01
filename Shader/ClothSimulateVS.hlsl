#include "Constants.hlsli"
#include "GltfModel.hlsli"
struct VS_ClOTH_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    float3 velocity : VELOCITY;
    float3 oldPosition : OLDPOSITION;
    float3 oldVelocity : OLDVELOCITY;
    float4 rotation : ROTATION;
    uint isPinned : ISPINNED;
};


float4 QuatMul(float4 q1, float4 q2)
{
    return float4(
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
        q1.w * q2.w - dot(q1.xyz, q2.xyz)
    );
}

// ベクトルにクォータニオンをかける
float3 RotateByQuaternion(float3 v, float4 q)
{
    float4 vq = float4(v, 0.0f);
    float4 qi = float4(-q.x, -q.y, -q.z, q.w); // conjugate (inverse if q is normalized)

    float4 r = QuatMul(QuatMul(q, vq), qi);
    return r.xyz;
}


VS_OUT main(VS_ClOTH_IN vin)
{
    float sigma = vin.tangent.w;
    VS_OUT vout;
    vin.position.w = 1;
    vout.position = mul(vin.position, mul(world, viewProjection));
    
    //vout.position /= vout.position.w; // to ndc
    vout.wPosition = mul(vin.position, world);
    
    // ここで world の回転の前に
    float3 normal = vin.normal.xyz;
    float3 tangent = vin.tangent.xyz;
    
#if 1
    normal = RotateByQuaternion(normal, vin.rotation);
    tangent = RotateByQuaternion(tangent, vin.rotation);
#endif
    vin.normal.w = 0;
    //vout.wNormal = normalize(mul(vin.gbuffer1Normal, world));
    vout.wNormal = normalize(mul(float4(normal, 0.0), world));
    
    vin.tangent.w = 0;
    //vout.wTangent = normalize(mul(vin.tangent, world));
    vout.wTangent = normalize(mul(float4(tangent, 0.0), world));
    vout.wTangent.w = sigma;
    
    vout.texcoord = vin.texcoord;
    
    if (vout.wPosition.y < 0.0)
    {
        //vout.wPosition.y = 0.0;
    }

    
    return vout;
}