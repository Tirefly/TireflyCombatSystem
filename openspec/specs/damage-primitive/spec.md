# damage-primitive Specification

## Purpose
TBD - created by archiving change add-tcsdamage-steps-and-primitive. Update Purpose after archive.
## Requirements
### Requirement: Damage 链步骤与执行器

`TcsDamage` MUST 提供链原语 `FTcsStepDamage`（住 `Public/Chain/TcsStepDamage.h`）：

- 字段：`FGameplayTag FlowTemplateId`（**无效 tag = 官方默认模板**；**2026-09-22 改造：类型 `FName` → `FGameplayTag`，空值判定 `IsNone()` → `!IsValid()`**）、`FTcsParamValue DamageBase`（基础伤害值输入——参数账本解算结果，PV-7）、`TMap<FGameplayTag, FTcsParamValue> FormulaParams`（公式参数初值；**2026-09-22 改造：键类型改 tag**）、`TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
  **MUST NOT 携带 `Conditions`**（链级条件归 M4a 触发行/求值器轮——R3 无链侧条件求值器，留着就是"配了没人执行"的死字段）；
  **MUST NOT 携带 `Delegate` / `HealthAttrKey`**（二者属**流程模板**配置：公式/护盾 hook 在模板的 `FTcsFlowBaseDamage`/`FTcsFlowExecute` 步骤上、扣血属性键在 `FTcsFlowExecute.AttrKey`）——同一件事只有一处配置，避免双真相与死字段（计划 sketch 曾把两者列在本 struct，落地收窄）；
- 执行器 MUST：构 `FTcsDamageFlowContext`（`Attacker`/`Instigator` 取 `Context.Caster`/`Instigator` 句柄、`Targets` 取 `Context.Targets`——**消费目标集，不内嵌选择器**，D4-4 v2）→ 把 `DamageBase` 与 `FormulaParams` 写入上下文（黑板契约键 `Tcs.Flow.Key.BaseDamage` / 公式参数表）→ `RunTemplate` → 恒返回 `TSR_Completed`（流程单帧同步完成，无挂起）；
- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **自注册进 TcsEffect 的执行器注册表**（D4-14 的又一次跨模块实证：TcsDamage 不依赖 TcsTargeting，TcsEffect 不认识伤害语义）；
- **流程零计算纪律**：步骤 MUST NOT 自行推导基础伤害（`DamageBase` 即解算结果；复合运算由链侧参数链承载——PV-7/M5 轮）。

#### Scenario: 链上跑通到流程

- **WHEN** 一条链执行到 `[Damage]` 步骤（目标集已由前置步骤或调用方填好）
- **THEN** 流程按模板跑完，目标的 `TargetAttrKey` 属性被事务扣减，记录事件已发布

#### Scenario: 跨模块注册可查

- **WHEN** TcsDamage 模块加载后查询 TcsEffect 的执行器注册表
- **THEN** `FTcsStepDamage` 的执行器已登记（零启动代码、TcsEffect 侧零改动）

#### Scenario: 无效模板 id 走官方默认

- **WHEN** 步骤的 `FlowTemplateId` 为无效 tag（未配置）
- **THEN** 执行器按官方默认模板（`Tcs.Flow.Template.Default`）起流程（与改造前"空 FName 兜底 `Default`"行为一致）

### Requirement: 流程委托契约（宿主实现）

`TcsDamage` MUST 提供 `ITcsDamageFlowDelegate`（UINTERFACE，宿主实现；**全部函数带中性默认实现**——"普通项目零 delegate"，PV-7/D7-2）：

- **反射面（2026-09-24 新增，台账 S-8）**：5 方法 MUST 带 `UFUNCTION(BlueprintNativeEvent)`，且 MUST 在接口类内声明对应的 `virtual <名>_Implementation(...)`——UHT 检测到声明则**不生成默认 stub**（`UhtFunction.cs:681` 的 `ImplFound`），故中性默认实现住接口声明处（同款先例 = 引擎 `ISequencerAnimationOverride`，`SequencerAnimationOverride.h:31-44`；本仓 `ITcsAttributeProvider` 为无默认体的同族先例）；
- **形参 MUST 全反射**：上下文参数类型 MUST 为 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)` 反射只读视图），**MUST NOT** 为 `FTcsDamageFlowContext`——后者是**纯 C++ struct（无 `USTRUCT`）**，出现在 `UFUNCTION` 签名里会让 UHT 报 `Unable to find 'struct' with name ...`（同款既有实证：`UTcsEffectSubsystem::ExecuteChain` 因此无法标记，只能另开 `ExecuteChainForCaster`）；
- **C++ 调用点 MUST 走 `ITcsDamageFlowDelegate::Execute_<名>`**（**MUST NOT** 虚表直调）——虚表直调会**静默跳过**脚本层实现（C# 覆写走 `ProcessEvent`），表现为"公式不生效"而非崩溃。`Execute_` 内部先查 `UFunction` 走反射、查不到才回落原生接口实现（`TcsAttributeProvider.gen.cpp:141-157` 同款生成代码），故**C++ 实现与脚本实现双轨并存**；
- 5 方法签名（换型后）：
  - `double GetBaseHitRate(攻击者句柄, 目标句柄, const FTcsDamageFlowContextView&)`——默认 `1.0`；
  - `double GetBaseCritRate(...)`——默认 `0.0`；
  - `FGameplayTag ResolveElement(...)`——默认空 Tag；
  - `double CalculateBaseDamage(double IncomingBase, ...)`——**降级逃生口**（默认实现 = 返回传入值，即"不改动输入"）；
  - `double ModifyShield(目标句柄, double IncomingDamage, ...)`——默认 `0.0`（无护盾）；
