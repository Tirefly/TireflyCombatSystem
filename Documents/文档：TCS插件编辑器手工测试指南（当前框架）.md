# 文档：TCS插件编辑器手工测试指南（当前框架，C#脚本版）

> 适用范围：`migrate-manager-api-to-component` 完成后的当前 TCS 框架。
>
> 目标：在 **不依赖自动化 Spec** 的前提下，通过编辑器、PIE、测试资产和 **UnrealSharp C# 测试脚本**，尽可能完整地覆盖当前 TCS 插件的核心能力。

---

## 1. 文档目标

这份文档是一份可执行的手工测试方案，但它的执行层已经不再默认是蓝图，而是改成：

- 用 **C# 测试脚本** 编排测试步骤、打印快照、绑定事件、批量触发行为。
- 用 **编辑器资产** 承载 DataAsset、GameplayTag、StateTree、Map 等内容。
- 只有在 StateTree Task 或可视化 UI 确实更适合放在资产侧时，才保留极小的蓝图兜底。

你按本文搭出一套最小测试场景后，应当能够覆盖：

- `UTcsAttributeComponent` 的属性增删改查、Modifier 应用与移除、范围约束、边界事件。
- `UTcsStateComponent` 的状态应用、持续时间、移除、合并、叠层、槽位优先级、Gate、查询一致性。
- 状态移除时，通过 `SourceHandle` 自动清理 Modifier 的整条链路。
- 迁移执行文档要求重点验证的 `9.2`、`9.3`、`11.1`、`11.2`、`11.3` 相关行为。

这份文档默认你做的是：

- **编辑器内手工验证**。
- **C# 测试脚本驱动**。
- **尽量少写蓝图测试逻辑**。

如果你的目标是“尽可能少手工拉蓝图线”，那么本文现在给出的闭环是：

- TCS 组件级功能验证由 C# 驱动完成。
- DataAsset / GameplayTag / StateTree 这类资源仍然在编辑器内配置。
- 状态实例自己的 StateTree 效果逻辑，可以继续用一个极小的蓝图 Task，也可以在 Glue 暴露足够时改成 C# Task。

有一个边界仍然必须提前讲清：

- OpenSpec 里的 `8.3` / `8.4` 本质上是在验证 **C++ native protected virtual override 是否被命中**。
- C# 测试脚本可以更稳定地验证功能行为，但它仍然不等于“验证 native C++ override 已命中”。

所以，这份文档现在闭合的是：

- **编辑器内、C# 脚本驱动的功能验证闭环**。

它仍然不是：

- **native C++ protected virtual override 命中证明**。

---

## 2. 当前框架的真实边界

先把边界讲清楚，否则后面的测试方案还是会走偏。

### 2.1 Manager 已经不是业务入口

当前迁移后的真实结构仍然是：

- `UTcsStateManagerSubsystem`：全局定义缓存、DefId/Tag 查询、实例 ID 分配、门面转发。
- `UTcsAttributeManagerSubsystem`：全局属性/修改器定义缓存、Tag/Name 查询、实例 ID 分配、`SourceHandle` 创建。
- `UTcsStateComponent`：Actor 本地状态生命周期、槽位、查询、移除、Duration、StateTree 调度。
- `UTcsAttributeComponent`：Actor 本地属性、Modifier、重算、Clamp、按 `SourceHandle` 清理。

所以这份文档的测试主体，依然必须围绕 **两个 Component** 来设计，而不是围绕 Manager 设计。

### 2.2 当前 TCS 的 StateTree 专用面并不大

当前 TCS 明确可用的 StateTree 专用能力主要还是：

- `UTcsStateTreeSchema_StateInstance`
- `FTcsStateChangeNotifyTask`
- `FTcsStateSlotDebugEvaluator`

`UTcsStateTreeSchema_StateInstance` 当前暴露的外部上下文包括：

- `Owner`
- `Instigator`
- `StateInstance`
- `OwnerController`
- `OwnerStateCmp`
- `OwnerAttributeCmp`
- `OwnerSkillCmp`
- `InstigatorController`
- `InstigatorStateCmp`
- `InstigatorAttributeCmp`
- `InstigatorSkillCmp`

这意味着：

- 你可以在状态实例自己的 StateTree 中读到拥有者、施加者、状态实例以及两侧组件。
- 但当前 **没有内置的通用“Buff 进入时自动应用属性 Modifier”任务节点**。

所以本文仍然保留两条现实路径：

1. **C# 烟雾路径**：在测试脚本里监听 `OnStateApplySuccess`，拿 `StateInstance.SourceHandle` 去调用 `ApplyModifierWithSourceHandle(...)`。
2. **完整 StateTree 路径**：把相同逻辑放进一个极小的 StateTree Task。这个 Task 可以继续是蓝图，也可以在 Glue 足够时改成 C#。

建议先跑路径 1，把大部分链路跑通；再跑路径 2，补足 StateTree 集成验证。

### 2.3 当前移除模型是单阶段立即收敛

当前状态移除依然是单阶段模型：

- 没有 `PendingRemoval`
- 没有 `RemovalFlowPolicy`
- 没有 `HardTimeout`

测试文档里所有“移除后的期望结果”，都基于 **立即进入最终移除收敛** 的现状来写。

### 2.4 C# 测试脚本在当前项目里的正确位置

当前仓库的 UnrealSharp 结构已经明确：

- 用户脚本工程：`Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj`
- 生成的 Glue 工程：`Script/TireflyGameplayUtils.RuntimeGlue/TireflyGameplayUtils.RuntimeGlue.csproj`

因此当前测试脚本的正确放置方式是：

- 业务测试脚本放在 `Script/ManagedTireflyGameplayUtils/` 下面。
- `*.Glue`、`obj/UHT/**/*.generated.cs` 都是生成物，**禁止手改**。

当前手测方案会用到的 TCS 核心接口，已经是反射可见的 `UFUNCTION(BlueprintCallable)`，例如：

- `AddAttribute`
- `AddAttributeByTag`
- `SetAttributeBaseValue`
- `SetAttributeCurrentValue`
- `ResetAttribute`
- `CreateAttributeModifier`
- `ApplyModifier`
- `ApplyModifierWithSourceHandle`
- `GetModifiersBySourceHandle`
- `RemoveModifiersBySourceHandle`
- `TryApplyState`
- `RemoveState`
- `RemoveStatesByDefId`
- `RemoveAllStatesInSlot`
- `RemoveAllStates`
- `GetStateDebugSnapshot`
- `GetSlotDebugSnapshot`
- `SetSlotGateOpen`

