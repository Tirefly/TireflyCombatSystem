# TCS 手工测试方案（Attribute / State / StateSlot / StateComponent / Buff）

## 1. 目标

这份文档只服务一件事：基于当前仓库里已经存在的 TestTCS 骨架，按步骤完成对以下成熟模块的手工验证。

1. `StateSlot`：槽位配置、激活模式、Gate 关闭策略、抢占策略。
2. `StateComponent`：应用状态、切换 Gate、按 DefId 清理、按槽位清理、全量清理。
3. `State`：优先级竞争、同优先级仲裁、AllActive 并存、条件失败、移除事件。
4. `Attribute`：创建、写值、边界 Clamp、Modifier 应用 / 更新 / 移除、Reset、Remove。
5. `Buff`：最小运行时 Buff 应用链路。

## 2. 一次性准备

### 2.1 编译

1. 先编译 `TireflyGameplayUtilsEditor Win64 Development`。
2. 如果编辑器正在开着并启用了 Live Coding，先关闭编辑器，或者先按 `Ctrl+Alt+F11` 停止 Live Coding，再跑 UBT。
3. 托管侧至少确认 `Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj` 可以通过 `dotnet build`。

### 2.2 当前仓库里已经有的基础

当前仓库已经有下面这些内容，不要重复造轮子：

1. `Script/ManagedTireflyGameplayUtils/TestTCS/`：`Core`、`Fixtures`、`Probes`、`Directors` 骨架已经存在。
2. `Content/TCS_Test/Definition/StateSlot/`：
   - `DA_StateSlot_Test_Action`
   - `DA_StateSlot_Test_Overlay`
   - `DA_StateSlot_Test_Control`
   - `DA_StateSlot_Test_Buff`
3. `Content/TCS_Test/Definition/Attribute/`：
   - `DA_Attr_Test_MaxHealth`
   - `DA_Attr_Test_Health`
   - `DA_Attr_Test_Shield`
   - `DA_Attr_Test_AttackPower`
   - `DA_Attr_Test_MoveSpeed`
4. `Content/TCS_Test/Definition/AttributeModifier/`：
   - `DA_AttrMod_Test_Health_Add25`
   - `DA_AttrMod_Test_MaxHealth_Add50`
   - `DA_AttrMod_Test_AttackPower_Add10`
   - `DA_AttrMod_Test_MoveSpeed_Pct20`
   - `DA_AttrMod_Test_MoveSpeed_Mul120`
   - `DA_AttrMod_Test_Health_UseNewest`
   - `DA_AttrMod_Test_Health_UseOldest`
   - `DA_AttrMod_Test_Shield_UseMinimum`
   - `DA_AttrMod_Test_Shield_UseMaximum`
5. `Config/DefaultGameplayTags.ini` 已包含：
   - `StateSlotTag.Test.Action`
   - `StateSlotTag.Test.Overlay`
   - `StateSlotTag.Test.Control`
   - `StateSlotTag.Test.Buff`
   - `TCS.Attribute.Test.Health`
   - `TCS.Attribute.Test.MaxHealth`
   - `TCS.Attribute.Test.Shield`
   - `TCS.Attribute.Test.AttackPower`
   - `TCS.Attribute.Test.MoveSpeed`
   - `StateTag.Test.ActionLow`
   - `StateTag.Test.ActionHigh`
   - `StateTag.Test.OverlayA`
   - `StateTag.Test.OverlayB`
   - `StateTag.Test.Control`
   - `StateTag.Test.AttrGate`
6. `Config/DefaultGame.ini` 已包含 AssetManager 对 `Attribute`、`AttributeModifier`、`StateSlot`、`StateDef(Buff/Skill)` 的扫描路径。

### 2.3 这次测试要新增的资产

当前 `Content/TCS_Test/Definition/Buff/` 还是空目录。请在这里创建下面 9 个 `BuffDefinition`。

