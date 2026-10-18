# Player戦闘システム 詳細調査

## 調査の前提

これは `Player.cpp`、`Player.h`、`PlayerStateDerived.*`、`Character.*`、`StateMachine.*`、物理Sweepの呼び出し箇所を読んだ静的調査である。実際の60fps/30fps比較、全アニメーションNotify資産、プレイ時の更新順は未検証である。

- **事実**: コードから直接確認できる内容。
- **推測**: 実装から合理的に読めるが、当時の作者意図としては断定できない内容。

## 1. 現在の処理の流れと責務

### 全体の流れ

```text
InputSystem Trigger
  → Player::CaptureActionRequest
  → 単一スロットの ActionRequest（期限・優先度付き）
  → Character::Update
  → StateMachine::Update
  → Idle/Running/Attack/Dodge/Rush 等の State::Execute
  → AnimationController::OnUpdate
  → Animation Notify Begin/End/Event
  → Playerの戦闘フラグ・MotionWarpを更新
  → 次フレームの Player::Update で剣Sweep・ダメージ判定
```

`Player::Update` は、入力、近接攻撃判定、残像・軌跡、UI、カメラ遮蔽、時間制御、移動、状態機械、アニメーションを統合する更新地点である。`Character::Update` はその後半で状態機械を更新し、さらにAnimation Controllerを更新する。`Character.h:34-53`。

### フレーム内の詳細な順序（事実）

1. `Player::Update` は最初に、残像とLow HP UIを更新する。停止中ならbufferを消しintentをclearしてreturnする。`Player.cpp:411-428`。
2. 牢獄ムービー、ボスPhase遷移、最終ヒット待機は、それぞれ通常戦闘を早期returnで停止する。`Player.cpp:434-490`。
3. HP表示、カメラ遮蔽Dither、Just Dodgeの時間制御、移動速度を更新する。`Player.cpp:494-506`。
4. 現在の剣のRoot/Mid/Tip骨位置を取得し、`hitBox` が有効なら前フレーム位置からSphere Sweepする。`Player.cpp:510-666`。
5. 軌跡・剣残像を更新する。`Player.cpp:674-750`。
6. 入力をActionRequestへ積み、カメラ基準の移動方向を計算し、Motion Warpを加味した移動を行う。`Player.cpp:761-771`。
7. `Character::Update` がStateMachineを更新し、Locomotion更新、Animation Controller更新を行う。`Player.cpp:774-775`、`Character.h:34-53`。
8. 更新後の剣位置を次フレーム用に保存する。`Player.cpp:795-798`。

この順序から、Animation NotifyはAnimation Controller更新で発火し、`hitBox`、`inputWindow`、`transitionWindow`等を変更する。従って、NotifyによるHitBox開始後の最初のSweepは通常は次フレームに実施される。ただしHitBox Begin時に前回剣位置を現在位置へ再初期化しているため、前の攻撃区間から誤ってSweepしない。`Player::OnAnimationNotifyBegin` の `HitBox` 分岐、`Player.cpp:1008-1018`。

### クラスと関数の役割

|対象|責務|根拠|
|---|---|---|
|`Player`|戦闘の統合、入力buffer、剣hit、被弾、Just Dodge、UI/VFX、移動・Camera連携|`Player.cpp`|
|`Character`|方向ベクトル、StateMachine呼出し、Animation Controller更新の共通基盤|`Character::Update`|
|`StateMachine`|登録済みStateの切替と`Execute`の呼出し|`StateMachine.cpp:14-50`|
|`PlayerIdleState`/`PlayerRunningState`|ActionRequestを実行可能にし、移動有無で遷移|`PlayerStateDerived.cpp:15-64`|
|`PlayerAttackState`|攻撃開始、ターゲット回転、入力窓でのCombo/Dodge予約、遷移窓での消費|`PlayerStateDerived.cpp:68-195`|
|`PlayerDodgeState`|回避アニメ、Just Dodge成功後のRush受付、transitionでRush/移動へ遷移|`PlayerStateDerived.cpp:197-314`|
|`PlayerRushState`|Windup→敵へのMoveToActor→Rushコンボ→終了の小状態機械|`PlayerStateDerived.h:177`、`PlayerStateDerived.cpp:572`以降|
|`AnimationController`/Notify|アニメの時間にHitBox、無敵、入力窓、遷移、Just Dodge、Motion Warpを接続|`Player::OnAnimationNotifyBegin/End/Event`|
|`Physics` / `CollisionFunction`|PhysXのSphere SweepをPlayer用Hit Resultへ橋渡し|`Player.cpp:542-547`、`Physics.cpp::SphereCast`|

