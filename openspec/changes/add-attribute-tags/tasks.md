# Tasks: Add Attribute GameplayTag Tagging

## 1. Data Model

- [x] 1.1 在 `Public/Attribute/TcsAttribute.h` 的 `FTcsAttributeDefinition` 中新增 `FGameplayTag AttributeTag`（可选字段）
- [ ] 1.2 视需要在属性定义中新增 `FGameplayTagContainer AttributeTags`（仅当确实需要"一个属性多标签"时）
- [x] 1.3 确认编辑器侧 Tag 选择体验（必要时添加 `meta=(Categories="TCS.Attribute")` 之类过滤）

## 2. Runtime Index + Validation

- [x] 2.1 在 `UTcsAttributeManagerSubsystem`（或更合适的持有者）中新增 `AttributeTagToName` 映射
- [x] 2.2 在初始化阶段构建映射（遍历 AttributeDefTable 的所有行）
- [x] 2.3 增加校验与日志：
  - 重复 Tag：Error + 明确保留策略
  - 空/无效 Tag：Warning + 不入表

## 3. Public API (Tag Entry Points)

- [x] 3.1 新增 `TryResolveAttributeNameByTag()`（BlueprintCallable + C++）
- [x] 3.2 （可选）新增 `TryGetAttributeTagByName()` 用于反查/调试
- [x] 3.3 （可选）为常用行为补齐 Tag 版本重载：AddAttribute / GetAttributeValue / SetAttributeValue 等（内部统一转换为 FName）

## 4. Rule Nodes / Conditions (Optional but Recommended)

- [ ] 4.1 为 AttributeComparison 这类条件提供 Tag 版本入口（新增 Payload 或在原 Payload 中增加 Tag 字段并规定优先级）
- [ ] 4.2 补齐蓝图节点 ToolTip/注释，说明 Name 与 Tag 的定位与推荐用法

## 5. Tests

- [ ] 5.1 添加 AutomationTest：索引构建与校验（正常/重复/空/无效）
- [ ] 5.2 添加 AutomationTest：Tag 解析 API（成功/失败）
- [ ] 5.3 （如做了条件节点）添加 AutomationTest：Tag 版本 AttributeComparison 行为正确

## 6. Documentation

- [ ] 6.1 补充使用示例：如何为 AttributeDef 行补齐 Tag、推荐命名（`TCS.Attribute.<RowName>`）
- [ ] 6.2 迁移建议：新逻辑优先用 Tag，旧蓝图按需迁移