> 说明：#1 ~ #8 是 State 槽位测试的"状态载体"，不是 Buff 生命周期测试对象。
> 它们的 `DurationType` 统一设为 `Infinite`，确保在人为操作或竞争淘汰前一直存活，
> 避免 SDT_None 在一个 Tick 后自动过期干扰测试观察。
> 只有 #9 专门用于 Buff 运行时测试，使用 `DurationType = Duration`。

1. `DA_Buff_Test_State_ActionLow`
   - `StateDefId = Buff_Test_State_ActionLow`
   - `StateSlotType = StateSlotTag.Test.Action`
   - `Priority = 10`
   - `StateTag = StateTag.Test.ActionLow`
   - `DurationType = Infinite`
   - `Period = 0`
   - `MaxStackCount = 1`
   - `MergerType = UTcsBuffMerger_NoMerge`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
2. `DA_Buff_Test_State_ActionHigh`
   - `StateDefId = Buff_Test_State_ActionHigh`
   - `StateSlotType = StateSlotTag.Test.Action`
   - `Priority = 20`
   - `StateTag = StateTag.Test.ActionHigh`
   - 其余与 `DA_Buff_Test_State_ActionLow` 相同
3. `DA_Buff_Test_State_ActionSameA`
   - `StateDefId = Buff_Test_State_ActionSameA`
   - `StateSlotType = StateSlotTag.Test.Action`
   - `Priority = 15`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
4. `DA_Buff_Test_State_ActionSameB`
   - `StateDefId = Buff_Test_State_ActionSameB`
   - `StateSlotType = StateSlotTag.Test.Action`
   - `Priority = 15`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
5. `DA_Buff_Test_State_OverlayA`
   - `StateDefId = Buff_Test_State_OverlayA`
   - `StateSlotType = StateSlotTag.Test.Overlay`
   - `Priority = 10`
   - `StateTag = StateTag.Test.OverlayA`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
6. `DA_Buff_Test_State_OverlayB`
   - `StateDefId = Buff_Test_State_OverlayB`
   - `StateSlotType = StateSlotTag.Test.Overlay`
   - `Priority = 10`
   - `StateTag = StateTag.Test.OverlayB`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
7. `DA_Buff_Test_State_ControlA`
   - `StateDefId = Buff_Test_State_ControlA`
   - `StateSlotType = StateSlotTag.Test.Control`
   - `Priority = 10`
   - `StateTag = StateTag.Test.Control`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
8. `DA_Buff_Test_State_AttrGate`
   - `StateDefId = Buff_Test_State_AttrGate`
   - `StateSlotType = StateSlotTag.Test.Action`
   - `Priority = 30`
   - `StateTag = StateTag.Test.AttrGate`
   - `DurationType = Infinite`
   - `StateTreeRef = ST_TCS_Buff_Minimal`
   - `ActiveConditions` 添加 `UTcsStateCondition_AttributeComparison`
   - 条件参数：`AttributeTag = TCS.Attribute.Test.AttackPower`、`CheckTarget = Owner`、`ComparisonType = GreaterThanOrEqual`、`CompareValue = 10`
9. `DA_Buff_Test_Runtime_Minimal`
   - `StateDefId = Buff_Test_Runtime_Minimal`
   - `StateSlotType = StateSlotTag.Test.Buff`
   - `DurationType = Duration`
   - `Duration = 5`
   - `Period = 0`
   - `MaxStackCount = 1`
   - `MergerType = UTcsBuffMerger_NoMerge`
   - `StateTreeRef = ST_TCS_Buff_Minimal`

### 2.4 StateSlot 资产必须对齐的配置

打开现有 `StateSlotDefinition`，确保它们至少满足下面的测试前提。若不一致，先改资产，再继续测试。

1. `DA_StateSlot_Test_Action`
   - `SlotTag = StateSlotTag.Test.Action`
   - `ActivationMode = PriorityOnly`
   - `GateCloseBehavior = Pause`
   - `PreemptionPolicy = PauseLowerPriority`
   - `SamePriorityPolicy = UseNewest`