这意味着本文现在完全可以把 **C#** 作为测试执行层来设计，而不必再让蓝图承担绝大部分测试编排工作。

---

## 3. 推荐测试目录结构

建议继续把测试资产与测试脚本分开管理，不要把它们混入正式逻辑目录。

### 3.1 编辑器资产目录

```text
/Game/TCS_Test/
  Blueprints/
  Maps/
  StateTrees/
  Attributes/
  AttributeModifiers/
  States/
  StateSlots/
  Widgets/
```

建议命名保持统一：

- Attribute 资产：`DA_Attr_*`
- Modifier 资产：`DA_AttrMod_*`
- State 资产：`DA_State_*`
- StateSlot 资产：`DA_StateSlot_*`
- 状态树：`ST_*`
- 测试地图：`Map_TcsManualTest`
- 可选调试 UI：`WBP_TcsTestPanel`

### 3.2 C# 测试脚本目录

建议在用户脚本工程下建立单独的测试目录：

```text
Script/
  ManagedTireflyGameplayUtils/
    TcsTests/
      Actors/
        ATcsTestEntity.cs
        ATcsTestHarness.cs
      Support/
        FTcsAttributeSnapshot.cs
        FTcsStateSnapshot.cs
        UTcsTestLogLibrary.cs
```

这里的命名建议是：

- Actor 用 `A*`
- UObject / Library 用 `U*`
- Struct 用 `F*`

这是 UnrealSharp 的硬性命名前缀约束之一，别在测试脚本里破坏它。

---

## 4. 测试前准备

### 4.1 编译基线

开始搭测试场景前，至少先保证下面三件事：

1. `TireflyGameplayUtilsEditor Win64 Development` 能正常编译。
2. `Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj` 能正常编译并被编辑器热重载。
3. 如果你新增了新的反射类型或接口，相关的 Glue 工程能生成成功；但 `Script/TireflyGameplayUtils.RuntimeGlue` 只用于验证，不用于手改。

如果第 2 或第 3 步不稳定，先修 UnrealSharp / Glue / 热重载问题，再谈测试资产。否则你后面会把“脚本层没加载”误判成“TCS 功能坏了”。

### 4.2 GameplayTag 预配置

建议至少准备以下 Tag：

```text
TCS.Attribute.Health
TCS.Attribute.MaxHealth
TCS.Attribute.Attack
TCS.Attribute.MoveSpeed

TCS.State.Buff.AttackUp
TCS.State.Buff.MaxHealthAura
TCS.State.Buff.UniqueShield
TCS.State.Debuff.Slow
TCS.State.Action.LowAction
TCS.State.Action.HighAction
TCS.State.Buff.LowHealthOnly

StateSlot.Buff
StateSlot.Action
StateSlot.Debuff
```

如果你不想一次配太多，最少也要先配好：

- `TCS.Attribute.*`
- `TCS.State.*`
- `StateSlot.*`

### 4.3 让测试资产能被 TCS 扫描到

当前插件里，`UTcsDeveloperSettings` 会在编辑器模式下扫描 `AssetManager` 配置过的 Primary Asset 目录，并缓存：

- Attribute Definition
- State Definition
- StateSlot Definition
- Attribute Modifier Definition

所以你创建测试资产后，要确认两件事：

1. 这些资产所在目录被 `AssetManager` 的对应 `PrimaryAssetTypesToScan` 覆盖。
2. 编辑器已经重新扫描到这些资产。

推荐做法：

1. 把 `/Game/TCS_Test/Definition/Attribute`
2. 把 `/Game/TCS_Test/Definition/AttributeModifier`
3. 把 `/Game/TCS_Test/Definition/Buff`
4. 把 `/Game/TCS_Test/Definition/Skill`
5. 把 `/Game/TCS_Test/Definition/StateSlot`

分别加入项目对应的 Primary Asset 扫描路径。

如果你已经加入路径，但 `TryApplyState` 或 `AddAttributeByTag` 仍然找不到定义，先不要怀疑业务代码，先检查：

- AssetManager 扫描路径是否正确
- 资产是否已保存
- 编辑器是否重启过一次

### 4.4 调试窗口建议

开始 PIE 前，把这几个窗口都打开：

- Output Log
- Details
- World Outliner
- StateTree Debugger（如果你要测 StateTree 路径）

推荐额外加一个调试输出层，至少实时显示：

- 当前 Attribute 值
- 当前 State 快照
- 最近一次 State / Attribute 事件
- 当前测试步骤编号

这里的输出层，优先用 **C# 脚本统一打印**，不要再把事件打印逻辑分散在多个蓝图 Event Graph 里。

---

## 5. 测试资产设计

这一节仍然是整套手测方案的核心。测试脚本换成 C# 之后，测试资产设计本身并没有变。

## 5.1 Attribute 定义

建议先只做 4 个属性，已经足够覆盖大多数链路。

| 资产名 | AttributeDefId | AttributeTag | 初始值建议 | Range 配置 | 用途 |
|---|---|---|---|---|---|
| `DA_Attr_MaxHealth` | `Attr_MaxHealth` | `TCS.Attribute.MaxHealth` | 100 | `Min=Static 1`, `Max=None` | 测试动态上限依赖 |
| `DA_Attr_Health` | `Attr_Health` | `TCS.Attribute.Health` | 100 | `Min=Static 0`, `Max=Dynamic Attr_MaxHealth` | 测试 Clamp、边界事件、移除时回落 |
| `DA_Attr_Attack` | `Attr_Attack` | `TCS.Attribute.Attack` | 10 | `Min=Static 0`, `Max=None` | 测试加攻 Buff |
| `DA_Attr_MoveSpeed` | `Attr_MoveSpeed` | `TCS.Attribute.MoveSpeed` | 600 | `Min=Static 0`, `Max=None` | 测试减速 Debuff |

推荐字段填写：

- `DA_Attr_MaxHealth`
  - `AttributeDefId = Attr_MaxHealth`
  - `AttributeTag = TCS.Attribute.MaxHealth`
  - `AttributeName = Max Health`
  - `AttributeRange.MinValueType = Static`
  - `AttributeRange.MinValue = 1`

