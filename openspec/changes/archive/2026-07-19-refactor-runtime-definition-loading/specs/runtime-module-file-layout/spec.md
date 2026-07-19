## REMOVED Requirements
### Requirement: StateManagerSubsystem 可以作为结构重组例外
**Reason**: `UTcsStateManagerSubsystem` 已从 runtime 模块删除，不再存在需要保留为单一 `.cpp` 的实现文件。
**Migration**: Definition cache/load 与查询改由 `UTcsDefinitionManagerSubsystem` 按 source cache、同步补充、异步加载等职责拆分实现；State Actor 本地业务保留在 `UTcsStateComponent` 及其职责文件中。
