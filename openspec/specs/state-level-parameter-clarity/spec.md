# state-level-parameter-clarity Specification

## Purpose
TBD - created by archiving change fix-state-safety-and-api. Update Purpose after archive.
## Requirements
### Requirement: 等级数组参数使用约定文档化

系统 MUST 在等级数组参数相关的结构体和 Evaluator 中明确说明数组下标约定：数组下标直接对应等级值，即 `LevelValues[0]` 对应等级 0，`LevelValues[1]` 对应等级 1，以此类推。

#### Scenario: StateLevelArray 结构体注释明确约定

**Given** `FTcsStateNumericParam_StateLevelArray` 结构体定义
**When** 查看 `LevelValues` 成员变量的注释
**Then** 注释明确说明："参数值数组，索引对应状态等级（从0开始），即 LevelValues[0] 对应等级 0，LevelValues[1] 对应等级 1，以此类推"

#### Scenario: InstigatorLevelArray 结构体注释明确约定

**Given** `FTcsStateNumericParam_InstigatorLevelArray` 结构体定义
**When** 查看 `LevelValues` 成员变量的注释
**Then** 注释明确说明："参数值数组，索引对应施法者等级（从0开始），即 LevelValues[0] 对应等级 0，LevelValues[1] 对应等级 1，以此类推"

#### Scenario: StateLevelArray Evaluator 注释明确约定

**Given** `UTcsStateNumericParamEvaluator_StateLevelArray::Evaluate_Implementation()` 函数实现
**When** 查看函数注释或实现中的关键逻辑注释
**Then** 注释明确说明数组下标直接对应等级值的约定

#### Scenario: InstigatorLevelArray Evaluator 注释明确约定

**Given** `UTcsStateNumericParamEvaluator_InstigatorLevelArray::Evaluate_Implementation()` 函数实现
**When** 查看函数注释或实现中的关键逻辑注释
**Then** 注释明确说明数组下标直接对应等级值的约定

---

### Requirement: 提供 Map 版 StateLevelMap Evaluator

系统 MUST 提供基于 `TMap<int32, float>` 的状态等级参数 Evaluator，允许用户显式指定等级到参数值的映射，避免数组下标约定的歧义。

#### Scenario: 创建 StateLevelMap 参数

**Given** 用户需要配置状态等级参数
**When** 用户选择 `FTcsStateNumericParam_StateLevelMap` 类型
**Then** 用户可以使用 `TMap<int32, float>` 显式指定等级到参数值的映射（例如 {1: 10.0, 5: 50.0, 10: 100.0}）

#### Scenario: StateLevelMap Evaluator 正确解析参数

**Given** StateInstance 的等级为 5
**And** `FTcsStateNumericParam_StateLevelMap` 配置为 `{{1, 10.0}, {5, 50.0}, {10, 100.0}}`，默认值为 0.0
**When** 调用 `UTcsStateNumericParamEvaluator_StateLevelMap::Evaluate_Implementation()`
**Then** 返回 `true`，`OutValue` 为 50.0

#### Scenario: StateLevelMap Evaluator 使用默认值

**Given** StateInstance 的等级为 3
**And** `FTcsStateNumericParam_StateLevelMap` 配置为 `{{1, 10.0}, {5, 50.0}, {10, 100.0}}`，默认值为 5.0
**When** 调用 `UTcsStateNumericParamEvaluator_StateLevelMap::Evaluate_Implementation()`
**Then** 返回 `true`，`OutValue` 为 5.0（使用默认值）

---

### Requirement: 提供 Map 版 InstigatorLevelMap Evaluator

系统 MUST 提供基于 `TMap<int32, float>` 的施法者等级参数 Evaluator，允许用户显式指定等级到参数值的映射，避免数组下标约定的歧义。

#### Scenario: 创建 InstigatorLevelMap 参数

**Given** 用户需要配置施法者等级参数
**When** 用户选择 `FTcsStateNumericParam_InstigatorLevelMap` 类型
**Then** 用户可以使用 `TMap<int32, float>` 显式指定等级到参数值的映射（例如 {1: 10.0, 5: 50.0, 10: 100.0}）

#### Scenario: InstigatorLevelMap Evaluator 正确解析参数

**Given** Instigator 实现 `ITcsEntityInterface` 接口
**And** Instigator 的等级为 5
**And** `FTcsStateNumericParam_InstigatorLevelMap` 配置为 `{{1, 10.0}, {5, 50.0}, {10, 100.0}}`，默认值为 0.0
**When** 调用 `UTcsStateNumericParamEvaluator_InstigatorLevelMap::Evaluate_Implementation()`
**Then** 返回 `true`，`OutValue` 为 50.0

#### Scenario: InstigatorLevelMap Evaluator 使用默认值

**Given** Instigator 实现 `ITcsEntityInterface` 接口
**And** Instigator 的等级为 3
**And** `FTcsStateNumericParam_InstigatorLevelMap` 配置为 `{{1, 10.0}, {5, 50.0}, {10, 100.0}}`，默认值为 5.0
**When** 调用 `UTcsStateNumericParamEvaluator_InstigatorLevelMap::Evaluate_Implementation()`
**Then** 返回 `true`，`OutValue` 为 5.0（使用默认值）

#### Scenario: InstigatorLevelMap Evaluator 处理无效 Instigator

**Given** Instigator 为 `nullptr` 或未实现 `ITcsEntityInterface` 接口
**And** `FTcsStateNumericParam_InstigatorLevelMap` 配置了默认值为 5.0
**When** 调用 `UTcsStateNumericParamEvaluator_InstigatorLevelMap::Evaluate_Implementation()`
**Then** 返回 `true`，`OutValue` 为 5.0（使用默认值）

---

### Requirement: Map 版 Evaluator 遵循项目代码规范

系统 MUST 确保新增的 Map 版 Evaluator 遵循项目的代码规范，包括命名、注释、空行、文件头部等。

#### Scenario: 文件头部符合规范

**Given** 新增的 Map 版 Evaluator 头文件和源文件
**When** 查看文件头部
**Then** 文件头部包含 Copyright 声明、`#pragma once`、必要的 includes，并符合空行规范

#### Scenario: 类和结构体命名符合规范

**Given** 新增的 Map 版 Evaluator 类和结构体
**When** 查看类和结构体命名
**Then** 类名使用 `UTcs*` 前缀，结构体名使用 `FTcs*` 前缀

#### Scenario: 注释符合规范

**Given** 新增的 Map 版 Evaluator 类和结构体
**When** 查看成员变量和成员函数的注释
**Then** 所有成员变量和成员函数都有中文注释，说明其用途

---

