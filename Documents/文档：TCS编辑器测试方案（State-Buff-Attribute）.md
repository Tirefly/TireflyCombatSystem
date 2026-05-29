# TCS 编辑器测试教程（State / Buff / Attribute）

## 1. 使用范围

这份教程用于在当前项目中搭建一套可在编辑器内执行的 TCS 测试环境，覆盖：

1. `State` 共享主链。
2. `Buff` 生命周期、叠层、周期、合并。
3. `Attribute` 属性、Modifier、Clamp、Boundary、条件联动。

本教程以现有编辑器资产入口为基础，并要求你自行补齐 `C#` 测试驱动，不引入新的 `C++` 测试代码。

### 1.1 当前阶段说明

1. 这份文档是“测试环境搭建教程”，不是仓库当前已经内置完成的现成测试套件。
2. 当前仓库已经提供 `Definition Asset` 与 `Gameplay Runtime` 的编辑器 authoring 入口，但并没有现成的 `TestTCS/**` C# 测试类、导演蓝图或三张测试图。
3. 当前 `Gameplay Runtime` 的运行时树入口已经收敛为 `State Component StateTree` 与 `Buff StateTree`；generic `State Instance StateTree` 和 `UTcsStateTreeSchema_StateInstance` 不再是当前契约。
4. 如果要从 C# 跑 `State` 用例，当前应优先使用 `UTcsStateManagerSubsystem.TryApplyStateToTarget()` 或你自己额外封装的可反射桥接入口，而不是直接依赖 `UTcsStateComponent.TryApplyState()`。

## 2. 前置准备

### 2.1 编译与菜单检查

1. 先编译 `TireflyGameplayUtilsEditor Win64 Development`。
2. 打开编辑器后，确认 Content Browser 中能看到以下菜单：
	- `Tirefly Combat System -> Definition Asset -> Attribute Definition`
	- `Tirefly Combat System -> Definition Asset -> Attribute Modifier Definition`
	- `Tirefly Combat System -> Definition Asset -> Buff Definition`
	- `Tirefly Combat System -> Definition Asset -> State Slot Definition`
	- `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree`
	- `Tirefly Combat System -> Gameplay Runtime -> Buff StateTree`

### 2.2 测试目录

在项目内容目录下创建以下结构：

`/Game/Dev/TCS_TestSuite/`

建议子目录：

1. `Maps/`
2. `Definitions/StateSlots/`
3. `Definitions/Attributes/`
4. `Definitions/AttributeModifiers/`
5. `Definitions/StateLikeBuffs/`
6. `Definitions/Buffs/`
7. `StateTrees/Component/`
8. `StateTrees/Buff/`
9. `Blueprints/Actors/`

### 2.3 C# 目录

在 `Script/ManagedTireflyGameplayUtils/` 下创建：

`TestTCS/`

建议子目录：

1. `Core/`
2. `Fixtures/`
3. `Probes/`
4. `Directors/`
5. `Services/`

当前 `ManagedTireflyGameplayUtils.csproj` 使用 SDK 默认包含规则，因此只要把 `.cs` 文件放进该目录树，不需要额外修改 `.csproj`。

当前仓库里的 `ManagedTireflyGameplayUtils` 还只有模块引导文件，`TestTCS/**` 目录树和下面第 6 节提到的类型都需要你自行创建。

### 2.4 Gameplay Tags

在 `Project Settings -> Gameplay Tags` 中创建以下标签：

1. `StateSlotTag.Test.Action`
2. `StateSlotTag.Test.Overlay`
3. `StateSlotTag.Test.Control`
4. `StateSlotTag.Test.Buff`
5. `TCS.Attribute.Test.Health`
6. `TCS.Attribute.Test.MaxHealth`
7. `TCS.Attribute.Test.Shield`
8. `TCS.Attribute.Test.AttackPower`
9. `TCS.Attribute.Test.MoveSpeed`
10. `Event.Test.Slot.Idle`
11. `Event.Test.Slot.Action`
12. `Event.Test.Slot.Overlay`
13. `Event.Test.Slot.Control`
14. `Event.Test.Slot.Buff`
15. `Event.Buff.PeriodTick`

### 2.5 Asset Manager 扫描路径

在 `Project Settings -> Asset Manager` 中确认以下 `Primary Asset Type` 已存在，并把测试目录加入扫描路径：

