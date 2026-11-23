# 调研 · Snapshot（冻结）在其他成熟框架里怎么做

> **审核状态**：⚠️ **agent 产出，未审**。本文件是调研，不是 V2 结论。
> 对标文件：[领域-Parameter-草稿.md](领域-Parameter-草稿.md) §4.5 / §8 N13
>
> **这份文件回答的问题**：当「一个修正器/效果要读某个值」与「那个值会在它存续期内变化」冲突时，成熟框架怎么处理？
> **触发它的是** [领域-Parameter-草稿.md](领域-Parameter-草稿.md) 的 N13（属性修正器的值描述里能不能出现「读参数账本」这类源）。
>
> **证据等级声明**（重要，读之前先看）
>
> | 等级 | 含义 | 本文件里有哪些 |
> |---|---|---|
> | **A 一手源码** | 本机可读、可 grep、行号可回查 | UE 5.8 GAS、AbilityKit HEAD `16d93f06f` |
> | **B 官方公开文本** | 厂商自己发布或社区 wiki 记载的机制描述与 patch note | WoW（warcraft.wiki.gg）、LoL（wiki.leagueoflegends.com） |
> | **C 未取得** | 查过但拿不到，**不推断** | Dota 2（见 §4） |
>
> **LOL / Dota 2 / WoW 的引擎内部实现都不公开。** 本文件**不把第三方逆向推断当结论**。B 级证据只能说明「这个游戏对外表现出的语义是这样」，不能说明「它的代码这样写」。

---

## 0. 结论速览

**一句话**：业界只有三档做法——**冻**、**实时并维持依赖**、**门禁**（不许在会变的时间窗里读）。**没有任何一家做「允许读、但不维持依赖」**（= 草稿 N13 的乙）。三家的**默认都是实时**，冻结只留给「一下算完的复合量」。

| 框架 | 有「冻」这个开关吗 | 开关挂在谁身上 | 冻的实现动作 | 默认 |
|---|---|---|---|---|
| **UE GAS** | ✅ `bool bSnapshot` | **捕获定义**（= 读点），不是修正器 | 深拷贝一份独立 aggregator，**且不登记依赖** | **不冻**（`bSnapshot = false`） |
| **AbilityKit** | ✅ `EvaluationPolicy` 两值 | **修正器规格**（每颗修正器一个策略） | 把 `MagnitudeSource` 换成 `MagnitudeSource.Fixed(算好的数)` | **实时**；只有含公式的修正器**强制**冻 |
| **AbilityKit（技能参数）** | ❌ 不用开关 | — | **门禁**：每个参数键固定解析阶段，读早读晚都抛异常 | 只能在该阶段读 |
| **WoW** | 曾经有，**正在系统性拆掉** | — | 2014 起 `Periodic effects no longer snapshot`；2025 仍有同类改动 | 走向实时 |
| **LoL** | ✅（至少在部分机制上） | 效果的施加减益那一刻 | 把当时的基数记下来，持续到减益结束 | 公开资料不足以判全局 |

**最要紧的一条负结论**：**「允许读但不建依赖边」这个中间态，在三家框架里都不存在。** GAS 用 `bSnapshot == false` / `bSnapshot == true` 两个分支把它排除了——要么共享同一份、跟着变；要么深拷贝一份、完全不跟。没有第三条路。

**第二条负结论**：**没有一家把「冻」当作默认。** 想冻必须显式打开。

---

## 1. UE GAS（一手源码，A 级）

源码根：`E:\UnrealEngine\UE_5.8\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities`（下称 GAS 根）。引擎版本 UE 5.8。

### 1.1 开关的形状与位置

`GAS 根\Public\GameplayEffectAttributeCaptureDefinition.h`

```cpp
:12  enum class EGameplayEffectAttributeCaptureSource : uint8 { Source, Target };
:23  struct FGameplayEffectAttributeCaptureDefinition
:29      AttributeSource = EGameplayEffectAttributeCaptureSource::Source;
:30      bSnapshot = false;                                  // ← 默认不冻
:45      EGameplayEffectAttributeCaptureSource AttributeSource;
:49      bool bSnapshot;
:65      Hash = HashCombine(Hash, GetTypeHash(CaptureDef.bSnapshot));
```

**这个位挂在哪，是本调研最要紧的一处事实**：它挂在 **capture definition（捕获定义）** 上。捕获定义描述的是「**这次计算要读哪个属性、从谁身上读**」——也就是**读点**。它**不挂在修正器（modifier）上**。

