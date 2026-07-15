# 借鉴 04：多源数值引用 NumericValueRef

## 概述

AbilityKit 在 `com.abilitykit.modifiers` 模块引入 `NumericValueRef`，把一个数值的"来源"显式分为五种：**Const / Blackboard / PayloadField / Var / Expr**。这让 Buff、装备、天赋、场景状态可以动态改写技能、投射物、召唤物参数，避免为每个来源复制大量相似配置。

TCS 当前 SkillModifier 的 Evaluator 仅按 TargetParamType（Numeric / Bool / Vector）分类，偏固定。借鉴"数值来源引用"思想能让 TCS 的参数更动态、更可组合。

## AbilityKit 设计思想与架构实践

### 数值来源分类

AbilityKit 定义 `ENumericValueRefKind`（`Docs/AbilityKit_vs_GAS_Comparison.md:368-407`）：

```csharp
public enum ENumericValueRefKind : byte
{
    Const = 0,        // 固定常量
    Blackboard = 1,   // 黑板变量（通用数据接口）
    PayloadField = 2, // 上下文字段（事件载荷内字段）
    Var = 3,          // 变量域（命名变量域）
    Expr = 4,         // RPN 表达式
}
```

对应结构 `NumericValueRef` 承载一组可选字段，运行时按 Kind 解析：

```csharp
public readonly struct NumericValueRef
{
    public ENumericValueRefKind Kind;
    public double ConstValue;     // Const 用
    public int BoardId;           // Blackboard 用
    public int KeyId;              // Blackboard 用
    public string DomainId;        // Var 用
    public string Key;             // Var 用
    public string ExprText;        // Expr 用
}
```

### 与 GAS 对比

AbilityKit 把它对标 GAS 的 `SetByCaller`（`AbilityKit_vs_GAS_Comparison.md:390-407`）：

| 来源 | GAS | AbilityKit |
|------|-----|-----------|
| 常量 | SetByCaller | Const |
| 属性引用 | 直接属性 | Blackboard |
| 上下文参数 | SetByCaller | PayloadField |
| 表达式 | Execution 内部 | 内置 RPN Expr |
| 作用域 | 无 | boardId 隔离 |

### 设计要点

- **作用域隔离**：Blackboard 用 `boardId` 隔离作用域，让同一 Key 名在不同作用域解析为不同值。
- **表达式内建**：`Expr` 内置 RPN 表达式支持，不需要落到 ExecutionCalculation 内部硬编码。
- **统一接口**：不论来源如何，对外统一为 `NumericValueRef`，由解析层处理差异。

### 运行时性能

AbilityKit 强调"零 GC"与"缓存优化"（`Docs/AbilityKit_vs_GAS_Comparison.md:519-532`）。`ModifierCalculator` 使用 `ReadOnlySpan<ModifierData>` 单遍遍历，并通过 `_lastCount + _lastHash + _lastBaseValue` 缓存避免重复计算。

## 与 TCS 现状的对比

### TCS 当前 SkillModifier

TCS 通过活动 change `add-def-strategy-defaults-and-validation` 把 `UTcsSkillModifierDefinition` 的 `EvaluatorClass` 扩展为按 `TargetParamType` 暴露的 Numeric / Bool / Vector 三类字段，默认归一化为 `Addition / SetBool / SetVector`。

证据：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\openspec\changes\add-def-strategy-defaults-and-validation\specs\skill-runtime\spec.md:1-24`。

SkillModifier 运行时账本（活动 change `add-skill-modifier-runtime-management`）：

- `UTcsSkillComponent` 作为唯一权威账本宿主，承载 `FTcsSkillModifierRuntimeEntry` 与 `FTcsSkillModifierRuntimeIndex`（4 组索引：ById / by SourceHandleId / by TargetEntry / by ConflictKey）。
- Snapshot 只冻结 Evaluator 重求值，不冻结 Modifier 链；来源存活期间写入对全部读取者共享可见。

证据：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\Source\TireflyCombatSystem\Public\Skill\TcsSkillModifierRuntime.h:24-224`；`changes/add-skill-modifier-runtime-management/specs/skill-runtime/spec.md:51-65`。

### SkillEntry 三容器

`UTcsSkillEntry` 持有 `NumericParamInstances / BoolParamInstances / VectorParamInstances` 三容器；`UTcsSkillInstance` 通过覆写 Get 访问器透传到 Entry（共享同一条 SkillModifier 链）。

证据：`Public/Skill/TcsSkillEntry.h:113-153`；`Public/Skill/TcsSkillInstance.h:62-90`。

### 差异点

- **数值来源**：TCS 的 SkillModifier Evaluator 偏"按目标类型分类的执行函数"，缺少"来源分类"维度。
- **动态引用**：TCS 没有"引用黑板变量""引用上下文 Payload 字段""引用命名变量域"的能力，所有数值要么是 SkillDef 固定字段，要么是 SkillModifier 直接写入。
- **表达式**：TCS 没有内置表达式引擎，复杂伤害公式依赖 CDO 策略类（`UTcsAttributeModifierExecution`）实现。
- **作用域**：TCS 没有"作用域隔离"概念，参数共享同一 SkillEntry 容器。

