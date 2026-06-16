# Proposal: 实现 Skill 核心 — Learned / Level / Cooldown / Activation / SkillStateTree

## 概述

State、Buff、Attribute 三个模块已成熟。Skill 模块骨架存在但核心逻辑为空。本提案填补 Skill 的第一阶段：Learned、Level、Cooldown、Activation、SkillStateTree 运行时驱动。

同时修复 `CreateStateInstance` 中参数初始化逻辑重复的问题（`EvaluateAndApplyStateParameters` + inline 代码块做同样的事），引入 `virtual PopulateStateParamInstances`。

## 动机

当前代码中存在完整的 Skill 类型骨架（`UTcsSkillDefinition`、`UTcsSkillInstance`、`UTcsSkillEntry`、`UTcsSkillComponent`、`UTcsSTSchema_Skill`），但 `UTcsSkillComponent` 为空壳，`UTcsSkillEntry` 仅持有一个 Def 引用——无 Learned 管理、无 Level、无 Cooldown、无 Activation 流程。

## 范围

### 包含

1. **Learned**：`UTcsSkillComponent` 管理 learned skill 集合，提供 `LearnSkill` / `ForgetSkill` / `HasSkill`
2. **Level**：`UTcsSkillEntry` 承载技能等级；`UTcsStateInstance::GetLevel()` 加 `virtual`，SkillInstance 覆写
3. **Cooldown**：`UTcsSkillDefinition` 用 `FTcsStateParameter` 配置（支持 LevelArray）；`UTcsSkillEntry` 持有 `FTcsStateParamInstance` 并运用求值-修正公式
4. **Activation**：`ActivateSkill` → Learned → Cooldown → CancelPrev → Create → TryApplyStateInstance → StartCooldown
5. **SkillStateTree**：复用 `UTcsSTSchema_Skill`（双上下文 `SkillInstance + SkillEntry`）
6. **StateParamInstance 重构**：拆分 `BaseValue` + `ModifierScale/Offset`，新增 `GetStateParamInstance` virtual 访问器
7. **`CreateStateInstance` 清理**：合并 `EvaluateAndApplyStateParameters` + inline 代码块 → `virtual PopulateStateParamInstances`；删除旧方法
8. **AttributeModifierInstance 引用优化**：新增 `SourceStateInstance` + `SourceSkillEntry` 字段；`ResolveStateInstanceFromModifier` 改为 `ResolveStateParamInstances`（三层回退）

### 不包含（独立后续提案）

- SkillModifier（修改其他技能的 StateParam / Cooldown 值）
- Cost 系统（魔法/能量消耗）
- Learned-skill 的存档/序列化

## 受影响 Spec

本提案的实现完成后，需确保以下 spec 反映变更：

| Spec | 需更新的内容 |
|------|-----------|
| `attribute-modifier-runtime` | `ResolveStateInstanceFromModifier` → `ResolveStateParamInstances`；新增 `SourceStateInstance`/`SourceSkillEntry` 字段 |
| `state-parameter-management` | `FTcsStateParamInstance` 新增 `BaseValue`/`ModifierScale`/`ModifierOffset`；`GetNumeric()` 公式 |
| `state-runtime-access` | `PopulateStateParamInstances` virtual 方法；`GetLevel()` virtual；`GetStateParamInstance()` virtual |

## 架构边界

```
UTcsSkillComponent (UActorComponent)
  └── LearnedSkills: TMap<FName, UTcsSkillEntry*>
        └── UTcsSkillEntry (UObject)  ← 权威持有者
              ├── SkillDefinition
              ├── Level
              ├── RemainingCooldown
              ├── StateParamInstances ← 完整集合，与 StateInstance 同构
              │     ├── DamageFactor  → BaseValue=1.55, ModifierScale=1.0
              │     ├── Cooldown       → BaseValue=10s,  ModifierScale=0.5 (被 SkillModifier 修正)
              │     └── Range          → BaseValue=5m,   ModifierScale=1.0
              └── ActiveInstance

UTcsSkillDefinition (UTcsStateDefinition)
  ├── SkillInstanceClass  /  SkillEntryClass
  ├── CooldownParam  (FTcsStateParameter, Numeric, LevelArray...)
  └── StateTreeRef    (继承自 UTcsStateDefinition)

UTcsSkillInstance (UTcsStateInstance)
  ├── SkillEntry → 反向引用
  ├── GetStateParamInstance(Tag) → Entry->StateParamInstances.Find(Tag)  [override]
  ├── PopulateStateParamInstances → 空  [override]
  └── GetLevel() → Entry->GetLevel()  [override]
```

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| StateParamInstances 权威源 | SkillEntry 持有，Instance 通过 virtual 访问器读取 | SkillModifier 写入即时对存活 Instance 可见；无需同步 |
| Cooldown 配置 | `FTcsStateParameter`（Numeric, LevelArray 等） | 支持等级/状态级别动态 CD |
| Cooldown 求值时机 | `StartCooldown` 时 Evaluate | 确保读到最新 Level + SkillModifier 修正 |
| 求值-修正分离 | `BaseValue`（求值器）+ `ModifierScale/Offset`（SkillModifier） | Evaluate 不覆盖外部修正 |
| 同一技能多实例 | 单实例，新激活取消前一个 | 无真实场景需要同技能多实例并行 |
