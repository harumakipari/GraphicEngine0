struct ClothVertex
{
    float4 position;
    float4 normal;
    float4 tangent;
    float4 texcoord;
    
    float4 oldVelocity;
    float4 velocity;
    float4 newPosition;
};

struct ClothEdge
{
    uint id;
    float restLength;
    float3 delta;
};

float Rand(float n)
{
    // Return value is greater than or equal to 0 and less than 1.
    return frac(sin(n) * 43758.5453123);
}

// Returns a random value in the specified range [min, max] 
float RandRange(float seed, float min, float max)
{
    return lerp(min, max, Rand(seed));
}

StructuredBuffer<ClothVertex> clothVerticesSRV : register(t0);
StructuredBuffer<ClothEdge> clothEdges : register(t1);
RWStructuredBuffer<ClothVertex> clothVerticesUAV : register(u0);


#define CLOTH_EDGES_PER_VERTEX 4
#define SPRING_FORCE 20000.0
#define MASS 0.1
#define GRAVITY 0.98
#define DAMPING 0.04

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    bool enableSSAO;
    float reflectionIntensity;
    float time;
    // shader のフラグ
    bool enableCascadedShadowMaps;
    
    bool enableSSR;
    bool enableFog;
    bool enableBloom;
    float deltaTime;
    
    float gravity;
}

#define NUM_THREAD_X 16

[numthreads(NUM_THREAD_X, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    ClothVertex readVertex = clothVerticesSRV[id.x];
    ClothVertex writeVertex = clothVerticesUAV[id.x];
#if 0
    //v.position.x += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    //v.position.z += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    //v.position.y += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    //v.newPosition.x += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    //v.newPosition.z += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    //v.newPosition.y += RandRange(id.x * 3.14, 0.2f, 0.5f) * deltaTime;
    writeVertex.position.x += 0.2 * deltaTime;
    writeVertex.position.z += 0.2 * deltaTime;
    writeVertex.position.y -= 0.2 * deltaTime;
   
    if (writeVertex.position.y < 0.0f)
    {
        writeVertex.position.y = 0.0;
    }
#else
#if 0
    float3 curretPosition = readVertex.position;
    float3 currentVelocity = readVertex.velocity;
    float3 force = float3(0.0, 0.0, 0.0);
    float3 avgVelocity = float3(0.0, 0.0, 0.0);
    int edgeCount = 0;
    for (int i = 0; i < CLOTH_EDGES_PER_VERTEX; i++)
    {
        ClothEdge edge = clothEdges[id.x * CLOTH_EDGES_PER_VERTEX + i];
        if (edge.id == 0xffffffff)
        {
            continue;
        }
        float3 delta = readVertex.position.xyz - curretPosition;
        float distance = length(delta);
        if (distance > 0.0f)
        {
            force += delta * (SPRING_FORCE * (distance - edge.restLength) / distance);
        }
    
        //avgVelocity += clothVertices[edge.id].oldVelocity;
        
        edgeCount++;
    }
    
    if (edgeCount > 0)
    {
        force += float3(0.0f, -GRAVITY * MASS, 0.0);
        

        // 外力入れたかったらここに後で        
        //avgVelocity /= edgeCount;
        
        float3 newVelocity = (currentVelocity + force * deltaTime / MASS) * exp(-DAMPING * deltaTime);
        
        float3 newPosition = curretPosition + newVelocity * deltaTime;
        
        //　仮の当たり判定
        if (newPosition.y < 0.0f)
        {
            newPosition.y = 0.0;
            newVelocity.y = 0.0;
        }
        writeVertex.velocity = float4(newVelocity.xyz, 0.0);
        writeVertex.newPosition = float4(newPosition.xyz, 1.0);
        writeVertex.position = float4(newPosition.xyz, 1.0);
        //v.position = float4(0.0f,2.0f,0.0f, 1.0);
    }
    else
    {
        writeVertex.newPosition = float4(curretPosition.xyz, 1.0);
        writeVertex.position = float4(curretPosition.xyz, 1.0);
        //v.position = float4(0.0f,2.0f,0.0f, 1.0);
    }
#endif
     // --- 固定点はスキップ ---
    if (readVertex.oldVelocity.x == 1.0)
    {
        // 速度はゼロにして、位置を元のまま固定
        writeVertex.velocity = float4(0, 0, 0, 0);
        writeVertex.newPosition = readVertex.position;
        writeVertex.position = readVertex.position;
        clothVerticesUAV[id.x] = writeVertex;
        return;
    }
    
    float3 curretPosition = readVertex.position.xyz;
    float3 currentVelocity = readVertex.velocity.xyz;
    float3 force = float3(0.0, 0.0, 0.0);
    int edgeCount = 0;

    // --- スプリング力計算 ---
    for (int i = 0; i < CLOTH_EDGES_PER_VERTEX; i++)
    {
        ClothEdge edge = clothEdges[id.x * CLOTH_EDGES_PER_VERTEX + i];
        if (edge.id == 0xffffffff)
            continue;

        ClothVertex neighbor = clothVerticesSRV[edge.id];
        float3 neighborPos = neighbor.position.xyz;

        float3 delta = curretPosition - neighborPos;
        float dist = length(delta);
        if (dist > 1e-6f)
        {
            force += -SPRING_FORCE * (dist - edge.restLength) * (delta / dist);
        }
        edgeCount++;
    }

    // --- 重力 ---
    force += float3(0.0f, -MASS * gravity, 0.0f);

    // --- ダンピング ---
    force += -DAMPING * currentVelocity;

    // --- 加速度, 速度, 位置更新 ---
    float3 acceleration = force / MASS;
    float3 newVelocity = (currentVelocity + acceleration * deltaTime);
    float3 newPosition = curretPosition + newVelocity * deltaTime;

    // 仮の地面衝突
    if (newPosition.y < 0.0f)
    {
        newPosition.y = 0.0f;
        newVelocity.y = 0.0f;
    }

    // 書き戻し
    writeVertex.velocity = float4(newVelocity, 0.0f);
    writeVertex.newPosition = float4(newPosition, 1.0f);
    writeVertex.position = float4(newPosition, 1.0f);
    
#endif
    //currentclothVertices[id.x] = v;
    clothVerticesUAV[id.x] = writeVertex;
}