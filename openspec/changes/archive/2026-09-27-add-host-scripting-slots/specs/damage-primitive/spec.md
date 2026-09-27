## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: 流程模板登记面的反射可达性

`UTcsDamageSubsystem` 的模板登记方法 MUST 对宿主脚本层可达（`RegisterTemplate` / `UnregisterTemplate`）：

- 标记 MUST 为 **`UFUNCTION()` 无 specifier**（口径同 `effect-interpreter` 的门面反射面——`FTcsFlowTemplate` 是 `USTRUCT()` 非 `BlueprintType`，加 `BlueprintCallable` 会被 UHT 蓝图参数校验拒绝）；
- 可见性依赖 C++ 侧 `public` 访问级别；实现后 MUST 以生成的 glue 产物验证（读 `*.generated.cs` 确认修饰符为 `public`）；
- 覆盖范围含步骤数据反射入口：`RegisterStepExecutor`（见 `effect-step-dispatch` 的"步骤执行器插槽"）——**脚本层要能"登记自己的步骤执行器"就必须能先"登记含该步骤的模板"**，两者是同一闭环的两半。

#### Scenario: 脚本层可登记流程模板

- **WHEN** 检查 `UTcsDamageSubsystem` 的生成绑定产物
- **THEN** `RegisterTemplate` / `UnregisterTemplate` 出现且修饰符为 `public`
