## 背景

这次结构优化面对的不是单一超长文件，而是三类不同问题：

- `BuffComponent` 与 `StateInstance` 已经具备相对稳定的职责分块，可以直接执行 `.cpp` 拆分。
- `AttributeComponent` 与 `StateComponent` 的头文件虽然已经存在 `#pragma region`，但当前 region 边界并不适合作为 `.cpp` 文件的切分依据，必须先重整声明面。
- `StateManagerSubsystem` 超长，但当前实现仍然围绕“定义加载 / 定义查询 / 跨 Actor apply 门面”单线收敛，机械拆分 `.cpp` 不会带来明显收益。

## 目标

- 在不改变运行时行为的前提下，按稳定职责重组超长运行时文件。
- 让目标头文件的 region 布局能够自然映射到后续 `.cpp` 拆分边界。
- 统一目标头文件的声明格式、空白规则与成员注释风格。
- 为后续继续维护这些模块提供更低风险的局部修改入口。

## 非目标

- 不引入新功能。
- 不改变公开 API 名称、参数、返回值、委托签名。
- 不把这次变更扩展成 State / Attribute / Buff 的职责再设计。

## 设计决策

### 决策：以“稳定职责边界”而不是“500 行阈值”作为最终拆分依据

- 原因：500 行只适合作为触发审查阈值，不适合作为机械切分规则。
- 结果：允许保留个别超过 500 行但职责单一、暂时不适合继续切分的实现文件；前提是 design 中明确记录原因。

### 决策：`BuffComponent` 直接拆 `.cpp`

- 原因：`BuffComponent` 现有公开接口已经较清晰地分成 Owner、Duration、Lifecycle、Event、Internal 等块。
- 结果：优先采用 `TcsBuffComponent.<Responsibility>.cpp` 命名方式拆分实现文件。

推荐职责分块：

- `TcsBuffComponent.OwnerAndQuery.cpp`
- `TcsBuffComponent.DurationAndTick.cpp`
- `TcsBuffComponent_Lifecycle.cpp`
- `TcsBuffComponent.MergeAndEvents.cpp`

### 决策：`AttributeComponent` 先重排头文件，再拆 `.cpp`

- 原因：当前 `Attribute` / `AttributeInstance` / `AttributeModifier` / `AttributeCalculation` 四块并没有把查询、广播、运行时存储、生命周期边界分清楚。
- 结果：先重整 `TcsAttributeComponent.h` 的 region，再让新的 region 结构反向决定 `.cpp` 切分边界。

目标 region 方向：

- `ActorComponent`
- `ManagerReference`
- `QueryAndSnapshot`
- `EventBroadcast`
- `AttributeInstanceLifecycle`
- `ModifierLifecycle`
- `CalculationAndConstraint`
- `RuntimeStorage`

补充约束：

- `RecalculateAttributeBaseValues` 与 `RecalculateAttributeCurrentValues` 仍保持相邻，避免把“修改器求值主链”机械拆散。
- 当 `CalculationAndConstraint` 切片仍然过厚时，允许把 `ClampAttributeValueInRange` 与 `EnforceAttributeRangeConstraints*` 组成的范围约束求解器继续抽成独立实现文件；前提是保持该求解器自身局部完整。

### 决策：`StateInstance` 直接拆 `.cpp`

- 原因：`StateInstance` 当前实现基本围绕初始化、参数、StateTree 三块职责展开。
- 结果：直接按职责拆分，不要求先重排头文件。

推荐职责分块：

- `TcsStateInstance.Initialization.cpp`
- `TcsStateInstance_Parameters.cpp`
- `TcsStateInstance_StateTree.cpp`

### 决策：`StateManagerSubsystem` 保持单 `.cpp`，只重整头文件

- 原因：当前 `.cpp` 虽然超过 500 行，但主要职责仍然集中在定义加载、定义查询与 apply façade，不值得为了“行数达标”强拆编译单元。
- 结果：本次只整理 `TcsStateManagerSubsystem.h` 的 region、注释和声明布局，不对 `.cpp` 做物理拆分。

目标 region 方向：

- `GameInstanceSubsystem`
- `DefinitionCaches`
- `DefinitionLoading`
- `DefinitionQueries`
- `RuntimeIds`
- `CrossActorApplyFacade`

### 决策：`StateComponent` 先重排头文件，再拆 `.cpp`

- 原因：当前 `StateInstance`、`StateSlot_Gate`、`StateTree_State` 等 region 过宽，无法直接映射成稳定的 `.cpp` 边界。
- 结果：先重整 `TcsStateComponent.h`，再按新边界拆 `.cpp`。

目标 region 方向：

- `ActorComponent`
- `EventSurface`
- `ExtensibilityHooks`
- `ManagerReferences`
- `ApplyAndCreation`
- `LifecycleAndRemoval`
- `QueryAndDebug`
- `SlotRuntimeData`
- `SlotActivation`
- `StateTreeIntegration`
- `RuntimeFlagsAndScheduler`

补充约束：

- `SlotRuntimeData` 与 `SlotActivation` 可以进一步拆成相邻实现文件：前者负责槽位容器重建、StateTree 绑定、槽位分配与变更传播；后者只保留 Gate/激活收敛主链。
- 进一步细化时，仍然保持 `UpdateStateSlotActivation` 到 `ProcessPriorityOnlyMode` 的激活执行主链局部完整，不把它拆成零散工具文件。
- `ApplyAndCreation` 允许继续细化成“apply 入口/条件校验主链”和“StateInstance 构建/参数求值支链”两个相邻实现文件；前提是 `TryApplyState -> TryApplyStateInstance -> CheckStateApplyConditions` 仍保持局部收敛。

## 头文件布局规则

对纳入本次变更的目标头文件，统一执行以下约束：

- `#include` 块之后，按“全局声明块 -> 委托声明块 -> 前置声明块”的顺序组织顶部声明。
- 这三个块之间固定使用三行空白。
- 类内每个 `#pragma region` 之间固定使用三行空白。
- 成员函数声明与成员变量声明补齐注释。
- 对复杂实现函数，仅在 `.cpp` 中为关键流程补注释，不写机械性逐行说明。

## 实施顺序

1. 先整理 `TcsAttributeComponent.h`、`TcsStateManagerSubsystem.h`、`TcsStateComponent.h` 的 region 与注释结构。
2. 先执行 `BuffComponent` 与 `StateInstance` 的 `.cpp` 拆分。
3. 再根据新的头文件边界拆分 `AttributeComponent.cpp` 与 `StateComponent.cpp`。
4. 每完成一个阶段，执行一次 editor-target 编译验证。

## 风险

- 最大风险不是编译错误，而是把高耦合流程机械拆散，导致后续读代码时跨文件跳转成本更高。
- `AttributeCalculation` 主链与 `StateComponent` 的槽位激活主链都存在强耦合区，拆分时必须保持单一职责切片内的局部完整性。
- 本次需要同步补齐大量注释，如果范围控制不住，容易把结构重组变成高噪声 diff；因此注释补齐只限定在本次 touched 的目标头文件与新拆分出的实现文件中。