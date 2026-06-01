#include "pch.h"
#include "RigidBodyComponent.h"

#include <PxPhysicsAPI.h>
#include "Physics/PhysicsHelper.h"
#include "ShapeComponent.h"
#include "Core/Actor.h"

void SingleRigidBodyComponent::Initialize(physx::PxPhysics* physics)
{
    using namespace physx;
    auto info = shapeComponent_->GetPhysicsShapeInfo();

    DirectX::XMFLOAT3 pos = shapeComponent_->GetOwner()->GetPosition();
    DirectX::XMFLOAT4 rot = shapeComponent_->GetComponentRotation();
    //pos.y += shapeComponent_->GetCollisionOffsetY();
    PxVec3 pxPosition(pos.x, pos.y, pos.z);
    PxQuat pxRotation(rot.x, rot.y, rot.z, rot.w);
    // physx の原点を上げるため
    // PxTransform を作成して使用する
    PxTransform transform(pxPosition, pxRotation);

    pxActor_ = physics->createRigidDynamic(transform);
    pxActor_->userData = shapeComponent_->GetOwner();    // Actor へのポインタ

    {
        //material_->setRestitution(0.0f);
        //material_->setDynamicFriction(1.0f);
        //material_->setStaticFriction(1.0f);
    }
    material_ = Physics::Instance().GetMaterial(
        shapeComponent_->GetPhysicsMaterialType()
    );

    pxShape_ = physics->createShape(info.geometry.any(), *material_, true);

    // 第三引数を true にすることで一意的な専用の shape にする
    //pxShape_ = physics->createShape(info.geometry.any(), *material_, true);

    pxShape_->userData = shapeComponent_;    // ShapeComponent へのポインタ
    if (shapeComponent_->GetCollisionType() == "Capsule")
    {// カプセルの時
        // 形状のローカル姿勢を調整
        physx::PxTransform pxShapeTransform = pxShape_->getLocalPose();

        switch (shapeComponent_->GetCapusleAxis())
        {
        case ShapeComponent::CapsuleAxis::x:
            pxShapeTransform.q = physx::PxQuat(physx::PxPiDivTwo, physx::PxVec3(1.0f, 0.0f, 0.0f));
            break;
        case ShapeComponent::CapsuleAxis::y:
            pxShapeTransform.p = PxVec3(0.0f, shapeComponent_->GetCollisionOffsetY(), 0.0f);
            pxShapeTransform.q = physx::PxQuat(physx::PxPiDivTwo, physx::PxVec3(0.0f, 0.0f, 1.0f));	//90度回転させて
            break;
        case ShapeComponent::CapsuleAxis::z:
            pxShapeTransform.q = physx::PxQuat(physx::PxPiDivTwo, physx::PxVec3(0.0f, 1.0f, 0.0f));
            break;
        };
        //pxShapeTransform.p.y = shapeComponent_->GetCollisionOffsetY(); // 上に補正して
        pxShape_->setLocalPose(pxShapeTransform);	//シーンに設定(カプセルを立たせるため)通常は横向き
    }
    else
    {// カプセル以外の時
        // オフセット方向に補正する
        pxShape_->setLocalPose(physx::PxTransform(physx::PxVec3(shapeComponent_->GetCollisionOffsetX(), shapeComponent_->GetCollisionOffsetY(), shapeComponent_->GetCollisionOffsetZ())));
    }

    // レイキャストなど用のレイヤー設定
    physx::PxFilterData pxFilterData = pxShape_->getQueryFilterData();
    pxFilterData.word0 = layer_;
    //pxFilterData.word0 = 1;// 1なら当たる　０なら当たらない
    pxShape_->setQueryFilterData(pxFilterData);
#if 1
    //physx::PxFilterData queryData;
    //queryData.word0 = 0xFFFFFFFF;
    //queryData.word1 = 0xFFFFFFFF;

    //pxShape_->setQueryFilterData(queryData);
#else

    physx::PxFilterData filterData;
    filterData.word0 = layer_;

    pxShape_->setSimulationFilterData(filterData);
    pxShape_->setQueryFilterData(filterData);


#endif // 0
    pxShape_->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    //pxShape_->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
    //if (isTrigger_)
    //{
    //    pxShape_->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
    //}
    //else
    {// デフォルト
        pxShape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
    }
    //pxActor_->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
    pxActor_->attachShape(*pxShape_);

    pxActor_->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, shapeComponent_->IsKinematic());

    {
        pxActor_->setMaxLinearVelocity(300.0f);
        pxActor_->setMaxAngularVelocity(300.0f);

        pxActor_->setLinearDamping(0.0f);
        pxActor_->setAngularDamping(0.0f);
    }



    PxRigidBodyExt::updateMassAndInertia(*pxActor_, mass_);
}