2. `DA_StateSlot_Test_Overlay`
   - `SlotTag = StateSlotTag.Test.Overlay`
   - `ActivationMode = AllActive`
3. `DA_StateSlot_Test_Control`
   - `SlotTag = StateSlotTag.Test.Control`
   - `ActivationMode = PriorityOnly`
   - `GateCloseBehavior = Cancel`
   - `PreemptionPolicy = CancelLowerPriority`
4. `DA_StateSlot_Test_Buff`
   - `SlotTag = StateSlotTag.Test.Buff`

### 2.5 StateTree 与蓝图壳

1. 创建 `ST_TCS_Buff_Minimal`。
   - 入口：`Tirefly Combat System -> Gameplay Runtime -> Buff StateTree`
   - 最小内容：根下一个 `Observe` 状态，挂 `FTcsSTTask_StateChangeNotify`
   - 若需要给 BuffStateTree 中的节点绑定 `OwnerStateComponent`、`OwnerBuffComponent`、`Instigator` 等运行时对象引用，可额外添加 `FTcsSTEvaluator_ObjectRef`，再从其 `Output` 里做绑定。
2. 创建蓝图壳：
   - `BP_TCS_TestFixture`，父类 `ATestTcsFixtureActor`
   - `BP_TCS_StateDirector`，父类 `ATestTcsStateDirector`
   - `BP_TCS_AttributeDirector`，父类 `ATestTcsAttributeDirector`
   - `BP_TCS_BuffDirector`，父类 `ATestTcsBuffDirector`
3. 新建测试地图 `L_TCS_Test_Modules`。
4. 在地图里放置：
   - `BP_TCS_TestFixture` 2 个，分别命名为 `Fixture_Target`、`Fixture_Instigator`
   - `BP_TCS_StateDirector` 1 个
   - `BP_TCS_AttributeDirector` 1 个
   - `BP_TCS_BuffDirector` 1 个

### 2.6 关键信息来源

本方案里所有按钮都来自导演类和夹具类的 `CallInEditor` 函数。实际执行时请使用 `Simulate`。如果当前模式下按钮不可点，就切到 `Simulate` 再操作，并保持 `Output Log` 面板打开。

执行时重点看这几类信息：

1. `Output Log` 中带有 `[TestTCS]` 前缀的日志行
2. `Fixture_Target.StateProbe.LastSnapshot`
3. `Fixture_Target.AttributeProbe.LastSnapshot`
4. `Fixture_Target.BuffProbe.LastSnapshot`

说明：导演类已经不再依赖旧的 Details 面板结果缓存作为主要观测面。每次按钮执行后，导演会把请求结果、Probe 快照和事件名直接写到 `Output Log`。`RefreshObservedData` 的作用也变成“重新采集 Probe 并输出最新日志”。

## 3. 场景初始配置

### 3.1 Fixture 设置

1. `Fixture_Target`
   - `CombatEntityLevel = 1`
   - `StateProbe.ObservedStateDefinitionId` 先留空；做单个状态观察时再切成目标 `StateDefId`
   - `AttributeProbe.ObservedAttributeName` 先留空；做属性测试时改成当前属性名
2. `Fixture_Instigator`
   - `CombatEntityLevel = 1`

### 3.2 StateDirector 设置

1. `TargetFixture = Fixture_Target`
2. `InstigatorFixture = Fixture_Instigator`
3. 可用按钮：
   - `ApplyConfiguredState`
   - `OpenConfiguredSlotGate`
   - `CloseConfiguredSlotGate`
   - `RemoveConfiguredStatesByDefId`
   - `ClearConfiguredSlotStates`
   - `ClearAllStates`
   - `RefreshObservedData`

### 3.3 AttributeDirector 设置

1. `TargetFixture = Fixture_Target`
2. `InstigatorFixture = Fixture_Instigator`
3. 可用按钮：
   - `EnsureConfiguredAttribute`
   - `ResetConfiguredAttribute`
   - `RemoveConfiguredAttribute`
   - `ApplyConfiguredModifier`
   - `RefreshConfiguredModifier`
   - `RemoveWorkingModifiers`
   - `RefreshObservedData`