| Primary Asset Type | 扫描目录 |
| --- | --- |
| `TcsAttributeDef` | `/Game/Dev/TCS_TestSuite/Definitions/Attributes` |
| `TcsAttributeModifierDef` | `/Game/Dev/TCS_TestSuite/Definitions/AttributeModifiers` |
| `TcsStateSlotDef` | `/Game/Dev/TCS_TestSuite/Definitions/StateSlots` |
| `TcsStateDef` | `/Game/Dev/TCS_TestSuite/Definitions/StateLikeBuffs`、`/Game/Dev/TCS_TestSuite/Definitions/Buffs` |

在 `Project Settings -> Game -> Tirefly Combat System` 中，把 `StateLoadingStrategy` 临时设为 `PreloadAll`，用于测试阶段减少首次加载干扰。

### 2.6 AssetManagerSettings 勘误与忽略列表

当前 TCS 会在编辑器阶段检查以下四类 DefinitionAsset 的 `PrimaryAssetTypesToScan` 覆盖是否完整：

1. `UTcsAttributeDefinition`
2. `UTcsAttributeModifierDefinition`
3. `UTcsStateDefinition`
4. `UTcsStateSlotDefinition`

检查内容包括：

1. 对应 `PrimaryAssetType` 是否存在。
2. `AssetBaseClass` 是否仍然指向正确的 DefAsset 类型。
3. 现有 DefinitionAsset 所在目录是否真的被扫描路径或 `SpecificAssets` 覆盖。

如果存在未修复的漏配，编辑器日志会在 `AssetManager` 设置变更后的下一次刷新时输出明确勘误，并附带需要补齐的 `PrimaryAssetType` 或扫描目录。只要问题还存在，之后每次成功执行一次 Save，日志都会再提示一次；这是刻意保留的强提醒，不会自动改写项目配置。

如果某一类 DefAsset 在当前项目里确实暂时不打算接入 AssetManager，可在 `Project Settings -> Game -> Tirefly Combat System` 中把该类型加入 `Ignored Definition Asset Types`。被加入忽略列表的类型不再参与这套勘误报错；移出忽略列表后，检查会立即恢复。

## 3. 创建测试资产的顺序

按下面顺序创建：

1. `StateSlotDefinition`
2. `AttributeDefinition`
3. `AttributeModifierDefinition`
4. 组件级 `StateTree`
5. `Buff` 运行时 `StateTree`
6. `State` 共享主链测试资产（使用最小 `BuffDefinition` 作为具体状态载体）
7. `Buff` 专属测试资产
8. `C#` 测试驱动类
9. 蓝图子类与三张测试图

## 4. 创建 Definition 资产

### 4.1 StateSlotDefinition

在 `Definitions/StateSlots/` 下依次创建 4 个 `State Slot Definition` 资产：

| 资产名 | StateSlotDefId | SlotTag | StateTreeStateName | ActivationMode | GateCloseBehavior | PreemptionPolicy | SamePriorityPolicy |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `DA_StateSlot_Test_Action` | `Test_Action` | `StateSlotTag.Test.Action` | `Slot_ActionOpen` | `Priority Only` | `Pause States` | `Pause Lower Priority` | `UTcsStateSamePriorityPolicy_UseNewest` |
| `DA_StateSlot_Test_Overlay` | `Test_Overlay` | `StateSlotTag.Test.Overlay` | `Slot_OverlayOpen` | `All Active` | `Pause States` | `Pause Lower Priority` | 留空 |
| `DA_StateSlot_Test_Control` | `Test_Control` | `StateSlotTag.Test.Control` | `Slot_ControlOpen` | `Priority Only` | `Cancel States` | `Cancel Lower Priority` | `UTcsStateSamePriorityPolicy_UseNewest` |
| `DA_StateSlot_Test_Buff` | `Test_Buff` | `StateSlotTag.Test.Buff` | `Slot_BuffOpen` | `All Active` | `Pause States` | `Pause Lower Priority` | 留空 |

### 4.2 AttributeDefinition

在 `Definitions/Attributes/` 下创建以下资产。所有属性统一把 `ClampStrategyClass` 设为 `UTcsAttrClampStrategy_Linear`。

