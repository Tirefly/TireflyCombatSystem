# Spec: 属性修改器按ModifierId分组合并

## ADDED Requirements

### Requirement: 属性修改器合并分组键

属性修改器的合并逻辑 MUST 按ModifierId（DataTable RowName）分组，而不是按ModifierName分组。

#### Scenario: 不同ModifierId但相同ModifierName的修改器不应被合并

**Given**:
- DataTable中存在两个修改器定义：
  - RowName: `Mod_Damage_Fire`, ModifierName: `DamageBoost`
  - RowName: `Mod_Damage_Ice`, ModifierName: `DamageBoost`
- 两个修改器都应用到同一个Actor

**When**: 执行属性修改器合并

**Then**:
- 两个修改器应该被视为不同的修改器
- 不应该被合并到一起
- 每个修改器独立生效

#### Scenario: 相同ModifierId的修改器应该被合并

**Given**:
- DataTable中存在一个修改器定义：RowName: `Mod_Damage_Fire`
- 同一个修改器被应用两次到同一个Actor（不同SourceHandle）

**When**: 执行属性修改器合并

**Then**:
- 两个修改器实例应该按照合并策略进行合并
- 合并结果取决于修改器定义的MergerType

#### Scenario: ModifierId追踪

**Given**: 创建属性修改器实例

**When**: 从DataTable读取修改器定义

**Then**:
- 修改器实例 MUST 记录原始的ModifierId（DataTable RowName）
- ModifierId MUST 在整个生命周期中保持不变
- ModifierId用于合并分组

### Requirement: 合并分组实现

合并逻辑 MUST 使用ModifierId作为分组键。

#### Scenario: 合并分组逻辑

**Given**: 一组待合并的属性修改器实例

**When**: 执行 `MergeAttributeModifiers`

**Then**:
- 按ModifierId分组修改器
- 每个ModifierId组独立执行合并策略
- 合并结果包含所有组的合并后修改器

## ADDED Requirements

### Requirement: ModifierId字段

`FTcsAttributeModifierInstance` MUST 包含ModifierId字段。

#### Scenario: ModifierId字段存在

**Given**: `FTcsAttributeModifierInstance` 结构体

**When**: 检查结构体定义

**Then**:
-  MUST 包含 `FName ModifierId` 字段
- 字段 MUST 是BlueprintReadOnly
- 字段记录DataTable的RowName

#### Scenario: ModifierId初始化

**Given**: 创建属性修改器实例

**When**: 调用 `CreateAttributeModifierInstance`

**Then**:
- ModifierId MUST 被设置为DataTable的RowName
- ModifierId不能为空
- ModifierId MUST 与ModifierDef对应

## Implementation Notes

### 数据结构变更

```cpp
// FTcsAttributeModifierInstance
UPROPERTY(BlueprintReadOnly)
FName ModifierId = NAME_None;  // DataTable RowName
```

### 合并逻辑变更

```cpp
// TcsAttributeManagerSubsystem::MergeAttributeModifiers
// 旧代码：
// ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);

// 新代码：
ModifiersToMerge.FindOrAdd(Modifier.ModifierId).Add(Modifier);
```

### 向后兼容性

- ModifierName字段保留，用于显示和日志
- 现有代码中使用ModifierName的地方需要评估是否需要改为ModifierId
- 如果现有配置依赖ModifierName分组，需要迁移

## Related Specs

- `attribute-tags`: 属性标签系统
- `source-handle-attribute-integration`: SourceHandle与属性集成
