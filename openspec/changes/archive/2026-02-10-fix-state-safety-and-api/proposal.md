# fix-state-safety-and-api

## Summary
修复 State 模块中的安全性问题和 API 完整性问题，包括空指针解引用风险、条件验证逻辑、等级数组参数的使用约定以及状态等级 API 的完整性。

## Why
当前 State 模块存在以下问题：

1. **空指针解引用风险**：`UTcsStateInstance::SetDurationRemaining()` 在 `OwnerStateCmp` 无效时只打印日志但不返回，随后必然解引用空指针导致崩溃
2. **条件验证逻辑不严格**：`CheckStateApplyConditions()` 中，无效的条件配置只记录错误但继续执行，可能导致条件被静默忽略
3. **等级数组参数使用约定不明确**：`StateLevelArray` 和 `InstigatorLevelArray` 直接用 Level 作为数组下标，但未明确说明是否需要 `LevelValues[0]` 占位，容易导致配表错误
4. **状态等级 API 不完整**：`TryApplyStateToTarget()` 固定使用 Level=1，而系统支持按等级取参的 Evaluator，导致蓝图侧无法按等级施加状态

这些问题虽然都是小问题，但为了保证系统的一致性和规范性，需要统一修复。

## Goals
- 修复 `SetDurationRemaining()` 的空指针解引用风险
- 加强 `CheckStateApplyConditions()` 的条件验证逻辑
- 明确等级数组参数的使用约定，并提供 Map 版本的 Evaluator 作为替代方案
- 完善状态等级 API，支持蓝图侧按等级施加状态

## Non-Goals
- 不改变现有的状态系统架构
- 不影响现有的状态合并和优先级逻辑
- 不修改 StateTree 的集成方式

## Proposed Solution

### 1. 修复空指针解引用风险
在 `UTcsStateInstance::SetDurationRemaining()` 中，当 `OwnerStateCmp` 无效时直接返回，避免后续解引用空指针。

### 2. 加强条件验证逻辑
在 `UTcsStateManagerSubsystem::CheckStateApplyConditions()` 中，当激活条件无效时直接返回 `false`，而不是继续执行。

### 3. 明确等级数组参数使用约定
- 在现有的 Array 版 Evaluator 的注释中明确说明：数组下标直接对应等级值，即 `LevelValues[0]` 对应等级 0，`LevelValues[1]` 对应等级 1，以此类推
- 新增 Map 版 Evaluator（`InstigatorLevelMap` 和 `StateLevelMap`），使用 `TMap<int32, float>` 存储等级到参数值的映射，避免数组下标约定的歧义

### 4. 完善状态等级 API
为 `TryApplyStateToTarget()` 添加可选的 `StateLevel` 参数（默认值为 1），支持蓝图侧按等级施加状态。

## Alternatives Considered

### 替代方案 1：只修复空指针问题，不处理其他问题
**优点**：改动最小，风险最低
**缺点**：其他问题仍然存在，系统一致性和规范性得不到保证
**决策**：不采用，因为这些问题都是小问题，一起修复更合理

### 替代方案 2：只提供 Map 版 Evaluator，废弃 Array 版
**优点**：避免数组下标约定的歧义
**缺点**：破坏向后兼容性，现有配置需要迁移
**决策**：不采用，保留 Array 版并明确约定，同时提供 Map 版作为替代方案

### 替代方案 3：为状态等级 API 创建新函数，不修改现有函数
**优点**：不影响现有调用
**缺点**：API 冗余，增加维护成本
**决策**：不采用，使用可选参数既保持兼容性又避免 API 冗余

## Impact

### 向后兼容性
- 修复空指针和条件验证逻辑不影响现有行为（只是修复 bug）
- 等级数组参数的注释更新不影响现有代码
- 新增 Map 版 Evaluator 是新功能，不影响现有代码
- `TryApplyStateToTarget()` 的参数是可选的，默认值为 1，保持向后兼容

### 性能影响
- 空指针检查和条件验证的性能影响可忽略不计
- Map 版 Evaluator 的查询性能略低于 Array 版，但在可接受范围内

### 测试影响
- 需要添加单元测试验证空指针检查
- 需要添加单元测试验证条件验证逻辑
- 需要添加单元测试验证 Map 版 Evaluator
- 需要添加单元测试验证状态等级 API

## Open Questions
无

## Related Changes
无

## Spec Deltas
- `state-safety-fixes` - 修复空指针解引用和条件验证逻辑
- `state-level-parameter-clarity` - 明确等级数组参数使用约定并提供 Map 版 Evaluator
- `state-level-api-completeness` - 完善状态等级 API