### 3.4 BuffDirector 设置

1. `TargetFixture = Fixture_Target`
2. `InstigatorFixture = Fixture_Instigator`
3. 可用按钮：
   - `ApplyConfiguredBuff`
   - `RefreshObservedData`

## 4. 执行步骤

### 4.1 StateSlot 静态检查 ✅

1. 打开 `DA_StateSlot_Test_Action`，确认它是 `PriorityOnly + Pause + PauseLowerPriority + UseNewest`。
2. 打开 `DA_StateSlot_Test_Overlay`，确认它是 `AllActive`。
3. 打开 `DA_StateSlot_Test_Control`，确认它是 `PriorityOnly + Cancel + CancelLowerPriority`。
4. 如果这三项不对，先修正资产，再继续下面所有运行时测试。

### 4.2 State / StateComponent / StateSlot 运行时测试 ✅

先进入 `Simulate`，然后按下面顺序执行。

#### ✅ 用例 S1：Action 槽位低高优先级竞争

1. 在 `BP_TCS_StateDirector.AssetCatalog` 设置：
   - `StateDefinitionId = Buff_Test_State_ActionLow`
   - `StateLevel = 1`
   - `StateSlotTag = StateSlotTag.Test.Action`
   - `bRemoveAllMatchingStates = true`
2. 点 `ClearAllStates`。
3. 点 `ApplyConfiguredState`。
4. 期望：
   - `Output Log` 出现 `[TestTCS][State][Accepted=True]`
   - `Output Log` 出现 `OnStateApplySuccess`
   - `StateProbe.LastSnapshot` 能看到 `Buff_Test_State_ActionLow`
5. 把 `StateDefinitionId` 改成 `Buff_Test_State_ActionHigh`。
6. 再点一次 `ApplyConfiguredState`。
7. 期望：
   - `Output Log` 出现新的 `OnStateApplySuccess`
   - `Output Log` 出现 `OnStateStageChanged`
   - `Buff_Test_State_ActionHigh` 成为 `Action` 槽位里的活动状态
   - `Buff_Test_State_ActionLow` 不再是活动状态，按当前槽位策略应进入 `Pause`

#### ✅ 用例 S2：Action 槽位同优先级仲裁

1. 点 `ClearAllStates`。
2. 把 `StateDefinitionId` 改成 `Buff_Test_State_ActionSameA`，点 `ApplyConfiguredState`。
3. 把 `StateDefinitionId` 改成 `Buff_Test_State_ActionSameB`，再次点 `ApplyConfiguredState`。
4. 期望：
   - `Output Log` 至少出现两次 `OnStateApplySuccess`
   - `Output Log` 至少出现一次 `OnStateStageChanged`
   - `StateProbe.LastSnapshot` 里 `Action` 槽位最终活动状态应是后应用的 `Buff_Test_State_ActionSameB`

#### ✅ 用例 S3：Overlay 槽位 AllActive 并存

1. 点 `ClearAllStates`。
2. 把 `StateDefinitionId` 改成 `Buff_Test_State_OverlayA`，`StateSlotTag = StateSlotTag.Test.Overlay`，点 `ApplyConfiguredState`。
3. 把 `StateDefinitionId` 改成 `Buff_Test_State_OverlayB`，再点 `ApplyConfiguredState`。
4. 期望：
   - 两次请求都成功
   - `StateProbe.LastSnapshot` 中 `Overlay` 槽位能同时看到 `Buff_Test_State_OverlayA` 和 `Buff_Test_State_OverlayB`
   - 两者都应处于活动态，不应该互相挤掉

#### ✅ 用例 S4：Control 槽位 Gate 关闭触发 Cancel