## 攻撃・コンボ

### 事実

Idle/Runningで`TryExecuteActionRequest()`が呼ばれ、Attackなら`"Attack"`状態へ遷移する。`Player.cpp:2143-2172`、`PlayerStateDerived.cpp:20-31`。Attack Enterは、ターゲットを固定し、移動速度を0にして、`currentAttackAnimation`を再生し、同一攻撃中の重複ヒット集合`hitActors`をclearする。`PlayerStateDerived.cpp:68-91`。

攻撃中、`inputWindow`内にAttackを押すと `comboQueued`、Dodgeを押すと`dodgeQueued`にする。`transitionWindow`に入るとDodgeを優先し、次にアニメーション資産の`nextCombo`を読む。Comboは状態を`Attack → Attack`に再遷移し、`continuingComboTransition`で通常の終了cleanupと区別する。`PlayerStateDerived.cpp:94-168`。

### なぜこの設計か（推測）

攻撃ボタンを押した時点で即次攻撃へ飛ぶのでなく、「入力受付時間」と「実際の遷移時間」を分けたかったと読める。アニメ資産が`nextCombo`を持つため、コンボ順をPlayerのswitch文に固定せず、アニメ側に遷移先を持たせる意図も読み取れる。

### メリット

- 攻撃の受付時間とキャンセル時間をモーションに同期できる。
- Dodge優先がコード上で明示され、攻撃中の緊急回避を作りやすい。
- `nextCombo`を使うため、コンボを追加する際のPlayer側分岐が少ない。

### デメリット

- `inputWindow`、`transitionWindow`、`comboQueued`、`dodgeQueued`、`bufferCommand`に「予約状態」が分散する。
- `Attack → Attack`の再遷移は意図的だが、一般的なStateMachineの同状態切替と異なる。`StateMachine::ChangeState`は同じ状態名でも必ずExit→Enterするため、呼び出し側がこの副作用を理解している必要がある。`StateMachine.cpp:23-44`。
- `nextCombo`は文字列であり、アセット名変更の安全性が弱い。

## 入力バッファと優先度

### 事実

`ActionRequest`は `type`、`remainTime`、回避方向スナップショットを持つ。`Player.h:59-66`。`CaptureActionRequest` はDodgeに0.3秒、Attackに0.5秒の保持時間を設定する。`Player.cpp:2846-2875`。優先度はDodge=3、Attack=2であり、低優先度要求は既存の高優先度要求を上書きできない。`Player.cpp:31-39`、`2878-2897`。

```cpp
// 実際の核となる優先判定
if (bufferCommand.type != ActionType::None && newPriority < currentPriority)
    return false;
bufferCommand.type = type;
bufferCommand.remainTime = remainTime;
```

### 選択肢との比較

1. **現在: 単一スロット＋優先度** — もっとも新しい実行候補を一つに絞る。
2. **FIFOキュー** — 押した順を保つ。連打を残せるが、意図しない古い入力が後から発火しやすい。
3. **種類別buffer** — Attack/Dodgeを別々に保持し、状態ごとの裁定を行う。柔軟だが競合ルールを増やす。
4. **Command patternの時系列キュー** — リプレイやネットワークへ拡張しやすいが、この作品の規模には重い。

現在の選択は、単一キャラクターの近接アクションで「回避を攻撃より優先する」目的に対して小さく実用的である。

### 実際の注意点

- Attackをbufferした後にDodgeを押すとDodgeが上書きする。逆は拒否される。これは仕様としては明確。
- ただし一枠しかないため、たとえば「攻撃予約を残したまま回避予約も残す」はできない。将来、複数キャンセル規則や複雑な空中行動を増やすなら制限になる。
- `StoreActionRequest`内の`previous`は代入後に使われていない。`Player.cpp:2887`。機能不良ではないが、途中の設計変更跡であり、読み手に余計な意味を推測させる。

## 剣の当たり判定

### 現在の仕組み

HitBox Notify開始時に半径・ローカルoffsetを受け、Root/Mid/Tipの前回座標を現在座標で初期化して`hitBox=true`にする。終了時はフラグと設定を戻す。`Player.cpp:1008-1018`、`1110-1114`。