- `DA_Attr_Health`
  - `AttributeDefId = Attr_Health`
  - `AttributeTag = TCS.Attribute.Health`
  - `AttributeName = Health`
  - `AttributeRange.MinValueType = Static`
  - `AttributeRange.MinValue = 0`
  - `AttributeRange.MaxValueType = Dynamic`
  - `AttributeRange.MaxValueAttribute = Attr_MaxHealth`

- `DA_Attr_Attack`
  - `AttributeDefId = Attr_Attack`
  - `AttributeTag = TCS.Attribute.Attack`
  - `AttributeName = Attack`
  - `AttributeRange.MinValueType = Static`
  - `AttributeRange.MinValue = 0`

- `DA_Attr_MoveSpeed`
  - `AttributeDefId = Attr_MoveSpeed`
  - `AttributeTag = TCS.Attribute.MoveSpeed`
  - `AttributeName = Move Speed`
  - `AttributeRange.MinValueType = Static`
  - `AttributeRange.MinValue = 0`

这 4 个属性已经足够覆盖：

- 动态上限 Clamp
- Base / Current 差异
- 正向 Buff
- 负向 Debuff
- 边界事件

## 5.2 AttributeModifier 定义

当前内置执行器明确支持 `Operands["Magnitude"]`。为了手测确定性，建议所有测试 Modifier 都只使用这个键。

推荐先做 5 个 Modifier：

| 资产名 | AttributeModifierDefId | AttributeName | ModifierMode | ModifierType | Operands | MergerType | 用途 |
|---|---|---|---|---|---|---|---|
| `DA_AttrMod_AttackPlus20` | `AttrMod_AttackPlus20` | `Attr_Attack` | `BaseValue` | `UTcsAttrModExec_Addition` | `Magnitude=20` | `UTcsAttrModMerger_NoMerge` | 标准加攻 Buff |
| `DA_AttrMod_AttackPlus50` | `AttrMod_AttackPlus50` | `Attr_Attack` | `BaseValue` | `UTcsAttrModExec_Addition` | `Magnitude=50` | `UTcsAttrModMerger_NoMerge` | 更强加攻，用于 Unique Buff |
| `DA_AttrMod_MaxHealthPlus50` | `AttrMod_MaxHealthPlus50` | `Attr_MaxHealth` | `BaseValue` | `UTcsAttrModExec_Addition` | `Magnitude=50` | `UTcsAttrModMerger_NoMerge` | 测试动态 MaxHealth |
| `DA_AttrMod_MoveSpeedMinus200` | `AttrMod_MoveSpeedMinus200` | `Attr_MoveSpeed` | `BaseValue` | `UTcsAttrModExec_Addition` | `Magnitude=-200` | `UTcsAttrModMerger_NoMerge` | 标准减速 Debuff |
| `DA_AttrMod_HealthMinus120` | `AttrMod_HealthMinus120` | `Attr_Health` | `CurrentValue` | `UTcsAttrModExec_Addition` | `Magnitude=-120` | `UTcsAttrModMerger_NoMerge` | 测试最小值 Clamp 和边界 |

说明：

- `Attack` / `MoveSpeed` 用 `BaseValue` 更稳定，便于观察状态移除后的回退。
- `HealthMinus120` 刻意做成 `CurrentValue`，便于测试掉血、Clamp 到 0、触发 `OnAttributeReachedBoundary`。

## 5.3 StateSlot 定义

先做 3 个槽位，已经能覆盖绝大部分状态行为。

| 资产名 | StateSlotDefId | SlotTag | ActivationMode | GateCloseBehavior | PreemptionPolicy | SamePriorityPolicy | 用途 |
|---|---|---|---|---|---|---|---|
| `DA_StateSlot_Buff` | `StateSlot_Buff` | `StateSlot.Buff` | `AllActive` | `Pause` | `PauseLowerPriority` | `UseNewest` | 多个 Buff 共存 |
| `DA_StateSlot_Debuff` | `StateSlot_Debuff` | `StateSlot.Debuff` | `AllActive` | `Pause` | `PauseLowerPriority` | `UseNewest` | 多个 Debuff 共存 |
| `DA_StateSlot_Action` | `StateSlot_Action` | `StateSlot.Action` | `PriorityOnly` | `Cancel` | `PauseLowerPriority` | `UseNewest` | 优先级、抢占、Gate 关闭取消 |

这里的取舍是刻意的：

- `Buff` / `Debuff` 槽位用 `AllActive`，便于验证多个状态同时激活。
- `Action` 槽位用 `PriorityOnly + Cancel Gate`，便于验证低优先级被压制、关闭 Gate 直接失败或取消。

第一轮测试时，`StateTreeStateName` 可以先留空。这样你可以只用 C# 测试脚本直接调 `SetSlotGateOpen(...)`，不必一开始就引入组件级 StateTree Gate 驱动。

## 5.4 State 定义

建议先做 6 个 State，分别覆盖持续时间、叠层、合并、优先级、条件失败、查询一致性。

| 资产名 | StateDefId | StateTag | Slot | Priority | Duration | MaxStack | MergerType | 作用 |
|---|---|---|---|---|---|---|---|---|
| `DA_State_Buff_AttackUp` | `State_Buff_AttackUp` | `TCS.State.Buff.AttackUp` | `StateSlot.Buff` | 10 | `Duration=10` | 3 | `UTcsStateMerger_StackDirectly` | 可叠层加攻 Buff |
| `DA_State_Buff_MaxHealthAura` | `State_Buff_MaxHealthAura` | `TCS.State.Buff.MaxHealthAura` | `StateSlot.Buff` | 20 | `Duration=6` | 1 | `UTcsStateMerger_UseNewest` | 提升 MaxHealth，过期后测试 Health Clamp |
| `DA_State_Buff_UniqueShield` | `State_Buff_UniqueShield` | `TCS.State.Buff.UniqueShield` | `StateSlot.Buff` | 30 | `Duration=12` | 1 | `UTcsStateMerger_UseNewest` | 第二次施加淘汰旧实例，触发 `MergedOut` |
| `DA_State_Debuff_Slow` | `State_Debuff_Slow` | `TCS.State.Debuff.Slow` | `StateSlot.Debuff` | 5 | `Duration=8` | 1 | `UTcsStateMerger_UseNewest` | 移速降低 |
| `DA_State_Action_Low` | `State_Action_Low` | `TCS.State.Action.LowAction` | `StateSlot.Action` | 10 | `Duration=15` | 1 | `UTcsStateMerger_UseOldest` | 低优先级动作状态 |
| `DA_State_Action_High` | `State_Action_High` | `TCS.State.Action.HighAction` | `StateSlot.Action` | 100 | `Duration=5` | 1 | `UTcsStateMerger_UseOldest` | 高优先级动作状态 |