### 相似点

- TCS 的 SkillEntry 三容器与 AbilityKit 的"按类型分类"在精神上类似。
- TCS 的 CDO 策略模式（`UTcsSkillModifierDefinition`）与 AbilityKit 的 `ModifierData + ModifierCalculator` 都是"策略可替换"的体现。

## 借鉴建议

### 1. 引入"TcsParamValueRef"作为 SkillModifier Evaluator 的来源扩展

在 `UTcsSkillModifierDefinition` 层评估引入"TcsParamValueRef"字段，把单一 Evaluator 拆成"来源 + 操作"二维：

```cpp
// 仅作设计示意
enum class ETcsParamValueRefKind : uint8
{
    Const,            // 固定常量（来自 DefAsset）
    AttributeRef,     // 引用目标/自身属性
    ContextPayload,   // 引用上下文 Payload 字段（事件载荷）
    NamedVar,         // 引用命名变量域（SkillEntry 容器内的具名参数）
    Expr,             // 表达式（可能托管到 C# 侧）
};
```

每个 SkillModifier 可声明其 Value 来自哪里，运行时按 Kind 解析。

### 2. 分阶段推进

- **阶段 A**：引入 Const + AttributeRef 两类。Const 来源于 DefAsset 字段，AttributeRef 引用目标/自身属性（TCS 已有 `TcsAttributeComponent`，可对外提供 GetValue API）。
- **阶段 B**：引入 ContextPayload，配合借鉴 03 的"事件规则层"，让 Action 携带 Payload 被 SkillModifier 引用。
- **阶段 C**：引入 NamedVar，引用 SkillEntry 容器内具名参数（SkillEntry 三容器已具名）。
- **阶段 D**：引入 Expr，可考虑托管到 UnrealSharp C# 侧实现表达式解析。

### 3. 评估 UE 原生表达式方案

TCS 的 Expr 来源不必沿用 AbilityKit 的 RPN。可选：
- 引擎的 `FText::Format` / `FString::Printf` 不够。
- 可考虑托管到 C# 侧表达树（UnrealSharp 已 vendored）。
- 或引入轻量 C++ 表达式引擎（如 tinyexpr 起步）。
- 表达式解析必须可序列化、可 Blueprint 可见。

### 4. 数值缓存方案

TCS 已有 Snapshot 机制（`changes/add-skill-modifier-runtime-management`）。借鉴 AbilityKit 的"数量+哈希+基值"三元组缓存，可减少 Evaluator 重求值开销。需要设计"何时算脏"的策略，避免缓存失效误判。

## 风险与前置条件

- **风险：中**。数值来源扩展是 SkillModifier 架构性增量，会影响 Modifier 的运行时账本与 Snapshot 语义。
- **前置条件**：
  - 活动 change `add-skill-modifier-runtime-management` 的关键场景运行时验证测试未完成（`tasks.md:31`）。建议先完成该验证，确保账本稳定后再扩展来源。
  - 活动 change `add-def-strategy-defaults-and-validation` 的编译与 authoring 流程验证未完成（`tasks.md:21-22`）。建议先完成，再叠加来源分类。
- **落地形式**：
  1. 先完成现有两个活动 change 的验证。
  2. 创建 OpenSpec capability `add-skillmodifier-value-ref`，设计来源分类与解析接口。
  3. 分阶段落地（A→D），每阶段评审。

## 不建议的做法

- **不要**把 Blackboard 概念原样照搬。TCS 已有 `StateComponent` / `AttributeComponent`，应复用这些原生结构，不要为 NumericValueRef 新建一套"黑板"抽象。
- **不要**让 Expr 来源承担业务逻辑。数值计算应保持"纯函数式、可序列化、可缓存"，业务逻辑留在 Action / 策略类。
- **不要**破坏 SkillEntry 三容器作为唯一权威容器的地位。NamedVar 引用应指向 SkillEntry 容器，而不是另起容器。

## 参考

- AbilityKit: `Docs/AbilityKit_vs_GAS_Comparison.md`（NumericValueRef 模块、ModifierCalculator）
- AbilityKit: `README.md:225`（modifiers 模块说明）
- TCS: `Plugins/TireflyCombatSystem/Source/.../Public/Skill/TcsSkillModifierRuntime.h:24-224`
- TCS: `Plugins/TireflyCombatSystem/Source/.../Public/Skill/TcsSkillEntry.h:113-153`
- TCS: `Plugins/TireflyCombatSystem/Source/.../Public/Skill/TcsSkillInstance.h:62-90`
- TCS: `Plugins/TireflyCombatSystem/openspec/changes/add-skill-modifier-runtime-management/specs/skill-runtime/spec.md`
- TCS: `Plugins/TireflyCombatSystem/openspec/changes/add-def-strategy-defaults-and-validation/specs/skill-runtime/spec.md`
- TCS 借鉴 03：`03-Triggering事件规则引擎.md`（ContextPayload 配合的事件规则层）