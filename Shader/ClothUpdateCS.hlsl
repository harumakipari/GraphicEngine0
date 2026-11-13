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

struct ClothEdge
{
    uint neighbor; // ・ｽﾗ接抵ｿｽ・ｽ_・ｽﾌイ・ｽ・ｽ・ｽf・ｽb・ｽN・ｽX
    float3 delta; // ・ｽ・ｽ・ｽ・ｽ・ｽﾌ托ｿｽ・ｽﾎベ・ｽN・ｽg・ｽ・ｽ
    float restLength; // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
};

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    
    int material;
    bool hasTangent;
    int skin;
    int padding;
    
    row_major float4x4 invWorld;
}

StructuredBuffer<ClothVertex> inVertices : register(t0);
StructuredBuffer<ClothEdge> clothEdgeSRV : register(t1);
StructuredBuffer<float> initialBottomWeights : register(t2);
RWStructuredBuffer<ClothVertex> outVertices : register(u0);

#define MAX_EDGES 8
#define SPRING_FORCE 20000.0
#define MASS 0.5
#define GRAVITY 0.98
#define DAMPING 0.04

#define SPRING_STIFFNESS 40.0   
#define SPRING_DAMPING   25.0     
#define GLOBAL_DAMPING   0.05    

cbuffer CLOTH_TIME_CBUFFER : register(b11)
{
    float elapsedTime;
    float deltaTime;
    float gravityScale;
    float clothTimePadding;
}

cbuffer VIEW_CONSTANTS_BUFFER : register(b4)
{
    row_major float4x4 viewProjection;
    float4 cameraPositon;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 inverseViewProjection;
    row_major float4x4 invView;
}

cbuffer CLOTH_SIMULATE_CBUFFER : register(b10)
{
    float g;
    uint vertexCount;
    float windPhaseOffset;
    float windBase;

    float3 windEmitPosition;
    uint enableDriveForce;
    float bottomWindExponent;
    float driveForceAmplitude;
    float driveForceFrequency;
    float driveForcePhaseGradient;
};

cbuffer SPHERE_CBUFFER : register(b7)
{
    float3 worldPos;
    float radius;
    row_major float4x4 sphereWorld;
    row_major float4x4 sphereInvWorld;
}

void CollideSphere(inout ClothVertex v)
{
    float3 sphereCenterWorld = worldPos;
    float sphereRadiusWorld = radius;
    float4 centerLocal4 = mul(float4(sphereCenterWorld, 1.0), invWorld);
    float3 sphereCenterLocal = centerLocal4.xyz;

    float3 dir = v.position - sphereCenterLocal;
    float dist = length(dir);

    if (dist < sphereRadiusWorld)
    {
        float3 n = normalize(dir);
        float penetration = sphereRadiusWorld - dist;
        
        v.position += n * penetration * 1.0;

        float vn = dot(v.velocity, n);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * n;
            float3 velTangent = v.velocity - velNormal;
            v.velocity = velTangent * 0.9 - velNormal * 0.5;
        }
    }
}

struct Plane
{
    float3 normal;
    float d;
};

cbuffer PLANE_CBUFFER : register(b6)
{
    Plane planes[4];
}

void CollideFloor(inout ClothVertex v, Plane p)
{
    Plane worldFloor = p; 
    
    float3 localNormal = mul(float4(worldFloor.normal, 0.0f), invWorld).xyz;
    localNormal = normalize(localNormal);

    float3 worldPoint = worldFloor.normal * worldFloor.d;

    float3 localPoint = mul(float4(worldPoint, 1.0f), invWorld).xyz;

    float dist = dot(localNormal, v.position - localPoint);

    if (dist < 0.0f)
    {
        v.position -= localNormal * dist * 0.8f;
        float vn = dot(v.velocity, localNormal);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * localNormal;
            float3 velTangent = v.velocity - velNormal;
            float friction = 0.4f;
            float restitution = 0.1f;

            v.velocity = velTangent * (1.0f - friction) - velNormal * restitution;
        }
    }

}

