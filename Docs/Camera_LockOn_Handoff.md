
• # TPS → LockOn カメラ切替問題 引き継ぎメモ

  ## 現在確認できている原因

  主な原因は、DarkCameraActor 内でカメラ回転を管理する経路が二重化していることです。

  1. 切替中は currentYaw/currentPitch を補間する。
  2. ブレンド完了後、LockOn 通常更新で desiredYaw を敵方向から再計算する。
  3. UpdateRotation() が currentYaw = desiredYaw を補間なしで実行する。

  そのため、TPS→LockOn のブレンド終了直後に、currentYaw が新しい desiredYaw へ急に変化する可能性があります。

  また、UpdateBlend() では eye を開始ポーズと終了ポーズの間で補間せず、切替先モードの距離を使って毎フレーム再計算してい
  ます。TPS と LockOn で距離が異なるため、切替開始直後にカメラ位置が不連続になる可能性があります。

  さらに、ブレンド中は CameraComponent::yaw/pitch に対して SetYawAndPitch() を呼んでいません。ブレンド中の
  DarkCameraActor の回転値と CameraComponent 内の回転値が一時的に一致しません。

  ## 関係するファイル

  今回確認したファイルは以下の3つだけです。

  - Source/Game/Actors/Player/Player.cpp
  - Source/Game/Actors/Camera/DarkGameCamera.h
  - Source/Game/Actors/Camera/DarkGameCamera.cpp
  - Source/Components/Camera/CameraComponent.h
  - Source/Components/Camera/CameraComponent.cpp

  ## 重要な変数

  ### CameraMode

  enum class CameraMode
  {
      TPS,
      Focus,
      LockOn,
  };

  - currentMode
    現在確定しているカメラモード。

  - requestMode
    Player が要求している次のカメラモード。

  requestMode != currentMode になるとブレンドが開始されます。

  ### desiredYaw / desiredPitch

  次に向きたい角度です。

  TPS では右スティック入力から更新されます。

  LockOn では毎フレーム、プレイヤーから敵への方向を使って再計算されます。

  desiredYaw = atan2f(toEnemy.x, toEnemy.z);
  desiredYaw += XMConvertToRadians(lockOnYawOffsetDegree);
  desiredPitch = XMConvertToRadians(lockOnPitchDegree);

  ### currentYaw / currentPitch

  現在のカメラポーズ計算に使用される角度です。

  通常更新時は以下のように即時代入されます。

  currentYaw = desiredYaw;
  currentPitch = desiredPitch;

  ブレンド中だけ LerpAngle / std::lerp が使われます。

  ### CameraComponent::yaw / pitch

  CameraComponent 内で保持される回転値です。

  mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);

  で更新され、Actor の Quaternion に変換されます。

  ただし DarkCameraActor は常に useLookTarget = true にしているため、最終的な View 行列は主に eye と lookTarget の組み合
  わせで決まります。

  ## 現在の処理の流れ

  ### 1. Player がカメラモードを要求

  Player::UpdateMovement() が LockOn の押下状態を確認します。

  - LockOn 押下中かつボス戦中
    SetRequestMode(CameraMode::LockOn)

  - それ以外
    SetRequestMode(CameraMode::TPS)

  DarkCameraActor::SetRequestMode() は、すでにブレンド中の場合は要求を無視します。

  ### 2. DarkCameraActor が切替を検出

  DarkCameraActor::Update() で、

  if (requestMode != currentMode)
  {
      StartBlend(currentMode, requestMode);
  }

  が実行されます。

  ### 3. StartBlend が開始・終了ポーズを作成

  TPS→LockOn の場合、

  - 開始角度: 現在の currentYaw/currentPitch
  - 終了角度: CreateLockOnInfo() で計算した敵方向
  - LockOn pitch: lockOnPitchDegree
  - LockOn yaw: 敵方向 + lockOnYawOffsetDegree

  が使われます。

  ### 4. UpdateBlend が約0.3秒補間

  以下を補間します。

  - currentPose.target
  - currentYaw
  - currentPitch

  一方、currentPose.eye はポーズ間補間ではなく、補間中の角度とモード別距離から再計算されます。

  ### 5. ブレンド完了

  ブレンド完了時に、

  isBlending = false;
  currentMode = requestMode;
  desiredYaw = currentYaw;
  desiredPitch = currentPitch;

  が実行されます。

  ### 6. 次フレームから通常更新

  LockOn 通常更新では、敵の現在位置から desiredYaw を再計算します。

  その後、

  currentYaw = desiredYaw;
  currentPitch = desiredPitch;

  が補間なしで実行されます。

  この「ブレンド終了時の角度」と「次フレームに再計算された角度」の差が、急な向きの変化として現れます。

  ## 修正するときの方針

  ### 1. 切替中と通常更新で回転の正本を統一する

  DarkCameraActor の currentYaw/currentPitch と CameraComponent の yaw/pitch が切替中も一致するようにします。

  ### 2. ブレンド終了直後の再スナップをなくす

  LockOn の敵方向をブレンド終了後に即時適用せず、currentYaw から desiredYaw へ継続的に補間する設計にします。

  特に UpdateRotation() 内の補間を有効化する方向が候補です。

  ### 3. eye もポーズとして補間する

  blendStartPose.eye と blendTargetPose.eye を利用し、距離を切替先モードへ即時変更しないようにします。

  現在は角度・target と距離計算が別系統になっているため、位置変化が不連続になっています。

  ### 4. 動的な LockOn ターゲットの扱いを整理する

  LockOn 中は敵とプレイヤーが動くため、切替開始時に固定した終了角度と、毎フレーム再計算される敵方向に差が生じます。

  以下のどちらかを明確にする必要があります。

  - ブレンド中は終了ポーズを固定する
  - ブレンド中も敵方向を更新し、目標へ滑らかに追従する

  ### 5. 切替中の入力要求を保持する

  SetRequestMode() がブレンド中の要求を破棄しているため、切替途中の操作が遅れて反映されます。

  最新の要求モードを保存し、ブレンド完了後に必要なら逆方向へ遷移できる設計が望ましいです。

  ## まだ変更していないこと

  - ソースコードは変更していません。
  - desiredYaw/currentYaw の更新処理は変更していません。
  - UpdateBlend() の eye 補間処理は変更していません。
  - UpdateRotation() の補間処理は変更していません。
  - CameraComponent の useLookTarget や View 行列処理は変更していません。
  - SetRequestMode() のブレンド中の挙動は変更していません。
  - ビルド・実行による修正検証はまだ行っていません。