| 资产名 | AttributeDefId | AttributeTag | Min 设置 | Max 设置 | 用途 |
| --- | --- | --- | --- | --- | --- |
| `DA_Attr_Test_MaxHealth` | `MaxHealth` | `TCS.Attribute.Test.MaxHealth` | `Static / 0` | `None` | 作为 `Health` 的动态上限 |
| `DA_Attr_Test_Health` | `Health` | `TCS.Attribute.Test.Health` | `Static / 0` | `Dynamic / MaxHealth` | 生命值与条件测试 |
| `DA_Attr_Test_Shield` | `Shield` | `TCS.Attribute.Test.Shield` | `Static / 0` | `None` | 第二属性与边界事件 |
| `DA_Attr_Test_AttackPower` | `AttackPower` | `TCS.Attribute.Test.AttackPower` | `Static / 0` | `None` | 条件门控与数值修改 |
| `DA_Attr_Test_MoveSpeed` | `MoveSpeed` | `TCS.Attribute.Test.MoveSpeed` | `Static / 0` | `None` | 百分比与乘法 Modifier |

### 4.3 AttributeModifierDefinition

在 `Definitions/AttributeModifiers/` 下创建以下资产。内置执行器统一使用 `Operands` 中的 `Magnitude` 作为操作数字段。