#define NUM_THREAD_X 16

[numthreads(NUM_THREAD_X, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (vertexCount <= id.x)
    {
        return;
    }
    ClothVertex inVertex = inVertices[id.x];
    ClothVertex outVertex = inVertices[id.x];
    
    outVertex = inVertex;
    
    float3 currentPos = inVertex.position;
    const float bottomWeight = pow(saturate(initialBottomWeights[id.x]), bottomWindExponent);
    
    float3 force = float3(0, 0, 0);
    float3 avgVelocity = float3(0, 0, 0);
    uint edgeCount = 0;
    
    
    if (outVertex.isPinned == 1)
    {
        outVertex.position = inVertex.position;
        outVertex.velocity = float3(0.0, 0.0, 0.0);
        outVertex.oldPosition = inVertex.position;
        outVertex.oldVelocity = inVertex.velocity;
        outVertices[id.x] = outVertex;
        return;
    }
    
    outVertex.oldPosition = inVertex.position; 
    outVertex.oldVelocity = inVertex.velocity;

    uint baseEdge = id.x * MAX_EDGES;
    
    for (uint i = 0; i < MAX_EDGES; ++i)
    {
        ClothEdge e = clothEdgeSRV[baseEdge + i];
        if (e.neighbor == 0xffffffffu)
        { 
            continue;
        }
        float3 neighborPos = inVertices[e.neighbor].position;
        float3 delta = neighborPos - currentPos;
        float distance = length(delta);
        if (distance > 1e-6f)
        {
            float springDelta = distance - e.restLength;
            float3 n = normalize(delta);
            float springStiffness = SPRING_STIFFNESS; 
            force += n * springStiffness * springDelta;
            avgVelocity += inVertices[e.neighbor].oldVelocity;
            
            edgeCount++;
        }
    }

    if (edgeCount > 0)
    {
        float4 forceWorld = float4(0, -4.0 * MASS * gravityScale, 0, 0);
        float4 forceLocal4 = mul(forceWorld, invWorld);
        force += forceLocal4.xyz;
        float3 currentVelocity = inVertex.velocity;
        float3 windDir = windEmitPosition - outVertex.oldPosition;
        float4 windDirWorld = normalize(float4(windDir, 0));
        float4 windDirLocal4 = mul(windDirWorld, invWorld);
        float windVariation = windPhaseOffset;

        float phase = elapsedTime * 2.0f + windPhaseOffset - currentPos.y * 1.5f;

        float w = sin(phase) * 0.5f + 0.5f;
        float strength = windBase + windVariation * w;

        float3 windVel = windDirLocal4 * strength;
        float3 relativeVel = windVel - currentVelocity;
        float dragCoef = 0.2f;
        float3 windForce = dragCoef * relativeVel;

        float wave = sin(elapsedTime * driveForceFrequency - currentPos.y * driveForcePhaseGradient);

        float3 driveForce =float3(0.0f, 0.0f, wave) * driveForceAmplitude;

        if (enableDriveForce != 0u)
        {
            force += driveForce * bottomWeight;
        }

        windForce *= bottomWeight;

        force += windForce;
        avgVelocity /= float(edgeCount);
        avgVelocity *= 0.2f;
        float3 newVelocity = currentVelocity + (force / MASS) * deltaTime ;
        newVelocity *= (1.0f - GLOBAL_DAMPING);
        const float smoothing = 5.0;
        float3 smoothedVelocity = lerp(avgVelocity, newVelocity, exp(-smoothing * deltaTime));
        
        outVertex.velocity = smoothedVelocity;
        outVertex.position = currentPos + smoothedVelocity * deltaTime;
    }
    
    CollideSphere(outVertex);
    for (uint j = 0; j < 2; j++)
    {
        CollideFloor(outVertex, planes[j]);
    }

    outVertices[id.x] = outVertex;
}