void SingleRigidBodyComponent::Tick(float deltaTime)
{
    using namespace physx;

    if (!pxActor_)
    {
        return;
    }

    if (shapeComponent_->IsKinematic())
    {
        Transform t = shapeComponent_->GetComponentWorldTransform();
        //Transform t = shapeComponent_->GetOwner()->rootComponent_->GetFinalWorldTransform();

#if 0
        if (shapeComponent_->GetCollisionType() == "Capsule")
        {// ShapeComponent がカプセルの時
            ShapeComponent::CapsuleAxis axis = shapeComponent_->GetCapusleAxis();
            if (axis == ShapeComponent::CapsuleAxis::y)
            {// physx は z 軸が　上軸だから
                DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1, 0, 0, 0), DirectX::XMConvertToRadians(90.0f));
                DirectX::XMFLOAT4 rotation = t.GetRotation();
                DirectX::XMVECTOR qOriginal = DirectX::XMLoadFloat4(&rotation);
                DirectX::XMVECTOR qResult = DirectX::XMQuaternionMultiply(q, qOriginal);
                DirectX::XMStoreFloat4(&rotation, qResult);
                //t.SetRotation(rotation);
            }
            else if (axis == ShapeComponent::CapsuleAxis::x)
            {// ゲーム内で x軸
                DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMConvertToRadians(90.0f));
                DirectX::XMFLOAT4 rotation = t.GetRotation();
                DirectX::XMVECTOR qOriginal = DirectX::XMLoadFloat4(&rotation);
                DirectX::XMVECTOR qResult = DirectX::XMQuaternionMultiply(q, qOriginal);
                DirectX::XMStoreFloat4(&rotation, qResult);
                //t.SetRotation(rotation);
                PxTransform pxT = PhysicsHelper::ToPxTransform(t);
                // 当たり判定が地面の下に行くのを防ぐ
                pxT.p.x += shapeComponent_->GetCollisionOffsetY();
                pxActor_->setKinematicTarget(pxT);
                //PhysicsHelper::DebugPrintCollisionInfo("capsule", shapeComponent_->GetPhysicsShapeInfo().geometry.any(), pxT);
                return;
            }
            else
            {// ゲーム内で z軸
                //DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1, 0, 0, 0), DirectX::XMConvertToRadians(90.0f));
                //DirectX::XMFLOAT4 rotation = t.GetRotation();
                //DirectX::XMVECTOR qOriginal = DirectX::XMLoadFloat4(&rotation);
                //DirectX::XMVECTOR qResult = DirectX::XMQuaternionMultiply(q, qOriginal);
                //DirectX::XMStoreFloat4(&rotation, qResult);
                //t.SetRotation(rotation);
            }

        }
#endif // 0

        PxTransform pxT = PhysicsHelper::ToPxTransform(t);
        // 当たり判定が地面の下に行くのを防ぐ
        //pxT.p.y += shapeComponent_->GetCollisionOffsetY();
        pxActor_->setKinematicTarget(pxT);
    }
    else
    {// ダイナミックの時
        // 上に補正する

        PxTransform pxT = pxActor_->getGlobalPose();
        // 当たり判定が地面の下に行くのを防ぐ
        if (shapeComponent_->GetCollisionType() == "Sphere")
        {// 動的に半径を変更した時に使用
            //pxShape_->setLocalPose(physx::PxTransform(physx::PxVec3(0.0f, currentRadius_, 0.0f)));
            pxT.p.y += currentRadius_;
        }
        GetOwner()->SetPosition({ pxT.p.x,pxT.p.y,pxT.p.z });
        GetOwner()->SetQuaternionRotation({ pxT.q.x,pxT.q.y,pxT.q.z ,pxT.q.w });
        //shapeComponent_->SetWorldLocationDirect({ pxT.p.x,pxT.p.y,pxT.p.z });
        //shapeComponent_->SetWorldRotationDirect({ pxT.q.x,pxT.q.y,pxT.q.z ,pxT.q.w });
    }
}

