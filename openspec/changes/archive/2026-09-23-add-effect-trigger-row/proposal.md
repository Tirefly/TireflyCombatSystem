# Change: 触发行数据形状与条件求值（M4a 首批）

## Why

`04-module-effects.md` 把 TcsEffect 的职责定义为"**事件 → 触发行 → 效果链**的触发执行引擎"，`D4-1` 已拍板触发行 10 字段。但代码里**零实现**：全库 grep `FTcsTrigger*` / `触发行` 只命中注释，插件侧**零事件订阅者**——今天 TCS 只能手动调 `UTcsCombatEntityComponent::ExecuteChainById`（其头注释自陈"R3 无触发行，手动触发是竖切入口"）。

**后果**：`09 §2.3` 的"伤害修改器唯一通道（D7-6）"两端皆断——没有触发行订阅流程收集事件，`Execute` 步骤里已实现的 `SortKey` 消耗裁决（`TcsFlowStepsCore.cpp:127-140`）无物可裁。

本次是 R4（plan3）的第一批：**只落数据形状与条件求值**，登记表/求值器/订阅生命周期归下一批（Task 2），保持每个提案可独立验证。

## What Changes

新增能力 `effect-trigger`，两条需求：

1. **触发行数据形状**：`FTcsTriggerRow`（D4-1 终版 10 字段 + 登记簿记的 `Source`）+ `ETcsExecutionGate` 枚举（R4 只实现恒通过值）。
2. **触发条件最小集**：`FTcsTriggerCondition_HasAllTags` / `FTcsTriggerCondition_Chance` + `FTcsTriggerContext`（触发期最小上下文）+ `EvaluateTriggerConditions` 助手。

**范围收窄（明示，非遗漏）**：D4-1 的 10 字段中 `EventPayloadFilter` / `Cues` / `InterruptPriority` / `ExecutionGate` 非默认值四项**本轮只存不裁**——各自消费者未出现（载荷无字段可筛、TcsCue 属 R8、链打断语义未实现、R4 无联网）。D4-5 条件最小集的其余四项（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom）同样后置。**均入台账**。

## 影响面

- **Affected specs**: `effect-trigger`（ADDED × 2）
- **Affected code**（全部新建，零既有文件改动）:
  - `Source/TcsEffect/Public/Trigger/TcsTriggerRow.h`
  - `Source/TcsEffect/Public/Trigger/TcsTriggerConditions.h`
  - `Source/TcsEffect/Private/Trigger/TcsTriggerConditions.cpp`
- **不改**：`TcsEffect.Build.cs`（`FTcsSourceHandle` 住 TcsCore，已在依赖内——无新依赖边）；既有链/解释器/注册表（本批纯新增类型）。

## 设计口径（须与设计文档一致）

- **条件类型与流程侧同名不同物**：`TcsDamage` 已有 `FTcsConditionHasAllTags` / `FTcsConditionChance`（流程步骤用，求值上下文是 `FTcsDamageFlowContext`）。本轮在 TcsEffect 建的是**触发期**版本（上下文是 `FTcsTriggerContext`）——**TcsEffect 不能 include TcsDamage**（依赖铁律），且两者上下文形状本就不同。故用 `FTcsTriggerCondition_*` 前缀区分，并各自注释互相指名。
- **求值纪律沿用流程侧同款**（已实证的形态）：条件类型之间无公共基类（D4-16）→ 用"同名字段 + 求值点首行调用助手"替代基类虚函数；**未知条件类型视为不过 + Warning**（不静默通过——静默会让"条件写错"表现成"步骤照跑"）；`Chance` 的随机值**由调用方注入**（D0-1 确定性纪律：求值内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）。
- **`FTcsTriggerContext` 是最小集**：只含本批条件真正需要的字段（`EventTag` / `ClassificationTags` / `Caster`）。**MUST NOT** 为将来的条件预建字段（零消费者不预建）。

## 非目标

- 不做登记表、总线订阅、求值器（Task 2）；
- 不做 `ModifyFlow` 链原语（Task 3）；
- 不做 D4-5 剩余条件与 D4-1 的四个留位字段的裁决逻辑（各自消费者出现时）。