额外再补一个条件型 State：

| 资产名 | StateDefId | 条件 | 用途 |
|---|---|---|---|
| `DA_State_Buff_LowHealthOnly` | `State_Buff_LowHealthOnly` | `Owner.Attr_Health <= 50` | 测试 `ApplyConditionsFailed` |

### 5.4.1 条件型 State 配置建议

给 `DA_State_Buff_LowHealthOnly` 配一个 `ActiveConditions`：

- `ConditionClass = UTcsStateCondition_AttributeComparison`
- `bCheckWhenApplying = true`
- `Payload.AttributeName = Attr_Health`
- `Payload.CheckTarget = Owner`
- `Payload.ComparisonType = LessThanOrEqual`
- `Payload.CompareValue = 50`

这个状态不需要效果多复杂，它的主要职责就是稳定触发：

- HP > 50 时应用失败
- HP <= 50 时应用成功

## 5.5 推荐的 Buff 效果映射

建议把 State 和 Modifier 的关系固定下来，后面直接在 C# 测试脚本里维护一份相同映射：

| StateDefId | 进入时应用的 Modifier |
|---|---|
| `State_Buff_AttackUp` | `AttrMod_AttackPlus20` |
| `State_Buff_MaxHealthAura` | `AttrMod_MaxHealthPlus50` |
| `State_Buff_UniqueShield` | `AttrMod_AttackPlus50` |
| `State_Debuff_Slow` | `AttrMod_MoveSpeedMinus200` |

这样你后面不管是用 C# 代理逻辑还是 StateTree Task，逻辑都很清楚。

---

## 6. 测试脚本与控制器设计

这一节是本次文档更新的核心。以前这里主要靠蓝图，现在应该改成：

- 一个 **C# 测试实体** 负责承载 TCS 组件、初始化、事件绑定、快照打印。
- 一个 **C# 测试控制器 / 测试平台** 负责触发动作、组合步骤、实现同帧多次 Apply 等测试场景。

## 6.1 C# 测试实体：`ATcsTestEntity`

建议基于 `AActor` 在 C# 中建立一个统一的测试实体，并让它承担原来 `BP_TcsTestEntity` 的全部职责。

职责建议：

- 挂载 `UTcsStateComponent`
- 挂载 `UTcsAttributeComponent`
- 挂载 `UTcsSkillComponent`
- 在 `BeginPlay` 里初始化基础属性
- 绑定 Attribute / State 关键事件
- 输出统一格式的日志与快照
- 提供若干测试辅助函数

### 6.1.1 最小脚本结构示意

下面这段不是完整实现，只是说明 UnrealSharp 下推荐的代码形状：

```csharp
using UnrealSharp.Attributes;
using UnrealSharp.Engine;

namespace ManagedTireflyGameplayUtils.TcsTests;

[UClass]
public partial class ATcsTestEntity : AActor
{
    [UProperty(DefaultComponent = true)]
    public partial UTcsStateComponent StateComponent { get; set; }

    [UProperty(DefaultComponent = true)]
    public partial UTcsAttributeComponent AttributeComponent { get; set; }

    [UProperty(DefaultComponent = true)]
    public partial UTcsSkillComponent SkillComponent { get; set; }

    public override void BeginPlay()
    {
        base.BeginPlay();
        InitializeBaseAttributes();
        BindTcsEvents();
    }
}
```

你真正实现时，建议把它放在：

- `Script/ManagedTireflyGameplayUtils/TcsTests/Actors/ATcsTestEntity.cs`

### 6.1.2 接口实现建议

如果你的测试流程需要严格遵守 TCS 的实体接口约束，建议这个 Actor 实现 `ITcsEntityInterface`，并稳定返回：

- `GetStateComponent` -> `StateComponent`
- `GetAttributeComponent` -> `AttributeComponent`
- `GetSkillComponent` -> `SkillComponent`
- `GetCombatEntityType` -> 固定测试 Tag
- `GetCombatEntityLevel` -> `1`

如果你当前只是做组件级手测，不需要把接口实现写得很重，但至少要保证 TCS 状态实例初始化时能把 Owner / Instigator 识别成合法战斗实体。

### 6.1.3 BeginPlay 初始化

在 `BeginPlay` 中做：

1. 添加 4 个基础属性。
2. 初始化值：
   - `Attr_MaxHealth = 100`
   - `Attr_Health = 100`
   - `Attr_Attack = 10`
   - `Attr_MoveSpeed = 600`
3. 绑定 State / Attribute 的所有关键事件。
4. 每次事件发生都统一打印到屏幕和日志。

建议绑定的事件：

- `OnAttributeValueChanged`
- `OnAttributeBaseValueChanged`
- `OnAttributeModifierAdded`
- `OnAttributeModifierRemoved`
- `OnAttributeModifierUpdated`
- `OnAttributeReachedBoundary`
- `OnStateApplySuccess`
- `OnStateApplyFailed`
- `OnStateStageChanged`
- `OnStateRemoved`
- `OnStateMerged`
- `OnStateStackChanged`
- `OnStateDurationRefreshed`
- `OnSlotGateStateChanged`

这里的关键变化是：

- 以前这些事件可能散落在蓝图节点里。
- 现在应该统一由 `ATcsTestEntity` 在 C# 中绑定，并输出标准化日志。

### 6.1.4 必做的调试函数

给 `ATcsTestEntity` 至少做这几个可直接调用的函数：

1. `DumpAttributeSnapshot()`
2. `DumpStateSnapshot()`
3. `DumpAllSnapshot()`
4. `SetHealthForConditionTest(float newValue)`

建议输出内容：

- Attribute：`MaxHealth / Health / Attack / MoveSpeed`
- State：调用 `GetStateDebugSnapshot()`
- Slot：调用 `GetSlotDebugSnapshot()`
- 当前时间戳 / 当前测试步骤编号

### 6.1.5 建议增加的脚本状态

建议在 `ATcsTestEntity` 里额外保留几个测试用字段：