// 動的に球の当たり判定の半径を大きくする関数
void SingleRigidBodyComponent::ResizeSphere(float newRadius)
{
    if (!pxShape_)
    {
        return;
    }

    if (shapeComponent_->GetCollisionType() != "Sphere")
    {
        return;
    }

    // Geometryを取得
    physx::PxGeometryHolder holder = pxShape_->getGeometry();

    // Sphereに変換して半径を変更
    physx::PxSphereGeometry sphere = holder.sphere();
    sphere.radius = newRadius;

    // 動的に position を上に上げるため
    currentRadius_ = newRadius;

    // Geometryを再設定
    pxShape_->setGeometry(sphere);
}

// 動的にカプセルの判定を大きくする関数
void SingleRigidBodyComponent::ResizeCapsule(float newRadius, float newHeight)
{
    if (!pxShape_)
    {
        return;
    }

    if (shapeComponent_->GetCollisionType() != "Capsule")
    {
        return;
    }

    // Geometryを取得
    physx::PxGeometryHolder holder = pxShape_->getGeometry();

    // Capsule に変換して半径を変更
    physx::PxCapsuleGeometry capsule = holder.capsule();
    capsule.radius = newRadius;
    capsule.halfHeight = newHeight * 0.5f;

    // Geometryを再設定
    pxShape_->setGeometry(capsule);
}

// 動的にボックスの判定を大きくする関数
void SingleRigidBodyComponent::ResizeBox(float newExtentX, float newExtentY, float newExtentZ)
{
    if (!pxShape_)
    {
        return;
    }

    if (shapeComponent_->GetCollisionType() != "Box")
    {
        return;
    }

    // Geometryを取得
    physx::PxGeometryHolder holder = pxShape_->getGeometry();

    // Box に変換して半径を変更
    physx::PxBoxGeometry box = holder.box();
    box.halfExtents.x = newExtentX;
    box.halfExtents.y = newExtentY;
    box.halfExtents.z = newExtentZ;

    // Geometryを再設定
    pxShape_->setGeometry(box);
}

