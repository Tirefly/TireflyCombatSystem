# attribute-modifier-merge-by-id Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
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

