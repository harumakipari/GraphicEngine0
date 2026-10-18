# Veil of Aurora 技術評価レポート

## 調査範囲と読み方

2026-10-17 に `Source/` 配下の主要 C++ 実装を静的に調査した。対象は Actor/Component/Scene、Player、Grux、Behavior Tree、DarkCamera、PhysX ラッパー、DirectX 11 描画、アセット、UI とチュートリアルである。実行時プロファイル、GPU キャプチャ、プレイテスト、全関数の網羅読解は**未実施**である。したがって性能値や「当時の意図」は断定しない。

「確認できた事実」はファイルと関数で裏付けた内容、「評価・推測」はそこから導く設計評価として区別する。

## 結論

このコードベースの強みは、単にエンジンを自作していることではなく、**一人称では見えにくいゲームプレイ上の境界条件を、実装とデバッグ可視化まで落とし込んでいること**にある。特に、アニメーション Notify を中心とした戦闘、剣の連続Sweep、Just Dodge/Rush の時間制御、LockOn の構図補正、Grux の実行継続型 Behavior Tree は、アクションゲーム固有の難所に正面から取り組んでいる。

一方で、完成度を上げるための反復の結果、`GameScene`、`Player`、`GruxEnemy`、`DarkCameraActor` が「統合地点」を越えて多責務化している。現行規模の単一プレイヤー・単一ボス作品では実用的な面もあるが、敵・ステージ・演出を増やすと変更の波及と回帰確認が急速に重くなる構造である。

## 全体アーキテクチャ

```text
Scene / SceneBase
  ├─ ActorManager ─ Actor ─ Component / SceneComponent ツリー
  ├─ CameraManager ─ DarkCameraActor / MovieCameraManagerActor
  ├─ PhysX Physics singleton
  ├─ SceneRenderer / render queues / G-Buffer / post effects
  └─ GameScene ─ Player / GruxEnemy / tutorial UI / battle flow
```

### 確認できた事実

- `Scene::_boot()` は Scene と `ActorManager` を作成し、Scene を所有者として設定してから初期化する。`Source/Engine/Scene/Scene.h::_boot`。
- `ActorManager::CreateAndRegisterActorWithTransform()` は Actor を登録し、Root Component、Transform、初期化、Transform 更新までを一定順序で実施する。`Source/Core/ActorManager.h:81`。
- `Actor::AddComponent()` は所有コンテナ、名前重複回避、SceneComponent の親子接続、`OnRegister()` を一箇所に集約する。`Source/Core/Actor.h:75`。
- `SceneBase` はライト、G-Buffer、シャドウ、TAA、ポストエフェクト、UI、レンダーHookを所有する。`Source/Engine/Scene/SceneBase.h`、`SceneBase.cpp::Initialize/Render/DeferredRender`。

### 評価

小規模の自作エンジンとして、Actorをゲームオブジェクト、Componentを機能、Sceneを寿命・描画単位とする役割は読みやすい。`shared_ptr` 所有と `weak_ptr` 参照の組合せも、ComponentがActorを逆参照する構造に対して妥当である。

ただし SceneBase が描画パイプラインだけでなく、特定キャラクターのゴースト描画や Grux 参照も持つため、エンジン層とゲーム層の境界は一部崩れている。`SceneBase.cpp` は `Player.h` と `GruxEnemy.h` を直接 include し、遅延描画中に両者の特別処理をしている。これは汎用Scene基盤としては密結合である。

## 技術的に評価できる実装

### 1. アニメーションNotifyをゲームプレイ時間軸にする戦闘設計

- **対象**: `AnimationController`、`Player::OnAnimationNotifyBegin/End/Event`、`GruxEnemy::OnAnimationNotifyBegin/End`。
- **仕組み（事実）**: Playerは HitBox、InputWindow、Invincible、TransitionWindow、JustDodgeWindow、MotionWarp 等の Notify を受けて状態を切り替える。`Player.cpp:1003`。Gruxも DangerWindow を含む同じ時間軸のNotifyを扱う。`GruxEnemy.cpp:5029`。
- **優れている点（評価）**: 攻撃判定、無敵、入力受付、演出を秒単位の別Timerではなくアニメーションに同期できる。アニメ差し替え時にゲームプレイ窓を修正しやすく、見た目と判定のずれを減らせる。
- **解決する課題**: フレーム依存のハードコード、攻撃モーションの変更時に発生する判定ずれ。
- **面接での論点**: 「アニメを再生する」だけでなく、Notifyをゲームプレイイベントのデータ境界にし、HitBox・キャンセル・無敵・ジャスト回避窓を同期させた、と説明できる。

### 2. 高速武器に対する連続Sweepと接触順序の選択