void MultiRigidBodyComponent::Initialize(physx::PxPhysics* physics)
{
#if 1
    using namespace physx;
    auto& model = meshComponent_->model;

    animatedNodes_ = model->GetNodes();

    ComputeGlobalTransforms(animatedNodes_);

    //// owner_ の position と rotaion を取得
    //// owner_ のTransform（ワールド空間基準）をPhysX Transformに変換
#if 1
    //DirectX::XMFLOAT4X4 world = collisionComponent_->GetComponentWorldTransform().ToWorldTransform();
    //PxTransform ownerTransform = PhysicsHelper::ToPxTransform(world);

    DirectX::XMFLOAT3 pos = meshComponent_->GetOwner()->GetPosition(); // world空間に生成しないと当たり判定がおかしくなる
    DirectX::XMFLOAT4 rot = meshComponent_->GetOwner()->GetQuaternionRotation();
    //PxVec3 pxPosition(pos.x, pos.y, pos.z);
    PxVec3 pxPosition(pos.x, pos.y, pos.z);
    PxQuat pxRotation(rot.x, rot.y, rot.z, rot.w);
    // PxTransform を作成して使用する
    PxTransform ownerTransform(pxPosition, pxRotation);

#endif // 0

    // 各 Node から mesh を持っているノードのみを処理する
    for (size_t nodeIndex = 0; nodeIndex < animatedNodes_.size(); ++nodeIndex)
    {
        const auto& node = animatedNodes_[nodeIndex];

        if (node.mesh < 0)
        {// mesh　を持たない node はスキップ
            continue;
        }

        auto& mesh = model->meshes[node.mesh];
        PxTransform nodeTransform = PhysicsHelper::ToPxTransform(node.globalTransform);
        PxTransform worldPx = ownerTransform * nodeTransform;

        // ノードのワールド変換を PhysX 変換に
        //PxTransform nodePx = PhysicsHelper::ToPxTransform(node.globalTransform);
        //PxTransform worldPx = ownerTransform * nodePx;

        // RigidDynamic を作成
        PxRigidDynamic* currentBody = physics->createRigidDynamic(worldPx);
        currentBody->userData = meshComponent_->GetOwner();

        std::vector<DirectX::XMFLOAT3> physicsVertices = ReturnPhysxVertices(mesh);
        PxConvexMesh* convexMesh = ToPxConvexMesh(physics, physicsVertices);
        bool isMeter = meshComponent_->model->isModelInMeters;
        float unitScale = isMeter ? 1.0f : 0.01f;

        //const DirectX::XMFLOAT3 scale = meshComponent_->GetComponentScale();
        const DirectX::XMFLOAT3 scale = { unitScale,unitScale,unitScale };
        PxConvexMeshGeometry geometry(convexMesh, PxMeshScale(physx::PxVec3(scale.x, scale.y, scale.z)));
        PxMaterial& material = *Physics::Instance().GetMaterial(PhysicsMaterialType::Default);
        //PxMaterial& mat = *Physics::Instance().GetMaterial();
        //mat.setStaticFriction(1.0f);
        //mat.setDynamicFriction(1.0f);
        //mat.setRestitution(0.0f); // バウンドしないように
        if (!geometry.isValid())
        {
            OutputDebugStringA("PxConvexMeshGeometry is invalid.\n");
            continue;
        }
        if (!convexMesh)
        {
            OutputDebugStringA("ToPxConvexMesh failed: convexMesh is null.\n");
            continue;
        }

        //if (primitive.cachedVertices.size() < 4 || primitive.cachedVertices.size() > 256)
        //if (mesh.primitives[0].cachedVertices.size() < 4 || mesh.primitives[0].cachedVertices.size() > 256)
        //{
        //    OutputDebugStringA("ConvexMesh creation skipped due to invalid vertex count.\n");
        //    continue;
        //}
        if (!geometry.isValid())
        {
            OutputDebugStringA("PxConvexMeshGeometry is invalid.\n");
            continue;
        }
        PxShape* pxShape_ = physics->createShape(geometry, material, true); // 専用 shape を作成する
        //pxShape_->userData = meshComponent_;   // MeshComponent へのポインタ
        pxShape_->userData = collisionComponent_;   // MeshComponent へのポインタ

        physx::PxFilterData filterData;
        filterData.word0 = layer_;
        filterData.word1 = mask_;

        pxShape_->setSimulationFilterData(filterData);
        pxShape_->setQueryFilterData(filterData);
        pxShape_->setLocalPose(PxTransform(PxIdentity));
        pxShape_->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
        pxShape_->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
        pxShape_->setFlag(PxShapeFlag::eVISUALIZATION, true); // これがないと表示されない
        // nodeTransform（ローカル空間）をワールド空間に変換（ownerTransformと合成）
        PxTransform worldTrnasform = ownerTransform * nodeTransform;

        // RigidDynamic を作成
        //PxRigidDynamic* curretBody/* pxActor_ */ = physics->createRigidDynamic(worldTrnasform);
        //curretBody->userData = meshComponent_->GetOwner();     // Actor へのポインタ

        // 重力を無効化する
        //curretBody->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

        // 最初キネマティックとして登録する
        currentBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        //currentBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);

        //currentBody->setActorFlag(PxActorFlag::eVISUALIZATION, true);
        //currentBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
        //PxReal pxMass = 0.0f;
        //PxMat33 inertia;
        //PxVec3 centerOfMass;
        //convexMesh->getMassInformation(pxMass, inertia, centerOfMass);
        //float volumeScale = pxMass;       // 体積を取得
        //float baseMass = 10.0f; // 基準質量
        //float mass = baseMass * volumeScale;
        //currentBody->setMass(mass);
        currentBody->setMass(20.0f);
        currentBody->setMassSpaceInertiaTensor(PxVec3(1.0f, 1.0f, 1.0f));  // Y軸の回転を抑える
        currentBody->setLinearDamping(0.9f);
        //currentBody->setLinearDamping(0.3f);
        currentBody->setAngularDamping(0.8f);

        currentBody->attachShape(*pxShape_);
        // 今回はこの時点で addActor　すると 物理演算の挙動に差し支えるからやめる
        //Physics::Instance().GetScene()->addActor(*curretBody);

        nodeIndexToRigidBody_[nodeIndex] = currentBody;
        rigidBodies_.push_back(currentBody);
        pxShapes_.push_back(pxShape_);
        //}
    }
#else
    using namespace physx;
    auto& model = owner_->model;

    // owner のワールドTransform（原点と回転）を取得
    DirectX::XMFLOAT3 pos = owner_->GetOwner()->GetPosition();
    DirectX::XMFLOAT4 rot = owner_->GetOwner()->GetQuaternionRotation();
    PxTransform ownerTransform(PxVec3(pos.x, pos.y, pos.z), PxQuat(rot.x, rot.y, rot.z, rot.w));

    // スケール（1つだけ取得、ノード単位に変更したければこの取得位置を工夫）
    const DirectX::XMFLOAT3 scale = owner_->GetComponentScale();
    PxVec3 pxScale(scale.x, scale.y, scale.z);

    // 前回の剛体・Transformを保持
    PxRigidDynamic* previousBody = nullptr;
    DirectX::XMFLOAT4X4 previousNodeTransform;

    animatedNodes_ = model->nodes;

    for (size_t nodeIndex = 0; nodeIndex < animatedNodes_.size(); ++nodeIndex)
    {
        const auto& node = animatedNodes_[nodeIndex];

        if (node.mesh < 0)
        {
            continue; // メッシュを持たないノードはスキップ
        }

        const auto& mesh = model->meshes[node.mesh];
        PxMaterial& material = *Physics::Instance().GetMaterial();

        // ノードのワールドTransformを取得
        PxTransform nodeTransform = PhysicsHelper::ToPxTransform(node.globalTransform);
        PxTransform worldTransform = ownerTransform * nodeTransform;

        // 剛体を作成（primitiveが複数あっても1個にまとめる）
        PxRigidDynamic* currentBody = physics->createRigidDynamic(worldTransform);
        currentBody->userData = owner_->GetOwner(); // Actorポインタなど

        for (const auto& primitive : mesh.primitives)
        {
            PxConvexMesh* convexMesh = ToPxConvexMesh(physics, primitive.cachedVertices);
            PxConvexMeshGeometry geometry(convexMesh, PxMeshScale(pxScale));

            PxShape* shape = physics->createShape(geometry, material);
            shape->userData = owner_;

            PxFilterData filterData;
            filterData.word0 = layer_;
            filterData.word1 = mask_;
            shape->setSimulationFilterData(filterData);
            shape->setQueryFilterData(filterData);

            shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

            currentBody->attachShape(*shape);
        }

        // シーンに剛体を追加
        Physics::Instance().GetScene()->addActor(*currentBody);

        // 登録
        nodeIndexToRigidBody_[nodeIndex] = currentBody;
        rigidBodies_.push_back(currentBody);

        // 前の剛体がある場合、ジョイントで接続
        if (previousBody)
        {
            // Transform A（前）と B（今）を PxTransform に変換
            PxTransform a = PhysicsHelper::ToPxTransform(previousNodeTransform);
            PxTransform b = PhysicsHelper::ToPxTransform(node.globalTransform);

            // A空間でのBの相対Transform
            PxTransform relativeToA = a.getInverse() * b;

            // FixedJoint 作成
            PxFixedJoint* joint = PxFixedJointCreate(*physics,
                previousBody, PxTransform(PxIdentity),
                currentBody, relativeToA);
            joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, true);
        }

        // 次のジョイント用に保存
        previousBody = currentBody;
        previousNodeTransform = node.globalTransform;
    }
