# Proposal: Attribute - FName Identifier + GameplayTag Tagging

## Why

TCS Attribute 目前使用 `FName` 作为属性的唯一标识（通常对应 AttributeDef DataTable 的 RowName）。这对“唯一 ID + 轻量访问”很合适，但在以下场景会变得不够用：

- 需要层级/分类语义（例如 `TCS.Attribute.Resistance.Fire`），并希望用 `GameplayTag` 的父子匹配/容器筛选能力完成查询与规则判断。
- 希望跨系统对齐标识体系：TCS 的 State/Slot 已经大量使用 `GameplayTag`，Attribute 继续只用 `FName` 会导致规则编排与数据表达割裂。
- 需要更可维护的重命名/迁移路径：GameplayTags 自带 Redirect 机制，适合做“对外稳定”的语义标识。

同时，完全用 `FGameplayTag` 替换 `FName` 会引入破坏性变更，并且会把“DataTable 行名”与“运行时标签体系”强耦合，增加配置与迁移成本。

因此本提案选择“保留 FName 作为权威 ID，同时为 Attribute 增加可选 GameplayTag 标记”，以获得两者的优点并保持向后兼容。

## What Changes

- AttributeDef（`FTcsAttributeDefinition`）新增可选字段：`AttributeTag`（或同等语义命名），用于为“属性本体”提供 GameplayTag 标识。
- Attribute 子系统在初始化阶段构建 `AttributeTag -> AttributeName(FName)` 的映射，并做一致性校验（重复/缺失/无效 Tag）。
- 提供一组“Tag 入口”的查询/访问 API（Blueprint/C++），内部统一转换为 `FName` 再走现有逻辑，避免核心计算路径被改写。
- 明确约定：`FName` 仍是唯一权威 ID；`FGameplayTag` 是语义标识/分类入口（可选但推荐）。

## Compatibility / Breaking Changes

- 默认不替换现有 `FName AttributeName` 参数签名；所有现有蓝图/代码保持可用。
- 新增字段为可选：老数据表行不需要立刻补齐 Tag，也不会阻塞运行（但会在使用 Tag API 时返回失败并给出可诊断日志）。

## Impact

- Affected areas:
  - `Public/Attribute/TcsAttribute.h`（AttributeDefinition 增加 Tag 字段）
  - `AttributeManagerSubsystem`（初始化校验 + 映射 + 新 API）
  - 可选：AttributeCondition/其它使用 AttributeName(FName) 的规则节点，增加 Tag 版本的 Payload 或重载
- Performance:
  - 常规路径维持 `FName` 查表/索引；Tag 入口只在“解析 Tag -> Name”时走一次 `TMap` 查找。