- **対象**: `Player::Update` の SwordSweep、`Physics::SphereCast`。
- **仕組み（事実）**: 剣Root/Mid/Tipについて前フレーム位置から現在位置までSphereCastし、通常Hit、初期重なり、ダメージ用候補を分け、TOIや貫入深さで一つを選択する。`Player.cpp:518-640`。PhysX側はSweep結果に `initialOverlap`、距離、法線、Actorを返す。`Physics.cpp::SphereCast`。
- **優れている点（評価）**: 点判定だけでは高速な剣がすり抜ける。三点の連続判定と初期重なりの扱いにより、見た目に近い攻撃判定を狙っている。単純なOverlapより実装難度が高い。
- **解決する課題**: フレームレートやモーション速度で当たり方が変わる tunneling。
- **面接での論点**: 「3本のSweepを行った」だけでなく、開始重なり、最初の接触、エフェクト位置、実ダメージ候補を分離して選んだ理由を説明する。

### 3. Playerの入力バッファと状態遷移

- **対象**: `Player::CaptureActionRequest/StoreActionRequest/TryExecuteActionRequest`、`PlayerStateDerived.*`。
- **仕組み（事実）**: `ActionRequest` は種類、残り時間、回避方向を持つ。`Player.h:59`。Dodge/Attack要求は状態で実行可能になるまで保持され、優先順位を持つ。`Player.cpp:2143`、`2846`。Stateは Idle/Running/Attack/Dodge/Damage/KnockBack/Death/Rush に分けられる。`Player.cpp:149-162`。
- **優れている点（評価）**: 入力取得と実行可能なアニメーション窓を切り離しており、アクションゲームに必要な先行入力を実装している。全てをPlayer::Updateに直書きするより、状態ごとのEnter/Update/Exitが追いやすい。
- **解決する課題**: 入力の1フレーム取りこぼしと、攻撃・回避の競合。
- **面接での論点**: バッファ長、優先度、入力を破棄する条件（被弾/死亡等）はゲームフィールと不整合防止のトレードオフである。

### 4. Just Dodge/Rushを複数系統の時間制御として扱う

- **対象**: `Player::UpdateJustDodgeTimeScales`、`StartJustDodgeSuccess`、`PlayerRushState`。
- **仕組み（事実）**: Player側とBoss側のSlow Phaseを別管理し、Hold、Return、RushHoldを区別する。`Player.h:50-57`, `902-917`。Rush開始・終了はGruxへ通知される。`PlayerStateDerived.cpp:502`, `727`。
- **優れている点（評価）**: 「全体を止める」一つのフラグではなく、プレイヤーと対象の復帰責務を分けている。Rush中にAIを止め、終了後に復帰処理を行う必要を意識した実装になっている。
- **面接での論点**: スローは視覚演出だけでなく、AI、入力、アニメ、カメラ、ダメージ順序に影響するため、誰が時間倍率を所有するかを明確化した重要性を話せる。

### 5. 実行状態を持つBehavior Treeと攻撃選択のデバッグ

- **対象**: `BehaviorTree::Run`、`NodeBase::Inference/SelectAttackRandom`、`GruxEnemy::UpdateBehaviorTree`、`Grux*BT.cpp`。
- **仕組み（事実）**: BTは Action の `Run/Complete/Failed` を返し、Sequenceのスタックを使って次ノードへ進む。`BehaviorTree.cpp:75-106`。攻撃選択は近距離正面、FastCombo確率、繰り返しペナルティを考慮し、選択理由を記録する。`NodeBase.cpp:131-223`。Charge、Dash、Jump、Roar、FastComboは別翻訳単位に分かれる。
- **優れている点（評価）**: 長時間行動を1フレームで完結させず、BT Actionの内部状態として継続している。さらにデバッグ用に選択理由・実行回数・停止理由を残しており、確率AIの調整を観測可能にしている。
- **解決する課題**: ランダムAIが「なぜその行動をしたか」追えない問題、演出中断後のランタイム残留。
- **面接での論点**: BT自体の実装より、行動中断/死亡/Rush/フェーズ移行時に `ResetActionRuntimes()` を通して復帰させる設計を説明すると強い。

### 6. LockOn・Cameraの遷移Poseと衝突補正

- **対象**: `DarkCameraActor::StartBlend/UpdateBlend`、`UpdateLockOnComposition`、`ResolveCameraCollision`、`GameScene::UpdateLockOnTargetSelection`。
- **仕組み（事実）**: カメラは `currentMode` と `requestMode` を分け、開始Poseから目標Pose、Yaw/Pitch、距離、FOVを補間する。`DarkGameCamera.cpp:54-100`, `558-652`, `958-1030`。LockOnは敵距離・壁衝突比・必要フレーミング距離を使って距離を補正する。`DarkGameCamera.cpp:1185`。対象選択は距離と画面中心性を使う。`GameScene.cpp:1864-1985`。
- **優れている点（評価）**: モードを即時切替せず表示中Poseから補間することで、カメラ衝突補正後の位置を含めて滑らかに遷移している。LockOnを「敵を向く」だけでなく、壁際や距離で画角を守る問題として扱っている。
- **面接での論点**: カメラはPlayerに追従するだけではなく、ゲームプレイ可読性を守るシステムである。視認性、衝突、敵との距離、手動入力、演出カメラの優先度をどう競合解決したかを説明する。

