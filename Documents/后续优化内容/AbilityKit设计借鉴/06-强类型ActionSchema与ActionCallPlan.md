# 借鉴 06：强类型 ActionSchema 与 ActionCallPlan

## 概述

AbilityKit 在 `com.abilitykit.actionschema` 与 `triggering` 模块中引入"强类型 Action Schema + ActionCallPlan"机制，把配置化动作落到带参数 Schema、服务解析、失败原因与日志的运行时代码，而不是散落的字符串脚本。这避免了配置黑盒化：TriggerPlan 既能在运行时执行，也能反向转换回更适合审查的 Source 配置。

TCS 的 StateTree Task 已是强类型，但跨触发器/跨来源的 Action 复用模型还没有成型。借鉴此模式能让 TCS 的"事件规则层"具备可校验、可复用、可解释的 Action 体系。

## AbilityKit 设计思想与架构实践

### 模块定位

`com.abilitykit.actionschema`（`README.md:252`）：

> Action/Timeline 数据结构与运行时辅助，用于把时序动作、技能事件和编辑器数据表达为稳定 DTO。

### 执行链路中的位置

AbilityKit 的 Triggering 执行链路（借鉴 03 已展开）中，ActionSchema 占据"规则落到动作"这一关键环节（`README.md:376-385`）：

```
Conditions → ActionCallPlan → ActionSchemaRegistry（validate / resolve args）
          → PlannedTriggerActionExecutor → 解析为 giveDamage / addBuff / shootProjectile / playCue
```

### 核心抽象

#### Action Definition

Action 是带强类型参数的 definition，含 effect type、key、op、value、scope（`Docs/AbilityKit_vs_GAS_Comparison.md:816-839`）：

```csharp
public class EffectItem
{
    public string Type;     // "Stat", "Tag", "Damage"...
    public string Key;      // "Health", "Mana", "Stun"...
    public EffectOp Op;     // Add, Mul, Set, Grant, Remove...
    public EffectValue Value;
    public EffectScopeDef Scope;
}
```

#### ActionRegistry

Action 在注册表里登记，可被多个触发器 / 触发源复用（`Docs/AbilityKit_vs_GAS_Comparison.md:239-251` ExecCtx 提及 `ActionRegistry`）。

#### 校验与解析

`ActionSchemaRegistry` 在执行前校验参数合法性、解析服务引用，避免运行时 NPE。`PlannedTriggerActionExecutor` 把校验后的 ActionCallPlan 落到具体执行代码。

### 框架价值

AbilityKit 在 README 里强调（`README.md:91-111`）：

- **强类型 Action 执行**：配置化动作最终落到带参数 schema、服务解析、失败原因和日志的运行时代码，而不是散落字符串脚本。
- **可反向导出**：Trigger Plan 可以在运行时执行，也可以转换回更适合审查和维护的 Source 配置，降低配置黑盒化风险。
- **跨来源复用**：同一个 `giveDamage` / `addBuff` / `shootProjectile` / `playPresentation` Action 可以被主动技能、被动触发、投射物命中、区域效果、Buff 周期事件复用。

### Action 生命周期与 Cue

每个 Action 执行时可附带 `Cue`、`Lifecycle`、`Tracer` 钩子（借鉴 03、05 已展开）。Action 的执行结果通过 ExecutionControl 上报中断 / 跳过 / 短路。

## 与 TCS 现状的对比

### TCS 当前的 Action 表达

TCS 的"动作"目前分散在：
- `StateTree Task`：StateTree 内部的执行单元，强类型、UE 原生。
- `State 生命周期回调`：`OnStateEnter / OnStateTick / OnStateExit` 等。
- `Modifier 执行策略`：CDO 策略类 `UTcsAttributeModifierExecution / UTcsStateCondition / UTcsStateMerger` 等。
- `Skill 激活`：`UTcsSkillComponent::ActivateSkill` 及 `ETcsSkillActivateResult`（TCS 子代理报告 §3.3）。

### 差异点

- **跨触发器复用**：TCS 的 Modifier 策略类可复用，但"giveDamage / addBuff / shootProjectile"这一类通用 Action 没有统一注册表。
- **配置化动作**：TCS 的动作要么是 StateTree Task（引擎内），要么是 CDO 策略类（C++ 内），缺少"数据驱动 Action Schema"层。
- **校验链**：TCS 没有显式"Action 参数 Schema 校验"层，校验依赖运行时断言。
- **失败原因**：TCS 有 `ETcsStateApplyFailReason` 枚举（`state-management/spec.md:204-222`），但只是 State 施加失败原因，不是 Action 执行失败原因。
- **反向导出**：TCS 有 DataTable↔DefAsset 双向同步（`def-editor-authoring/spec.md:154-240`），但没有"运行时 Action 配置反向导出"能力。