- `bUseScriptDrivenBuffEffects`
- `bDestroyOwnerOnFirstExpire`
- `bVerboseEventLog`
- `LastAppliedSourceHandleId`

这些字段能让你在同一个测试实体上切换：

- 是用脚本路径给状态挂效果，还是用 StateTree 路径。
- 在 S1 场景里是否启用“过期时自毁 Owner”。

## 6.2 C# 测试控制器：`ATcsTestHarness`

再做一个单独的测试控制 Actor，放到关卡里，专门持有：

- `TargetEntity`
- `InstigatorEntity`

它的职责是：

- 提供一组稳定的测试入口函数。
- 把“同帧多次 Apply”“批量清状态”“打印完整快照”等组合操作集中起来。
- 避免你每轮手测都要手工点组件、手工拼顺序。

建议放置路径：

- `Script/ManagedTireflyGameplayUtils/TcsTests/Actors/ATcsTestHarness.cs`

### 6.2.1 推荐入口形式

你可以按实际习惯选择一种或多种入口：

1. `CallInEditor` 按钮：适合编辑器内直接点测试步骤。
2. PIE 按键绑定：适合快速重复跑同一组步骤。
3. 可选 UMG 面板：适合把若干关键测试按钮可视化。

现在的建议是：

- **测试逻辑写在 C#**。
- **是否提供按钮 / 按键 / UI 只是触发方式问题**。

### 6.2.2 推荐函数列表

建议至少做下面这组函数：

| 入口函数 | 默认输入（可选） | 动作 |
|---|---|---|
| `ApplyAttackUp()` | `1` | Target 应用 `State_Buff_AttackUp` |
| `ApplyMaxHealthAura()` | `2` | Target 应用 `State_Buff_MaxHealthAura` |
| `ApplyUniqueShield()` | `3` | Target 应用 `State_Buff_UniqueShield` |
| `ApplySlow()` | `4` | Target 应用 `State_Debuff_Slow` |
| `ApplyLowAction()` | `5` | Target 应用 `State_Action_Low` |
| `ApplyHighAction()` | `6` | Target 应用 `State_Action_High` |
| `CloseActionGate()` | `7` | `SetSlotGateOpen(StateSlot.Action, false)` |
| `OpenActionGate()` | `8` | `SetSlotGateOpen(StateSlot.Action, true)` |
| `RemoveAllStates()` | `9` | Target 清空所有状态 |
| `DumpAll()` | `0` | 打印完整快照 |
| `SetLowHealth()` | `-` | 把 `Health` 设置为 `40` |
| `ApplyLowHealthOnly()` | `=` | 应用 `State_Buff_LowHealthOnly` |
| `ApplyBurstSameFrame()` | 自定义 | 同一帧连续 Apply 多个 State |

### 6.2.3 建议的日志策略

`ATcsTestHarness` 不要只负责调用 API，还应该在每个入口前后都打统一日志，例如：

```text
[TCS-Test][C-1][Before] ApplyAttackUp
[TCS-Test][C-1][After] Attack=30, ActiveStates=1
```

这样你后面回看 Output Log 时，能够准确对应到本文里的测试编号，而不是只看到一堆散乱事件。

## 6.3 推荐的脚本内数据结构

建议在 C# 测试脚本里统一维护几份静态映射：

1. `AttributeDefId -> 初始值`
2. `StateDefId -> ModifierId[]`
3. `测试步骤编号 -> 执行动作`

最重要的是第二张表，也就是：

```text
State_Buff_AttackUp      -> [AttrMod_AttackPlus20]
State_Buff_MaxHealthAura -> [AttrMod_MaxHealthPlus50]
State_Buff_UniqueShield  -> [AttrMod_AttackPlus50]
State_Debuff_Slow        -> [AttrMod_MoveSpeedMinus200]
```

这份映射会直接用于 7.1 的脚本烟雾路径。

---

## 7. 两条 Buff 效果实现路径

## 7.1 路径 A：最快的 C# 烟雾测试路径

这是推荐你最先跑的一条路径，因为实现成本最低，而且最能验证当前 TCS 的核心清理链路。

### 7.1.1 思路

在 `ATcsTestEntity` 的 `OnStateApplySuccess` 回调里：

1. 取 `CreatedStateInstance`
2. 读取 `CreatedStateInstance.SourceHandle`
3. 按 `StateDefId` 查本地映射表
4. 调 `AttributeComponent.ApplyModifierWithSourceHandle(SourceHandle, ModifierIds, OutModifiers)`

这样你不需要先做自定义 StateTree Task，就可以验证：

- 状态能应用成功
- `SourceHandle` 能绑定 Modifier
- 状态移除时 Modifier 会自动清理

### 7.1.2 这条路径验证什么

它主要验证：

- `TryApplyState`
- `TryApplyStateInstance`
- `SourceHandle` 生命周期
- `FinalizeStateRemoval -> RemoveModifiersBySourceHandle`
- Attribute 事件广播

它**不充分验证**状态实例自己的 `StateTreeRef` 逻辑，所以跑完这条路径后，还要走 7.2。

## 7.2 路径 B：完整的 StateTree 集成路径

这是更贴近最终系统设计的一条路径。

### 7.2.1 推荐做一个极小的 StateTree Task

这个 Task 的职责只做一件事：

- `EnterState` 时，根据配置的 `ModifierIds`，对 `OwnerAttributeCmp` 调用 `ApplyModifierWithSourceHandle(StateInstance.SourceHandle, ModifierIds)`。

它不需要承担清理工作，因为移除时 TCS 已经会基于 `SourceHandle` 做回收。

### 7.2.2 资产侧与脚本侧怎么分工

推荐分工是：

- **脚本侧**：负责建测试实体、控制器、事件日志、快照、断点式观察。
- **StateTree 资产侧**：只保留最小的效果挂载逻辑。

当前更务实的选择是：

1. 先做一个极小的蓝图 `StateTreeTaskBlueprintBase` Task。
2. 如果后续确认相关 StateTree 基类在 Glue 中已经稳定可用，再把这个 Task 改写成 C#。

这样不会把“StateTree 任务类是否已完全适配 UnrealSharp”与“TCS 功能是否正常”混在一起排查。

### 7.2.3 推荐的状态树结构

对每个 Buff State，都做一个很小的状态树即可：

```text
Root
└─ Active
   ├─ Tcs State Change Notify Task
   └─ ApplyModifiersFromState
```

其中：