#endif
}

void MultiRigidBodyComponent::Destroy()
{
    using namespace physx;
#if 0
    for (physx::PxRigidActor* actor : rigidBodies_)
    {
        if (actor)
        {
            if (physx::PxScene* scene = actor->getScene())
            {// actor が scene に属している時だけ
                scene->removeActor(*actor);
            }

            actor->release();
        }
    }

#endif // 0
    for (auto actor : rigidBodies_)
    {
        if (auto scene = actor->getScene())
        {
            scene->removeActor(*actor);
        }


        PxU32 nb = actor->getNbShapes();
        std::vector<PxShape*> shapes(nb);
        actor->getShapes(shapes.data(), nb);

        for (PxShape* shape : shapes)
        {
            PxGeometryHolder holder = shape->getGeometry();
            if (holder.getType() == PxGeometryType::eCONVEXMESH)
            {
                auto geom = holder.convexMesh(); // PxConvexMeshGeometry
                geom.convexMesh->release();
            }

            //if (Actor* ownerA = static_cast<Actor*>(actor->userData))
            //{
            //    char b[256];
            //    sprintf_s(b, "Actor Name :%s is destroy\n", ownerA->GetName());
            //    OutputDebugStringA(b);
            //}
            actor->detachShape(*shape);
            shape->release();
        }
        actor->release();

    }

    rigidBodies_.clear();


    pxShapes_.clear();
    nodeIndexToRigidBody_.clear();
    animatedNodes_.clear();
    isAddedToScene_ = false;
}



