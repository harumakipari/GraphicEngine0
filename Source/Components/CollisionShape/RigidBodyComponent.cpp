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
	PxVec3 pxPosition(pos.x, pos.y, pos.z);
	PxQuat pxRotation(rot.x, rot.y, rot.z, rot.w);
	// physx の原点を上げるためPxTransform を作成して使用する
	PxTransform transform(pxPosition, pxRotation);
	pxActor_ = physics->createRigidDynamic(transform);
	pxActor_->userData = shapeComponent_->GetOwner();    // Actor へのポインタ
	material_ = Physics::Instance().GetMaterial(shapeComponent_->GetPhysicsMaterialType());

	// 第三引数を true にすることで一意的な専用の shape にする
	pxShape_ = physics->createShape(info.geometry.any(), *material_, true);

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
		pxShape_->setLocalPose(pxShapeTransform);	//シーンに設定(カプセルを立たせるため)通常は横向き
	}
	else
	{// カプセル以外の時オフセット方向に補正する
		pxShape_->setLocalPose(physx::PxTransform(physx::PxVec3(shapeComponent_->GetCollisionOffsetX(), shapeComponent_->GetCollisionOffsetY(), shapeComponent_->GetCollisionOffsetZ())));
	}

	// レイキャストなど用のレイヤー設定
	physx::PxFilterData pxFilterData = pxShape_->getQueryFilterData();
	pxFilterData.word0 = layer_;
	pxShape_->setQueryFilterData(pxFilterData);

	pxShape_->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
	{// デフォルト
		pxShape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
	}
	pxActor_->attachShape(*pxShape_);

	pxActor_->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, shapeComponent_->IsKinematic());
	pxActor_->setMaxLinearVelocity(300.0f);
	pxActor_->setMaxAngularVelocity(300.0f);

	pxActor_->setLinearDamping(0.0f);
	pxActor_->setAngularDamping(0.0f);

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
		PxTransform pxT = PhysicsHelper::ToPxTransform(t);
		// 当たり判定が地面の下に行くのを防ぐ
		pxActor_->setKinematicTarget(pxT);
	}
	else
	{// ダイナミックの時
		// 上に補正する
		PxTransform pxT = pxActor_->getGlobalPose();
		// 当たり判定が地面の下に行くのを防ぐ
		if (shapeComponent_->GetCollisionType() == "Sphere")
		{// 動的に半径を変更した時に使用
			pxT.p.y += currentRadius_;
		}
		GetOwner()->SetPosition({ pxT.p.x,pxT.p.y,pxT.p.z });
		GetOwner()->SetQuaternionRotation({ pxT.q.x,pxT.q.y,pxT.q.z ,pxT.q.w });
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
	using namespace physx;
	auto& model = meshComponent_->model;

	animatedNodes_ = model->GetNodes();

	ComputeGlobalTransforms(animatedNodes_);

	// owner_ の position と rotaion を取得
	// owner_ のTransform（ワールド空間基準）をPhysX Transformに変換
	DirectX::XMFLOAT3 pos = meshComponent_->GetOwner()->GetPosition(); // world空間に生成しないと当たり判定がおかしくなる
	DirectX::XMFLOAT4 rot = meshComponent_->GetOwner()->GetQuaternionRotation();
	PxVec3 pxPosition(pos.x, pos.y, pos.z);
	PxQuat pxRotation(rot.x, rot.y, rot.z, rot.w);
	// PxTransform を作成して使用する
	PxTransform ownerTransform(pxPosition, pxRotation);


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

		// RigidDynamic を作成
		PxRigidDynamic* currentBody = physics->createRigidDynamic(worldPx);
		currentBody->userData = meshComponent_->GetOwner();

		std::vector<DirectX::XMFLOAT3> physicsVertices = ReturnPhysxVertices(mesh);
		PxConvexMesh* convexMesh = ToPxConvexMesh(physics, physicsVertices);
		bool isMeter = meshComponent_->model->isModelInMeters;
		float unitScale = isMeter ? 1.0f : 0.01f;

		const DirectX::XMFLOAT3 scale = { unitScale,unitScale,unitScale };
		PxConvexMeshGeometry geometry(convexMesh, PxMeshScale(physx::PxVec3(scale.x, scale.y, scale.z)));
		PxMaterial& material = *Physics::Instance().GetMaterial(PhysicsMaterialType::Default);
		if (!geometry.isValid())
		{
			Logger::Warning("PxConvexMeshGeometry is invalid.\n");
			continue;
		}
		if (!convexMesh)
		{
			Logger::Warning("ToPxConvexMesh failed: convexMesh is null.\n");
			continue;
		}

		if (!geometry.isValid())
		{
			Logger::Warning("PxConvexMeshGeometry is invalid.\n");
			continue;
		}
		PxShape* pxShape_ = physics->createShape(geometry, material, true); // 専用 shape を作成する
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

		// 最初キネマティックとして登録する
		currentBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		currentBody->setMass(20.0f);
		currentBody->setMassSpaceInertiaTensor(PxVec3(1.0f, 1.0f, 1.0f));  // Y軸の回転を抑える
		currentBody->setLinearDamping(0.9f);
		currentBody->setAngularDamping(0.8f);

		currentBody->attachShape(*pxShape_);
		nodeIndexToRigidBody_[nodeIndex] = currentBody;
		rigidBodies_.push_back(currentBody);
		pxShapes_.push_back(pxShape_);
	}
}

void MultiRigidBodyComponent::Destroy()
{
	using namespace physx;
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
	for (const auto& [nodeIndex, body] : nodeIndexToRigidBody_)
	{

		{
			const PxTransform& pxT = body->getGlobalPose();
			DirectX::XMFLOAT3 pos = { pxT.p.x, pxT.p.y, pxT.p.z };
			DirectX::XMFLOAT4 rot = { pxT.q.x, pxT.q.y, pxT.q.z, pxT.q.w };
			DirectX::XMFLOAT3 scale = { 1.0f,1.0f,1.0f };
			DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
			DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rot));
			DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

			DirectX::XMMATRIX LocalMatrix = S * R * T;

			// 親の Transform を乗算して最終の Transform にする
			DirectX::XMMATRIX M = LocalMatrix/* * OwnerMatrix*/;

			DirectX::XMStoreFloat4x4(&animatedNodes_[nodeIndex].globalTransform, M);
		}
	}
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

			for (size_t i = 0; i < mesh.cachedIndices.size(); i += 3)
			{
				indices32.push_back(mesh.cachedIndices[i + 0] + vertexOffset);
				indices32.push_back(mesh.cachedIndices[i + 2] + vertexOffset);
				indices32.push_back(mesh.cachedIndices[i + 1] + vertexOffset);
			}
		}
		else
		{
			for (size_t i = 0; i < mesh.cachedIndices.size(); i += 3)
			{
				indices16.push_back(static_cast<PxU16>(mesh.cachedIndices[i + 0] + vertexOffset));
				indices16.push_back(static_cast<PxU16>(mesh.cachedIndices[i + 2] + vertexOffset));
				indices16.push_back(static_cast<PxU16>(mesh.cachedIndices[i + 1] + vertexOffset));
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
	physx::PxFilterData queryData = pxShape_->getQueryFilterData();
	queryData.word0 = layer_;
	pxShape_->setQueryFilterData(queryData);
	pxShape_->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
	pxShape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true); // 念のため
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