| 资产名 | AttributeName | ModifierMode | ModifierType | MergerType | Operands |
| --- | --- | --- | --- | --- | --- |
| `DA_AttrMod_Test_Health_Add25` | `Health` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_NoMerge` | `Magnitude = 25` |
| `DA_AttrMod_Test_MaxHealth_Add50` | `MaxHealth` | `BaseValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_NoMerge` | `Magnitude = 50` |
| `DA_AttrMod_Test_AttackPower_Add10` | `AttackPower` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_NoMerge` | `Magnitude = 10` |
| `DA_AttrMod_Test_MoveSpeed_Pct20` | `MoveSpeed` | `CurrentValue` | `UTcsAttrModExec_MultiplyAdditive` | `UTcsAttrModMerger_UseAdditiveSum` | `Magnitude = 0.2` |
| `DA_AttrMod_Test_MoveSpeed_Mul120` | `MoveSpeed` | `CurrentValue` | `UTcsAttrModExec_MultiplyContinued` | `UTcsAttrModMerger_NoMerge` | `Magnitude = 1.2` |
| `DA_AttrMod_Test_Health_UseNewest` | `Health` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_UseNewest` | `Magnitude = 10` |
| `DA_AttrMod_Test_Health_UseOldest` | `Health` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_UseOldest` | `Magnitude = 15` |
| `DA_AttrMod_Test_Shield_UseMinimum` | `Shield` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_UseMinimum` | `Magnitude = 10` |
| `DA_AttrMod_Test_Shield_UseMaximum` | `Shield` | `CurrentValue` | `UTcsAttrModExec_Addition` | `UTcsAttrModMerger_UseMaximum` | `Magnitude = 30` |

### 4.4 State 模块共享主链测试资产

当前仓库没有单独的 canonical `UTcsStateDefinition` 创建工厂，因此 `State` 模块的共享主链测试资产第一版统一使用 `UTcsBuffDefinition` 作为具体 `StateDefinition` 载体。

这些资产都放在 `Definitions/StateLikeBuffs/` 下，并统一采用以下公共配置：

1. `DurationType = None`
2. `Period = 0`
3. `MaxStackCount = 1`
4. `MergerType = UTcsBuffMerger_NoMerge`
5. `StateTreeRef = ST_TCS_Buff_Minimal`

然后分别创建：

| 资产名 | StateDefId | StateTag | StateSlotType | Priority | 额外配置 |
| --- | --- | --- | --- | --- | --- |
| `DA_Buff_Test_State_ActionLow` | `Buff_Test_State_ActionLow` | `StateTag.Test.ActionLow` | `StateSlotTag.Test.Action` | `10` | 无 |
| `DA_Buff_Test_State_ActionHigh` | `Buff_Test_State_ActionHigh` | `StateTag.Test.ActionHigh` | `StateSlotTag.Test.Action` | `20` | 无 |
| `DA_Buff_Test_State_OverlayA` | `Buff_Test_State_OverlayA` | `StateTag.Test.OverlayA` | `StateSlotTag.Test.Overlay` | `5` | 无 |
| `DA_Buff_Test_State_OverlayB` | `Buff_Test_State_OverlayB` | `StateTag.Test.OverlayB` | `StateSlotTag.Test.Overlay` | `5` | 无 |
| `DA_Buff_Test_State_Control` | `Buff_Test_State_Control` | `StateTag.Test.Control` | `StateSlotTag.Test.Control` | `10` | 无 |
| `DA_Buff_Test_State_AttrGate` | `Buff_Test_State_AttrGate` | `StateTag.Test.AttrGate` | `StateSlotTag.Test.Action` | `15` | 添加 `UTcsStateCondition_AttributeComparison`，`AttributeTag = TCS.Attribute.Test.AttackPower`，`CheckTarget = Owner`，`ComparisonType = 大于等于（Greater Than Or Equal）`，`CompareValue = 10` |

这些资产在 `State` 测试图中统一通过 `UTcsStateManagerSubsystem.TryApplyStateToTarget()`，或你自己额外封装的可反射桥接入口调用，不走 `ApplyBuff()`。当前不应把 C# 直连 `UTcsStateComponent.TryApplyState()` 当作前提，因为它不是稳定的反射入口。

### 4.5 Buff 专属测试资产

在 `Definitions/Buffs/` 下创建以下 Buff 资产：

| 资产名 | StateDefId | Slot | DurationType / Duration | Period | MaxStackCount | MergerType | 额外配置 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `DA_Buff_Test_Finite` | `Buff_Test_Finite` | `StateSlotTag.Test.Buff` | `Duration / 5` | `0` | `1` | `UTcsBuffMerger_NoMerge` | `StateTreeRef = ST_TCS_Buff_Minimal` |
| `DA_Buff_Test_Infinite` | `Buff_Test_Infinite` | `StateSlotTag.Test.Buff` | `Infinite` | `0` | `1` | `UTcsBuffMerger_NoMerge` | `StateTreeRef = ST_TCS_Buff_Minimal` |
| `DA_Buff_Test_Periodic` | `Buff_Test_Periodic` | `StateSlotTag.Test.Buff` | `Duration / 6` | `1` | `1` | `UTcsBuffMerger_NoMerge` | `StateTreeRef = ST_TCS_Buff_PeriodObserve` |
| `DA_Buff_Test_StackRefresh` | `Buff_Test_StackRefresh` | `StateSlotTag.Test.Buff` | `Duration / 5` | `0` | `3` | `UTcsBuffMerger_StackByInstigator` | `OnStackIncrease.DurationPolicy = RefreshRemainingToTotal` |
| `DA_Buff_Test_StackExpireOne` | `Buff_Test_StackExpireOne` | `StateSlotTag.Test.Buff` | `Duration / 3` | `0` | `3` | `UTcsBuffMerger_StackByInstigator` | `OnDurationExpired.ExpirationPolicy = RemoveSingleStack` |
| `DA_Buff_Test_StackExpireOneRefresh` | `Buff_Test_StackExpireOneRefresh` | `StateSlotTag.Test.Buff` | `Duration / 3` | `0` | `3` | `UTcsBuffMerger_StackByInstigator` | `OnDurationExpired.ExpirationPolicy = RemoveSingleStackAndRefreshDuration` |
| `DA_Buff_Test_MergeOldest` | `Buff_Test_MergeOldest` | `StateSlotTag.Test.Buff` | `Duration / 5` | `0` | `1` | `UTcsBuffMerger_UseOldest` | `StateTreeRef = ST_TCS_Buff_Minimal` |
| `DA_Buff_Test_MergeNewest` | `Buff_Test_MergeNewest` | `StateSlotTag.Test.Buff` | `Duration / 5` | `0` | `1` | `UTcsBuffMerger_UseNewest` | `StateTreeRef = ST_TCS_Buff_Minimal` |
| `DA_Buff_Test_StackByInstigator` | `Buff_Test_StackByInstigator` | `StateSlotTag.Test.Buff` | `Duration / 5` | `0` | `3` | `UTcsBuffMerger_StackByInstigator` | `StateTreeRef = ST_TCS_Buff_Minimal` |

## 5. 创建 StateTree 资产

### 5.1 组件级槽位编排 StateTree

在 `StateTrees/Component/` 下通过菜单 `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 创建：

`ST_TCS_StateSlots_Orchestration`

这个入口会自动使用 `UTcsStateSchema_StateComponent`。

树内容按下面配置：