void MultiRigidBodyComponent::Tick(float deltaTime)
{
    if (animatedNodes_.empty())
    {
        return;
    }

    // owner_ の worldTransform を取得
    DirectX::XMFLOAT4X4 ownerTransform = meshComponent_->GetOwner()->GetWorldTransform();
    DirectX::XMMATRIX OwnerMatrix = DirectX::XMLoadFloat4x4(&ownerTransform);

    using namespace physx;
#if 1
    for (const auto& [nodeIndex, body] : nodeIndexToRigidBody_)
    {

        {
#if 0
            DirectX::XMFLOAT3 pos = animatedNodes_[nodeIndex].translation;
            DirectX::XMFLOAT4 rot = animatedNodes_[nodeIndex].rotation;
            DirectX::XMFLOAT3 scale = animatedNodes_[nodeIndex].scale;
#else
            const PxTransform& pxT = body->getGlobalPose();
            DirectX::XMFLOAT3 pos = { pxT.p.x, pxT.p.y, pxT.p.z };
            DirectX::XMFLOAT4 rot = { pxT.q.x, pxT.q.y, pxT.q.z, pxT.q.w };
            //DirectX::XMFLOAT3 scale = meshComponent_->GetComponentScale();
            //DirectX::XMFLOAT3 scale = animatedNodes_[nodeIndex].scale;
            DirectX::XMFLOAT3 scale = { 1.0f,1.0f,1.0f };
#endif
            //DirectX::XMFLOAT3 pos = { 0.0f,0.0f,0.0f };
            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rot));
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

            DirectX::XMMATRIX LocalMatrix = S * R * T;

            // 親の Transform を乗算して最終の Transform にする
            DirectX::XMMATRIX M = LocalMatrix/* * OwnerMatrix*/;

            DirectX::XMStoreFloat4x4(&animatedNodes_[nodeIndex].globalTransform, M);
        }
    }

#endif // 0

    //for (const auto& [nodeIndex, body] : nodeIndexToRigidBody_)
    //{
    //    // 動的剛体かチェック＆キャスト
    //    physx::PxRigidDynamic* dynamicBody = body->is<physx::PxRigidDynamic>();
    //    if (dynamicBody)
    //    {
    //        const PxTransform& pxT = dynamicBody->getGlobalPose();

    //        // PxTransform -> DirectX::XMFLOAT4x4 変換
    //        DirectX::XMMATRIX M =
    //            DirectX::XMMatrixScaling(animatedNodes_[nodeIndex].scale.x,
    //                animatedNodes_[nodeIndex].scale.y,
    //                animatedNodes_[nodeIndex].scale.z) *
    //            DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(pxT.q.x, pxT.q.y, pxT.q.z, pxT.q.w)) *
    //            DirectX::XMMatrixTranslation(pxT.p.x, pxT.p.y, pxT.p.z);

    //        DirectX::XMStoreFloat4x4(&animatedNodes_[nodeIndex].globalTransform, M);
    //    }
    //}
}

