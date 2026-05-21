# 变更：重构 TCS State Core 与 Buff 边界，并在当前阶段保持 Skill 不变

## 背景

TCS 当前的 `State` 体系已经同时承载了三类职责：

- 共享的运行时状态宿主与生命周期框架
- Buff 专属的持续时间、叠层、合并语义
- 少量 Skill 专属的快照与拥有态泄漏

这导致两个根本问题：

1. `UTcsStateDefinition` 继续膨胀，而且已经不再是一个干净的共享基类。
2. 插件使用者和策划面对的是一个语义混杂的数据模型，难以判断哪些字段属于 Buff、哪些属于 Skill、哪些才是所有运行态共享的核心契约。

本轮架构结论已经确认：TCS 的长期方向仍然是按 `State Core`、`Buff`、`Skill` 三个职责模块拆分；仅 `UTcsStateDefinition` 适合抽象化；`Duration` 与 `DurationType` 应视为 Buff 专属并从当前 `State` 模块剥离。

但当前阶段先不进一步实现 `Skill` 模块，保持 `UTcsSkillComponent` / `UTcsSkillInstance` / `UTcsSkillManagerSubsystem` 现状。本提案只处理 `State Core` 与 `Buff` 的边界收敛，并为后续独立的 Skill 议题预留干净接口。

## 变更内容

- **BREAKING**：把 `UTcsStateDefinition` 改为抽象共享基类，不再允许直接把它当作最终可创建设计资产。
- 引入 Buff 模块边界：新增 `UTcsBuffDefinition`，并把 `DurationType`、`Duration`、`MaxStackCount`、`MergerType` 以及后续的 Period / Refresh / Expire 等 Buff 专属语义迁入 Buff 侧。
- 当前 `UTcsStateMerger` 与 `MergerType` 不再继续挂在抽象 `StateDefinition` 上；本阶段把它们与 Buff 的叠层/同 Def 合并语义一起迁到 Buff 侧，并把真正的共享重复态冲突策略留作后续再决定是否抽出独立 `State Core` 抽象。
- 移除 `ETcsStateType` / `StateType` 这类通过共享基类枚举区分子类型的做法，改由具体定义类型本身表达语义。
- 保留 `UTcsStateComponent` 为具体类，继续作为 Actor 侧统一运行态宿主；State Core 只保留 Apply / Remove / Query / Slot / StateTree 等共享框架职责。
- 明确当前 Skill 语义基线：`UTcsSkillInstance` 只表示“已学会的技能”，不表示一次技能执行实例；技能释放仍由 Skill 侧发起，再向 `UTcsStateComponent` 申请 `StateInstance` 进入运行态。
- 明确当前阶段的 Skill 边界：不新增 `UTcsSkillDefinition`，不扩展 `UTcsSkillInstance` / `UTcsSkillComponent` 的职责，不实现新的 Skill 运行时能力；仅要求本次 `State Core` / `Buff` 重构不要继续向共享基类增加新的 Skill 泄漏。
- 对现有 `State` 体系中的 Skill 引用接入点做盘点和冻结，留待后续独立 Skill change 处理，而不是在本提案里继续扩大范围。
- 同 Def 的 Skill 重复激活策略在当前阶段不统一定型；后续需要按具体技能类型做完整调研，再决定是否引入 Skill 自己的冲突策略抽象。
- 把 `FTcsStateParameter` 中的快照语义重新定位为共享参数评估策略，而不是继续视为 Skill-only 功能。
- 把 `UTcsStateInstance` 暴露给 `StateTreeSchema` 的上下文面视为待定设计；当前实现仅作为实验基线，不把现状直接固化为最终契约。
- 本提案不直接修改编辑器资产创建流程、`UFactory`、`UAssetDefinition` 或编辑器侧相关 Registry；若 `UTcsBuffDefinition` 需要新的 authoring 入口，应归入 `add-tcs-def-editor-authoring` 议题处理。
- 本提案以 `migrate-manager-api-to-component` 当前已落地的代码结果作为冻结基线；该议题剩余的是测试与归档，不再假设后续还会改写 `UTcsStateComponent` / `UTcsStateManagerSubsystem` 代码。

## 影响范围

- 受影响规范：
  - `state-runtime-core`
  - `buff-runtime`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateDefinition.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateInstance.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystemTests/**`
- 受影响文档：
  - `Plugins/TireflyCombatSystem/Documents/调研：TCS状态体系下Buff与Skill区分方案对比.md`
  - TCS 手工测试 / 作者指南 / 架构文档
- 对使用方的影响：
  - 现有直接创建 `UTcsStateDefinition` 的资产与流程需要迁移到新的具体定义类型；本阶段主要面向 Buff authoring 路径
  - 现有依赖 `Duration` / `DurationType` / `MaxStackCount` / `MergerType` 的代码和脚本需要跟随 Buff 新模型调整
  - 现有 Skill 模块 API 与骨架在本阶段保持现状，不要求调用方迁移到新的 Skill 运行时契约
  - `UTcsSkillInstance` 在当前与后续方向里都只表示“已学会技能”；真正的技能执行仍通过 `UTcsStateComponent` 申请 `StateInstance`
  - 同 Def 技能重复激活的处理规则当前仍未定型，后续需要按技能类型单独调研
  - 若需要补 `UTcsBuffDefinition` 的编辑器创建入口，应在 `add-tcs-def-editor-authoring` 中补充，不在本提案内实现