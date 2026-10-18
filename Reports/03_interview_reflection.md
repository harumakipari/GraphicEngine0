# Veil of Aurora 技術面接に向けた振り返り資料

## 最初に守るべき表現

以下では、ソースから確認できる事実と、面接で説明できる設計判断を分ける。当時の苦労、計測値、性能改善率はコードだけでは確認できないので、経験していないことを創作して話さない。

使える型は次の通り。

> 「実装上は X にしています。理由は Y です。現状の規模では Z が利点で、次回は W を改善します。」

## 面接で優先して話せる題材

### 1. 最も技術的に難しい題材: 剣の連続判定とアニメーション同期

- **確認できる事実**: Playerは剣のRoot/Mid/Tipを前フレームから現在フレームへSphere Sweepし、初期重なり・接触順・ヒット位置を選別する。`Player::Update`（`Player.cpp:518`付近）、`Physics::SphereCast`。
- **何が難しいか**: 高速に動く武器は離散Overlapではすり抜ける。複数サンプルのうち何をダメージ、エフェクト、ノックバックの根拠にするかも決める必要がある。
- **必要知識**: Sweep/TOI、法線、penetration、物理レイヤ、アニメーションの骨行列、フレーム間状態。
- **設計選択肢**: (a) boneにtrigger colliderを追従させる、(b) raycastを複数本打つ、(c) sphere/capsule sweepを使う。本作は実装難度と安定性のバランスから三点sphere sweepを選んだ、とコードに沿って説明できる。
- **回答例**: 「見た目の剣位置だけを毎フレームOverlapすると高速モーションで抜けるため、Root/Mid/Tipの前後位置を保存してSweepしました。開始重なりも別扱いにして、接触順と貫入深さから一貫したヒットを選ぶようにしています。次回はHit Resultの選別を独立したCombat Hit Resolverにしてテスト可能にします。」

### 2. 設計判断が重要な題材: Notify駆動のアクション状態

- **事実**: HitBox、無敵、入力窓、Just Dodge窓、Motion WarpをAnimation Notifyで扱う。`Player::OnAnimationNotifyBegin/End`。
- **難しさ**: 仕様は「攻撃中」では粗すぎる。攻撃のどの数百msに判定とキャンセルを置くかがゲームフィールになる。
- **回答例**: 「攻撃状態に固定Timerを置くのではなく、アニメーションにNotifyを持たせました。モーションを調整しても判定窓を追従させるためです。ただし文字列イベントが残っているため、次回はC++側イベントを型付きIDにします。」

### 3. 複数システム連携: Just DodgeからRushまで

- **事実**: PlayerはJust DodgeのスローをPlayer/Bossで別phaseに管理し、Rush開始/終了をGruxへ通知する。`Player::UpdateJustDodgeTimeScales`、`PlayerRushState`、`GruxEnemy::NotifyPlayerRushStarted/Ended`。
- **難しさ**: 入力、状態、敵AI、時間倍率、UI、VFX、カメラを同時に壊さず遷移させる必要がある。
- **回答例**: 「スローを単一のグローバル停止にせず、PlayerとBossの保持・復帰を分離しました。Rush中はBoss AIを中断して、終了時に既存のBT reset/recovery経路に戻します。次回は時間倍率の所有者と適用順を設計書に先に固定します。」

### 4. 品質向上への工夫: LockOnとカメラ構図

- **事実**: DarkCameraは表示Poseから目標Poseへ補間し、LockOnでは距離、壁衝突比、必要フレーミングを加味する。`DarkCameraActor::StartBlend/UpdateBlend/UpdateLockOnComposition`。
- **難しさ**: カメラは対象を見るだけでなく、壁際、敵距離、手動操作、死亡・ムービーを優先順位付きで処理する必要がある。
- **回答例**: 「LockOnは敵を中心に置くだけでなく、壁でカメラが近づいた時もPlayer/Enemyの可読性を維持する問題として実装しました。現在はrequestModeが複数の呼び出し元から書かれるので、次回はカメラ要求の優先度を型で明示します。」