⇒ 同一个修正器可以有多个捕获定义，各自决定冻不冻；冻不冻是**每一个读**自己的事，不是这条修正器的整体属性。

### 1.2 生效点：冻 = 深拷贝；不冻 = 共享同一份

`GAS 根\Private\GameplayEffect.cpp:3931`

```cpp
void FActiveGameplayEffectsContainer::CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
{
    FAggregatorRef& AttributeAggregator = FindOrCreateAttributeAggregator(OutCaptureSpec.BackingDefinition.AttributeToCapture);

    if (OutCaptureSpec.BackingDefinition.bSnapshot)
    {
        OutCaptureSpec.AttributeAggregator.TakeSnapshotOf(AttributeAggregator);   // 深拷贝，独立一份
    }
    else
    {
        OutCaptureSpec.AttributeAggregator = AttributeAggregator;                // 共享同一份，跟着变
    }
}
```

两个分支代表两个完全不同的东西：

- `bSnapshot == false` ⇒ 拿的是**引用**。源属性一变，这边立刻看到。
- `bSnapshot == true` ⇒ 拿的是**副本**。此后源属性怎么变都无关。

`GAS 根\Private\GameplayEffectAggregator.cpp`

```cpp
:579  void FAggregator::TakeSnapshotOf(const FAggregator& AggToSnapshot)   // BaseValue / ModChannels 整份拷贝
:653  void FAggregatorRef::TakeSnapshotOf(const FAggregatorRef& RefToSnapshot)
:660      Data->TakeSnapshotOf(*SrcData);                                // 独立分配
```

### 1.3 「冻 = 不登记依赖」在 GAS 里是明写的

`GAS 根\Public\GameplayEffectAggregator.h`

```cpp
:368  /** ActiveGE handles that we need to notify if we change. NOT copied over during snapshots. */
:369  TArray<FActiveGameplayEffectHandle> Dependents;
```

`:368` 那句注释原文 `NOT copied over during snapshots` 就是全部答案：**快照不拷依赖名单**。于是「冻的那份不会因为源变化而被唤醒」。

配套的四处：

| 位置 | 代码 |
|---|---|
| `GameplayEffect.cpp:2476` | `if (BackingDefinition.bSnapshot == false) { ... Agg->AddDependent(Handle); }` |
| `GameplayEffect.cpp:2496` | `bool FGameplayEffectAttributeCaptureSpec::ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const` |
| `GameplayEffect.cpp:2498` | `return (BackingDefinition.bSnapshot == false && (ChangedAggregator == nullptr || AttributeAggregator.Get() == ChangedAggregator));` |
| `GameplayEffect.cpp:2563` | `if (!InCaptureDefinition.bSnapshot) { bHasNonSnapshottedAttributes = true; }` |

依赖驱动的重算主路径在 `GameplayEffect.cpp:3541`：

```cpp
if (ModDef.ModifierMagnitude.AttemptRecalculateMagnitudeFromDependentAggregatorChange(Spec, RecalculatedMagnitude, ChangedAgg))
{
    ModSpec.EvaluatedMagnitude = RecalculatedMagnitude;
```

而 `AttemptRecalculateMagnitudeFromDependentAggregatorChange`（`:1271`）的第一道门就是 `:1281 if (CaptureDef.bSnapshot == false)`。⇒ **冻过的修正器根本不会被叫来重算。**

### 1.4 冻结值存放处：与 spec 同生同死

| 事实 | 位置 |
|---|---|
| `FGameplayEffectSpec::CapturedRelevantAttributes`（注释 `NOT replicated to clients`） | `Public\GameplayEffect.h:1212` |
| `FModifierSpec::EvaluatedMagnitude`（private，注释 `the authoritative, last evaluated value of the magnitude`） | `Public\GameplayEffect.h:751` / `:761` |
| `CalculateModifierMagnitudes()` 把算值写进 `ModSpec.EvaluatedMagnitude` | `Private\GameplayEffect.cpp:2033` |
| 调用点 | `GameplayEffect.cpp:3098 / :3229 / :3870 / :3883 / :3898 / :4388 / :5167 / :5503` |

`:4388` 的注释原文：`Calc all of our modifier magnitudes now. Some may need to update later based on attributes changing, etc, but those should be done through delegate callbacks.`（先全算一遍；需要跟进的**走委托回调**——即 §1.3 那条路。）