1. 点 `ClearAllStates`。
2. 把 `StateDefinitionId` 改成 `Buff_Test_State_ControlA`，`StateSlotTag = StateSlotTag.Test.Control`，点 `ApplyConfiguredState`。
3. 点 `CloseConfiguredSlotGate`。
4. 期望：
   - `Output Log` 包含 `OnSlotGateStateChanged`
   - `Output Log` 包含 `OnStateRemoved`
   - `StateProbe.LastSnapshot` 中 `Control` 槽位被清空
5. 点 `OpenConfiguredSlotGate`。
6. 期望：
   - `Output Log` 再次记录 `OnSlotGateStateChanged`
   - 槽位重新允许接收新状态，但不会自动恢复刚才被 Cancel 的实例

#### ✅ 用例 S5：状态条件失败与成功

1. 先切到 `BP_TCS_AttributeDirector`，把 `AssetCatalog` 设置为：
   - `AttributeName = AttackPower`
   - `AttributeInitValue = 5`
   - `AttributeBaseValue = 5`
   - `AttributeCurrentValue = 5`
2. 点 `EnsureConfiguredAttribute`。
3. 切回 `BP_TCS_StateDirector`，设置：
   - `StateDefinitionId = Buff_Test_State_AttrGate`
   - `StateSlotTag = StateSlotTag.Test.Action`
4. 点 `ApplyConfiguredState`。
5. 期望：
   - `Output Log` 出现 `[TestTCS][State][Accepted=False]`
   - `Output Log` 包含 `OnStateApplyFailed`
6. 再切回 `BP_TCS_AttributeDirector`，把 `AttackPower` 改成：
   - `AttributeBaseValue = 10`
   - `AttributeCurrentValue = 10`
7. 再点一次 `EnsureConfiguredAttribute`。
8. 回到 `BP_TCS_StateDirector` 再点 `ApplyConfiguredState`。
9. 期望：
   - 这次请求成功
   - `Output Log` 包含 `OnStateApplySuccess`

#### ✅ 用例 S6：StateComponent 清理入口

1. 在 `Action` 槽位重新应用一个状态，例如 `Buff_Test_State_ActionHigh`。
2. 点 `RemoveConfiguredStatesByDefId`。
3. 期望：
   - `Output Log` 包含 `OnStateRemoved`
   - `StateProbe.LastSnapshot` 不再包含该 `StateDefId`
4. 在 `Overlay` 槽位重新应用 `OverlayA` 和 `OverlayB`。
5. 确保 `StateSlotTag = StateSlotTag.Test.Overlay`，点 `ClearConfiguredSlotStates`。
6. 期望：
   - `Overlay` 槽位被清空
7. 最后点 `ClearAllStates`。
8. 期望：
   - 所有槽位都被清空

### 4.3 Attribute 运行时测试 ✅

#### ✅ 用例 A1：属性创建与边界 Clamp

1. 在 `BP_TCS_AttributeDirector.AssetCatalog` 设置：
   - `AttributeName = MaxHealth`
   - `AttributeInitValue = 100`
   - `AttributeBaseValue = 100`
   - `AttributeCurrentValue = 100`
2. 点 `EnsureConfiguredAttribute`。
3. 把 `Fixture_Target.AttributeProbe.ObservedAttributeName` 设为 `Health`。
4. 把 `AssetCatalog` 改成：
   - `AttributeName = Health`
   - `AttributeInitValue = 150`
   - `AttributeBaseValue = 150`
   - `AttributeCurrentValue = 150`
5. 再点 `EnsureConfiguredAttribute`。
6. 期望：
   - `Output Log` 包含 `OnAttributeValueChanged` 或 `OnAttributeBaseValueChanged`
   - 如果 `Health` 的范围由 `MaxHealth` 约束，`Output Log` 还应包含 `OnAttributesReachedBoundary`
   - `AttributeProbe.LastSnapshot` 中 `Health` 的最终值不应超过 `MaxHealth`

#### ✅ 用例 A2：Modifier 应用 / 更新 / 移除