### 5. AI: 継続Actionを持つBehavior Tree

- **事実**: BehaviorTreeはActionのRun/Complete/FailedとSequence stackを持つ。攻撃プランはCharge/Dash/Jump/FastCombo/Roarに分割され、選択理由もDebug記録する。`BehaviorTree::Run`、`NodeBase::SelectAttackRandom`、`Grux*BT.cpp`。
- **難しさ**: 行動中断、死亡、Rush、フェーズ移行で「途中のActionの状態」を確実に破棄・復帰すること。
- **回答例**: 「BTを一フレームのif文にせず、Actionの実行状態を維持しました。特に中断経路でランタイムをResetすることと、ランダム選択の理由をImGuiで観測できるようにしたのが調整面で重要でした。次回は乱数seedとノードIDを導入して再現性を上げます。」

### 6. 自作描画基盤: Deferred Renderingと実制作への接続

- **事実**: SceneBaseはG-Buffer、シャドウ、Bloom/SSAO/SSR/Fog/DoF、TAAを初期化し、Forward/Deferredの両経路を持つ。`SceneBase::Initialize/Render/DeferredRender`。
- **話すべき点**: APIを並べるのでなく、ゲームの材質、残像、VFX、シャドウ、キャラクター表現を一つの描画順へ統合した点を話す。
- **注意**: FPSやGPU最適化の成果は、計測していなければ数字を言わない。

## 「なぜこの設計にしたのですか？」への考え方

1. 制約を説明する: 自作C++/DirectX11、個人制作、単一プレイヤー・ボス戦、短い調整サイクル。
2. 選択肢を二つ以上示す: 例として武器判定なら離散Overlap、Trigger、Sweep。
3. 選択理由を品質・実装量・デバッグ性で説明する。
4. 残した負債を認める: 例としてPlayer/Grux/GameSceneの巨大化、文字列状態。
5. 次回の改善を、全面再設計でなく段階的に述べる。

## 「もう一度作るなら？」の回答骨子

> 「現在の設計は、一体のPlayerと一体の主要Bossを高い密度で仕上げるには合理的でした。反面、GameScene・Player・Gruxへ責務が集まり、状態識別にも文字列が残っています。次回は最初にゲームプレイイベント、時間倍率、カメラ要求の優先順位を型として決めます。そのうえでSceneは寿命と描画、BattleFlowは進行、Actorは個別振る舞いを持つ構成にします。ただし、全てを抽象化せず、ボス固有の攻撃Actionや調整UIは専用コードとして残します。」

## 強みと改善点を一文で言う練習

- **強み**: 「アニメーション、物理Sweep、AI、カメラ、演出を個別に実装しただけでなく、Just Dodge/RushやLockOnのような横断的なゲームプレイ体験として接続した点です。」
- **改善点**: 「完成度を上げる反復で統合クラスが大きくなったため、次は状態の所有者を小さなControllerへ分け、型付きイベントとテストで回帰を抑えます。」
- **誠実な範囲**: 「プロファイラのscopeやbatching経路はありますが、最適化効果を数値で主張するには改めてCPU/GPU計測を行います。」

## 面接前の確認リスト

- 自分で実際に触った箇所だけを、変更前の問題→選択肢→実装→検証の順で話せるか。
- `Player::Update`、`GruxEnemy::Update`、`GameScene`の更新順を図で説明できるか。
- Just Dodge/Rush、LockOn、死亡・ムービーが競合した場合の優先順位を実際にプレイして確認済みか。
- TracyでCPUフレーム、PhysX、Actor update、Draw callを計測し、数字ではなく観測結果を一つ持てるか。
- 改善案について「なぜ今はやらないのか」（リスク、作品規模、制作期限）も答えられるか。