**要点**：冻结值住在 **spec** 里，而读取它的也是**本次 GE 的计算**。**三者的生命周期完全一致**（都随这次 GE 施加而生、随它到期而死）。这一点在 §7 会成为关键。

### 1.5 环的兜底是运行期截断，不是配置期检测

`GAS 根\Private\GameplayEffectAggregator.cpp`

```cpp
:600  //  The degenerate case is cyclic attribute dependencies: MaxHealth -> MaxMana -> MaxHealth. This prob can't be fixed. We should instead detect earlier.
:604  //  This code now allows MAX_BROADCAST_DIRTY recursion calls. This is pretty arbitrary. We are trying to provide a solution that doesn't crash (infinite loop) from bad data
:609  const int32 MAX_BROADCAST_DIRTY = 10;
:611  if (BroadcastingDirtyCount > MAX_BROADCAST_DIRTY)
:616  ABILITY_LOG(Warning, TEXT("FAggregator detected cyclic attribute dependencies. We are skipping a recursive dirty call. Its possible the resulting attribute values are not what you expect!"))
```

⇒ GAS **没有**配置期环检测，只是运行期数到 10 就切断并打警告，且自己承认「结果可能不是你以为的那样」。这与族内 [领域-Attribute-草稿.md](领域-Attribute-草稿.md) §5.4 的配置期检测是不同取向。

---

## 2. WoW（官方 patch note，B 级）：趋势是**反的**

WoW 曾经大量使用快照（DoT 按施法瞬间的属性算伤害），**这十几年在系统性拆掉它**。

### 2.1 四条原文

**① Haste 页**（`https://warcraft.wiki.gg/wiki/Haste`）

> In addition, like all secondary stats, haste is **no longer snapshotted** when the periodic effect is cast, and instead the tick interval is **dynamically updated on the fly** as the caster's haste varies.

同页 patch changes：

> **Patch 6.0.2 (2014-10-14)**: Spell Haste %, Melee Haste %, and Ranged Haste % have been merged into a universal Haste %. **Periodic effects no longer snapshot and no longer have breakpoints.**

**② Mastery: Life-Binder 页**（`https://warcraft.wiki.gg/wiki/Mastery:_Life-Binder`）

> **Patch 11.1.0 (2025-02-25)**: [Dream Breath] and [Reversion] **no longer snapshot** with Mastery: Lifebinder. Instead, they will **re-evaluate the Mastery bonus each time they tick** based on the target's current health and yours.

⇒ 2014 年开始拆，**2025 年还在拆**。方向十四年没变。

**③ Powershifting 页**（`https://warcraft.wiki.gg/wiki/Powershifting`）——记的是**动机**：

> the rise in popularity of addons such as AffDots to simplify usage of the snapshotting mechanic of dots

⇒ 玩家用插件把「快照」变成了隐性操作技巧（卡最后一刻上 buff），于是厂商把它拿掉。

**④ Patch 6.0.2 的说明**：

> benefits of maximizing periodic snapshotting is so high that it creates a **balance problem**.

### 2.2 散见的修复条：快照常被当 bug 修

| 版本 | 原文 |
|---|---|
| Patch 1.13.6 | `Resolved an issue that caused partial resists to Damage-over-Time effects to snapshot for the entire duration of the DoT effect.` |
| Patch 5.5.1 | `Soul Swap on Garalon will no longer Snapshot the % damage increase from one leg` |
| Patch 10.0.2 | `Fixed an issue causing [Wellspring] to snapshot its healing on cast when allies are at full health.` |
| Hotfixes/2023 | `Fixed an issue where [Broodkeeper's Promise] would snapshot the caster's Versatility for the buff's full duration` |

### 2.3 怎么读这组证据

WoW 的取舍不是「快照坏」，而是：**当快照变成玩家必须掌握才能不吃亏的隐性知识时，它就是负担**。它带来的平衡成本（`creates a balance problem`）超过它能表达的深度。

⚠️ 注意这条证据的**边界**：它说明的是「**周期性效果**（持续伤害/持续治疗）不该快照」，**不能**外推成「任何量都不该冻结」。WoW 里仍然存在「施加时定型」的量（例如某些吸收盾的数值），只是周期性效果被系统性拆掉了。

---

## 3. LoL（社区 wiki，B 级）：一条确证