1. 新增全局 `Evaluator`：`FTcsStateSlotDebugEvaluator`
2. `UpdateInterval = 0.1`
3. `SlotFilter` 默认留空
4. 根下创建 5 个状态：
	- `Idle`
	- `Slot_ActionOpen`
	- `Slot_OverlayOpen`
	- `Slot_ControlOpen`
	- `Slot_BuffOpen`
5. 在 4 个 `Slot_*` 状态上各挂一个 `FTcsStateChangeNotifyTask`
6. 添加事件切换：
	- `Event.Test.Slot.Idle -> Idle`
	- `Event.Test.Slot.Action -> Slot_ActionOpen`
	- `Event.Test.Slot.Overlay -> Slot_OverlayOpen`
	- `Event.Test.Slot.Control -> Slot_ControlOpen`
	- `Event.Test.Slot.Buff -> Slot_BuffOpen`

这个组件级 `StateTree` 用来驱动 `StateSlots` 的 Gate 与可视化，不承担实例逻辑。

### 5.2 最小 Buff Runtime StateTree

在 `StateTrees/Buff/` 下通过菜单 `Tirefly Combat System -> Gameplay Runtime -> Buff StateTree` 创建：

`ST_TCS_Buff_Minimal`

配置：

1. 自动使用 `UTcsStateSchema_Buff`
2. 根下创建一个状态：`Observe`
3. 在 `Observe` 状态上挂 `FTcsStateChangeNotifyTask`

这个树用于普通 Buff 实例，以及第 4.4 节里那批“以 `UTcsBuffDefinition` 作为具体状态载体”的 `State` 共享主链测试资产的最小观测。

### 5.3 Buff 周期实例 StateTree

在 `StateTrees/Buff/` 下创建：

`ST_TCS_Buff_PeriodObserve`

配置：

1. 自动使用 `UTcsStateSchema_Buff`
2. 根下创建状态：`PeriodLoop`
3. 在 `PeriodLoop` 状态上挂：
	- `FTcsStateChangeNotifyTask`
	- `FTcsBuffPeriodDriverTask`
4. 新增状态：`PeriodObserved`
5. 在 `PeriodObserved` 状态上挂 `FTcsStateChangeNotifyTask`
6. 添加切换：
	- `Event.Buff.PeriodTick -> PeriodObserved`
	- `PeriodObserved -> PeriodLoop`

### 5.4 Attribute 条件联动实例 StateTree

Attribute 条件联动场景直接复用 `ST_TCS_Buff_Minimal`，不再额外创建新树。

## 6. 在 C# 方案中创建测试驱动

下面这些类和按钮当前仓库还没有提供；本节是在现有 UnrealSharp 能力下，你需要补齐的测试基建清单。只有完成本节和第 7 节后，第 8-9 节里的导演蓝图、CallInEditor 按钮和测试图流程才会真正存在。

### 6.1 Core

在 `TestTCS/Core/` 下创建以下文件：

1. `ETestTcsModule.cs`：模块枚举，区分 `State`、`Buff`、`Attribute`
2. `ETestTcsStateCase.cs`：State 图用例枚举
3. `ETestTcsBuffCase.cs`：Buff 图用例枚举
4. `ETestTcsAttributeCase.cs`：Attribute 图用例枚举
5. `FTestTcsEventRecord.cs`：记录事件名、时间戳、来源对象、附加文本
6. `FTestTcsSnapshotRecord.cs`：记录槽位快照、Buff 调试快照、属性值快照
7. `FTestTcsRunResult.cs`：记录当前 Case 是否通过、失败信息、证据摘要
8. `UTestTcsAssetCatalog.cs`：集中管理所有 DefId、GameplayTag、StateTree 名称、组件树事件名
9. `UTestTcsAssertLibrary.cs`：统一的断言与结果写回入口

### 6.2 Fixtures

在 `TestTCS/Fixtures/` 下创建：

1. `ATestTcsFixtureActor.cs`
	- 基类为 `AActor`
	- 默认组件包含：`UTcsStateComponent`、`UTcsBuffComponent`、`UTcsAttributeComponent`
	- 负责作为三张测试图的被测目标
2. `ATestTcsInstigatorActor.cs`
	- 作为状态或 Buff 的施加者
	- Buff 图至少放置两个实例，用于 `StackByInstigator` 与 merge 场景

### 6.3 Probes

在 `TestTCS/Probes/` 下创建：

