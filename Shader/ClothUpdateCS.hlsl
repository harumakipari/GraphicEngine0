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

//#define CLOTH_EDGES_PER_VERTEX 4
#define MAX_EDGES 8
#define SPRING_FORCE 20000.0
//#define MASS 0.5
#define MASS 0.5
#define GRAVITY 0.98
#define DAMPING 0.04

// ・ｽ關・
#define SPRING_STIFFNESS 40.0   // ・ｽﾎね定数 40-200
#define SPRING_DAMPING   25.0     // ・ｽﾎね粘・ｽ・ｽ・ｽi・ｽ・ｽ・ｽﾎ托ｿｽ・ｽx・ｽﾉ対ゑｿｽ・ｽ骭ｸ・ｽ・ｽ・ｽj
#define GLOBAL_DAMPING   0.05     // ・ｽS・ｽﾌ指・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ

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
 // ・ｽ・ｽ・ｽﾌ・ｿｽ・ｽ[・ｽ・ｽ・ｽh・ｽ・ｽ・ｽ
    float3 sphereCenterWorld = worldPos;
    float sphereRadiusWorld = radius;
    float friction = 0.4;
    float restitution = 0.1;

    // ・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽh・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽJ・ｽ・ｽ・ｽﾏ奇ｿｽ
    float4 centerLocal4 = mul(float4(sphereCenterWorld, 1.0), invWorld);
    float3 sphereCenterLocal = centerLocal4.xyz;

    // ・ｽ・ｽ・ｽ[・ｽJ・ｽ・ｽ・ｽﾅの会ｿｽ・ｽ・ｽ・ｽo・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
    float3 dir = v.position - sphereCenterLocal;
    float dist = length(dir);

    if (dist < sphereRadiusWorld)
    {
        float3 n = normalize(dir);
        float penetration = sphereRadiusWorld - dist;
        
        // ・ｽ・ｽ・ｽ濶ｻ・ｽt・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽ・ｽ
        v.position += n * penetration * 1.0;
        //v.position += n * penetration * 0.8;

        // ・ｽ・ｽ・ｽx・ｽ竦ｳ
        float vn = dot(v.velocity, n);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * n;
            float3 velTangent = v.velocity - velNormal;

            // restitution・ｽ・ｽfriction・ｽ・ｽK・ｽp
            //v.velocity = velTangent * (1.0 - friction) - velNormal * restitution;
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
    //Plane worldFloor = { float3(1, 1, 0), 1 }; // y=0 ・ｽﾌ擾ｿｽ
    
    //Plane worldFloor = { float3(0, 1, 0), 0 }; // y=0 ・ｽﾌ擾ｿｽ
    Plane worldFloor = p; // y=0 ・ｽﾌ擾ｿｽ
    
    float3 localNormal = mul(float4(worldFloor.normal, 0.0f), invWorld).xyz;
    localNormal = normalize(localNormal);

    // ・ｽ・ｽ・ｽﾊ擾ｿｽ・ｽ1・ｽ_・ｽ・ｽ・ｽ・ｽﾟゑｿｽi・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽh・ｽ・ｽﾔで）
    float3 worldPoint = worldFloor.normal * worldFloor.d;

    // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ[・ｽJ・ｽ・ｽ・ｽ・ｽﾔに変奇ｿｽ
    float3 localPoint = mul(float4(worldPoint, 1.0f), invWorld).xyz;

    // ・ｽz・ｽﾆ包ｿｽ・ｽﾊの具ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾟゑｿｽ
    float dist = dot(localNormal, v.position - localPoint);

    if (dist < 0.0f)
    {
        // ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽ・ｽ
        v.position -= localNormal * dist * 0.8f;

        // ・ｽ・ｽ・ｽx・ｽ竦ｳ
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
    { // ・ｽﾅ抵ｿｽ_・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
        outVertex.position = inVertex.position;
        outVertex.velocity = float3(0.0, 0.0, 0.0);
        outVertex.oldPosition = inVertex.position;
        outVertex.oldVelocity = inVertex.velocity;
        outVertices[id.x] = outVertex;
        return;
    }
    
    outVertex.oldPosition = inVertex.position; // ・ｽO・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ擾ｿｽ・ｽ
    outVertex.oldVelocity = inVertex.velocity;

    uint baseEdge = id.x * MAX_EDGES; // 8・ｽﾂゑｿｽ・ｽ・ｽ
    
    for (uint i = 0; i < MAX_EDGES; ++i)
    {
        ClothEdge e = clothEdgeSRV[baseEdge + i];
        if (e.neighbor == 0xffffffffu)
        { // ・ｽﾟ傍・ｽ_・ｽ・ｽ・ｽﾈゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
            continue;
        }
        float3 neighborPos = inVertices[e.neighbor].position;
        float3 delta = neighborPos - currentPos;
        float distance = length(delta);
        if (distance > 1e-6f)
        {
            // ・ｽ・ｽ・ｽ・ｽ・ｽ@・ｽ・ｽ・ｽ@f ・ｽ・ｽ・ｽ・ｽ・ｽ@x・ｽ@(・ｽ・ｽ・ｽ・ｽ-・ｽ・ｽ・ｽR・ｽ・ｽ)
            float springDelta = distance - e.restLength;
            float3 n = normalize(delta);
            // ・ｽt・ｽb・ｽN・ｽﾌ法・ｽ・ｽ・ｽO・ｽO
            float springStiffness = SPRING_STIFFNESS; // ・ｽ蛯ｫ・ｽ・ｽ・ｽﾙど硬・ｽ・ｽ・ｽﾈゑｿｽ ・ｽ・ｽ・ｽ・ｽ
            force += n * springStiffness * springDelta;
            
            // ・ｽﾟ傍・ｽ・ｽ・ｽx・ｽﾌ包ｿｽ・ｽ・ｽ
            avgVelocity += inVertices[e.neighbor].oldVelocity;
            
            edgeCount++;
        }
    }

    if (edgeCount > 0)
    {
        // ・ｽ^・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ ・ｽd・ｽ・ｽ ・ｽ~ ・ｽ・ｽ・ｽ・ｽ
        // float4 forceWorld = float4(0, 0, 0, 0);
        float4 forceWorld = float4(0, -4.0 * MASS * gravityScale, 0, 0);
        // ・ｽd・ｽﾍゑｿｽ・ｽ・ｽf・ｽ・ｽ・ｽ・ｽﾔに変奇ｿｽ・ｽ・ｽ・ｽ・ｽ
        float4 forceLocal4 = mul(forceWorld, invWorld);
        force += forceLocal4.xyz;
        float3 currentVelocity = inVertex.velocity;
#if 1
        // ・ｽ・ｽ・ｽﾌ影・ｽ・ｽ
        float3 windDir = windEmitPosition - outVertex.oldPosition;
        float4 windDirWorld = normalize(float4(windDir, 0));
        float4 windDirLocal4 = mul(windDirWorld, invWorld);
        //float windBase = 5.0f;
        float windVariation = windPhaseOffset;

        float phase = elapsedTime * 2.0f + windPhaseOffset - currentPos.y * 1.5f /*+ windPhaseOffset*/;

        // ・ｽ・ｽ・ｽﾔと位置・ｽﾉゑｿｽ・ｽ・ｽ轤ｬ
        float w = sin(phase) * 0.5f + 0.5f;
        float strength = windBase + windVariation * w;

        // ・ｽﾇ擾ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽi・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾆゑｿｽ轤ｮ・ｽj
        float3 windVel = windDirLocal4 * strength;

        // ・ｽ・ｽ・ｽﾎ托ｿｽ・ｽx・ｽR・ｽ・ｽ
        float3 relativeVel = windVel - currentVelocity;
        float dragCoef = 0.2f;
        float3 windForce = dragCoef * relativeVel;

        // ・ｽ・ｽ・ｽ・ｽ・ｽI・ｽﾉ揺・ｽ轤ｷ・ｽO・ｽﾍ（・ｽ・ｽ・ｽx・ｽﾉ依托ｿｽ・ｽ・ｽ・ｽﾈゑｿｽ・ｽj
        float wave = sin(elapsedTime * driveForceFrequency - currentPos.y * driveForcePhaseGradient);

        float3 driveForce =
    float3(0.0f, 0.0f, wave) * driveForceAmplitude;

        if (enableDriveForce != 0u)
        {
            force += driveForce * bottomWeight;
        }



        windForce *= bottomWeight;
    //    // ・ｽg・ｽﾆゑｿｽ・ｽﾄ伝・ｽd・ｽ・ｽ・ｽ髟暦ｿｽﾌ力（・ｽ・ｽ・ｽﾚ会ｿｽ・ｽ・ｽ・ｽj
    //    float waveForce =
    //sin(elapsedTime * 2.0f - currentPos.y * 1.5f);

    //    float3 oscillationForce =
    //float3(0.0f, 0.0f, waveForce) * 3.0f; // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽd・ｽv

    //    force += oscillationForce;

        force += windForce;
#endif
        avgVelocity /= float(edgeCount);
        avgVelocity *= 0.2f;
        // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
        const float damping = SPRING_DAMPING; // ・ｽﾎねの粘・ｽ・ｽ
        float3 newVelocity = currentVelocity + (force / MASS) * deltaTime /** exp(-damping * deltaTime)*/;
        newVelocity *= (1.0f - GLOBAL_DAMPING);
        
        const float smmothing = 5.0;
        float3 smoothedVelocity = lerp(avgVelocity, newVelocity, exp(-smmothing * deltaTime));
        
        
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