- `Tcs State Change Notify Task` 保证状态切换时组件能收到通知。
- `ApplyModifiersFromState` 负责真正把 Buff 效果挂到属性上。

### 7.2.4 推荐绑定

- `ST_Buff_AttackUp`
  - Task 参数：`ModifierIds = [AttrMod_AttackPlus20]`

- `ST_Buff_MaxHealthAura`
  - Task 参数：`ModifierIds = [AttrMod_MaxHealthPlus50]`

- `ST_Buff_UniqueShield`
  - Task 参数：`ModifierIds = [AttrMod_AttackPlus50]`

- `ST_Debuff_Slow`
  - Task 参数：`ModifierIds = [AttrMod_MoveSpeedMinus200]`

---

## 8. 推荐执行顺序

不要一上来就测最复杂的 S1 / S3。推荐按下面顺序执行。

1. 属性基础能力
2. Modifier 基础能力
3. 状态基础应用 / 移除
4. SourceHandle 清理链路
5. 槽位优先级 / Gate
6. 查询一致性
7. 条件失败与边界行为
8. 子类覆写验证边界说明
9. S1 / S2 / S3 边缘场景

---

## 9. 具体测试步骤

## 9.1 测试 A：属性基础能力

### A-1 初始化检查

操作：

1. 进入 PIE。
2. 通过 `ATcsTestHarness.DumpAll()` 或 `ATcsTestEntity.DumpAttributeSnapshot()` 打印初始快照。

预期：

- `MaxHealth = 100`
- `Health = 100`
- `Attack = 10`
- `MoveSpeed = 600`
- 无错误日志

### A-2 BaseValue 设置检查

操作：

1. 在脚本里调用 `SetAttributeBaseValue(Attr_Attack, 25)`。
2. 打印快照。

预期：

- `Attack` 变为 `25`
- 触发 `OnAttributeBaseValueChanged`
- 若当前值与基础值联动，`OnAttributeValueChanged` 也应出现

### A-3 CurrentValue 设置检查

操作：

1. 在脚本里调用 `SetAttributeCurrentValue(Attr_Health, 80)`。
2. 打印快照。

预期：

- `Health.CurrentValue = 80`
- `Health.BaseValue` 不变
- 触发 `OnAttributeValueChanged`

### A-4 ResetAttribute 检查

操作：

1. 对 `Attr_Attack` 做若干修改。
2. 调 `ResetAttribute(Attr_Attack)`。

预期：

- `Attack` 恢复到初始值
- 事件正常广播

### A-5 动态 Clamp 检查

操作：

1. 把 `Health` 直接设为 `999`。
2. 打印快照。

预期：

- `Health` 最终被 Clamp 到 `MaxHealth`
- 如果当前 `MaxHealth = 100`，则 `Health = 100`
- 会出现数值变更事件

### A-6 最小值边界检查

操作：

1. 应用 `AttrMod_HealthMinus120`。
2. 打印快照。

预期：

- `Health` 不会小于 `0`
- 触发 `OnAttributeReachedBoundary(Attribute=Attr_Health, bIsMaxBoundary=false)`

## 9.2 测试 B：Modifier 基础能力

### B-1 直接创建与应用 Modifier

操作：

1. 调 `CreateAttributeModifier(AttrMod_AttackPlus20, Instigator)`。
2. 调 `ApplyModifier(...)`。
3. 打印属性快照。

预期：

- `Attack = 30`
- `OnAttributeModifierAdded` 被触发
- `OnAttributeBaseValueChanged` 被触发

### B-2 按 SourceHandle 查询 Modifier

操作：

1. 通过 Manager 或状态实例创建一个独立 `SourceHandle`。
2. 用这个 Handle 调 `ApplyModifierWithSourceHandle`。
3. 立刻调 `GetModifiersBySourceHandle`。

预期：

- 能查询到刚才挂上的 Modifier
- 数量和 `ModifierIds` 一致

### B-3 按 SourceHandle 清理 Modifier

操作：

1. 继续使用 B-2 的 Handle。
2. 调 `RemoveModifiersBySourceHandle`。
3. 打印快照。

预期：

- `Attack` 回到清理前的基线值
- `OnAttributeModifierRemoved` 被触发

## 9.3 测试 C：状态基础应用与持续时间

### C-1 应用 AttackUp

操作：

1. 调 `ATcsTestHarness.ApplyAttackUp()`。
2. 立刻打印状态快照和属性快照。

预期：

- `OnStateApplySuccess` 被触发
- 状态在 `StateSlot.Buff`
- 阶段为 `SS_Active`（如果 Gate 是开的）
- `Attack` 增加 20

### C-2 Duration 到期

操作：

1. 保持不再手动移除。
2. 等待 10 秒。

预期：

- 状态自然过期
- 触发 `OnStateRemoved(RemovalReason=Expired)`
- `Attack` 回落到原值

这条就是执行文档 `9.2` 里的“Duration 到期触发 `ExpireState`”。

### C-3 手动 RemoveState / RemoveStatesByDefId / RemoveAllStatesInSlot / RemoveAllStates

建议分别做 4 轮操作：

1. 单实例移除
2. 同 DefId 批量移除
3. 清空 `StateSlot.Buff`
4. 清空全部状态

预期统一是：

- 状态从索引和槽位都消失
- `OnStateRemoved(RemovalReason=Removed)` 被触发
- 与该状态绑定的 Modifier 自动清掉

这条对应执行文档 `9.2` 中要求验证的全部手工移除入口。

## 9.4 测试 D：SourceHandle 清理链路

这是当前迁移里最关键的链路之一，必须单独验证。

操作：

1. 应用 `State_Buff_MaxHealthAura`。
2. 确认 `MaxHealth` 从 `100 -> 150`。
3. 手动把 `Health` 设到 `150`。
4. 等状态自然过期或手动移除。
5. 打印属性快照。

预期：

- `MaxHealth` 回到 `100`
- `Health` 被重新 Clamp 到 `100`
- 说明 `FinalizeStateRemoval -> RemoveModifiersBySourceHandle -> Recalculate / Clamp` 整条链路是通的

这条就是执行文档 `9.2` 里的：

- `FinalizeStateRemoval` 触发 `SourceHandle` 对应 Modifier 清理

## 9.5 测试 E：叠层与合并

### E-1 `StackDirectly`

操作：

1. 连续对同一 Target 施加 3 次 `State_Buff_AttackUp`。
2. 每次施加后打印状态快照。

