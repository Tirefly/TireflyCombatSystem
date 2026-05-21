# StateCore / Buff / Skill 迁移清单与资产迁移策略

## 文档目的

这份文档只做三件事：

1. 盘点当前 `Public/State/**` 与 `Private/State/**` 里仍残留的 Buff / Skill 债务。
2. 明确哪些点已经属于共享 `State Core`，哪些仍只是过渡阶段的暴露面。
3. 给出现有 `UTcsStateDefinition` / `UTcsBuffDefinition` 资产的迁移策略，避免后续实现阶段重新争论边界。

它不是实现真相文档，也不是未来 Skill 方案设计稿。当前实现真相请看 `文档：State模块与Buff模块核心函数流程详解（当前实现）.md`。

## 一、当前结论

当前 `State Core / Buff / Skill` 的边界已经有了明确收敛：

1. `UTcsStateDefinition` 已经是抽象共享基类，不再承担 Buff 专属配置。
2. `UTcsBuffDefinition` 已承载 `DurationType`、`Duration`、`Period`、`MaxStackCount`、`MergerType` 等 Buff 专属配置。
3. `UTcsBuffInstance` / `UTcsBuffComponent` 已接管 Duration / Stack / Merge 等 Buff 运行时语义。
4. `UTcsSkillInstance` 当前仍只表示 learned skill；Skill 执行态不在本轮 change 内扩面实现。

当前剩下的债务，主要不是 Definition 层字段，而是 `State` 运行时表面上仍暴露了一些实验性质的 Skill 上下文与 Buff removal 语义。

## 二、State 层当前仍残留的迁移债务

### 2.1 共享参数系统已收敛，但仍要明确它不是 Skill-only

`FTcsStateParameter` 当前保留：

1. 参数类型 `ParameterType`
2. 快照策略 `bIsSnapshot`
3. Numeric / Bool / Vector 参数求值器
4. 参数值容器 `ParamValueContainer`

这里的关键结论已经固定：

1. `bIsSnapshot` 属于共享参数求值时机策略，不是 Skill-only 字段。
2. Buff 也可能需要“生成时快照”和“运行时实时重算”两种策略。
3. 后续 Skill change 不应把这套策略再整体搬回 Skill 模块。

### 2.2 `UTcsStateInstance` 仍暴露实验性质的 Skill 上下文入口

当前 `UTcsStateInstance` 仍直接暴露：

1. `GetOwnerSkillComponent()`
2. `GetInstigatorSkillComponent()`
3. `OwnerSkillCmp`
4. `InstigatorSkillCmp`

这说明当前 `State` 运行态对象仍承担了“把 Skill 组件直接当作共享上下文透传”的历史债务。

当前处理原则：

1. 本轮先记录，不在 `State Core / Buff` 收敛阶段继续扩面修改 Skill 模块。
2. 后续独立 Skill change 需要重新判断：哪些 Skill 上下文应该保留为正式 Schema 契约，哪些只是过渡实现。

### 2.3 `StateTreeSchema` 仍直接把 Skill 组件写入上下文面

`UTcsStateTreeSchema_StateInstance` 当前仍会：

1. 解析 `OwnerSkillCmp`
2. 解析 `InstigatorSkillCmp`
3. 把这两个组件直接写入 StateTree context data

这意味着当前 `StateTreeSchema` 暴露面里仍包含实验性质的 Skill 接入点。

当前处理原则：

1. 这套暴露面暂时视为“可运行的过渡基线”，不是最终冻结契约。
2. 后续 Skill change 应把它拆成正式保留项和待下线项两张表，而不是默认现状永远正确。

### 2.4 Buff-only 语义仍有少量共享表面残留

当前最明显的残留不再是 Definition 字段，而是共享事件与注释表面：

1. `FTcsOnStateRemovedSignature` 的注释仍把 `MergedOut`、`StackDepleted` 和共享移除原因并列列出。
2. `NotifyStateRemoved()` 仍统一透传 `FName RemovalReason`，因此 Buff-only reason 仍会穿过共享事件面出现。

这里的现阶段结论是：

1. 共享主链可以继续统一负责“状态被移除了”这件事。
2. 但 Buff-only reason 不应继续被表述成“所有状态实例天然都支持的共享语义”。
3. 这部分属于后续 API 收口任务，不应再回滚 Definition 层边界。

## 三、后续独立 Skill change 的明确债务清单

后续 Skill change 至少需要处理下面几类问题：

1. `UTcsStateInstance` 是否还要继续持有 `OwnerSkillCmp` / `InstigatorSkillCmp`。
2. `UTcsStateTreeSchema_StateInstance` 中的 Skill context name 是否全部保留。
3. 哪些 Skill 上下文属于“共享运行态都可见”，哪些只该由 Skill 自己的执行/拥有态暴露。
4. Skill 的重复激活冲突策略是否需要独立抽象，而不是继续借用 Buff merge 概念。
5. `UTcsSkillInstance`、`UTcsSkillComponent` 与未来 Skill 定义资产之间的边界如何正式落地。