### 7. DirectX 11レンダリング基盤とアセットの再利用

- **対象**: `SceneBase::Initialize/Render/DeferredRender`、`SceneRenderer`、`AssetManager::LoadModel`、`InterleavedGltfModel`。
- **仕組み（事実）**: SceneBaseはForward/Deferredを切替え、G-Buffer、シャドウ、Bloom/SSAO/SSR/Fog/DoF、TAAを組み合わせる。`SceneBase.cpp:30-149`, `438-943`。`SceneRenderer` はStaticモデルをバッチ化しInstanced drawする経路を持つ。`SceneRenderer.cpp:182`。AssetManagerはファイル名をkeyに `weak_ptr` キャッシュする。`AssetManager.cpp:11-32`。glTFモデル側にも元データのweak cacheがある。`InterleavedGltfModel.h:34`。
- **優れている点（評価）**: GPUリソースを`ComPtr`、CPU所有を`unique_ptr/shared_ptr`で扱い、モデル共有をキャッシュする基礎がある。描画パスを自作しており、ゲーム固有の材質や残像描画まで繋いでいる。
- **注意**: 実測による性能評価は未実施。

### 8. 実制作を支えるランタイム調整・可視化

- **対象**: `GruxEnemy::DrawImGuiDetails`、`DarkCameraActor::DrawImGuiDetails`、`DebugRender`、AnimationのDanger OBB保存。
- **事実**: GruxにはDanger OBBをNotifyごとに編集・保存・ワールド表示するUIがある。`GruxEnemy.cpp:2619-2730`。Cameraにはモード/FOV/衝突のデバッグ表示がある。`DarkGameCamera.cpp:3018`以降。
- **評価**: これは完成ゲームのコードとしては混在の原因にもなるが、個人制作で「調整可能性」を確保した点は強い。アクションの品質は設計だけでなく、調整速度で決まる。

## 設計上の課題

### 1. God Object化したゲームプレイ統合クラス

- **対象**: `GameScene.cpp`（5,770行）、`GruxEnemy.cpp`（8,268行）、`Player.cpp`（3,608行）、各header。
- **事実**: GameSceneは戦闘フロー、フェーズ、チュートリアル、LockOn、死亡・勝利・ムービーを保持する。GruxはAI、攻撃、当たり判定、VFX、UI、ImGui、フェーズ、カメラ連携を持つ。Playerも戦闘・UI・VFX・入力・カメラ連携を持つ。
- **問題**: 変更理由と影響範囲が一つの巨大ファイルに混ざり、状態の所有者が読みにくい。小規模でも既にレビュー・マージ・回帰確認のコストが高い。敵種類・ステージ・演出が増えるなら確実に障害になる。
- **改善**: まずファイルを機械的に分割するのではなく、`BattleFlowController`、`TutorialController`、`PlayerCombatController`、`GruxCombatController` のように「状態と更新責務を一緒に移す」。描画専用ImGuiは各ControllerのDebug UIへ残してよい。
- **リスク**: 一度に移すと更新順と暗黙依存を壊す。小さな境界から移し、プレイ確認を行うべき。

### 2. ゲーム層からの型検索・直接依存

- **対象**: `ActorManager::GetActorOfType/GetActorsOfType`、Player/Grux/GameScene各所。
- **事実**: 毎フレームまたは多数のイベントで `GetActorOfType<Player/GruxEnemy/DarkCameraActor>` と `dynamic_cast` を利用する。ActorManagerの型検索は全Actor線形走査。`ActorManager.h:150-179`。
- **問題**: 現規模では性能問題と断定しないが、依存が「任意のActorを探せる」方向へ広がる。作成順・存在保証・複数同型Actorの意味が曖昧になり、将来の複数敵や複数カメラで事故が起きやすい。
- **改善**: Player/Grux/Camera等の固有関係は、生成時にweak参照または狭いサービス接口を注入する。検索は初期化・低頻度イベントに限定する。型IDキャッシュは計測後に導入する。
- **トレードオフ**: 小作品では登録手数が増える。すべてをサービス化する必要はない。

### 3. 文字列を状態・イベントIDに使う

