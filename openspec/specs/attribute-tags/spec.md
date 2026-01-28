# attribute-tags Specification

## Purpose
TBD - created by archiving change add-attribute-tags. Update Purpose after archive.
## Requirements
### Requirement: AttributeDefinition 支持可选 AttributeTag

系统 MUST 允许在 AttributeDef DataTable 中为每个 Attribute 行配置一个可选的 `FGameplayTag AttributeTag`，用于语义标识与分类，但不改变 `FName(RowName)` 作为权威唯一 ID 的事实。

#### Scenario: AttributeTag 为空时不影响旧逻辑

**Given** AttributeDef 的某一行没有配置 AttributeTag
**When** 系统使用现有 `FName AttributeName` API 访问/计算属性
**Then** 行为与未引入 AttributeTag 前完全一致

#### Scenario: AttributeTag 配置后可用于解析到 AttributeName

**Given** AttributeDef 的某一行配置了有效的 AttributeTag
**When** 调用 Tag 入口 API（例如 `TryResolveAttributeNameByTag`）
**Then** 返回 true 且解析得到正确的 AttributeName(FName/RowName)

---

### Requirement: 运行时构建 Tag->Name 索引并进行一致性校验

系统 MUST 在初始化阶段构建 `AttributeTag -> AttributeName(FName)` 的只读映射，并对数据一致性进行校验与记录日志。

#### Scenario: 重复 AttributeTag 会被检测并产生确定性行为

**Given** AttributeDefTable 中存在两行配置了相同的 AttributeTag
**When** 系统初始化构建映射
**Then** 记录 Error 日志，且映射结果具备确定性（例如只保留第一条映射，其余忽略）

#### Scenario: 空/无效 AttributeTag 不进入映射

**Given** AttributeDefTable 中某行 AttributeTag 为空或无效
**When** 系统初始化构建映射
**Then** 记录 Warning 日志，且该 Tag 不会出现在映射中

---

### Requirement: 提供 Tag 入口的查询/访问 API

系统 MUST 提供一组以 `FGameplayTag` 为输入的 Attribute 查询/访问 API，内部通过 Tag->Name 解析后走现有 `FName` 逻辑，以保证行为一致性与向后兼容。

#### Scenario: 使用 Tag API 访问不存在的 Tag 返回失败

**Given** 输入的 AttributeTag 未在映射中注册
**When** 调用 Tag 入口 API
**Then** 返回 false（或等价失败结果），并提供可诊断日志（可选）