在这些问题没有独立收敛前，当前 State 层里的 Skill 暴露面都只能被视为“冻结现状、避免继续扩散”，而不是“推荐继续复用”。

## 四、现有 DefinitionAsset 的迁移策略

### 4.1 当前 canonical authoring 目标

当前应直接 author 的 DefinitionAsset 是：

1. `UTcsAttributeDefinition`
2. `UTcsAttributeModifierDefinition`
3. `UTcsStateSlotDefinition`
4. `UTcsBuffDefinition`

`UTcsStateDefinition` 当前是抽象共享基类，不应继续作为新的直接 authoring 目标。

### 4.2 为什么 `UTcsBuffDefinition` 不会破坏现有运行时加载

当前 `UTcsBuffDefinition` 仍然：

1. 继承自 `UTcsStateDefinition`
2. 继承 `GetPrimaryAssetId()` 的实现
3. 继续使用 `UTcsStateDefinition::PrimaryAssetType`
4. 继续使用 `StateDefId` 作为 `PrimaryAssetId` 的 Name 部分

同时，`UTcsDefinitionRegistrySubsystem` 当前仍扫描 `UTcsStateDefinition::PrimaryAssetType`。

这意味着：

1. `UTcsBuffDefinition` 只是把 Buff 专属配置从抽象基类中迁走。
2. 它没有改变 StateDefId 驱动的运行时加载入口。
3. 也没有改变现有 Definition Registry 的扫描与缓存路径。

### 4.3 现有旧资产的处理策略

对于已经存在的 `UTcsStateDefinition` 资产，建议按语义分流：

#### 情况 A：它本质上是 Buff 资产

处理方式：迁到 `UTcsBuffDefinition`。

建议步骤：

1. 新建 `UTcsBuffDefinition` 资产。
2. 复制共享字段：`StateDefId`、`StateTag`、`StateSlotType`、`Priority`、Tag、StateTree、Condition、Parameters。
3. 复制 Buff 字段：`DurationType`、`Duration`、`Period`、`MaxStackCount`、`MergerType`。
4. 用新的 Buff 资产替换运行时引用。
5. 验证通过 `StateDefId` 的运行时查找和加载仍然成立。

#### 情况 B：它是历史遗留的裸 `UTcsStateDefinition` 资产，但语义上不是 Buff

处理方式：

1. 当前阶段允许把它视为 legacy runtime asset 暂时保留。
2. 不再新增新的裸 `UTcsStateDefinition` 作者流程。
3. 等未来真正的“非 Buff 具体状态定义资产”方案确定后，再做第二轮迁移。

这里的关键点是：

1. 当前 change 解决的是“不要继续把 Buff-only 配置塞回抽象基类”。
2. 它不是要在同一轮里发明出所有未来状态类型的最终 authoring 模型。

### 4.4 为什么当前不提供自动转换工具

当前不提供自动转换工具，原因不是做不到，而是目标类型还分两类：

1. Buff 资产已经有明确目标类型 `UTcsBuffDefinition`
2. 非 Buff 的 legacy `UTcsStateDefinition` 资产，未来尚未有稳定 concrete target

在目标类型还没完全稳定前，直接上批量自动转换，很容易把一部分资产错误迁入临时类型。

因此当前建议是：

1. 对 Buff 资产采用明确的人工迁移清单。
2. 对非 Buff legacy 资产先做分类和冻结。
3. 等未来 concrete state-side authoring 方案确定后，再决定是否补自动转换。

## 五、当前阶段的执行建议

如果当前工作目标是继续收敛现有 proposal，而不是再开新范围，建议按下面顺序处理：

1. 先把编辑器 authoring 面收敛到 concrete DefinitionAsset。
2. 再把这份文档里的 Skill 债务清单视为后续独立 Skill change 的输入基线。
3. 不要把 Skill 收敛任务反向塞回当前 `State Core / Buff` change。
4. 也不要为了追求“统一事件面”而把 Buff-only 语义重新挂回共享 Definition 层。

## 六、完成定义

当前如果满足下面四点，就可以认为文档面迁移策略已经收口：

1. 新建资产时不再直接创建裸 `UTcsStateDefinition`。
2. Buff 新资产统一走 `UTcsBuffDefinition`。
3. 运行时仍通过 `StateDefId` 和现有 `PrimaryAssetId` 路径加载 Buff 资产。
4. `State` 层中残留的 Skill 接入点已经被明确标注为后续独立 change 的迁移债务。