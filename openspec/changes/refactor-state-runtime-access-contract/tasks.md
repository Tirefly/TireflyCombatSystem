## 1. 提案与设计

- [x] 1.1 补齐本 change 的目标、非目标与边界，明确哪些议题应纳入同一 proposal。
- [x] 1.2 盘点与其他 active changes 的重叠点，避免重复声明或冲突承诺。
- [x] 1.3 新增 `design.md`，固定本 change 对抽象执行基类、三条 concrete schema 与 Skill 命名翻面的技术方向与取舍。

## 2. 类型与命名重构

- [x] 2.1 将 `UTcsStateInstance` 标记为 `Abstract`，并清理 generic concrete runtime 假设。
- [x] 2.2 删除 `UTcsSTSchema_StateInstance`，不再保留 generic `StateInstance` concrete schema。
- [x] 2.3 将组件树 schema 命名统一为 `UTcsSTSchema_StateComponent`。
- [x] 2.4 将 Buff 树 schema 命名统一为 `UTcsSTSchema_Buff`。
- [x] 2.5 将 Skill 树 schema 命名统一为 `UTcsSTSchema_Skill`。
- [x] 2.6 将当前 learned 数据对象 `UTcsSkillInstance` 重命名为 `UTcsSkillEntry`。
- [x] 2.7 新增运行时 `UTcsSkillInstance : UTcsStateInstance`，作为 Skill 激活执行态类型。

## 3. Definition 与运行时类型选择

- [x] 3.1 明确 `ITcsEntityInterface` 的稳定组件访问面，包含 BuffComponent 的契约与适用范围。
- [x] 3.2 记录该接口面调整对通用辅助库、缓存层和主要调用方的影响。
- [x] 3.3 保持 `UTcsStateDefinition` 作为抽象共享定义层，并校准 `ResolveStateInstanceClass()` 合同与抽象化后的 `UTcsStateInstance` 一致。
- [x] 3.4 为 `UTcsBuffDefinition` 新增 `TSubclassOf<UTcsBuffInstance> BuffInstanceClass`，并重写运行时实例解析逻辑。
- [x] 3.5 新增 `UTcsSkillDefinition`，提供 `SkillInstanceClass + SkillEntryClass` 两个静态配置入口。

## 4. Concrete Schema 契约与调用方迁移

- [x] 4.1 让 `UTcsSTSchema_StateComponent` 只暴露 `StateComponent` 根上下文，并保留 `LinkSubTree` / `LinkedSubTree` 兼容方向。
- [x] 4.2 让 `UTcsSTSchema_Buff` 只暴露 `BuffInstance` 根上下文，且不再复用已删除的 generic `StateInstance` schema。
- [x] 4.3 让 `UTcsSTSchema_Skill` 暴露 `SkillInstance + SkillEntry` 两个根上下文。
- [x] 4.4 将现有依赖旧 `StateInstance` schema 的任务、求值器、运行时回调迁移到三条新的 concrete schema 线上。

## 5. 与现有 authoring 议题的衔接

- [x] 5.1 记录 `add-tcs-def-editor-authoring` 中的组件树 schema 债务，明确其目标类型已切换为 `UTcsSTSchema_StateComponent`。
- [x] 5.2 记录 Buff / Skill 对应 concrete schema 进入 editor authoring 面时的命名与资产入口协同边界。

## 6. LinkedSubTree 统一方向

- [x] 6.1 记录“TCS 所有 StateTree 最好都支持 `LinkedSubTree`”作为统一目标，而不是只在组件树上局部成立。
- [x] 6.2 如果实现该目标需要跨多个 schema、节点契约或 capability 做较大范围改造，则在本提案中明确标记为后续独立 proposal。

## 7. 验证

- [x] 7.1 运行 `openspec validate refactor-state-runtime-access-contract --strict --no-interactive`。
- [x] 7.2 完成后续代码重构的编译验证。
- [ ] 7.3 等待开发者后续手动执行编辑器验证，确认三条 concrete schema 的上下文暴露面与 `LinkSubTree` / `LinkedSubTree` 需求一致。