HitBox中の各フレームでは、次の3本を敵レイヤに対してSweepする。

```cpp
SphereRayCast(prevSwordRootPos, swordRootPos, ...);
SphereRayCast(prevSwordMidPos,  swordMidPos,  ...);
SphereRayCast(prevSwordTipPos,  swordTipPos,  ...);
```

`TOI = hitDistance / sweepLength` が小さい通常Hitを選び、開始時重なりなら貫入深さが小さいものを選ぶ。エフェクトは位置・法線がある候補を優先し、ダメージ対象はそれが無ければ最初のダメージ候補へfallbackする。`Player.cpp:549-593`。

同じ攻撃中に同じActorへ複数回ダメージを与えないよう、`hitActors`で抑止する。Attack Enterでclearされる。`Player.cpp:597-626`、`2350-2351`。

### なぜ難しいか

剣の先端は1フレームで大きく移動するため、現在位置だけをOverlapすると敵を飛び越えてしまう。さらにRoot/Mid/Tipは異なる軌道を描くため、一点だけでは刃の広がりを近似しにくい。初期重なりはSweep方向や距離が不安定になりやすく、通常接触と同じ順位付けにするとエフェクトの位置・法線が壊れやすい。

### 現設計のメリットと限界

- **メリット**: 物理Shapeを毎フレーム骨へ再構成せず、3回のqueryで高速剣のすり抜けを抑える。演出用Hitとダメージ用Hitを分けている。
- **限界**: 3球は刃全体の連続swept volumeではないため、RootとTipの間を細い敵が抜ける可能性は残る。高速移動や低FPSで半径・サンプル数の調整が必要になる。
- **限界**: `dynamic_cast<GruxEnemy*>` を最優先に置いてDamage APIも分岐する。`Player.cpp:601-663`。Enemyが増えるほどPlayerの知識が増える。

## 被弾、無敵、Just Dodge、Rush

### 被弾

`TryTakeDamage` は `invincibleWindow`、`invincible`、Damage/Death状態をgateにし、HP、遅延HP表示、flash、SE、振動、buffer消去、被弾方向、状態遷移をまとめて行う。`Player.cpp:2191-2297`。被弾方向は攻撃者方向とPlayerのforward/rightのdot積からFront/Back/Left/Rightへ分類する。

**評価**: 実ゲームで必要な副作用が一箇所にあるため、「ダメージを受けた」結果の漏れを減らす。一方、UI・音・入力・状態を一つの関数が直接知るため、テストや仕様変更は難しくなる。

### Just Dodge

PlayerのJust Dodge受付はPlayer側Notifyの`justDodgeWindow`であり、Grux側のDangerWindowと重なった時に敵側から`StartJustDodgeSuccess`が呼ばれる構造であると読み取れる。成功時はRush対象を保存し、Player/Enemyの個別TimeScaleを下げ、Player/BossのHold/Return phaseを初期化し、radial blurとghostを起動する。`Player.cpp:2354-2432`。

**重要な設計点**: Global time scale一つで全世界を止めるのでなく、Playerと対象Enemyへ`SetTimeScale`している。一方、通常攻撃HitStopでは`Time::SetSlow(0.0f, duration)`を使う。`Player.cpp:632-633`。二種類の時間制御が併存している。

### Rush

Dodge StateはJust Dodge成功後、Attack入力を受け取るとRushを予約し、`transitionWindow`でRushへ遷移する。`PlayerStateDerived.cpp:230-295`。Rush Stateは独立した小状態機械である。

```text
Windup → DashToTarget (MoveToActor) → Attack (入力受付・combo) → Finished → Idle
```

`PlayerRushState::Exit` はGruxへ終了通知、HP表示終了、武器VFX解除、時間倍率復帰、移動速度・Animation rateをresetする。`PlayerStateDerived.cpp:714-752`。

これは複数系統のcleanupを一箇所にまとめた良い面がある。反面、PlayerRushStateがEnemy UI、AI通知、時間、VFXを知るため、依存範囲が広い。

## 2. なぜ現在の設計になったと考えられるか

以下は**推測**である。