### 相似点

- 两者都强调"配置驱动"。
- 两者都用强类型避免字符串脚本散乱。
- StateTree Task 本身就是 UE 原生的"强类型 Action Schema"实现。

## 借鉴建议

### 1. 引入"TcsActionDefinition"作为通用动作定义

```cpp
// 仅作设计示意
UCLASS(Abstract, BlueprintType, Const)
class UTcsActionDefinition : public UPrimaryDataAsset
{
public:
    virtual ETcsActionType GetActionType() const;
    virtual FTcsActionSchema GetSchema() const;  // 参数 schema
    virtual FName GetActionKey() const;
};
```

派生：`UTcsDamageActionDefinition`、`UTcsApplyStateActionDefinition`、`UTcsShootProjectileActionDefinition`、`UTcsPlayCueActionDefinition` 等。

每个 Action Definition 暴露自己的参数 Schema，供触发器/事件规则层引用。

### 2. 评估"ActionRegistry"运行时账本

借鉴 SkillModifier 运行时账本（`changes/add-skill-modifier-runtime-management`）的设计思路，为 Action 引入类似的注册 / 索引 / 查询层：

- 按 ActionKey 索引。
- 按 SourceHandle 索引（哪个效果施加了哪个 Action）。
- 按 Target 索引。

### 3. 分阶段推进

- **阶段 A**：先评估 Action Schema 是否值得作为 `UTcsActionDefinition` DataAsset。如果 TCS 的"动作"主要是引擎内 StateTree Task，则 ActionSchema 的增量价值有限。**这可能是一个"不引入"的结论**——需要先做 design 评估。
- **阶段 B**：如果评估为"值得引入"，先在"事件规则层"（借鉴 03）内试点 Action registry，只覆盖 giveDamage / PlayCue 等几个高频 Action。
- **阶段 C**：扩展到 addState / shootProjectile / summon 等更复杂的 Action。

### 4. 与 StateTree Task 的关系

**关键决策点**：TCS 已有 StateTree Task 作为强类型 Action 形式。引入 ActionSchema 是否会与 StateTree Task 重复？

可能结论：
- StateTree Task 是"Skill 内部流程的动作"——属 HOW 维度。
- ActionSchema 是"跨触发器的通用动作"——属 WHAT 维度。
- 两者可以共存：ActionSchema 作为"可复用动作定义"，StateTree Task 内部调用 ActionSchema 执行的单步动作。

但这需要 design 评审明确边界，避免双轨。

## 风险与前置条件

- **风险：中**。引入 ActionSchema 是架构性增量，会改变 TCS 的"动作"表达方式。
- **前置条件**：
  - 事件规则层（借鉴 03）需先评估并落地。没有规则层，ActionSchema 没有宿主。
  - 需先做 design 评估 ActionSchema 与 StateTree Task 的边界，避免双轨。
  - Skill 系统完成度仅 35%（`project.md:215`），是引入 ActionSchema 的时机，但也意味着 Skill 主路径未冻结，要先确认 Skill 主路径是否会大量依赖 ActionSchema。
- **落地形式**：
  1. 先做 design.md 评估 ActionSchema 必要性、与 StateTree Task 的边界。
  2. 若评估为"值得引入"，创建 OpenSpec capability `add-action-schema`。
  3. 先在事件规则层子域试点，不一次性覆盖全局。

## 不建议的做法

- **不要**让 ActionSchema 与 StateTree Task 各跑一套。会和 UE 引擎能力重叠，造成双轨混乱。先做边界明确设计。
- **不要**把 Action Schema 做成"配置字符串脚本"。必须强类型、可反射、可 Blueprint。
- **不要**绕过 TCS 的"跨 Actor 效果施加必须走独立 State"约束。Action 执行施加效果仍要改走独立 State。
- **不要**完全照抄 AbilityKit 的 EffectItem 结构（string Type / string Key）。TCS 应用 FName / GameplayTag 与枚举替换 string，保强类型与引擎 friend。

## 参考

- AbilityKit: `Docs/AbilityKit_vs_GAS_Comparison.md`（EffectItem、ActionRegistry）
- AbilityKit: `README.md:252`（actionschema 模块）
- AbilityKit: `README.md:91-111`（强类型 Action、反向导出、复用价值）
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/def-editor-authoring/spec.md:154-240`（DataTable 同步）
- TCS: `Plugins/TireflyCombatSystem/openspec/specs/state-management/spec.md:204-222`（ApplyFailReason）
- TCS: `Plugins/TireflyCombatSystem/openspec/changes/add-skill-modifier-runtime-management/`（运行时账本设计参考）
- TCS 借鉴 03：`03-Triggering事件规则引擎.md`（事件规则层作为 Action 宿主）