1. `UTestTcsStateProbeComponent.cs`
	- 订阅 `OnStateApplySuccess`
	- 订阅 `OnStateApplyFailed`
	- 订阅 `OnStateRemoved`
	- 订阅 `OnStateStageChanged`
	- 订阅 `OnSlotGateStateChanged`
	- 记录 `GetSlotDebugSnapshot()` 与 `GetStateDebugSnapshot()`
2. `UTestTcsBuffProbeComponent.cs`
	- 订阅 `OnBuffRuntimeDelta`
	- 校验单个 payload 内的 `bStackCountChanged` / `bMaxStackCountChanged` / `bPeriodChanged` / `bDurationRefreshed` 以及对应最终值
	- 订阅 `OnBuffRemoved`
	- 记录 `GetBuffMergeDebugLines()`
3. `UTestTcsAttributeProbeComponent.cs`
	- 订阅 `OnAttributeValueChanged`
	- 订阅 `OnAttributeBaseValueChanged`
	- 订阅 `OnAttributeModifiersAdded`
	- 订阅 `OnAttributeModifiersRemoved`
	- 订阅 `OnAttributeModifiersUpdated`
	- 订阅 `OnAttributesReachedBoundary`
	- 记录属性当前值、基础值、Modifier 列表

### 6.4 Services

在 `TestTCS/Services/` 下创建：

1. `UTestTcsComponentTreeDriver.cs`
	- 负责驱动组件级槽位编排 `StateTree`
	- 提供切换到 `Idle / Action / Overlay / Control / Buff` 的统一入口
	- 内部使用 `UTestTcsAssetCatalog` 中定义的事件名
2. `UTestTcsEvidenceExporter.cs`
	- 把 Probe 收集到的事件和快照整理成可读文本

### 6.5 Directors

在 `TestTCS/Directors/` 下创建：

1. `ATestTcsDirectorBase.cs`
	- 持有 Fixture、Instigator、Probe 和资产引用
	- 需要自行实现 `[UFunction(CallInEditor = true)]` 按钮：`Reset`、`RunSelectedCase`、`RunAllCases`、`DumpEvidence`
2. `ATestTcsStateDirector.cs`
	- 使用 `UTcsStateManagerSubsystem.TryApplyStateToTarget()`，或你自己补的可反射桥接入口，跑 State 图用例
	- 负责驱动组件级槽位编排 `StateTree`
	- 负责 Priority、Gate、Remove、Condition 用例
3. `ATestTcsBuffDirector.cs`
	- 使用 `UTcsBuffComponent.ApplyBuff()` 跑 Buff 图用例
	- 负责 Duration、Stack、Period、Merge、MergedOut 用例
4. `ATestTcsAttributeDirector.cs`
	- 负责 Add/Set/Reset/Remove Attribute
	- 负责 Create/Apply/HandleModifierUpdated/Remove Modifier
	- 负责 Clamp、Boundary、State 条件联动用例

## 7. 创建蓝图子类

在 `Blueprints/Actors/` 下创建以下蓝图子类：

1. `BP_TCS_TestFixture`，父类 `ATestTcsFixtureActor`
2. `BP_TCS_TestInstigator`，父类 `ATestTcsInstigatorActor`
3. `BP_TCS_StateDirector`，父类 `ATestTcsStateDirector`
4. `BP_TCS_BuffDirector`，父类 `ATestTcsBuffDirector`
5. `BP_TCS_AttributeDirector`，父类 `ATestTcsAttributeDirector`

在 `BP_TCS_TestFixture` 上完成以下配置：

1. `UTcsStateComponent` 挂载 `ST_TCS_StateSlots_Orchestration`
2. `UTcsBuffComponent` 保持默认
3. `UTcsAttributeComponent` 保持默认
4. 挂上三种 Probe 组件，或由基类在构造时默认创建

## 8. 创建三张测试图

在 `Maps/` 下创建：

1. `L_TCS_Test_State`
2. `L_TCS_Test_Buff`
3. `L_TCS_Test_Attribute`

### 8.1 State 图摆放

放置：

1. `BP_TCS_TestFixture` 1 个
2. `BP_TCS_TestInstigator` 1 个
3. `BP_TCS_StateDirector` 1 个

在导演上配置引用：