预期：

- 叠层数从 `1 -> 2 -> 3`
- `OnStateStackChanged` 被触发
- `Attack` 每层再加 20
- 第 4 次施加时行为应受 `MaxStackCount=3` 和当前合并策略约束

### E-2 `MergedOut`

操作：

1. 施加 `State_Buff_UniqueShield`。
2. 记录其实例 ID 或 Apply 时间。
3. 立即再施加一次同状态。

预期：

- 因为 `MergerType = UTcsStateMerger_UseNewest`
- 第二次应用后，**保留最新实例**
- 旧实例应被淘汰并触发 `OnStateRemoved(RemovalReason=MergedOut)`

这条就是执行文档 `9.2` 里的“合并淘汰 `MergedOut`”。

## 9.6 测试 F：Action 槽位优先级与 Gate

### F-1 PriorityOnly + 抢占

操作：

1. 先施加 `State_Action_Low`。
2. 确认 `StateSlot.Action` 中只有它激活。
3. 再施加 `State_Action_High`。

预期：

- 高优先级状态应接管该槽位
- 低优先级状态按 `PauseLowerPriority` 进入暂停语义
- `HasActiveStateInSlot(StateSlot.Action)` 仍为 `true`

### F-2 Gate 关闭取消路径

操作：

1. 保证 `State_Action_Low` 处于槽位中。
2. 调 `ATcsTestHarness.CloseActionGate()`。

预期：

- `OnSlotGateStateChanged` 被触发
- 由于 `GateCloseBehavior = Cancel`
- Action 槽位中的状态应被取消，或新应用请求失败

然后继续：

3. 在 Gate 关闭状态下再次施加 `State_Action_Low`

预期：

- 触发 `OnStateApplyFailed`
- `FailureReason = SlotGateClosed_CancelPolicy`

这条正是执行文档 `9.2` 中要求验证的“Gate 关闭时的 `Cancel` 路径”。

### F-3 重新打开 Gate

操作：

1. 调 `ATcsTestHarness.OpenActionGate()`。
2. 重新施加 `State_Action_Low`。

预期：

- 状态又可以正常进入并激活

## 9.7 测试 G：查询一致性

这部分要对照执行文档 `9.3` 一条条验证。

建议至少做 4 个时点：

1. 无状态时
2. 刚应用后
3. 合并后
4. 移除后

每个时点都检查：

- `GetStatesInSlot`
- `GetStatesByDefId`
- `GetAllActiveStates`
- `HasStateWithDefId`
- `HasActiveStateInSlot`

推荐记录格式：

```text
[Before Apply]
GetStatesInSlot(Buff)=0
GetStatesByDefId(State_Buff_AttackUp)=0
GetAllActiveStates=0
HasStateWithDefId=false
HasActiveStateInSlot=false

[After Apply]
...
```

预期原则：

- 新增后，这些接口都能看到新增状态
- `MergedOut` 后，淘汰实例不能再被查询到
- 移除后，不应残留幽灵实例

## 9.8 测试 H：条件失败路径

操作：

1. 确保 `Health = 100`。
2. 施加 `State_Buff_LowHealthOnly`。

预期：

- `OnStateApplyFailed`
- `FailureReason = ApplyConditionsFailed`

继续：

3. 把 `Health` 改为 `40`。
4. 再施加一次。

预期：

- 这次应用成功

## 9.9 测试 I：StateTree 完整路径

如果你已经完成 7.2 里的测试 Task，就把 9.3 到 9.8 再跑一遍，但把效果来源切换到 `StateDef.StateTreeRef`。

这轮重点不是重新验证“数值有没有变化”，而是验证：

- 状态实例级 StateTree 能正常启动
- `UTcsStateTreeSchema_StateInstance` 上下文绑定正确
- `Tcs State Change Notify Task` 没有被绕过
- `SourceHandle` 仍然稳定绑定到状态实例

如果你愿意补一条额外日志，建议在 Task 的 `EnterState` 打一条：

```text
[ManualStateTree] Enter State_Buff_AttackUp Owner=ATcsTestEntity SourceHandleId=...
```

这样出问题时定位非常快。

---

## 10. C# 脚本模式下的边界

现在测试逻辑迁到 C# 以后，功能验证能力比以前强了很多，但边界并没有彻底消失。

### 10.1 C# 能稳定验证什么

C# 测试脚本现在可以更稳定地验证：

- 组件 API 的直接调用结果
- 事件是否广播
- 同一帧的调用顺序
- `SourceHandle` 绑定与清理链路
- 查询接口的一致性
- 手测过程中的快照与日志收敛

### 10.2 C# 仍然不能精确证明什么

OpenSpec 里的 `8.3` / `8.4` 本质上验证的是：

- `UTcsStateComponent::FinalizeStateRemoval(...)` 的 native C++ override 是否真的命中
- `UTcsAttributeComponent::ClampAttributeValueInRange(...)` 的 native C++ override 是否真的命中

这件事和“能不能写 C# 测试脚本”不是一回事。

所以现在你可以把结论收敛成：

- 核心功能行为已通过 **C# 脚本驱动的编辑器手测** 验证。
- `8.3` / `8.4` 不在本轮范围内；如果需要严格按字面完成，就补最小 C++ 子类。

### 10.3 如果以后要补 8.3 / 8.4，最小代价是什么

如果你后面愿意接受极少量 C++，那就只补两个极小组件子类，各自打一条日志即可；除此之外，整套测试资产和大部分 C# 手测脚本都可以继续沿用本文方案。

---

## 11. 手工覆盖 S1 / S2 / S3

这三类场景原本更适合自动化测试，但现在也完全可以用“编辑器内 + C# 脚本驱动”的方式做成稳定手测。

## 11.1 S1：过期链路里销毁 Owner

目标：对应执行文档 `11.1`。

### 实现建议

在 `ATcsTestEntity` 的状态移除回调里，加一个可切换测试开关：

- 当收到指定状态 `Expired` 事件时，如果 `bDestroyOwnerOnFirstExpire` 为 `true`，就执行 `DestroyActor()`。

### 操作

1. 给同一个 Target 连续挂 3 个短持续时间状态。
2. 其中第一个状态到期时，在回调里 `DestroyActor()`。

### 预期

- PIE 不崩溃
- 不出现明显野指针访问
- 后续状态不要求继续优雅跑完，因为 Actor 已经被销毁