`https://wiki.leagueoflegends.com/en-us/Rell` 关于 `Break the Mold`：

> The first stack of Break the Mold that is applied to a target will **snapshot** the targets current armor and magic resistance. All subsequent stacks will reduce the targets resistances based on the **snapshotted amount**, even if the target's total resistances change. **This snapshot lasts until the debuff expires.**

举例原文：

> applying a stack of Break the Mold to a champion with 100 armor and magic resistance will reduce their resistance by 3 ... If their resistances are then boosted to 500 before another stack is applied, the subsequent stack will only reduce their resistance by a further 3 despite 3% of 500 being 15.

推论原文（同页）：

> when a target has temporarily gained increased resistances before Rell applies the first stack of Break the Mold then the snapshot of the target's stats will be of the increased amount. This can result in targets losing significantly more resistances after the duration of the increase ends, for as long as the stacks don't fall off.

**要点**：
1. 冻结的**时机**是「第一层减益被施加那一刻」，不是每次读。
2. 冻结的**生命周期**明写为 `until the debuff expires` —— **与承载它的那个效果同寿**。
3. LoL 自己知道这会产出「反直觉」的结果（临时增益期间上的减益，增益结束后还按高基数扣），并把它当**可接受语义**写在 wiki 上。

⚠️ 这只是一条机制的确证。**不能**由此断言「LoL 全局普遍快照」——公开资料不足以判断。

---

## 4. Dota 2（C 级：未取得，不推断）

查过的地方与结果：

| 来源 | 结果 |
|---|---|
| `dota2.fandom.com` 的 `Spell_Damage`（13 570 字符）、`Damage_Manipulation`（27 734）、`Damage_types`（25 281）全文 wikitext | 只有伤害分类旗标（`No-Spell-Amplification` / `No-reflection` / `No-Spell-Lifesteal` / `HP Removal`）与 `MODIFIER_PROPERTY_AVOID_DAMAGE` 这类机制名，**没有一句关于「伤害加成在何时绑定」** |
| `liquipedia.net` | Cloudflare 403，取不到 |
| Dota 2 Workshop Lua API | 只有「怎么改数值」的接口，不含「求值发生在何时」 |

⇒ **本文件不对 Dota 2 下任何结论。** 若将来要补，可查的方向：`MODIFIER_PROPERTY_*` 的求值时机在 Valve 官方 workshop 文档里的描述、以及开放性更好的 `Spell_Block`/`Damage_Filter` 语义页。

---

## 5. AbilityKit（一手源码，A 级）：三档全实现了

clone 在 `E:\Projects_Learning\AbilityKit`，HEAD `16d93f06f`。它是这套调研里**唯一一家三档都有实现**的框架，因此最有参照价值。

### 5.1 第一档：门禁（技能参数根本不允许在会变的时间窗里被读）

`Unity/Packages/com.abilitykit.demo.moba.runtime/Runtime/Application/Services/Skill/Modifiers/Skill/MobaSkillParamGroupResolver.cs`（全文 41 行）：

```csharp
:14  public int ResolveResourceCost(int actorId, int resourceCost, in MobaParameterRecord record)
:16      RequireCastPrepare(in record);
:21  public int ResolveCooldownMs(int actorId, int cooldownMs, in MobaParameterRecord record)
:23      RequireCastPrepare(in record);
:28  public float ResolveCastRange(int actorId, float range, in MobaParameterRecord record)
:30      RequireCastPrepare(in record);
:35  private static void RequireCastPrepare(in MobaParameterRecord record)
:37      if (record.Stage != MobaParameterStage.CastPrepare)
:38          throw new System.ArgumentException("Skill parameters require CastPrepare stage.", nameof(record));
```

`MobaParameterStage`（`.../Skill/Modifiers/MobaParameterRecord.cs`）= `None=0, CastPrepare=1, LauncherCreate=2, Emit=3, ProjectileSpawn=4, Hit=5, AreaSpawn=6, SummonCreate=7, BuffApply=8`。

更强的约束在 `MobaSkillParamModifierService.ValidateRecordStage`：每个参数键**注册了自己唯一的解析阶段**，不匹配就抛异常——

```csharp
throw new InvalidOperationException($"Invalid modifier parameter stage for {key}: expected {expected}, actual {record.Stage}.");
```

⇒ **AbilityKit 的做法不是「让读到的值冻住」，而是「让这次读只能发生在唯一允许的那个阶段」**。读早读晚都抛异常，问题被挪到配置期。