1. `FixtureActor`
2. `PrimaryInstigator`
3. `StateLikeBuffs` 资产数组：
	- `DA_Buff_Test_State_ActionLow`
	- `DA_Buff_Test_State_ActionHigh`
	- `DA_Buff_Test_State_OverlayA`
	- `DA_Buff_Test_State_OverlayB`
	- `DA_Buff_Test_State_Control`
	- `DA_Buff_Test_State_AttrGate`

### 8.2 Buff 图摆放

放置：

1. `BP_TCS_TestFixture` 1 个
2. `BP_TCS_TestInstigator` 2 个
3. `BP_TCS_BuffDirector` 1 个

在导演上配置引用：

1. `FixtureActor`
2. `PrimaryInstigator`
3. `SecondaryInstigator`
4. Buff 资产数组：
	- `DA_Buff_Test_Finite`
	- `DA_Buff_Test_Infinite`
	- `DA_Buff_Test_Periodic`
	- `DA_Buff_Test_StackRefresh`
	- `DA_Buff_Test_StackExpireOne`
	- `DA_Buff_Test_StackExpireOneRefresh`
	- `DA_Buff_Test_MergeOldest`
	- `DA_Buff_Test_MergeNewest`
	- `DA_Buff_Test_StackByInstigator`

### 8.3 Attribute 图摆放

放置：

1. `BP_TCS_TestFixture` 1 个
2. `BP_TCS_TestInstigator` 1 个
3. `BP_TCS_AttributeDirector` 1 个

在导演上配置引用：

1. `FixtureActor`
2. `PrimaryInstigator`
3. Attribute 资产数组：
	- `DA_Attr_Test_MaxHealth`
	- `DA_Attr_Test_Health`
	- `DA_Attr_Test_Shield`
	- `DA_Attr_Test_AttackPower`
	- `DA_Attr_Test_MoveSpeed`
4. Modifier 资产数组：
	- 第 4.3 节创建的全部 AttributeModifierDefinition
5. 条件联动状态资产：`DA_Buff_Test_State_AttrGate`

## 9. 执行步骤

以下步骤默认你已经完成第 6-8 节里的 C# 基建、蓝图子类和测试图摆放。当前仓库并不自带这些测试导演与地图资产；如果你还没先把它们搭起来，就不能直接从这一节开始执行。

### 9.1 首次检查

1. 打开任意一张测试图。
2. 选中 `BP_TCS_TestFixture`。
3. 确认 `UTcsStateComponent` 已绑定 `ST_TCS_StateSlots_Orchestration`。
4. 确认导演上的资产引用都已指向正确测试资产。
5. 打开 `StateTree Debugger`。

### 9.2 State 图

在 `BP_TCS_StateDirector` 上依次执行：

1. `Reset`
2. `RunSelectedCase` 或 `RunAllCases`
3. Priority 用例时切到 `Event.Test.Slot.Action`
4. Overlay 用例时切到 `Event.Test.Slot.Overlay`
5. Control 用例时切到 `Event.Test.Slot.Control`
6. 观察：
	- `GetSlotDebugSnapshot()`
	- `GetStateDebugSnapshot()`
	- `StateTree Debugger` 中 `FTcsStateSlotDebugEvaluator.Snapshot`

### 9.3 Buff 图

在 `BP_TCS_BuffDirector` 上依次执行：

1. `Reset`
2. `RunDurationSuite`
3. `RunStackSuite`
4. `RunPeriodSuite`
5. `RunMergeSuite`
6. 观察：
	- Buff 事件记录
	- `GetBuffMergeDebugLines()`
	- Buff 实例 `StateTree`
	- `Event.Buff.PeriodTick` 是否被记录

### 9.4 Attribute 图

在 `BP_TCS_AttributeDirector` 上依次执行：

1. `Reset`
2. `RunAttributeCrudSuite`
3. `RunModifierSuite`
4. `RunClampSuite`
5. `RunConditionSuite`
6. 观察：
	- 当前属性值与基础值快照
	- Modifier 事件记录
	- Clamp 与 Boundary 事件
	- `DA_Buff_Test_State_AttrGate` 是否随 `AttackPower` 变化正确通过或失败

## 10. 结果记录要求

每次执行至少保留以下信息：

1. 当前模块和当前用例名
2. 测试结果：`Pass / Fail`
3. 关键事件顺序
4. 槽位快照、Buff 快照或 Attribute 快照
5. 当前用到的 Definition 名称和 `StateTree` 名称
6. 一条最短复现说明