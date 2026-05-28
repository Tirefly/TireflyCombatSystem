# Archive 使用说明

`changes/archive/` 中的每个目录都是归档时刻的历史快照。

这些文档的主要用途是说明：

- 当时为什么提出这条变更
- 当时如何权衡设计与范围
- 当时是如何归档到当前 capability spec 的

它们**不是**当前事实的最高优先级来源。

## 读取优先级

如果 archive 文本与当前 OpenSpec 其他位置冲突，优先级始终是：

1. `openspec/specs/` 下的当前 capability spec
2. `openspec/changes/` 下的活动 change
3. `openspec/changes/archive/` 下的历史快照

## 使用规则

- 不要仅根据 archive 推断当前 API、命名、职责边界或运行时契约。
- 如果 archive 提到的术语、类名、schema 名称或实现方向与当前文档不一致，以当前 spec / 活动 change 为准。
- 对近期仍在快速收敛的主题，尤其是 `State Core / Buff / Skill` 分层、`StateTree schema` 命名、`UTcsSkillInstance` / `UTcsSkillEntry` 语义边界，必须先检查当前 spec 与活动 change，再把 archive 当背景资料阅读。
- 如果确实需要引用 archive，请在结论里显式标明“这是历史快照，不是当前契约”。