1. まず基本的なStateMachineとAnimation Controllerが存在し、アクションを追加する過程でNotify、input buffer、motion warp、Rush、演出をPlayerへ統合していった。
2. 個人制作の反復では、各機能を横断する「実際に見える品質」を先に仕上げる必要がある。そのため抽象層より、Playerから直接UI/音/Camera/Gruxを操作する方が実装・調整速度は高かった。
3. Gruxが主ボスであるため、`GruxEnemy`専用のDamage/Rush/final-hit分岐をPlayerに置くことは、現段階では汎用化より低コストだった。

これは「悪い設計だった」という意味ではない。単一プレイヤー・単一主ボスの完成を優先する局面では、一般化のコストを避ける合理性がある。

## 3. 他の実装方法

### State管理

- **階層状態機械**: LocomotionとActionを別stateにする。`Grounded + Attacking`等を表しやすいが、遷移規則が複雑化する。
- **Ability/Gameplay Action方式**: Attack/Dodge/Rushを開始・更新・終了するAbilityとして登録し、タグで競合を解決する。拡張性は高いが、現状の一体のPlayerには導入量が大きい。
- **アニメグラフ主導**: Animation側が状態遷移を持ち、ゲームコードはtagのみ送る。表現の強みはあるが、戦闘ルールがAnimatorに隠れやすい。

### 武器判定

- bone追従のCapsule/Trigger collider
- Root→Tip全体を一つのCapsule sweepで近似
- 複数球Sweep（現方式）
- メッシュ軌跡に対するswept volume

現方式は、PhysX queryを使いつつ骨に常時Colliderを同期する管理コストを避ける中間解である。

### 戦闘イベント

- 現方式: PlayerがEnemyを直接castしてダメージ、VFX、HitStopまで実行。
- 改善候補: `CombatHit`値を作り、`DamageSystem`またはSceneのCombat Resolverが受ける。
- イベントバス: `HitConfirmed`を購読させる。拡張性は高いが、順序・所有者が見えにくくなりやすい。

次回は中央イベントバスを先に置くより、`CombatHitResolver`という同期的で返り値を持つ狭いサービスから始めるのが適切である。

## 4. 実際のコードにある問題点

### 優先度: 高

1. **Playerが多責務**
   - `Player::Update`だけで判定、VFX、ghost、UI、入力、移動、state、animationを順序制御する。
   - 現規模でも読み取り・変更コストが高い。戦闘仕様の変更がUI/VFX更新順へ影響しやすい。
   - 次回は`PlayerCombatComponent`、`PlayerPresentationComponent`、`PlayerInteractionComponent`のように「更新責務」単位で分ける。ただし今すぐComponent化するより、まずprivate helper/値型を抽出する方が安全。

2. **文字列による状態とイベント識別**
   - `"Attack"`、`"Dodge"`、`"Rush"`、`"DeathCameraStart"`、`"rush_input_end"`を比較する。
   - リネームがコンパイル時に検出されず、`StateMachine::ChangeState`は存在しない名前を実行時assertする。
   - 次回はC++のstate/eventをenumにし、アニメ資産との境界でだけ文字列→enum変換と検証を行う。

3. **Rush/Just DodgeがPlayer・Gruxへ相互に直接依存**
   - Playerは`GruxEnemy`へ通知し、Grux固有のHitTypeを選ぶ。Grux側もPlayerの状態・Just Dodgeを読む。
   - 主ボス一体なら許容できるが、敵種や協力プレイ拡張で循環依存が増える。
   - `ICombatTarget`や`IRushTarget`のような狭い接口、または`CombatHitResolver`へ段階的に置換する。

### 優先度: 中

4. **時間制御の意味が分散**
   - Just DodgeはActor別`SetTimeScale`、HitStopは`Time::SetSlow`、アニメにはAnimation rate resetもある。
   - どの時間が何へ掛かるかは呼び出し元を横断しないと分からない。重複開始・復帰忘れは起きやすい。
   - `TimeDilationRequest { owner, target, scale, duration }`のように所有者と解除を明示する設計が次回向き。

5. **入力bufferが一枠のみ**
   - 現仕様には合うが、DodgeとAttackを独立に予約できない。
   - 拡張時は種類別bufferか小さな優先キューへ。ただし現在すぐ変更する必要はない。

6. **更新順への暗黙依存**
   - SweepはアニメNotify発火より前、State ExecuteもAnimation Controllerより前である。
   - 動作として設計されている可能性が高いが、変更時に一フレーム遅延・Notify終了とHit判定の境界を壊しやすい。
   - Update phaseを `Input → State → Animation events → Gameplay collision → Movement → Presentation` のように明文化するか、コメントとテストシナリオを残すべき。

