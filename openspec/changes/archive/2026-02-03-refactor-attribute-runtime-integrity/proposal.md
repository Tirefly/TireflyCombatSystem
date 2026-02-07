# Proposal: 属性系统运行时完整性重构

## Why

属性系统存在若干正确性和可维护性问题，影响系统的稳定性和可靠性：

1. **SourceHandle 索引不稳定**: 依赖数组下标导致查询/移除操作可能失败
2. **输入校验不足**: 允许非法输入导致运行时错误难以追踪
3. **静默覆盖风险**: AddAttribute 可能意外覆盖已存在的属性
4. **范围约束失效**: 动态范围约束（如 HP <= MaxHP）在某些情况下不生效

这些问题会导致：
- 运行时正确性风险
- 维护成本高
- 调试困难
- 玩法逻辑错误

## What Changes

本提案将进行以下修改：

1. **SourceHandle 索引重构**
   - 数据结构: 使用稳定 ID 缓存替换数组下标缓存
   - 删除操作: 使用 RemoveAtSwap 实现 O(1) 删除
   - 查询逻辑: 添加自愈能力，自动清理陈旧 ID

2. **严格输入校验**
   - CreateAttributeModifier: 拒绝 SourceName == NAME_None
   - CreateAttributeModifier: 验证 Instigator/Target 实现 ITcsEntityInterface
   - 所有非法输入都输出清晰的错误日志

3. **AddAttribute 防覆盖**
   - AddAttribute: 不再静默覆盖已存在的属性
   - AddAttributeByTag: 返回值明确表达是否真的添加成功

4. **动态范围约束传播**
   - 新增 EnforceAttributeRangeConstraints 函数
   - 支持多跳依赖（如 HP <= MaxHP，MaxHP 依赖 Level）
   - 迭代收敛算法确保约束在任意时刻都正确

详细的规范增量请参见 `specs/` 目录。

## 概述

本提案旨在修复属性系统中的若干正确性和可维护性问题,包括:

1. **SourceHandle 索引重构**: 移除对不稳定数组下标的依赖,使用稳定 ID 缓存
2. **严格输入校验**: 创建 Modifier 时拒绝非法输入
3. **AddAttribute 防覆盖**: 防止静默覆盖已存在的属性
4. **动态范围约束传播**: 确保属性范围约束(如 HP <= MaxHP)在任意时刻都正确

## 动机

### 问题 1: SourceHandle 查询/移除依赖不稳定数组下标

**当前情况**:
- `UTcsAttributeComponent::SourceHandleIdToModifierIndices` 存储 SourceId -> `AttributeModifiers` 数组下标列表
- `AttributeModifiers` 删除元素会导致下标整体左移
- 当前代码通过扫描所有桶来"修正下标",实现脆弱、容易漂移
- `RemoveModifiersBySourceHandle()` 等 API 在缓存漂移时可能出现"假失败"(明明存在但返回 false / 不移除)

**影响**:
- 运行时正确性风险: 移除操作可能失败
- 维护成本高: 需要在多处维护下标修正逻辑
- 性能开销: 每次删除都需要扫描所有桶

### 问题 2: 创建 Modifier 缺少严格校验

**当前情况**:
- `CreateAttributeModifier` 和 `CreateAttributeModifierWithOperands` 对输入参数校验不足
- 允许 `SourceName == NAME_None`
- 允许无效的 Instigator/Target
- 允许非战斗实体的 Actor

**影响**:
- 运行时错误难以追踪
- 数据不一致性
- 调试困难

### 问题 3: AddAttribute 静默覆盖

**当前情况**:
- `AddAttribute` 在属性已存在时会静默覆盖
- `AddAttributeByTag` 返回值语义不清晰

**影响**:
- 意外的数据丢失
- 调用方无法判断是否真的添加成功
- 难以发现配置错误

### 问题 4: 动态范围 Clamp 不传播

**当前情况**:
- `ClampAttributeValueInRange` 解析动态 min/max 时调用 `GetAttributeValue`,只读取已提交的 `CurrentValue`
- 在一次重算过程中,当 MaxHP 在"计算中途"发生变化时,HP 的 Clamp 仍可能读到旧的 MaxHP
- 当 MaxHP 下降(Buff 移除)时,HP 的 BaseValue 可能仍保持 > MaxHP

**影响**:
- 破坏不变量: HP 可能 > MaxHP
- 玩法逻辑错误: 角色可能保持超出上限的生命值
- 难以调试: 问题只在特定时序下出现

## 目标

1. **稳定索引**: 使用稳定 ID 缓存替换数组下标缓存,确保 SourceHandle 查询/移除的正确性
2. **严格校验**: 在创建 Modifier 时拒绝所有非法输入,提前发现错误
3. **防覆盖**: AddAttribute 不再静默覆盖,明确返回值语义
4. **范围约束**: 确保属性范围约束在任意时刻都正确,支持多跳依赖

## 非目标

- State 系统的修改(将在单独的提案中处理)
- 网络同步相关的修改(SourceHandle 网络同步已在其他规范中完成)
- 对象池集成(保持现有实现)

## 影响范围

**修改的文件**:
- `UTcsAttributeComponent` (头文件和实现)
- `UTcsAttributeManagerSubsystem` (头文件和实现)

**影响的系统**:
- 属性系统核心
- SourceHandle 机制
- 属性修改器管理

**向后兼容性**:
- API 签名不变,行为更严格
- 可能暴露之前被静默忽略的错误
- 需要更新测试用例

## 实施策略

采用分阶段实施:

1. **Phase 1**: SourceHandle 索引重构
   - 修改数据结构
   - 更新插入/删除/查询逻辑
   - 添加自愈能力

2. **Phase 2**: 严格校验
   - 在 CreateAttributeModifier 中添加校验
   - 在 CreateAttributeModifierWithOperands 中添加校验

3. **Phase 3**: AddAttribute 防覆盖
   - 修改 AddAttribute 语义
   - 修改 AddAttributes 语义
   - 修改 AddAttributeByTag 返回值语义

4. **Phase 4**: 动态范围约束
   - 设计范围约束传播机制
   - 实现迭代收敛算法
   - 处理事件语义

## 风险评估

**高风险**:
- 范围约束传播可能影响现有玩法平衡(需要仔细测试)

**中风险**:
- 索引重构可能引入新的 bug(需要充分的单元测试)
- 严格校验可能暴露现有代码中的问题(需要修复调用方)

**低风险**:
- AddAttribute 防覆盖(行为更安全,影响范围小)

## 成功标准

1. 所有 SourceHandle 查询/移除操作在任意删除序列下都正确
2. 所有非法输入都被拒绝并输出清晰的错误日志
3. AddAttribute 不再静默覆盖,调用方能正确判断结果
4. 属性范围约束在任意时刻都正确,包括多跳依赖场景
5. 所有现有测试通过
6. 新增测试覆盖所有修复的问题

## 相关文档

- [SourceHandle 使用指南](../../Documents/SourceHandle使用指南.md)
- [TCS 核心修复提案草案](../../Documents/tcs-core-fixes-proposal-draft.zh-CN.md)
- OpenSpec 规范: `source-handle-attribute-integration`