- **対象**: `StateMachine::ChangeState("Idle")`、`GetStateName()` 比較、Notifyの `event.parameter` 比較、BTノード名。
- **事実**: Player stateは文字列登録・比較、Notifyイベントは`"DeathCameraStart"`等、BTでは`"ChargeAttackPlan"`等で分岐する。`Player.cpp:149-162`, `1168`、`NodeBase.cpp:135-154`。
- **問題**: スペル変更が実行時にしか検出されにくく、リネームが安全でない。現状でも分岐が多く、文字列一致失敗が静かに機能欠落になる。
- **改善**: C++内の状態/ゲームプレイイベントは `enum class` と型付きpayloadへ段階移行する。アセット側に残すIDは文字列のままでも、ロード時にenumへ検証・変換する。
- **トレードオフ**: データ駆動の柔軟さは少し下がるため、アニメ名や外部JSON名まで無理にenum化しない。

### 4. グローバルSingletonと所有権の混在

- **対象**: `Physics::Instance()`、`AssetManager::Get()`、`InputSystem`、`Time`、`EffectManager`、`ResourceManager`。
- **事実**: 物理・入力・時間・エフェクトはグローバルアクセス可能。ResourceManagerは実装の多くがコメントアウトされており、AssetManagerとは別の資源管理経路がある。`Engine/Resources/ResourceManager.*`、`Graphics/Resource/AssetManager.*`。
- **問題**: Scene切替、再初期化、テスト、ロード失敗時の責務が分散する。どの資源が誰に所有され、どこで解放されるかを追いにくい。
- **改善**: 現在のSingletonをすぐ排除せず、`GameServices`/SceneContextを導入して依存を明示する。資源管理はモデル・テクスチャ・Shaderの生存方針とキャッシュkeyを一本化する。

### 5. BTの構築・ランタイムは良いが、ノード識別と乱数が脆い

- **対象**: `BehaviorTree::AddNode`、`NodeBase::SelectRandom/SelectAttackRandom`。
- **事実**: 親検索は名前で行い、選択にはCの`rand()`を使用する。`BehaviorTree.cpp:21-57`, `NodeBase.cpp:118-124`。
- **問題**: 同名ノードに弱く、シード・再現性・テストが扱いにくい。選択ロジックがNodeBaseにGrux固有の知識を持つ。
- **改善**: BT定義用のハンドル/ID、AI専用`std::mt19937`、攻撃選択ポリシーのGrux側への移動を検討する。現行の一体型ボスなら全面的な汎用BT化は過剰。

### 6. レンダラ基盤へのゲーム固有描画の混入

- **対象**: `SceneBase.cpp::DeferredRender`。
- **事実**: SceneBaseの描画中にPlayer/Gruxの特別なゴースト等を直接扱う箇所がある。`
- **問題**: 新しいActor固有の特殊描画のたびにエンジンのSceneBaseを編集する。レンダリング順は保てても、ゲーム層と基盤層の変更が強く結びつく。
- **改善**: RenderQueueに `CustomTransparent` 等を設け、Actor/Componentが描画項目を登録する。複雑な一回限り演出は既存の`RenderHook`を使う設計が現実的。

### 7. テスト・再現可能な検証が薄い

- **対象**: リポジトリ全体。
- **確認事実**: 自作ゲームランタイム向けのunit/integration testは今回のファイル探索では見つからず、`TestScene`、SoftBody試験、外部ライブラリのテストが中心。
- **問題**: 入力バッファ、BT中断、死亡/ムービー/LockOnの優先順位は回帰しやすい。現規模でもすでに重要。
- **改善**: 最初にpureな関数・状態遷移から小さなテストを追加する。例：Attack選択重み、ActionRequestの優先・期限、カメラのモード優先、Danger OBB生成。次に固定seedでBTのシナリオログを比較する。

## パフォーマンスとメモリの評価

### 確認できた良い点

- GPUリソースに`ComPtr`、Scene所有物に`unique_ptr`、Actor/Component共有に`shared_ptr/weak_ptr`を使い分けている。
- `AssetManager`はmodel cacheをweak_ptrで持つため、利用者が消えたモデルを永続保持しない。`AssetManager.cpp:14-30`。
- Static batch/instancing経路とTracy scopeが存在する。`SceneRenderer.cpp:182`, `ActorManager.h:224`。
- ActorManager更新はActor配列のコピーを取り、更新中の削除でiteratorを壊しにくい。`ActorManager.h:221`以降。

### 確認できた懸念

- `GetActorsOfType`はvectorを生成しつつ全Actorをdynamic castする。頻度次第でコストになるが、プロファイル未実施のため最適化優先度は中。
- `shared_ptr`の多用は小作品には安全側だが、ホットパスでの参照カウントと所有関係の追跡コストを持つ。
- `Physics::Update`は`simulate`後に`fetchResults(true)`で待機する。`Physics.cpp:124-129`。これは同期的で予測しやすい一方、並列化の余地を使い切れていない可能性がある。ただし実測なしに欠点とは断定しない。