7. **微小なコード品質上の点**
   - `StoreActionRequest` の `previous` は未使用。
   - hot pathを含む多くの箇所で `std::string(stateMachine_->GetStateName())` を生成して比較している。現規模で重大な性能問題とは断定しないが、enum化すれば安全性とともに解消する。

## 5. 次回制作時の推奨設計

目的はECS化ではなく、現在うまく動いている「アニメNotify + input buffer + sweep」を保ちながら、責務と競合規則を明示すること。

```cpp
enum class PlayerAction { None, LightAttack, Dodge, Rush };
enum class CombatEventType { HitBoxBegin, HitBoxEnd, InputOpen, TransitionOpen,
                             IFrameBegin, JustDodgeOpen, MotionWarpBegin };

struct BufferedAction {
    PlayerAction action;
    float expiresAt;
    DodgeSnapshot dodge;
};

struct CombatHit {
    ActorId attacker;
    ActorId target;
    DamageSpec damage;
    HitContact contact;
    AttackKind kind;
};
```

### 分割案

- `PlayerInputBuffer`: 入力取得ではなく、期限・優先・消費だけを値型で担当。unit test対象。
- `PlayerActionState`: Idle/Locomotion/Attack/Dodge/Rushの遷移規則だけを担当。アニメ再生要求を返す。
- `PlayerCombat`: Notifyを型付きイベントに変換し、HitBox期間、Sweep、hit onceを担当。
- `CombatHitResolver`: ダメージ、HitStop、VFX要求、死亡判定を順序付きで解決。
- `PlayerPresentation`: Trail、ghost、UI flash、音・振動の要求を受ける。

「Dodge中にAttackを押してRushへ」のようなルールはPlayerActionStateに残す。全ルールをイベントバスにばらすと、何がRush開始を決めたのか追えなくなるためである。

## 6. 技術面接で聞かれそうな深掘り質問 10個

1. **なぜ武器判定に3本のSphere Sweepを使い、Capsule Sweepにしなかったのですか？**
   - Root/Mid/Tipで刃の軌道を近似し、既存PhysX queryで高速移動の抜けを抑える選択。残る隙間・低FPSでの検証不足も正直に述べる。

2. **HitBox Notify開始フレームで前回剣位置を再初期化する理由は？**
   - 非HitBox期間の移動軌跡を次の攻撃の最初のSweepへ持ち込まないため。

3. **入力バッファが一つだけなのはなぜですか？**
   - 今の行動数では競合裁定を単純化し、Dodgeを高優先にしたかったため。将来は種類別bufferを検討する。

4. **InputWindowとTransitionWindowを分けた理由は？**
   - 入力を受ける期間と、実際にCombo/Dodgeへ切替えてよい瞬間は同一とは限らないため。

5. **AttackからAttackへ同じStateを再遷移する危険は？**
   - Exit/Enterが必ず走るため通常cleanupを壊さないよう`continuingComboTransition`を置いている。より安全には明示的な`AdvanceCombo`APIへする。

6. **Just DodgeのスローをGlobalでなくActor別にした理由は？**
   - Playerと対象Bossの演出を制御しつつ、全UI/全システムを止めないため。HitStopは別目的なのでGlobal slowを使う。二系統の統合は将来課題。

7. **ダメージ処理をPlayerに置いた問題は？**
   - 単一ボスの完成優先では直接分岐が早い。一方、敵種・属性・防御・協力プレイ増加には不利なので`CombatHitResolver`へ移す。

8. **Animation Notifyをゲームプレイに使う場合、アニメ差し替え事故をどう防ぎますか？**
   - 現状はEditor debug/Notify asset読み込みが助けになるが、型付きイベントとロード時validation、必須Notifyの自動チェックを追加すべき。

9. **フレームレートが変わると判定はどうなりますか？**
   - Sweepにより離散Overlapより頑健。ただし1フレームの回転曲線は直線近似であり、極端な低FPSの品質は実測検証が必要。

10. **もう一度作るなら、どこを変え、どこを維持しますか？**
   - 維持するのはNotify駆動の窓、入力buffer、継続Sweep。変えるのは文字列ID、Playerへの責務集中、時間制御の所有者、Gruxへの直接依存。

