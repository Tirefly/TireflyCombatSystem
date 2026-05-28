# 任务 —— refactor-tcs-state-buff-skill-split

> 本变更是架构重构提案，实施前应先确认与 `migrate-manager-api-to-component` 的顺序关系。除非提案获批，否则不得开始运行时代码迁移。

## 1. 前置收敛

- [x] 1.1 确认以 `migrate-manager-api-to-component` 当前已冻结的代码结果作为实现基线；该议题的剩余测试与归档不再视为代码前置阻塞
- [x] 1.2 盘点当前 `Public/State/**` 与 `Private/State/**` 中所有 Buff-only 字段、实验性质的 Skill 引用接入点、以及共享参数/Schema 暴露面，输出一份迁移清单
- [x] 1.3 盘点当前编辑器作者入口、Factory、`UAssetDefinition` 与相关 Registry 对 `UTcsStateDefinition` 具体类的直接假设，并把需要的配套调整记录到 `add-tcs-def-editor-authoring`

## 2. Definition 层拆分

- [x] 2.1 把 `UTcsStateDefinition` 收敛为抽象共享基类，并移除 `StateType`、`DurationType`、`Duration`、`MaxStackCount`、`MergerType` 等子类专属字段
- [x] 2.2 新增 `UTcsBuffDefinition`，承载 Duration / Stack / Merge / Period / Refresh / Expire 等 Buff 专属配置
- [x] 2.3 盘点当前 `UTcsStateMerger` 实现，把 `StackDirectly` / `StackByInstigator` 明确归类为 Buff merge 策略，并决定 `UseNewest` / `UseOldest` / `NoMerge` 在本阶段先随 Buff 迁移还是标记为候选共享冲突策略
- [x] 2.4 更新运行时文档与迁移说明，明确裸 `UTcsStateDefinition` 不再是最终目标类型；若需要 Buff 的编辑器 authoring 路径调整，则回写到 `add-tcs-def-editor-authoring` 而非在本任务中直接实现

## 3. State Core / Buff 运行时拆分

- [x] 3.1 保留 `UTcsStateComponent` 为统一具体宿主，只保留 Apply / Remove / Query / Slot / StateTree / 生命周期主流程等共享职责
- [x] 3.2 把 Duration 跟踪、Stack、Merge、Period、Refresh、Expire 及 Buff 专属事件从 State Core 中迁移到 Buff 模块
- [x] 3.3 删除 `ETcsStateType` / `StateType`，不再依赖共享枚举表达 State / Buff / Skill 语义
- [x] 3.4 重新定义 `UTcsStateInstance` 与 Buff 运行时数据的边界，避免在共享实例上继续固化 Buff-only API，并重新收敛其对 `StateTreeSchema` 的最小暴露面
- [x] 3.5 重新定义共享参数求值策略，明确 `bIsSnapshot` 一类能力属于共享参数系统而非 Skill-only 字段
- [x] 3.6 更新移除原因、查询接口和运行时事件，使 Buff-only 语义不再伪装成所有状态实例都支持的共享行为

## 4. Skill 兼容冻结

- [x] 4.1 盘点当前 `State` 结构中的 Skill 组件引用与 Schema 接入点，标记为后续独立 Skill change 的迁移债务
- [x] 4.2 记录并确认当前代码事实：learned-skill 持有态当前仍使用 `UTcsSkillInstance` 命名，且技能释放仍通过 `UTcsStateComponent` 申请运行态；但这不是长期命名契约
- [x] 4.3 确认本提案自身不实现 `UTcsSkillDefinition`、`UTcsSkillEntry`、新的 `UTcsSkillInstance` 或 Skill 专用 schema；这些 follow-up 由独立 change 推进
- [x] 4.4 把“同 Def 技能重复激活”记录为后续独立 Skill 议题，要求按技能类型做完整调研，不在本提案中统一定默认规则
- [ ] 4.5 等待开发者手动执行编辑器测试，确认 State / Buff 重构不会破坏当前 Skill 骨架与已有调用路径

## 5. 验证与迁移

- [x] 5.1 提供现有 `UTcsStateDefinition` 资产的迁移策略或转换工具
- [x] 5.2 更新架构文档、手工测试指南、作者文档，统一采用 `State Core / Buff / Skill` 术语
- [ ] 5.3 等待开发者手动执行编辑器测试，覆盖 Buff 的 Duration / Stack / Merge / Period，并确认当前 Skill 模块最小兼容场景未被本阶段改坏
- [ ] 5.4 `TireflyGameplayUtilsEditor Win64 Development` 编译通过，并等待开发者手动执行聚焦 TCS 的编辑器测试
- [x] 5.5 `openspec validate refactor-tcs-state-buff-skill-split --strict --no-interactive` 通过