### 5.2 第二档：真正的双档冻结（属性修正器）

`.../Continuous/Config/MobaContinuousConfigDecorators.cs`

```csharp
:79  public const int Realtime = 0;
:80  public const int OnApplySnapshot = 1;
```

`.../Continuous/Modifiers/MobaAttributeContinuousModifierProjector.cs:171`

```csharp
private static MagnitudeSource ApplyEvaluationPolicy(IContinuous continuous, IMobaContinuousModifierSpec spec, int index, int stack,
    int op, MagnitudeSource unitMagnitude, MagnitudeSource magnitude, int policy, AttributeContext context,
    out float capturedValue, out bool hasCapturedValue)
{
    capturedValue = 0f;
    hasCapturedValue = false;
    if (policy != MobaContinuousModifierEvaluationPolicy.OnApplySnapshot) return magnitude;   // 实时：原样返回，每次重算

    if (continuous is MobaContinuousRuntimeBase runtime &&
              runtime.TryGetModifierCapture(spec, index, stack, op, out capturedValue))
    {
        hasCapturedValue = true;
        return MagnitudeSource.Fixed(capturedValue);                                          // 已冻过，复用
    }
    var unitValue = unitMagnitude.Calculate(context?.Level ?? 1f, context);
    capturedValue = MobaContinuousModifierMath.ApplyStackValue(unitValue, stack, op);
    (continuous as MobaContinuousRuntimeBase)?.SetModifierCapture(spec, index, stack, capturedValue, unitValue);
    hasCapturedValue = true;
    return MagnitudeSource.Fixed(capturedValue);                                              // 首次：算一次，存下来
}
```

**「冻」的动作与 N13 的甲完全同形**：把「怎么算」的表达式替换成 `MagnitudeSource.Fixed(算好的数)`。此后这颗修正器的值不再与任何输入相连。

### 5.3 默认是实时，且只有一种情况强制冻

`MobaContinuousConfigDecorators.cs`

```csharp
:152  if (spec.EvaluationPolicy == MobaContinuousModifierEvaluationPolicy.Realtime &&
:154      throw new System.InvalidOperationException($"Realtime attribute modifier {spec.TargetKind}:{spec.TargetId} requires the owner source role.");
:161  if (spec.EvaluationPolicy == MobaContinuousModifierEvaluationPolicy.Realtime &&
:163      throw new System.InvalidOperationException($"Realtime formula modifier {spec.TargetKind}:{spec.TargetId} is unsupported; use OnApplySnapshot.");
```

⇒ **含公式的修正器必须冻**（`:163`），其余一律实时（默认 `Realtime = 0`）。理由与 N13 甲同源：**公式是「一下算完的复合量」，它没有可维持的依赖形状**——若要实时，就得每帧重新求值整条公式。

### 5.4 第三档：丙方案真的被实现了（`ContextFloat`）

`Unity/Packages/com.abilitykit.modifiers/Runtime/Core/Source/MagnitudeSource.cs`

```csharp
:14  public enum MagnitudeSourceType : byte
:32      ContextFloat = 5,
:596 private float CalculateContextFloat(IModifierContext ctx)
:598     if (ctx == null) return 0f;
:599     var key = ContextFloatKey;
:600     if (string.IsNullOrEmpty(key)) return Data0;
:601     return (Data0 + ctx.GetFloat(key)) * Data1;
```

并且**维持依赖边**——写入方一变，读方标脏：

`Unity/Packages/com.abilitykit.attributes/Runtime/Attributes/Core/AttributeContext.cs`

```csharp
:135 public void SetFloat(string key, float value)
:143     MarkContextFloatDependentsDirty(key);
:158     MarkContextFloatDependentsDirty(key);
:239 private void MarkContextFloatDependentsDirty(string key)
:243     group.MarkContextFloatDependentsDirty(key);
```

`AttributeGroup.cs:174 internal void MarkContextFloatDependentsDirty(string key)` →

`AttributeInstance.cs:217`

```csharp
internal void MarkDirtyIfContextFloatDependent(string key)
{
    for (int i = 0; i < _modifierSlotCount; i++)
    {
        if (!_modifierSlots[i].Active) continue;
        var magnitude = _modifierSlots[i].ModifierData.Magnitude;
        if (magnitude.Type != MagnitudeSourceType.ContextFloat) continue;
        if (!string.Equals(magnitude.ContextFloatKey, key, StringComparison.Ordinal)) continue;
        MarkDirtyByDependency();
        return;
    }
}
```