1. 把 `ObservedAttributeName` 改成 `AttackPower`。
2. 把 `AssetCatalog` 改成：
   - `AttributeName = AttackPower`
   - `AttributeInitValue = 10`
   - `AttributeBaseValue = 10`
   - `AttributeCurrentValue = 10`
3. 点 `EnsureConfiguredAttribute`。
4. 把 `ModifierId` 设为 `AttrMod_Test_AttackPower_Add10`。
5. 点 `ApplyConfiguredModifier`。
6. 期望：
   - `Output Log` 包含 `OnAttributeModifiersAdded`
   - `AttributeProbe.LastSnapshot` 中 `Modifiers` 计数增加
7. 点 `RefreshConfiguredModifier`。
8. 期望：
   - `Output Log` 包含 `OnAttributeModifiersUpdated`
9. 点 `RemoveWorkingModifiers`。
10. 期望：
   - `Output Log` 包含 `OnAttributeModifiersRemoved`
   - `AttributeProbe.LastSnapshot` 中 `Modifiers = 0`

#### ✅ 用例 A3：ResetAttribute

1. 保持 `AttributeName = AttackPower`。
2. 先执行一次 `ApplyConfiguredModifier`。
3. 点 `ResetConfiguredAttribute`。
4. 期望：
   - 属性数值回到 `InitialValue`
   - 应用在该属性上的 Modifier 被一起清掉
   - `AttributeProbe.LastSnapshot` 中 `Modifiers = 0`

#### ✅ 用例 A4：RemoveAttribute 与依赖保护

1. 确保 `MaxHealth` 和 `Health` 都已经存在。
2. 把 `AttributeName` 改成 `MaxHealth`，点 `RemoveConfiguredAttribute`。
3. 期望：
   - 请求失败
   - 因为 `Health` 的动态范围依赖 `MaxHealth`，当前实现应阻止删除
4. 把 `AttributeName` 改成 `Health`，点 `RemoveConfiguredAttribute`。
5. 再把 `AttributeName` 改成 `MaxHealth`，再点一次 `RemoveConfiguredAttribute`。
6. 期望：
   - 第二次移除 `Health` 成功
   - 最后移除 `MaxHealth` 也成功
   - `AttributeProbe.LastSnapshot` 中 `Attributes` 计数下降

### 4.4 Buff 最小运行时测试 ✅

1. 进入 `BP_TCS_BuffDirector.AssetCatalog`：
   - `BuffDefinitionId = Buff_Test_Runtime_Minimal`
   - `BuffLevel = 1`
2. 点 `ApplyConfiguredBuff`。
3. 期望：
   - `Output Log` 出现 `[TestTCS][Buff][Accepted=True]`
   - `Output Log` 包含 `OnBuffRuntimeDelta`
   - `BuffProbe.LastSnapshot` 显示 `ActiveBuffs=1`
4. 等待 5 秒，或者按需要刷新观察数据。
5. 期望：
   - `Output Log` 最终包含 `OnBuffRemoved`
   - `BuffProbe.LastSnapshot` 回到 `ActiveBuffs=0`

## 5. 收尾与通过标准

完成整套验证后，至少要满足下面 5 条，才算当前测试通过。

1. `StateSlot` 的三种关键配置已经按文档落地：`PriorityOnly`、`AllActive`、`Cancel Gate`。
2. `StateComponent` 的 6 个关键入口都验证过：应用、开 Gate、关 Gate、按 DefId 清理、按槽位清理、全量清理。
3. `State` 的 5 个核心行为都验证过：高低优先级、同优先级、AllActive、条件失败、移除事件。
4. `Attribute` 的 5 个核心行为都验证过：创建、Clamp / Boundary、Modifier 生命周期、Reset、Remove 与依赖保护。
5. `Buff` 最小运行时链路可以稳定应用并自动移除。

如果某一步失败，不要先改文档。先看对应导演输出的 `[TestTCS]` 日志、Probe 的 `LastSnapshot`，再回到相应组件实现定位真实原因。
