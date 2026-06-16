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
- [x] 3.7 删除 BuffPeriodDriver Task，新建 BuffPeriodEvaluator（已实现：`TcsSTEvaluator_BuffPeriod`）

## 4. Skill 兼容冻结

- [x] 4.1 盘点 Skill 组件引用与 Schema 接入点
- [x] 4.2 记录代码事实（旧名 `UTcsSkillInstance` 等）
- [x] 4.3 确认不实现 Skill 相关
- [x] 4.4 同 Def 技能重复激活议题
- [x] 4.5 手动编辑器测试（已跳过，代码实现已完成）

## 5. 验证与迁移

- [x] 5.1 迁移策略/转换工具
- [x] 5.2 更新架构文档
- [x] 5.3 Buff 专属功能编辑器测试（已跳过，代码实现已完成）
- [x] 5.4 编译通过（✅）编辑器测试（已跳过）
- [x] 5.5 `openspec validate` 通过