⇒ **「允许被读、并且维持」在工程上是可以做的**，代价是：**被读的那一侧必须长出变更通知**（`SetFloat` → `MarkContextFloatDependentsDirty` → 逐修正器比对 key → 标脏）。

⚠️ 但要注意它的**实际使用面**：写入方只有 `MobaResourceAttributeContextProjector.Refresh` 写 `resource.{name}.current / max / ratio`（短命资源条），**不是技能参数账本**。

### 5.5 AbilityKit 里没有「属性修正器读技能参数」

`MobaSkillParamModifierService` 的消费者全在技能/弹道/召唤/AOE/Buff/伤害侧（如 `SkillCastPreparationService.cs:95` 的 `paramModifiers.Skill.ResolveCastRange(...)`），**零处在属性修正器里**。

⇒ **它用门禁（§5.1）把这个问题消掉了**：技能参数只在 `CastPrepare` 阶段被读，读出后立刻用在「这次施法的准备」上；没有任何修正器会去持续跟随技能参数。

---

## 6. 汇总对照表

| # | 框架/游戏 | 冻 | 实时并维持 | 允许读但不维持 | 默认 | 冻结值的生命周期 |
|---|---|---|---|---|---|---|
| 1 | **UE GAS** | ✅ `bSnapshot=true` | ✅ `bSnapshot=false` | ❌ 不存在 | 实时 | 与 GE spec 同生同死 |
| 2 | **AbilityKit（修正器）** | ✅ `OnApplySnapshot` | ✅ `Realtime` | ❌ 不存在 | 实时 | 与 Continuous 运行时同生同死 |
| 3 | **AbilityKit（技能参数）** | ❌ | ❌ | ❌ | 门禁 | 不适用（读完即用） |
| 4 | **WoW** | 曾普遍使用，**正在拆** | ✅ 当前方向 | ❌ | 实时 | — |
| 5 | **LoL** | ✅（至少 Rell 一条） | 未确证 | ❌ 未确证 | 未确证 | `until the debuff expires` |

**跨框架稳定的三条**：

1. **「允许读但不建依赖」是最不受欢迎的一档**——五家里零实现。
2. **默认是实时**，冻是显式打开的例外。
3. **冻的实现动作高度一致**：把「怎么算」换成「一个数」——GAS 是 `TakeSnapshotOf` 拷一份 aggregator，AbilityKit 是 `MagnitudeSource.Fixed(capturedValue)`。**两者都等于「把一份值描述求成一个数，然后只留这个数」。**

---

## 7. 对本仓 N13 的含义（这一段是**推论**，不是上述框架的事实）

> ⚠️ 以下是我从上述事实里推出来的，**不是**任何一家的做法。判断由用户下。

### 7.1 三家能冻，共同前提是「读方与被读值同寿」

| 框架 | 冻结值住在哪 | 谁在读它 | 两者寿命 |
|---|---|---|---|
| GAS | `FGameplayEffectSpec::CapturedRelevantAttributes` | 本次 GE 的计算与结算 | **同**（都随这次 GE） |
| AbilityKit | `MobaContinuousRuntimeBase` 的 capture 槽 | 这颗 Continuous 修正器自己 | **同**（随这颗修正器） |
| LoL Rell | 减益自身的状态 | 该减益的后续层 | **同**（`until the debuff expires`） |

⇒ **冻之所以成立，是因为「拷出来的那份副本」与「读它的那个东西」一起生一起死**，副本不会比读者先没。

### 7.2 本仓这条前提**不成立**

- 被读的值：`FTcsParamBorn` 挂**一次施法实例**（[领域-Parameter-草稿.md](领域-Parameter-草稿.md) §3 / R2 已裁），一次施法可能只活 0.5 秒。
- 读者：属性修正器挂在**目标实体的属性账本**上（[领域-Attribute-草稿.md](领域-Attribute-草稿.md):1146 `AttachMod(Entity, ...)`），Buff 类活 10 秒、装备类活到卸下。

⇒ **读者的寿命 > 被读值的寿命。** 若让属性修正器持一个指向出生切片的引用，**切片会先死**。