- 契约全部以**实体句柄**为参与者（承接 2026-09-20 句柄化）；插件 MUST NOT 内置任何公式。

#### Scenario: 零 delegate 也能跑

- **WHEN** 链步骤的 `Delegate` 为空（宿主完全不实现）
- **THEN** 默认模板仍能跑通（命中率 1、暴击 0、元素空、基础值不被改写、无护盾）

#### Scenario: 宿主自定义公式

- **WHEN** 宿主实现 `CalculateBaseDamage` 返回自己的公式结果
- **THEN** `BaseDamage` 步骤采用该值（逃生口生效）

#### Scenario: 脚本层实现可被 C++ 调用点抵达

- **WHEN** 宿主脚本层（C#）实现 `CalculateBaseDamage` 并把实现对象配进流程模板的 delegate 字段，随后流程跑到 `FTcsFlowBaseDamage`
- **THEN** 基础伤害值等于脚本实现返回的值（`Execute_` 经 `ProcessEvent` 抵达脚本实现）——**这是"自定义伤害流程可用脚本编写"的判据**

#### Scenario: C++ 实现与脚本实现并存

- **WHEN** 宿主以 C++ 类（非脚本）实现同一接口并配进模板
- **THEN** 行为与换型前一致（`Execute_` 的 `else if` 分支回落原生 `_Implementation`，无需宿主额外适配）

### Requirement: 流程上下文反射视图（`FTcsDamageFlowContextView`）