void TriangleMeshRigidBodyComponent::Initialize(physx::PxPhysics* physics)
{
    using namespace physx;

    auto& model = owner_->model; //batchMeshes

    //モデルがメートル単位か cm単位の時はfalseにする
    bool isMeter = model->isModelInMeters;

    float unitScale = isMeter ? 1.0f : 0.01f;

    PxTolerancesScale tolerancesScale;
    PxCookingParams cookingParams(tolerancesScale);
    cookingParams.convexMeshCookingType = PxConvexMeshCookingType::Enum::eQUICKHULL;
    cookingParams.gaussMapLimit = 256;

    //　頂点数だけ先に合計
    size_t totalVertexCount = 0;
    for (const auto& mesh : model->batchMeshes)
    {
        totalVertexCount += mesh.cachedVertices.size();
    }
    // 頂点数に応じて分岐する
    bool use32BitIndex = (totalVertexCount >= 65536);

    // 頂点とインデックスを構築
    std::vector<PxVec3> vertices;
    std::vector<PxU32> indices32;
    std::vector<PxU16> indices16;

    PxU32 vertexOffset = 0;

    for (const auto& mesh : model->batchMeshes)
    {
        for (const auto& vertex : mesh.cachedVertices)
        {
            vertices.emplace_back(vertex.position.x * unitScale, vertex.position.y * unitScale, vertex.position.z * unitScale);
        }

        if (use32BitIndex)
        {
            for (const auto& index : mesh.cachedIndices)
            {
                indices32.push_back(index + vertexOffset);
            }
        }
        else
        {
            for (const auto& index : mesh.cachedIndices)
            {
                indices16.push_back(static_cast<PxU16>(index + vertexOffset));
            }
        }

        // 頂点数オフセット込みで加算
        vertexOffset += static_cast<PxU32>(mesh.cachedVertices.size());
    }

    PxTriangleMeshDesc pxMeshDesc;
    pxMeshDesc.points.count = static_cast<PxU32>(vertices.size());
    pxMeshDesc.points.stride = sizeof(PxVec3);
    pxMeshDesc.points.data = vertices.data();

    if (use32BitIndex)
    {
        pxMeshDesc.triangles.count = static_cast<PxU32>(indices32.size() / 3);
        pxMeshDesc.triangles.stride = 3 * sizeof(PxU32);
        pxMeshDesc.triangles.data = indices32.data();
        //pxMeshDesc.flags = PxMeshFlag::eFLIPNORMALS; // 32ビットインデックス これレイキャスト効かなくなる
    }
    else
    {
        pxMeshDesc.triangles.count = static_cast<PxU32>(indices16.size() / 3);
        pxMeshDesc.triangles.stride = 3 * sizeof(PxU16);
        pxMeshDesc.triangles.data = indices16.data();
        pxMeshDesc.flags = PxMeshFlag::e16_BIT_INDICES;
    }

    PxTriangleMesh* triangleMesh = nullptr;
    PxDefaultMemoryOutputStream writeBuffer;
    if (!PxCookTriangleMesh(cookingParams, pxMeshDesc, writeBuffer))
    {
        _ASSERT(L" PxCookTriangleMesh failed.");
    }

    PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
    triangleMesh = physics->createTriangleMesh(readBuffer);

    PxTriangleMeshGeometry geometry(triangleMesh);
    pxShape_ = physics->createShape(geometry, *material_, true);
    pxShape_->userData = owner_;   // MeshComponent へのポインタ

    // 衝突フィルタ
    //physx::PxFilterData pxFilterData = pxShape_->getQueryFilterData();
    physx::PxFilterData queryData = pxShape_->getQueryFilterData();
    //queryData.word0 = 0xFFFFFFFF;
    //queryData.word1 = 0xFFFFFFFF;
    queryData.word0 = layer_;
    //queryData.word0 = 1;
    pxShape_->setQueryFilterData(queryData);
    pxShape_->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    pxShape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true); // 念のため
    //PxFilterData filterData(layer_, mask_, 0, 0);
    //pxShape_->setSimulationFilterData(filterData);
    //pxShape_->setQueryFilterData(filterData);

    DirectX::XMFLOAT3 pos = owner_->GetOwner()->GetPosition();
    DirectX::XMFLOAT4 rot = owner_->GetComponentRotation();
    PxVec3 pxPosition(pos.x, pos.y, pos.z);
    PxQuat pxRotation(rot.x, rot.y, rot.z, rot.w);
    PxTransform transform(pxPosition, pxRotation);

    pxActor_ = physics->createRigidStatic(transform);
    pxActor_->userData = owner_->GetOwner();     // Actor へのポインタ
    pxActor_->attachShape(*pxShape_);


    physx::PxTransform pose(
        PxVec3(pos.x, pos.y, pos.z),
        PxQuat(rot.x, rot.y, rot.z, rot.w).getNormalized()
    );

    pxActor_->setGlobalPose(pose);

}