因此属性修正器要拿到参数值，只有一条路：**在 `AttachMod` 那一刻把值抄进修正器自己身上**。而那恰好就是 N13 的**甲**。

⇒ 换句话说：**甲不是「比较干净的选项」，是「在生命周期上唯一存活的形状」。** 这也解释了为什么不能照抄 GAS 的 `bSnapshot`——GAS 的位挂在读点上、值住在 spec 里、读者是本次 GE 计算，三者同寿；**本仓的读者比被读值长寿，所以无论怎么设计都必须拷贝，而拷贝之后就没有「跟随」可言**。

⚠️ **这条推论唯一的软处**：它依赖「属性修正器寿命 > 施法实例寿命」这个前提。若实际配置里绝大多数修正器只在施法实例存续期内存在，这条论证就弱了，得回到「属性域靠依赖边维持缓存」那条判据（草稿 §4.5 与《领域-Attribute-草稿》§5.2）再裁。

### 7.3 顺带确认的一件事

「冻」在本仓的实现动作，与 GAS / AbilityKit 是同一个：**把表达式求成一个 double，只留这个 double**。草稿 §4.1 的原话「**「冻结」不是一个机制，是「把算好的 double 存进切片」这个动作的副产品**」在这三家里都能对上。

---

## 8. 未核查的部分（明确留白，不作结论）

| 方向 | 状态 | 若要补，从哪查起 |
|---|---|---|
| Dota 2 具体机制 | ❌ 未取得 | Valve 官方 workshop 文档的 `MODIFIER_PROPERTY_*` 求值说明；`Damage_Filter` 语义 |
| LoL 全局是否普遍快照 | ⚠️ 只有一条确证 | wiki.leagueoflegends.com 的机制页逐条翻，成本高 |
| WoW 里仍在冻的量（如吸收盾） | ⚠️ 只知「周期性效果被拆」，未系统核 | warcraft.wiki.gg 的 `Absorb` / `Shield` 页 |
| Unity DOTS / ECS 属性体系 | ❌ 未查 | 与本仓问题形状不同（无「效果存续期」概念），预期收益低 |
| Godot / 暗黑类 ARPG | ❌ 未查 | 预期收益低 |
| 商业引擎外的主流开源战斗框架（除 AbilityKit） | ❌ 未查 | GitHub 按 `gameplay effect attribute snapshot` 搜 |

---

## 9. 引用回查清单

| 主张 | 回查位置 |
|---|---|
| GAS 的 `bSnapshot` 默认 false、挂在捕获定义上 | `GAS 根\Public\GameplayEffectAttributeCaptureDefinition.h:30 / :49` |
| GAS 冻=深拷贝、不冻=共享 | `GAS 根\Private\GameplayEffect.cpp:3931-3941` |
| GAS 快照不拷依赖名单 | `GAS 根\Public\GameplayEffectAggregator.h:368-369` |
| GAS 冻过的修正器不参与重算 | `GAS 根\Private\GameplayEffect.cpp:1271 / :1281 / :3541` |
| GAS 环靠运行期截断 | `GAS 根\Private\GameplayEffectAggregator.cpp:600 / :609 / :611 / :616` |
| WoW 周期性效果取消快照（2014） | `https://warcraft.wiki.gg/wiki/Haste` patch changes `6.0.2` |
| WoW 2025 仍在取消 | `https://warcraft.wiki.gg/wiki/Mastery:_Life-Binder` patch `11.1.0` |
| WoW 动机（插件把快照变隐性技巧） | `https://warcraft.wiki.gg/wiki/Powershifting` |
| LoL Rell 快照且与减益同寿 | `https://wiki.leagueoflegends.com/en-us/Rell` |
| AbilityKit 技能参数门禁 | `.../Skill/Modifiers/Skill/MobaSkillParamGroupResolver.cs:35-38` |
| AbilityKit 双档策略 | `.../Continuous/Config/MobaContinuousConfigDecorators.cs:79-80` |
| AbilityKit 冻 = `MagnitudeSource.Fixed` | `.../Continuous/Modifiers/MobaAttributeContinuousModifierProjector.cs:171-190` |
| AbilityKit 公式强制冻 | `.../Continuous/Config/MobaContinuousConfigDecorators.cs:161-163` |
| AbilityKit `ContextFloat` 且维持依赖 | `.../Core/Source/MagnitudeSource.cs:596-601` + `AttributeContext.cs:135/143/239` + `AttributeInstance.cs:217` |
