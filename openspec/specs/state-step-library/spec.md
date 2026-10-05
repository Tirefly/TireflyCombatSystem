# state-step-library Specification

## Purpose
定义 `TcsState` 领域**链步骤**的最小能力面（D4-14 注册制分派下"领域模块自持步骤与执行器"的落点）：状态层在效果链里的**施加入口**——取值来源（黑板 `RunSource` / `Instigator`）、目标缺省（黑板目标集首个）、失败两档（软失败不断链）与四档回执的日志口径。它让"链能施加状态"成为可配置的事实，并划清一条边界：**链侧的参数表载体不存在**（引用类参数源落兜底），步骤只负责"何时施加"，施加语义全归 `state-instance-lifecycle` 与 `state-stacking-policies`。

## Requirements

### Requirement: 状态施加步骤（ApplyState）

`TcsState` MUST 提供 `FTcsStepApplyState`（D4-16 十五原语之一，属本模块——`04 §2.1`"ApplyState 住 TcsState"）：

- 字段：`Target: FTcsCombatEntityHandle`（无效则取黑板 `Targets[0]`，仍无效 ⇒ 软失败）/ `DefTag: FGameplayTag` / `Overrides: TMap<FGameplayTag, double>`（施加方参数覆盖，参与快照构建）；
- 执行体 MUST 经黑板 `Owner`（链运行态门面弱引用）取世界 → 状态门面 → `ApplyState(...)`：`Source` MUST = 黑板 `RunSource`（链运行态来源锚点）、`Instigator` MUST = 黑板 `Instigator`（无效时由门面退化为目标）、参数表 = **空**（链侧参数表载体不存在 ⇒ 引用类参数源落兜底；如实边界）；
- 步骤为**即时步骤**：恒 `TSR_Completed`，不挂起；失败面 MUST 分两档——定义未在本世界登记 / 目标无效 ⇒ `Warning`（门面已留痕，步骤不重复报错），`DefTag` 无效（未配置）⇒ `Error`；**两档都 MUST NOT 断链**（`ETcsStepResult` 只有 `Completed` / `Running`，步骤无法中断链；软失败接管归 `OnError`，R5.5-a）；
- 四档回执（`Applied` / `Refreshed` / `Stacked` / `Rejected`）MUST 以 `Log` 级日志如实记录且**MUST NOT** 因"被拒绝"产生红字（业务结果不是错误——与 `state-stacking-policies` 同口径）；
- 自注册 MUST 走 `effect-step-dispatch` 的静态自注册宏（键 = 步骤 struct 的 `const UScriptStruct*`；**不存在步骤 tag 词表**，不得为此新开 tag 根）。

#### Scenario: 链里施加成功

- **WHEN** 一条链执行 `ApplyState`（`DefTag` = 已登记定义），`Target` 取黑板 `Targets[0]`
- **THEN** 该单位在册状态数 +1，且新实例的 `Source` = 本次运行的 `RunSource`（两次不同运行所施加的实例异源）

#### Scenario: 同一运行内重复施加同源

- **WHEN** 同一条链里连续两次对同一目标施加同一 `DefTag`（两次都取同一份黑板 `RunSource`）
- **THEN** 第二笔按**同一来源**判共存（默认策略下回执为 `Refreshed`、层数不变），而不是叠层

#### Scenario: 定义未登记时不断链

- **WHEN** 步骤的 `DefTag` 未在本世界登记
- **THEN** 门面返回 `Rejected`、本步返回 `TSR_Completed`，后续步骤照常执行（链不断）

#### Scenario: 目标与黑板目标集都为空时软失败

- **WHEN** 步骤的 `Target` 无效且黑板 `Targets` 为空
- **THEN** 本步记为软失败（`Warning`）并返回 `TSR_Completed`——MUST NOT 崩溃、MUST NOT 断链
