# Design: Attribute Tagging (FName + GameplayTag)

## Context

在 TCS 的 Attribute 模块中：

- **AttributeName**：以 `FName` 表示属性的唯一标识，通常对应 AttributeDef DataTable 的 RowName。
- **State/Slot**：大量使用 `FGameplayTag` 表示类型与槽位，并利用 Tag 的层级匹配与容器筛选。

本设计希望在不破坏现有 `FName` 体系的前提下，引入 `FGameplayTag` 作为 Attribute 的“语义标记/分类入口”，用于规则编排、筛选与跨系统统一表达。

## Goals / Non-Goals

Goals:
- 保留 `FName` 作为 Attribute 的权威唯一 ID（数据表 RowName，不变更核心计算路径）。
- 为 Attribute 增加可选 `FGameplayTag` 标记，并提供 Tag->Name 的稳定映射与校验。
- 提供面向 Blueprint/C++ 的 Tag 入口 API，做到“易用且可诊断”。
- 允许逐步迁移：不要求一次性改完所有 DataTable/蓝图。

Non-Goals:
- 不在本变更中把所有现有 `FName` 参数签名替换为 `FGameplayTag`（避免破坏性升级）。
- 不引入新的资产格式或强制迁移工具（必要时仅提供校验与日志提示）。
- 不改变 AttributeModifier 的合并/执行策略与既有数值语义。

## Key Decision

**Decision**: `FName` 仍然是 Attribute 的唯一权威 ID；`FGameplayTag` 是可选的“语义/分类/对外标识”，所有 Tag 入口在内部先解析为 `FName` 后再走既有逻辑。

理由：
- 现有 DataTable/Blueprint API 都围绕 RowName 运转，替换为 Tag 会造成大量破坏性改动。
- `FName` 更轻量，也更符合“数值计算热路径”的需求。
- `FGameplayTag` 适合做“可组织、可筛选、可重定向”的语义层；把它作为入口而不是核心 ID，能同时满足扩展能力与维护成本。

## Data Model

### Attribute Definition

在 `FTcsAttributeDefinition` 中新增字段（命名可选，示例）：

```cpp
// 属性的语义标识（可选，但推荐）：
// - 用于父子 Tag 匹配、分类筛选、跨系统对齐
// - 仍然以 Attribute DataTable RowName(FName) 作为权威唯一 ID
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
FGameplayTag AttributeTag;
```

可选增强（如果确实需要“一个属性多个标签”）：
- 再加 `FGameplayTagContainer AttributeTags` 作为分类标签集合；`AttributeTag` 仍为“主键式语义标识”。

### Runtime Index

在 AttributeManagerSubsystem 初始化时，构建只读索引：

```cpp
TMap<FGameplayTag, FName> AttributeTagToName;
TMap<FName, FGameplayTag> AttributeNameToTag; // 可选：便于反查/调试
```

## Validation Rules

初始化（或首次访问）时对 AttributeDefTable 做校验，并输出可诊断日志：

- **Duplicate Tag**：多个 Attribute 行配置了相同 `AttributeTag`
  - 处理：记录错误；保留第一个映射，其余行的 Tag 映射忽略（避免不确定性）。
- **Invalid/Empty Tag**：Tag 未填写或在 TagManager 中无效
  - 处理：记录 Warning；不写入映射；使用 Tag API 访问该属性将返回失败。
- **Name/Tag mismatch**（可选规则）：如果团队希望强约束“Tag 必须以 RowName 派生”
  - 处理：记录 Warning，方便统一命名规范。

## API Shape

原则：不动现有 `FName` API；新增 Tag 入口 API（BlueprintCallable + C++）。

建议新增（示例，最终以现有类职责为准）：

- `bool TryResolveAttributeNameByTag(FGameplayTag AttributeTag, FName& OutAttributeName) const`
- `bool TryGetAttributeTagByName(FName AttributeName, FGameplayTag& OutAttributeTag) const`（可选）
- `bool AddAttributeByTag(AActor* CombatEntity, FGameplayTag AttributeTag, float InitValue = 0.f)`（可选）
- `bool GetAttributeValueByTag(AActor* CombatEntity, FGameplayTag AttributeTag, float& OutValue) const`（可选）

对于 StateCondition/规则节点：
- 新增一个 Tag 版本的 Payload（例如 `FTcsStateConditionPayload_AttributeComparison_Tag`），或在现有 Payload 中增加可选 Tag 字段并规定优先级（Tag 优先，其次 Name）。

## Naming & Authoring Convention

建议约定一个明确的根前缀，避免与项目其它 Tag 冲突：

- 推荐：`TCS.Attribute.<RowName>`
- 示例：
  - `TCS.Attribute.Health`
  - `TCS.Attribute.Stamina`
  - `TCS.Attribute.Resistance.Fire`

约定建议：
- `AttributeTag` 用于标识“属性本体”，尽量稳定，避免频繁改名。
- 分类/分组如果需要，放到 `AttributeTags`（Container）或使用 Tag 的层级结构表达。

## Migration Plan

阶段 0（无痛引入）：
- 仅新增字段 + 索引 + 基础解析 API；
- 现有逻辑完全不改，Tag 为空也不会影响游戏运行。

阶段 1（数据补齐）：
- 在 AttributeDef DataTable 中逐步补齐 `AttributeTag`；
- 在 GameplayTags 配置中补齐 Tag（通过 Project Settings 或 ini）。

阶段 2（规则/系统逐步接入）：
- 新写的条件/技能/状态规则优先用 Tag；
- 旧蓝图保持 `FName`，按需迁移。

## Risks / Trade-offs

- **维护双源标识**：需要同时维护 DataTable RowName 与 GameplayTag。通过“FName 仍为权威 ID + Tag 可选 + 初始化校验”把成本控制在可接受范围。
- **Tag 存在性**：`FGameplayTag` 必须在 TagManager 中存在。通过校验日志与团队命名约定降低踩坑概率。
- **重复 Tag**：必须防止冲突。通过初始化校验 + 明确的失败策略保证确定性。

## Test Strategy (Automation)

建议添加自动化测试（模块私有 Tests）覆盖：

- 读取 AttributeDefTable 时构建索引：正常/重复 Tag/空 Tag/无效 Tag 的行为。
- Tag->Name 解析：成功与失败返回值、日志可诊断（可用 UE 的 Automation Log 捕获）。
- （如新增）Tag 版本的 AttributeComparison Condition：在指定属性值下比较结果正确。

