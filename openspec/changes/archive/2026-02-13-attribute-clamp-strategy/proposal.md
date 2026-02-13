# Proposal: 属性 Clamp 功能策略模式化

## Problem

当前 TCS 插件的属性 Clamp 功能使用硬编码的 `FMath::Clamp(NewValue, MinValue, MaxValue)` 实现线性约束，虽然能满足大多数基础需求，但存在以下局限性：

1. **扩展性不足**：只支持简单的 min/max 线性 Clamp，无法支持其他约束模式：
   - 循环 Clamp（如角度 0-360°，超过 360 回到 0）
   - 软 Clamp（超出范围时衰减而非硬截断）
   - 阶梯式 Clamp（离散值，如等级只能是整数）
   - 非对称 Clamp（不同的正负值范围）
   - 条件 Clamp（根据游戏状态动态改变约束规则）

2. **设计不一致**：属性修改器合并器已经使用了完整的策略模式（`UTcsAttributeModifierMerger` 及其子类），而 Clamp 功能却是硬编码的，缺乏架构一致性。

3. **通用性受限**：作为通用框架插件，当前实现限制了不同项目对属性约束的自定义需求。

## Solution

将属性 Clamp 功能完全策略模式化，参考属性修改器合并器的设计：

1. **创建抽象基类** `UTcsAttributeClampStrategy`，定义 Clamp 接口
2. **实现默认策略** `UTcsAttrClampStrategy_Linear`，保持当前的线性 Clamp 行为（向后兼容）
3. **在 `FTcsAttributeDefinition` 中添加字段** `ClampStrategyClass`，允许每个属性指定自己的 Clamp 策略
4. **在 `FTcsAttributeDefinition` 构造函数中设置默认值**（在 .cpp 文件中实现，避免头文件引用），默认使用 `UTcsAttrClampStrategy_Linear`
5. **完全移除 `ClampAttributeValueInRange` 函数中的硬编码逻辑**，统一使用策略对象执行 Clamp
6. **提供额外的内置策略**（可选），如循环 Clamp、阶梯 Clamp 等

## Benefits

1. **架构一致性**：与属性修改器合并器的策略模式设计保持一致，完全策略化
2. **高扩展性**：开发者可以通过继承 `UTcsAttributeClampStrategy` 实现自定义 Clamp 逻辑
3. **向后兼容**：通过构造函数默认值自动使用线性 Clamp 策略，现有项目无需修改
4. **代码简洁**：移除硬编码分支逻辑，统一使用策略对象
5. **蓝图友好**：支持蓝图继承和实现自定义策略
6. **避免头文件污染**：默认值设置在 .cpp 文件中，头文件无需引用策略类

## Scope

### In Scope
- 创建 `UTcsAttributeClampStrategy` 抽象基类
- 实现 `UTcsAttrClampStrategy_Linear` 默认策略
- 在 `FTcsAttributeDefinition` 中添加 `ClampStrategyClass` 字段
- 在 `FTcsAttributeDefinition` 构造函数中设置默认值为 `UTcsAttrClampStrategy_Linear::StaticClass()`（在 .cpp 文件中）
- 完全移除 `ClampAttributeValueInRange` 函数中的硬编码 Clamp 逻辑，统一使用策略对象
- 更新相关文档和注释

### Out of Scope
- 实现额外的内置策略（如循环 Clamp、软 Clamp 等）- 可在后续迭代中添加
- 修改现有属性定义数据表 - 保持向后兼容，使用默认策略
- 性能优化 - 当前阶段专注于功能实现

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| 性能开销（策略对象创建和调用） | 中 | 使用 CDO（Class Default Object）避免重复创建；所有属性统一使用策略对象，性能一致 |
| 向后兼容性问题 | 高 | 通过构造函数默认值自动设置为 `UTcsAttrClampStrategy_Linear`，现有数据表加载时自动应用默认策略 |
| 策略接口设计不当导致后续难以扩展 | 中 | 参考成熟的合并器设计；接口保持简单，只传递必要参数 |
| 头文件循环引用 | 低 | 默认值设置在 .cpp 文件的构造函数中，头文件使用前置声明 |

## Alternatives Considered

### 1. 保持现状
- **优点**：简单，无开发成本
- **缺点**：无法满足复杂项目需求，设计不一致

### 2. 使用函数指针/委托
- **优点**：轻量级
- **缺点**：不支持蓝图，难以序列化和配置

### 3. 使用模板特化
- **优点**：编译期优化，零运行时开销
- **缺点**：不支持蓝图，不支持数据驱动配置

## Dependencies

- 依赖现有的 `FTcsAttributeDefinition` 和 `FTcsAttributeRange` 结构
- 依赖 `ClampAttributeValueInRange` 函数的现有实现
- 参考 `UTcsAttributeModifierMerger` 的策略模式设计

## Success Criteria

1. ✅ 创建了 `UTcsAttributeClampStrategy` 抽象基类，支持蓝图继承
2. ✅ 实现了 `UTcsAttrClampStrategy_Linear` 默认策略，行为与当前实现完全一致
3. ✅ `FTcsAttributeDefinition` 包含 `ClampStrategyClass` 字段
4. ✅ `ClampAttributeValueInRange` 函数正确使用策略对象
5. ✅ 现有项目无需修改即可正常运行（向后兼容）
6. ✅ 开发者可以通过继承 `UTcsAttributeClampStrategy` 实现自定义 Clamp 逻辑
7. ✅ 代码通过编译，无警告和错误
