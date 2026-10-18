# Veil of Aurora リファクタリング計画書

本計画は品質と面接で説明できる設計理解を高めるためのもの。全面改修ではなく、既存の完成度を損なわずに変更境界を整える順序を示す。

## A. 現プロジェクトで優先的に改善したいもの

|優先度|対象|問題と方針|規模・リスク|今すぐか|
|---|---|---|---|---|
|高|`Player.cpp`、`DarkGameCamera.*`|Focus、LockOn、死亡、ムービーの競合を、入力要求とカメラ遷移状態に分けて明文化する。Focus短押しはカメラ側の遷移完了を保つ（前回調査の提案）。|小。入力・カメラ回帰を要確認。|はい。既知の仕様不具合。|
|高|`GameScene.*`|Battle flowの状態遷移と副作用（入力、HUD、カメラ、敵停止）を`BattleFlowController`相当に集約する。まずPhase2/死亡/勝利の一連を抽出する。|中。更新順が最大リスク。|はい。5,770行の統合点。|
|高|`Player`のActionRequestと状態名|状態名・重要Notifyイベントを型付きIDへ段階移行。状態の文字列比較を新規追加しないルールにする。|中。アセットとの境界を残す必要。|はい。回帰予防。|
|中|Player/Gruxのpure判定|ActionRequest優先、攻撃選択、Danger OBB、Focusモード優先を小テスト化する。|小〜中。D3D/PhysXなしで実行できる単位から。|はい。|

### A-1. Battle Flowの抽出

- **対象**: `GameScene.cpp/.h` のBossPhase、死亡結果、フェーズ遷移、カメラ・HUD切替。
- **現状**: 一つのSceneが生成、戦闘状態、UI、ムービー、対象選択まで所有する。
- **方針**: `BattleFlowController`をScene所有にし、`Update`、`StartPhaseTransition`、`EnterPlayerDeath`、`EnterBossDeath`、`ResetForContinue`を移す。Player/Grux/Cameraへの操作は狭いインターフェースにする。
- **なぜこの方法か**: ECS化やイベントバス全面導入より小さく、現在の単一戦闘の明確な状態機械に合う。
- **代案**: GameScene内の関数順序だけ整理する方法は低リスクだが所有責務が残る。汎用Gameplay Event Busは現規模では過剰。
- **効果**: Phaseの副作用の漏れを減らし、面接で状態遷移の責務を説明しやすくなる。
- **リスク**: 依存先の初期化順。移行中は旧関数をwrapperにし、1遷移ずつ置換する。

### A-2. Camera要求の優先規約

- **対象**: `DarkCameraActor::SetRequestMode`、`Player::UpdateMovement`、Movie/Death起点。
- **現状**: `requestMode`の書込み元が複数で、ブレンド中も即再開始する。
- **方針**: `Gameplay`、`LockOn`、`Cinematic`、`Death`等の要求優先度を小さなenumで明示し、入力由来だけを保留可能にする。全カメラを一般化するのでなく、既存`currentMode/requestMode`を維持する。
- **効果**: Focus既知不具合だけでなく、将来の演出割込みの意図をコードで読める。
- **リスク**: 優先度を過剰抽象化しない。既存のMovieCameraはRender Camera優先なので、その構造を壊さない。

### A-3. テスト可能なゲームプレイルールの分離

- **対象**: `Player::StoreActionRequest/TryExecuteActionRequest`、`NodeBase::SelectAttackRandom`。
- **方針**: D3D/Actor/Sceneに触れない `ActionBuffer`、`AttackSelectionPolicy` を値型として切り出し、固定乱数で検証する。
- **比較**: すべてのActorをDI可能にするより、まず結果が値で比較できるロジックから始める方が費用対効果が高い。
- **効果**: 「回避優先」「入力期限」「同じ攻撃の連続抑制」を自動回帰できる。

## B. 将来的に改善したいもの

|優先度|対象|改善方針|規模・リスク|今すぐか|
|---|---|---|---|---|
|中|`GruxEnemy.*` と `Grux*BT.cpp`|AI、攻撃実行、演出・VFX、UI/Debugを別の責務に分割する。|大。完成済みボスなので慎重に。|新機能追加前。|
|中|Actor検索|頻繁な`GetActorOfType`を初期化時weak参照/Contextへ置換。|中。複数敵対応前。|計測・拡張時。|
|中|`SceneBase`描画|キャラクター固有の特殊描画をRenderQueue/Hookへ移動。|中。描画順のリスク。|新しい演出追加時。|
|中|Asset/Resource管理|`ResourceManager`と`AssetManager`の責務を棚卸し、モデル/texture/shaderに統一方針を作る。|中。ロード順・hot reload注意。|ロード画面等の追加前。|
|低|Physics同期|計測してから非同期simulateやクエリバッチを検討。|大。デバッグが難化。|いいえ。|

### B-1. Gruxの責務分離

`GruxEnemy`を「Actor本体」「Combat runtime」「Presentation runtime」「Debug tooling」に分ける。BT ActionがGruxの公開APIを呼ぶ形は保持し、まず同クラス内部のprivateサブオブジェクトへ移すのが安全である。

完全に汎用Enemy AI基盤に作り替える代案は、敵が一体の現段階では過剰になりやすい。先にGrux専用の読みやすい構造を作るべきである。

### B-2. レンダリング固有処理の移設

`SceneBase::DeferredRender`にあるPlayer/Grux特殊経路を、ComponentがRender Itemを出す設計に移す。標準Mesh、SkinnedMesh、透明、カスタム演出の最小カテゴリで十分。全レンダリングをECS化する必要はない。

## C. 次回の新規ゲームで最初から見直すもの

|優先度|設計領域|最初からの方針|理由|
|---|---|---|---|
|高|ゲームプレイイベント|型付きイベントとデータ定義を最初に決め、文字列は外部アセット境界だけに留める。|状態/Notifyのリネーム安全性。|
|高|時間管理|Global Time Scale、Actor Time Scale、演出Time Scaleの適用順と所有者を仕様化する。|Just Dodge/Rush/ムービーの競合を初期から防ぐ。|
|高|Scene/Game分離|Engine SceneはRender/Actorライフサイクルのみ、ゲーム進行は別Controllerにする。|SceneBaseのゲーム依存を防ぐ。|
|中|依存管理|SceneContextまたは明示注入を採用し、SingletonはGraphics/OSのような基盤だけに限定。|テスト・再利用性。|
|中|AIデータ|BT構造、攻撃パラメータ、重み、クールダウンをデータ資産に寄せる。ただしカスタムActionコードは残す。|ボス調整の反復を高速化。|
|中|テスト戦略|ビルド時unit test、固定seedのAI scenario、短いplaytest checklistを初日から用意。|完成前の回帰コスト削減。|

## 実行順序

1. 既知のFocus不具合を小変更で直し、カメラ優先の回帰チェックを作る。
2. ActionBufferとAttackSelectionPolicyを抽出してテストを導入する。
3. Battle Flowの死亡/勝利/Phase遷移をControllerへ移す。
4. GruxのDebug/UIを本体から剥がす。
5. 新機能が必要になった時だけRender QueueとResource管理を整理する。

各段階で「動作を変えないリファクタ」と「仕様変更」を同一PR/コミットに混ぜないことを推奨する。

