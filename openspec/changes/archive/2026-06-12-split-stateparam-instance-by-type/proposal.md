# Proposal: StateParamInstance 按类型拆分

## Why

当前 `FTcsStateParamInstance` 用一个结构体承载三种类型的值，任一实例只有一种类型有效。`GetStateParamInstance()` 返回统一指针，`ResolveStateParamInstances()` 返回混合容器——调用方必须通过 `ParamType` 或 `CachedEvaluator != nullptr` 自行判断类型，容易出错且语义不干净。

## What Changes

- `FTcsStateParamInstance` → `FTcsNumericStateParamInstance` / `FTcsBoolStateParamInstance` / `FTcsVectorStateParamInstance` 三个独立 USTRUCT
- `StateParamInstances` TMap → `NumericParamInstances` / `BoolParamInstances` / `VectorParamInstances` 三个容器
- `ResolveStateParamInstances` → `ResolveNumericParamInstances`，仅返回 Numeric 容器
- `UTcsSkillInstance` 全系列覆写指向 `SkillEntry` 对应容器

## 概述

将 `FTcsStateParamInstance` 按 `ETcsStateParameterType` 拆分为三个独立结构体，对应三个独立容器——消除当前统一结构体中 `NumericValue / BoolValue / VectorValue` 三种字段共存的语义混乱。

## 范围

### 包含

1. `FTcsStateParamInstance` 拆为 `FTcsNumericStateParamInstance` / `FTcsBoolStateParamInstance` / `FTcsVectorStateParamInstance`
2. `UTcsStateInstance` 上 `TMap<FGameplayTag, FTcsStateParamInstance>` 拆为三个独立 TMap
3. `UTcsSkillEntry` 上同构拆分
4. `PopulateStateParamInstances`、`GetStateParamInstance`、`ResolveStateParamInstances` 全部适配
5. 所有调用方迁移（`RecalculateAttributeCurrentValues`、`SetNumericParamByTag` 系列、StateTree Task 等）

### 不包含

- SkillModifier 任何内容
- Level/Cost 迁移

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 拆分粒度 | 三个独立 USTRUCT + 三个独立 TMap | 编译期类型安全，调用方不需求值运行时判别 |
| `GetStateParamInstance` 返回类型 | 三个具体方法：`GetNumericParamInstance` / `GetBoolParamInstance` / `GetVectorParamInstance` | 避免 variant/union，零 Cast |
| `ResolveStateParamInstances` 收窄 | 只返回 Numeric TMap*，函数名收窄 | 实际使用仅 Numeric 路径 |
| SkillEntry 同步 | 完全同构拆分 | 与 StateInstance 保持一致 |

## 受影响 Spec

- `state-parameter-management`
- `attribute-modifier-runtime`
- `state-runtime-access`
- `skill-runtime`
