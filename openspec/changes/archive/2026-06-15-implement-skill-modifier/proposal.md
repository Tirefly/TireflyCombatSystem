# Proposal: 实现 SkillModifier 机制

## Why

当前 TCS 中，AttributeModifier 可以修改目标的 Attribute 值（伤害、生命等），但没有机制让"一个技能/天赋/装备修改另一个技能的 StateParam 运行时值"。典型需求：

- 装备"冷却戒指"：技能 A 的冷却时间 × 0.5
- 天赋"强化"：技能 B 的伤害系数 + 20%
- Buff 期间：技能 C 的等级 + 1

目前 `SkillEntry` 上虽然已有 `NumericParamInstances`，但没有 Modifier 挂载点和链式求值管线。Level 还是独立的 `int32` 字段，无法被修改。

## What Changes

| 类别 | 内容 |
|------|------|
| **新建类型** | `ETcsSkillModifierMergePolicy`、`FStateParamModifierInstance`、`UTcsStateParamNumericModifierExecution`、`UTcsSkillEntrySelector`、`UTcsSkillModifierDefinition`（DefAsset） |
| **NumericInstance 扩展** | `ModifierInstances` 列表 + `DeriveModifiedValue()` |
| **Level 迁移** | `SkillEntry.Level`（int32）→ `NumericParamInstances[LevelTag]` |
| **StartCooldown 适配** | 使用 `DeriveModifiedValue()` 获取含修正的 CD 值 |
| **内建子类** | EntrySelector × 3、Evaluator × 6（Add/MultiplyAdditive/MultiplyContinued/Override/SetBool/SetVector） |
| **Spec 更新** | `skill-runtime` 新增 Requirement；`state-parameter-management` MODIFIED |

### 不包含

- Cost 系统
- Bool/Vector SkillModifier
- SkillModifier 的 Apply/Remove 入口（由 Buff/装备系统自行调用 `Assign`/`Remove`）
- 存档序列化

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| ModifierInstance 挂载位置 | `FTcsNumericStateParamInstance.ModifierInstances` | 局部性：求值所需一切在同一结构体内 |
| Level 迁移方案 | LevelParamTag 放 `UTcsStateDefinition` 基类；Level 运行时常量注入 | 所有 State 类型共用，无需 DefAsset 配置 Evaluator |
| EntrySelector 模式 | CDO 策略，对标 AttributeModifierExecution | 扩展点统一 |
| Evaluator 模式 | CDO 策略 + 类型基类筛选 | 对标 AttributeModifierExecution，Bool/Numeric/Vector 各有一个抽象基类 |
| 互斥键 | `ModifierId` | 两件装备引用同一 ModifierDef 自然互斥 |

## 受影响 Spec

| Spec | 变更 |
|------|------|
| `skill-runtime` | 新增 SkillModifier 相关 Requirement |
| `state-parameter-management` | MODIFIED：NumericInstance 新增 ModifierInstances + DeriveModifiedValue |