这里的重点不是“所有剩余状态都继续正常结算”，而是验证迁移文档里提到的：

- Component 自毁后，不应继续在悬空 `this` 上迭代

## 11.2 S2：同一帧多次 Apply

目标：对应执行文档 `11.2`。

### 实现建议

在 `ATcsTestHarness` 里做一个单独入口：

- 一个函数里连续调用 3 次 `TryApplyState`

例如同一帧内依次施加：

1. `State_Buff_AttackUp`
2. `State_Debuff_Slow`
3. `State_Action_Low`

### 预期

- 不会出现死循环
- `PendingSlotActivationUpdates` 相关行为从结果上看是收敛的
- 最终状态与槽位结果正确
- 不会看到重复乱序的激活 / 停用风暴

如果你想把这条测得更狠一点，可以在一个调用点内对同一 `Action` 槽位连续施加高低优先级状态。

## 11.3 S3：StateTree Tick 中调用 `RemoveAllStates`

目标：对应执行文档 `11.3`。

### 实现建议

做一个专门的测试用 StateTree Task：

- 在 `Tick` 中第一次执行时，调用 `OwnerStateCmp.RemoveAllStates()`

这里仍然建议保持务实：

- 默认可以先做一个极小的蓝图 Task。
- 如果你已经验证相关 StateTree 基类在 C# Glue 中稳定可用，再把这个 Task 改成 C#。

### 操作

1. 给 Target 应用带这个 Task 的状态。
2. 状态进入 Tick 后，在 Task 内触发 `RemoveAllStates()`。

### 预期

- 不崩溃
- 不出现明显重复移除
- `NotifyStateRemoved` 顺序是稳定可重复的

这里的重点不是模拟引擎内部全部极端时序，而是确认当前“引擎延迟 Stop + TCS 立即执行 Step 2 到 Step 8”的组合，不会在你的实际接入路径里直接炸掉。

---

## 12. 组件级 StateTree Gate 测试（可选增强）

如果你想把 `StateSlotDefinitionAsset.StateTreeStateName` 这条链路也手测掉，可以额外补一个 **组件级** StateTree。

注意，这和 `StateDefinitionAsset.StateTreeRef` 是两层不同的东西：

- `StateComponent` 自身的 StateTree：主要用于驱动槽位 Gate
- `StateDefinitionAsset.StateTreeRef`：状态实例自己的运行时逻辑

### 12.1 推荐做法

给 `ATcsTestEntity` 的 `StateComponent` 指一个简单的组件级 StateTree，例如：

```text
Root
├─ Idle
├─ BuffGateOpen
└─ ActionGateOpen
```

在 `BuffGateOpen` 和 `ActionGateOpen` 节点里放 `Tcs State Change Notify Task`。

然后把：

- `DA_StateSlot_Buff.StateTreeStateName = BuffGateOpen`
- `DA_StateSlot_Action.StateTreeStateName = ActionGateOpen`

这样你在 StateTree Debugger 切换组件级状态时，就能观察：

- `RefreshSlotsForStateChange(...)`
- `SetSlotGateOpen(...)`
- `OnSlotGateStateChanged`

这条链路是否正常。

### 12.2 为什么这是可选项

因为在当前阶段，C# 测试脚本直接调 `SetSlotGateOpen(...)` 已经足够覆盖大多数业务行为。组件级 Gate 驱动更像是附加验证，而不是第一优先级。

---

## 13. 建议的最终验收清单

当你完成这套手工测试后，建议按下面清单勾一次：

- [ ] `ManagedTireflyGameplayUtils.csproj` 能稳定编译并热重载
- [ ] 属性初始化、BaseValue、CurrentValue、Reset、Remove 全部正常
- [ ] 动态 Clamp 正常，`Health <= MaxHealth`
- [ ] `OnAttributeReachedBoundary` 在最小值场景能触发
- [ ] 直接 Modifier 应用 / 移除正常
- [ ] `GetModifiersBySourceHandle` 结果正确
- [ ] `State_Buff_AttackUp` 的应用、过期、移除正常
- [ ] `State_Buff_MaxHealthAura` 过期后能把 `Health` 正确压回新上限
- [ ] `State_Buff_UniqueShield` 第二次施加能触发 `MergedOut`
- [ ] `State_Action_High` 能正确抢占 `State_Action_Low`
- [ ] `StateSlot.Action` Gate 关闭时，`Cancel` 路径正常
- [ ] `GetStatesInSlot` / `GetStatesByDefId` / `GetAllActiveStates` / `HasStateWithDefId` / `HasActiveStateInSlot` 在新增、合并、移除后三个时点都正确
- [ ] `State_Buff_LowHealthOnly` 的条件失败路径正常
- [ ] 如果启用了状态实例 StateTree，`Schema + Notify Task + 效果 Task` 整体正常
- [ ] S1 / S2 / S3 手工场景至少各跑过一轮，无崩溃、无明显时序错乱

如果你坚持“严格 native override 验证闭环”，那么还要额外补：

- [ ] `8.3` / `8.4` 的最小 C++ 子类验证

---

## 14. 我对实际执行的建议

如果你想降低一次性工作量，建议按下面顺序实施：

1. 先做 `ATcsTestEntity`、`ATcsTestHarness`，以及 4 个 Attribute、5 个 Modifier、6 个 State、3 个 Slot。
2. 先走本文 7.1 的 C# 烟雾路径，把 9.1 到 9.8 跑通。
3. 再补一个极小的 StateTree 效果 Task，把 9.9 跑通。
4. 如果你接受最小 C++，最后再补两个很小的组件子类，专门完成 `8.3` / `8.4`；如果不接受，就把本轮目标停在 C# 脚本驱动的功能验证闭环。

这么做的原因很简单：

- 前 80% 的迁移验证，其实不需要先把 StateTree 测试基建搭到很满。
- 最容易出问题的链路，是 `SourceHandle` 清理、槽位优先级 / Gate、查询一致性，以及状态实例 StateTree 绑定。
- 这些都可以先由 C# 测试脚本独立验证；只有虚函数覆写命中这件事，天然不是 C# 手测的主要目标。

如果你后面决定把这份手测方案进一步固化成自动化测试，我建议优先把本文里的 `11.1` / `11.2` / `11.3` 先翻译成 Spec；`8.3` / `8.4` 则继续保留为最小 C++ 扩展点验证。