`TcsDamage` MUST 提供 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)`）——**`FTcsDamageFlowContext` 的可反射数据面投影**，专供宿主脚本层在插槽实现里读取流程状态：

- 字段 MUST 覆盖：参与者句柄（`Attacker` / `Instigator` / `Targets`）、公式参数初值（`FormulaParams`）、分类 Tag 集（`ClassificationTags`）、请求字段（`BaseDamageInput` / `TargetAttrKey`）；
- **MUST NOT 含黑板**（`FTcsFlowAttributes`）——其提交项深处嵌 `FTcsConsumePolicy::OnConsumed`（`TFunction<void()>`），**物理不可反射**（`TcsFlowAttributes.h:44-46` 自注、UHT 实证）；
- **MUST NOT 含宿主门面弱引用**（`Owner`）——脚本层经自身 `GetWorld()` 取门面，无需框架代传（且 `TWeakObjectPtr` 非脚本友好）；
- **MUST NOT 含 `FlowSource` / `CapturedAttrs`**（框架簿记面，宿主脚本无消费场景）；
- **构造方向 = 单向**：由框架从 `FTcsDamageFlowContext` 投影（`MakeView`）；**MUST NOT** 提供反向写回——视图是**只读投影**，不是"另一个可写上下文"（避免双真相）；
- **为什么是视图而非整体反射化**：把 `FTcsDamageFlowContext` 整体反射化需先做**上下文/黑板分层**（把含闭包的运行态面摘出）——那是台账 S-3 的范围。视图只摘可反射面，**成本低一个数量级**，且满足插槽的全部读取需求。

#### Scenario: 视图不含不可反射成员

- **WHEN** 检查 `FTcsDamageFlowContextView` 的 `UPROPERTY` 字段集
- **THEN** 全部字段可反射（无 `TFunction`、无 `TWeakObjectPtr`、无裸 C++ struct）；UHT 编译通过

#### Scenario: 宿主脚本读得到流程状态

- **WHEN** 脚本层在 `CalculateBaseDamage` 实现里读取 `Context.Attacker` / `Context.Targets` / `Context.FormulaParams`
- **THEN** 读到的是本次流程的真实值（非默认值/空集）

#### Scenario: 投影不改动原上下文

- **WHEN** 框架从 `FTcsDamageFlowContext` 构造视图
- **THEN** 原上下文内容不变（视图是拷贝投影，非别名）

### Requirement: 伤害记录与事件

`TcsDamage` MUST 以扁平记录承载伤害结果（09 §2.4）：

- `FTcsDamageRecord`（USTRUCT）：`uint64 FlowId` / `FTcsCombatEntityHandle Source` / `FTcsCombatEntityHandle Target` / `FGameplayTag Element` / `bool bHit` / `bool bCrit` / `double Base` / `double Final` / `double Executed` / `double Absorbed` / `bool bKill` / `uint64 Sequence` / `double Timestamp`；
- **参与者用实体句柄**（**MUST NOT 用 `AActor*`**——承接句柄化）；**字段全扁平**（无 `TMap`/`TFunction`——符合过网结构纪律，将来可直接上 FastArray）；
- `bKill` 的语义 = **记账而非裁定**：本次事务提交后目标 `Health ≤ 0`（死亡规则仍归宿主）；
- `FTcsFlowCompleted` 步骤 MUST 填充记录并经总线**立即通道**发布事件（**原生 Tag** `Tcs.Event.Damage.Recorded`，载荷 = 该记录）；
- `UTcsDamageSubsystem` MUST 持**环形缓冲**（容量常量初值 128）并提供读取口 `GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const`（旧→新序）。

#### Scenario: 完成即出记录

- **WHEN** 默认模板跑完
- **THEN** 记录事件在 `RunTemplate` 返回前派发（立即通道）、环形缓冲可读回同一笔记录（字段与本次伤害一致）

#### Scenario: 记录可跨模块消费

- **WHEN** 装置/宿主订阅该原生 Tag
- **THEN** 收到载荷即拿到完整记录（无需访问流程上下文——记录是结果快照）

### Requirement: 流程模板登记面的反射可达性

`UTcsDamageSubsystem` 的模板登记方法 MUST 对宿主脚本层可达（`RegisterTemplate` / `UnregisterTemplate`）：

- 标记 MUST 为 **`UFUNCTION()` 无 specifier**（口径同 `effect-interpreter` 的门面反射面——`FTcsFlowTemplate` 是 `USTRUCT()` 非 `BlueprintType`，加 `BlueprintCallable` 会被 UHT 蓝图参数校验拒绝）；
- 可见性依赖 C++ 侧 `public` 访问级别；实现后 MUST 以生成的 glue 产物验证（读 `*.generated.cs` 确认修饰符为 `public`）；
- 覆盖范围含步骤数据反射入口：`RegisterStepExecutor`（见 `effect-step-dispatch` 的"步骤执行器插槽"）——**脚本层要能"登记自己的步骤执行器"就必须能先"登记含该步骤的模板"**，两者是同一闭环的两半。

#### Scenario: 脚本层可登记流程模板

- **WHEN** 检查 `UTcsDamageSubsystem` 的生成绑定产物
- **THEN** `RegisterTemplate` / `UnregisterTemplate` 出现且修饰符为 `public`

