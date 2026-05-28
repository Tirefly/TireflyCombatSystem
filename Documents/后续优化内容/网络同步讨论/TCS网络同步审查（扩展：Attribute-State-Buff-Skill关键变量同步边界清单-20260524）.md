# TCS 网络同步审查（扩展：Attribute-State-Buff-Skill 关键变量同步边界清单）

> 自 2026-05-25 起，本文档已从原“网络同步与性能优化审查”系列提纯并改名；已实现的本地性能 / 流程优化内容已抽离归档至 [归档：TCS 既有优化落地记录（2026-05-25）](<./_archive/TCS既有优化落地记录（2026-05-25）.md>)。本文只保留后续 `TCS` 网络同步边界与权威快照候选字段讨论。

## 1. 文档定位

本文档是以下三份文档的进一步扩展清单：

1. [TCS网络同步审查（讨论基线-20260521）](<./TCS网络同步审查（讨论基线-20260521）.md>)
2. [TCS网络同步审查（扩展：GAS预测边界与TCS预测分层-20260522）](<./TCS网络同步审查（扩展：GAS预测边界与TCS预测分层-20260522）.md>)
3. [设计：战斗系统架构（UE5-Version）](./%E5%8E%9F%E5%A7%8B%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1/%E8%AE%BE%E8%AE%A1%EF%BC%9A%E6%88%98%E6%96%97%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%EF%BC%88UE5-Version%EF%BC%89.md)

本文档的目标不是立刻决定最终网络协议，也不是要求现在就把现有代码全部改造成可复制结构。

本文档只做一件事：

把当前 `Attribute / State / Buff` 三个模块中的关键运行时变量，以及未来 `Skill` 模块很可能出现的关键变量，按“是否适合同步、如果适合同步更适合怎样同步、如果不适合同步原因是什么”逐项列出来，避免后续本地运行时重构先把未来联机边界做乱。

## 2. 范围说明

本文档**只盘点运行时关键字段**，主要覆盖：

1. `UTcsAttributeComponent`、`FTcsAttributeInstance`、`FTcsAttributeModifierInstance`
2. `UTcsStateComponent`、`FTcsStateSlot`、`UTcsStateInstance`
3. `UTcsBuffComponent`、`FTcsBuffDurationTracker`、`UTcsBuffInstance`
4. 未来 `UTcsSkillComponent`、`UTcsSkillInstance` 及其派生类型的关键候选字段

本文档**不把下面这些对象纳入主盘点表**：

1. `DeveloperSettings`、`RegistrySubsystem`、`ManagerSubsystem` 这类定义缓存或静态查询缓存
2. 纯编辑器 authoring 数据
3. 定义资产中的静态配置表，除非它们直接决定未来运行时同步边界

原因很简单：

这里要确认的是“未来服务器需要同步什么运行时语义”，不是“项目里所有 `TMap / TSet` 到底有哪些”。

还需要把一个引擎层前提明确写出来：按 UE 5.7 当前 `UPROPERTY` 复制链路，`Replicated` 的 `map / set` 本身并不是可直接落地的通用方案。引擎源码里的 `UHT` 对 replicated map / set 会直接报 `Replicated maps are not supported.`、`Replicated sets are not supported.`；属性复制路径里也有 `Replicated TMaps are not supported.`、`Replicated TSets are not supported.` 的明确错误日志。

所以，本文这里按“语义边界”盘点，并不是忽视 `TMap / TSet` 限制，恰恰相反，是因为某些未来确实需要服务器同步的权威语义当前恰好放在这些容器里，后续才更需要把那一部分语义抽成专门快照项或显式字段，而不是试图直接复制容器本体。

## 3. 表格判定规则

为避免表格里“适合 / 不适合”被误读，这里先定义三种判定：

| 判定 | 含义 |
| --- | --- |
| `是` | 这类数据天然接近服务器权威结果，本身就是未来快照候选字段 |
| `条件适合` | 这类数据不是所有模式都必须同步，但在客户端需要显示、预测收敛、局部重建时可能需要进入快照 |
| `否` | 这类数据更像本地缓存、索引、调度或对象引用，不应直接作为权威同步边界 |

另外，表格中“推荐同步方式”如果写成“拍平成快照数组 / 结构化字段”，意思是：

不要直接复制当前 `TMap / TSet / UObject` 工作结构本体，而是把真正有网络语义的字段整理成专门的权威快照项。

这里再补两个容易误读的边界：

1. 这**不是**在说“所有未来可能要同步的内容，现在都应该整体改造成结构体”。
2. 这说的是：如果某些未来确实要同步的权威语义当前落在 `TMap / TSet` 里，那么后续只把那部分必要字段提取成稳定的快照项、数组项或显式字段，而不是把整个运行时容器原样搬过去。

## 4. 总体结论先行

在逐表展开前，当前可以先确认四条总原则：

1. 未来真正值得服务器同步给客户端的，应该是**权威结果**和**最小可重建语义**，而不是当前运行时容器原样。
2. 当前大量 `TMap / TSet` 实际上只是本地缓存、索引、脏标记、调度工作结构；同时，UE 5.7 当前复制链路本身也不支持把 replicated map / set 当成通用落地方向，因此不应因为“以后可能联机”就现在整体改造成复制对象。
3. 如果后续真的进入联机实现阶段，更稳妥的方向仍然是：只把未来确实需要服务器同步的那部分权威结果或最小可重建语义，整理成 `Attribute / State / Buff / Skill` 的专门快照结构体或快照项，而不是直接复制组件内部工作集。
4. 后续任何本地运行时重构，都应该尽量避开“把本地缓存和未来网络边界提前绑死”的重构方式。

### 4.1 当前已确认的通用实践指导

基于这一轮讨论，当前已经可以把后续实践方向进一步收敛成下面几条：

1. 对服务器运行时必须用 `TMap / TSet` 维护、但客户端又需要长期持有其**当前权威状态**的数据，优先采用“服务器运行时容器 + 拍平后的同步快照数组”两层模型，而不是试图直接复制原始容器。
2. 如果这类快照集合会长期存在，并且元素会发生新增、删除、修改，那么优先考虑使用 `FFastArraySerializer` 承载快照数组；它解决的是 `TArray<UStruct>` 的差量同步问题，不是 `TMap / TSet` 直接可复制化。
3. `FFastArraySerializer` 更适合承载“稳定身份字段 + 高频变化字段”共存的快照项。像 `AttributeDefId / AttributeInstId` 这类身份字段通常会在条目首次同步时一并下发，后续只要字段本身不变，就不会成为频繁重复发送的主因；但它并不提供“同一条目内部每个成员都拥有完全不同复制策略”的通用框架。
4. 对当前 `TCS` 而言，`Attribute` 的长期同步方向已经可以明确为：服务器侧继续使用 `UTcsAttributeComponent::Attributes` 这种运行时容器承载真值，再额外设计拍平后的 `AttributeSnapshot[]` 作为客户端长期持有的权威快照层。
5. `AttributeModifier` 当前不应进入客户端长期同步层。原因不是它毫无信息价值，而是当前 `TCS` 的倾向性规范已经要求属性修改必须经由 `AttributeModifier`，并且由 `StateInstance` 托管的 `StateTree` 执行；在这个前提下，让客户端长期持有 `Modifier` 快照既不能自然形成可靠的本地属性预测，也会把执行态语义错误地固化到同步边界上。
6. 如果未来客户端确实要显示伤害归因、击杀归因、结算统计这类结果，更合理的方向是：在事件发生时或结算时，基于 `SourceHandle` 生成独立的事件结构或汇总结构同步给客户端，而不是直接把 `AttributeModifier` 作为长期驻留同步对象。

## 5. Attribute 模块关键变量盘点

### 5.1 `UTcsAttributeComponent`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsAttributeComponent` | `Attributes` | `TMap<FName, FTcsAttributeInstance>` | `条件适合` | 不直接同步 `TMap` 本体，拍平成 `AttributeSnapshot[]`，以 `AttributeDefId` 或 `AttributeInstId` 作为稳定键 | 这是属性最终值语义的主要来源，但容器形态本身不是网络边界 |
| `UTcsAttributeComponent` | `AttributeModifiers` | `TArray<FTcsAttributeModifierInstance>` | `否` | 不进入长期同步层；如果未来需要显示归因或结算信息，改为基于 `SourceHandle` 的独立事件结构或汇总结构 | 当前 `TCS` 约束下，`Modifier` 更像服务器执行态输入和归因载体，而不是客户端长期权威状态 |
| `UTcsAttributeComponent` | `SourceHandleIdToModifierInstIds` | `TMap<int32, TArray<int32>>` | `否` | 不同步；由本地根据 `AttributeModifiers` 或 Modifier 快照重建 | 代码注释已明确它是运行时优化索引，可从主数据重建 |
| `UTcsAttributeComponent` | `ModifierInstIdToIndex` | `TMap<int32, int32>` | `否` | 不同步；本地重建 | 这是典型的快速定位索引，不是服务器权威结果 |

### 5.2 `FTcsAttributeInstance`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `FTcsAttributeInstance` | `AttributeDef` | `const UTcsAttributeDefinition*` | `否` | 不同步裸指针；如需标识定义，使用 `AttributeDefId` | `UObject` 指针不是稳定网络边界 |
| `FTcsAttributeInstance` | `AttributeDefId` | `FName` | `是` | 作为快照中的定义键按值同步 | 这是最直接、最稳定的属性身份标识 |
| `FTcsAttributeInstance` | `AttributeInstId` | `int32` | `条件适合` | 如客户端需要做稳定配对、局部更新或调试对账，则按值同步 | 是否必须同步，取决于未来快照是否以实例身份做配对 |
| `FTcsAttributeInstance` | `Owner` | `TWeakObjectPtr<AActor>` | `否` | 不直接同步；由组件宿主天然决定 | 这是运行时引用关系，不应作为快照字段本体 |
| `FTcsAttributeInstance` | `InitialValue` | `float` | `条件适合` | 仅当客户端需要无损重建 Reset/恢复语义时才同步 | 很多显示和结算场景并不需要它 |
| `FTcsAttributeInstance` | `BaseValue` | `float` | `是` | 作为属性快照字段按值同步 | 它直接影响属性真值与后续结算 |
| `FTcsAttributeInstance` | `CurrentValue` | `float` | `是` | 作为属性快照字段按值同步 | 它是客户端最直接需要消费的权威结果之一 |

### 5.3 `FTcsAttributeModifierInstance`

当前基线已经进一步确认：`AttributeModifier` 不应进入客户端长期同步层。下面这张表保留的意义，主要是为了说明它们为什么不适合作为直接同步边界；如果未来客户端只需要显示归因、击杀或结算结果，更合理的方向仍然是基于 `SourceHandle` 另行设计事件结构或汇总结构。

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `FTcsAttributeModifierInstance` | `ModifierDef` | `const UTcsAttributeModifierDefinition*` | `否` | 不同步裸指针；如果事件结果需要标识定义，改为在独立事件结构里使用稳定 ID | 指针不是稳定复制边界，且当前不应直接同步 Modifier 语义 |
| `FTcsAttributeModifierInstance` | `ModifierInstId` | `int32` | `否` | 不进入长期同步层；如未来事件结果确实需要实例级对账，再在事件结构中单独设计 | 当前不建议把 Modifier 作为长期驻留快照项 |
| `FTcsAttributeModifierInstance` | `ModifierId` | `FName` | `否` | 同上；如未来事件结果需要类型标识，再单独进入事件结构 | 这更适合作为事件/结算语义的一部分，而不是长期权威状态 |
| `FTcsAttributeModifierInstance` | `SourceHandle` | `FTcsSourceHandle` | `否` | 不直接同步整个 Modifier；如未来归因结果需要来源链，则在独立事件结构中复用 `SourceHandle` | `SourceHandle` 仍然有价值，但更适合事件/归因同步而不是 Modifier 长驻同步 |
| `FTcsAttributeModifierInstance` | `Instigator` | `TWeakObjectPtr<AActor>` | `否` | 不同步裸指针；如事件结果必须显示来源对象，则在事件结构中同步更稳定的 Actor 标识 | 当前不应把它作为 Modifier 快照字段 |
| `FTcsAttributeModifierInstance` | `Target` | `TWeakObjectPtr<AActor>` | `否` | 同上，不直接同步裸指针 | 当前不应把它作为 Modifier 快照字段 |
| `FTcsAttributeModifierInstance` | `Operands` | `TMap<FName, float>` | `否` | 不直接同步；如未来某类事件结果必须暴露少量操作数，再在独立结构中拍平必要字段 | 当前不建议直接同步执行态操作数表 |
| `FTcsAttributeModifierInstance` | `ApplyTimestamp` | `int64` | `否` | 不直接同步当前字段；如未来仍要做排序键，应改成服务器权威序号 | 当前语义里它是本地时间戳，不能承担权威验证责任 |
| `FTcsAttributeModifierInstance` | `UpdateTimestamp` | `int64` | `否` | 同上 | 当前字段本质仍是本地时间信息 |
| `FTcsAttributeModifierInstance` | `LastTouchedBatchId` | `int64` | `否` | 不直接同步；如果未来需要预测事务或操作批次，应另设计服务器/预测事务键 | 这是本地归因批次号，不等于网络权威顺序键 |

## 6. State 模块关键变量盘点

### 6.1 `UTcsStateComponent`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsStateComponent` | `StateInstanceIndex` | `FTcsStateInstanceIndex` | `否` | 不同步；由状态实例主数据本地重建 | 这是按 Id/DefId/Slot 查询的本地索引结构 |
| `UTcsStateComponent` | `StateTreeTickScheduler` | `FTcsStateTreeTickScheduler` | `否` | 不同步；本地根据当前运行中的状态实例重建 | 它是调度器，不是权威结果 |
| `UTcsStateComponent` | `Mapping_StateSlotToStateTreeStateName` | `TMap<FGameplayTag, FName>` | `否` | 不同步；由 StateTree 与槽位定义本地建立绑定 | 这是绑定映射，不是服务器裁决后的运行态结果 |
| `UTcsStateComponent` | `RuntimeStateSlots` | `TMap<FGameplayTag, FTcsStateSlot>` | `条件适合` | 不直接同步 `TMap` 本体；客户端所需的是可用于预测的本地槽位运行时视图，更合理的方向是由 `StateSlotSnapshot[]` + `StateInstanceSnapshot[]` 在本地重建 | 客户端预测确实需要“槽位当前语义”，但不需要把容器本体当成同步对象；真正进入同步边界的是槽位结果与状态实例快照 |
| `UTcsStateComponent` | `CachedActiveStateNames` | `TArray<FName>` | `否` | 不同步；本地用于变化检测 | 典型的本地缓存 |
| `UTcsStateComponent` | `PendingSlotActivationUpdates` | `TSet<FGameplayTag>` | `否` | 不同步；它只是本地延迟排空的槽位刷新请求集合 | 这是解决本地执行顺序与重入问题的同帧工作集，不是服务器权威结果 |
| `UTcsStateComponent` | `bIsUpdatingSlotActivation` | `bool` | `否` | 不同步 | 这是本地槽位激活刷新过程的防重入控制位，不是网络真值 |
| `UTcsStateComponent` | `bIsInStateTreeCallback` | `bool` | `否` | 不同步 | 这是本地是否处于 `StateTree` Tick/回调上下文的诊断与保护标志，不是权威状态 |

### 6.2 `FTcsStateSlot`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `FTcsStateSlot` | `StateSlotDef` | `TObjectPtr<UTcsStateSlotDefinition>` | `否` | 不同步裸指针；如需标识槽位，使用 `SlotTag` 或定义 ID | 指针本体不是合适同步对象 |
| `FTcsStateSlot` | `States` | `TArray<UTcsStateInstance*>` | `条件适合` | 默认不直接同步指针数组本体，也不默认额外同步一份 `StateInstanceId` 列表；优先由 `StateInstanceSnapshot[]` 在本地按槽位投影恢复，只有未来出现无法从状态实例快照单独恢复的成员关系时，才考虑补充额外摘要字段 | 客户端预测确实需要知道“槽位里当前有哪些状态”，但更稳的做法是把它作为状态实例快照的投影结果，而不是制造第二份独立真值来源 |
| `FTcsStateSlot` | `bIsGateOpen` | `bool` | `是` | 作为槽位快照字段按值同步 | 这直接影响槽位语义和客户端可见结果 |
| `FTcsStateSlot` | `BuffMergeGroups` | `TMap<FName, FTcsBuffMergeGroupRuntime>` | `否` | 不同步；由服务器权威结果驱动本地重建或刷新 | 这是 Buff merge 运行时缓存，不是最终可见语义 |
| `FTcsStateSlot` | `DirtyBuffMergeStateDefIds` | `TSet<FName>` | `否` | 不同步 | 纯本地脏标记 |
| `FTcsStateSlot` | `bBuffMergeRequiresFullRebuild` | `bool` | `否` | 不同步 | 纯本地重建控制位 |

### 6.3 `UTcsStateInstance`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsStateInstance` | `StateDef` | `const UTcsStateDefinition*` | `否` | 不同步裸指针；使用 `StateDefId` | 指针不稳定，定义标识才稳定 |
| `UTcsStateInstance` | `StateDefId` | `FName` | `是` | 作为状态快照中的定义键按值同步 | 是最直接的状态身份标识 |
| `UTcsStateInstance` | `StateInstanceId` | `int32` | `是` | 作为状态实例稳定键按值同步 | 客户端重建、差量更新、调试对账都需要稳定实例键 |
| `UTcsStateInstance` | `SourceHandle` | `FTcsSourceHandle` | `条件适合` | 如客户端需要来源显示、局部回收或预测配对，则同步 | 它是比裸对象引用更稳的因果链键 |
| `UTcsStateInstance` | `ApplyTimestamp` | `int64` | `否` | 不直接同步当前字段；如后续仍要排序，应改为服务器权威排序键 | 当前字段本质是本地时间戳 |
| `UTcsStateInstance` | `Stage` | `ETcsStateStage` | `是` | 作为状态生命周期字段按值同步 | 直接决定状态当前阶段和客户端可见语义 |
| `UTcsStateInstance` | `bPendingGC` | `bool` | `否` | 通常不直接同步；移除或无效状态应通过生命周期结果表达 | 这是本地对象回收控制位，不是业务真值 |
| `UTcsStateInstance` | `Level` | `int32` | `条件适合` | 若客户端显示或后续读值依赖等级，则按值同步 | 是否必须同步取决于等级是否影响客户端可见行为 |
| `UTcsStateInstance` | `Owner / OwnerController / OwnerStateCmp / OwnerAttributeCmp / OwnerSkillCmp` | 弱引用 / 组件引用 | `否` | 不直接同步；由宿主天然推导 | 这些都是运行时上下文引用 |
| `UTcsStateInstance` | `Instigator / InstigatorController / InstigatorStateCmp / InstigatorAttributeCmp / InstigatorSkillCmp` | 弱引用 / 组件引用 | `条件适合` | 不同步裸指针；如客户端必须知道来源对象，则同步更稳定的 Actor 标识或借助 `SourceHandle` | 是否需要取决于显示、归因和特定玩法责任 |
| `UTcsStateInstance` | `NumericParameters` | `TMap<FName, float>` | `条件适合` | 如客户端显示、预测或收敛依赖这些参数，则拍平成数值参数快照项 | 这是显式参数结果，但不应直接复制 `TMap` 本体 |
| `UTcsStateInstance` | `NumericParametersTag` | `TMap<FGameplayTag, float>` | `条件适合` | 同上，拍平为 Tag 参数快照项 | 同上 |
| `UTcsStateInstance` | `BoolParameters` | `TMap<FName, bool>` | `条件适合` | 拍平成布尔参数快照项 | 是否同步取决于客户端是否消费它们 |
| `UTcsStateInstance` | `BoolParametersTag` | `TMap<FGameplayTag, bool>` | `条件适合` | 拍平成 Tag 布尔参数快照项 | 同上 |
| `UTcsStateInstance` | `VectorParameters` | `TMap<FName, FVector>` | `条件适合` | 拍平成向量参数快照项 | 同上 |
| `UTcsStateInstance` | `VectorParametersTag` | `TMap<FGameplayTag, FVector>` | `条件适合` | 拍平成 Tag 向量参数快照项 | 同上 |
| `UTcsStateInstance` | `bStateTreeRunning` | `bool` | `否` | 通常不直接同步；多数情况下可由 `Stage` 或激活结果推导 | 这是执行态控制位，不一定需要直接进入快照 |
| `UTcsStateInstance` | `CurrentStateTreeStatus` | `EStateTreeRunStatus` | `条件适合` | 如果客户端显示或诊断确实需要，可作为调试/可见状态字段考虑；否则通常可省略 | 是否需要同步取决于客户端是否消费这一层状态 |
| `UTcsStateInstance` | `StateTreeInstanceData` | `FStateTreeInstanceData` | `否` | 不直接同步 | 这是典型执行态内部数据，当前不适合作为通用网络边界 |

## 7. Buff 模块关键变量盘点

### 7.1 `UTcsBuffComponent` 与 `FTcsBuffDurationTracker`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsBuffComponent` | `OwnerStateComponent` | `TWeakObjectPtr<UTcsStateComponent>` | `否` | 不同步 | 这是共享宿主组件缓存，不是 Buff 权威结果 |
| `UTcsBuffComponent` | `OwnerStateSlotActivationHandle` / `OwnerStateDebugOverlayHandle` | `FDelegateHandle` | `否` | 不同步 | 纯本地绑定句柄 |
| `UTcsBuffComponent` | `DurationTracker` | `FTcsBuffDurationTracker` | `否` | 不同步；由 Buff 实例主数据驱动本地跟踪结构重建 | 这是典型 Tick 工作结构 |
| `FTcsBuffDurationTracker` | `TrackedInstances` | `TSet<TObjectPtr<UTcsBuffInstance>>` | `否` | 不同步 | 它只是“哪些 Buff 需要时长 Tick”的本地注册表 |

### 7.2 `UTcsBuffInstance`

| 所在对象 | 关键变量 | 当前类型 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsBuffInstance` | `StateInstance` 继承字段 | 继承自 `UTcsStateInstance` | `条件适合` | 参照上面的 State 表处理 | Buff 本质上仍借道 State 主链执行 |
| `UTcsBuffInstance` | `TotalDuration` | `float` | `条件适合` | 作为 Buff 快照字段按值同步 | 如果客户端要准确显示总时长或重建生命周期边界，它有意义 |
| `UTcsBuffInstance` | `RemainingDuration` | `float` | `是` | 作为 Buff 快照字段按值同步 | 这是客户端显示倒计时和收敛生命周期结果的直接来源 |
| `UTcsBuffInstance` | `Period` | `float` | `条件适合` | 如客户端显示或局部纯表现需要，则按值同步；不表示客户端可自行推进周期真值 | `Period` 本身可以是服务器结果的一部分，但周期执行主链仍是服务器权威 |
| `UTcsBuffInstance` | `MaxStackCount` | `int32` | `条件适合` | 如客户端 UI、叠层显示或逻辑读取需要，则按值同步 | 是否需要同步取决于客户端是否必须知道当前最大叠层上限 |
| `UTcsBuffInstance` | `StackCount` | `int32` | `是` | 作为 Buff 快照字段按值同步 | 叠层数是最直接的 Buff 可见结果之一 |

## 8. 未来 Skill 模块关键变量候选盘点

`Skill` 当前代码几乎还是空框架，因此这里不是“当前实现真相”，而是结合 [设计：战斗系统架构（UE5-Version）](./%E5%8E%9F%E5%A7%8B%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1/%E8%AE%BE%E8%AE%A1%EF%BC%9A%E6%88%98%E6%96%97%E7%B3%BB%E7%BB%9F%E6%9E%B6%E6%9E%84%EF%BC%88UE5-Version%EF%BC%89.md) 对未来关键变量做候选盘点。

### 8.1 未来 `UTcsSkillComponent` 候选字段

| 所在对象 | 未来候选变量 | 当前来源依据 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsSkillComponent` | 全部 learned skill 实例集合 | 架构文档明确指出 SkillComponent 应持有全部 learned skill 实例 | `条件适合` | 设计成 `SkillSnapshot[]` 或按 `SkillDefId / SkillInstanceId` 键控的快照项集合 | 这是宿主级技能拥有态数据中心，但不宜把运行时容器原样复制 |
| `UTcsSkillComponent` | 宿主级 `SkillModifiers` 集合 | 架构文档明确指出 SkillModifier 应归 SkillComponent 持有 | `条件适合` | 如客户端需要技能筛选、显示或局部重建，则同步精简 `SkillModifierSnapshot[]` | 很可能需要按标签、等级段、技能槽、技能类型筛选，但不代表要直接复制内部容器 |
| `UTcsSkillComponent` | 面向技能筛选、批量修改、装备/养成影响的统一缓存 | 架构文档明确指出它应提供统一缓存与入口 | `否` | 不同步；本地根据 learned skill 与 modifier 快照重建 | 缓存与入口是运行时组织方式，不是权威结果本体 |

### 8.2 未来 `UTcsSkillInstance` 候选字段

| 所在对象 | 未来候选变量 | 当前来源依据 | 是否适合同步 | 推荐同步方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- | --- |
| `UTcsSkillInstance` | 跨 Activation 持久变量 | 架构文档明确指出 SkillInstance 是 learned skill 的持久数据载体 | `条件适合` | 使用强类型 `UPROPERTY` 按字段复制或进专门 Skill 快照 | 这正是 SkillInstance 的核心职责之一 |
| `UTcsSkillInstance` | 参数结果与状态字段 | 架构文档明确指出 SkillInstance 可声明自己的参数结果与状态字段 | `条件适合` | 按字段复制或进入专门 Skill 快照 | 取决于客户端是否需要直接消费这些字段 |
| `UTcsSkillInstance` | 技能自己的网络复制变量与复制策略 | 架构文档明确指出它们应放在 SkillInstance 派生类上 | `是` | 优先走强类型字段复制和明确 `OnRep`，而不是塞进统一泛型袋 | 这是架构文档当前最明确的 Skill 网络方向 |
| `UTcsSkillInstance` | 冷却真值 / 可再次激活时刻 | 当前讨论已确认 Skill CD 应归 SkillInstance，而非 Buff/StateDuration | `是` | 作为 SkillInstance 的显式字段或 Skill 快照字段同步 | 这是未来客户端最需要消费的技能真值之一 |
| `UTcsSkillInstance` | 派生技能自己的 `OnRep` 事件分发表面 | 架构文档明确指出 SkillInstance 可拥有 `OnRep` 表面 | `条件适合` | 不是独立同步字段，而是字段同步后的本地响应层 | 它是复制后的消费机制，不是快照值本体 |
| `UTcsSkillInstance` | 统一泛型容器里的“大数据袋” | 架构文档明确反对把所有变量都压进统一泛型容器 | `否` | 不应作为未来主要同步方向 | 架构方向已经明确倾向“显式、强类型、可控复制面” |

### 8.3 未来 `SkillDef` 配置侧的边界提醒

虽然 `SkillDef` 不属于本次主盘点的运行时变量对象，但有一条边界必须在这里写清楚：

| 所在对象 | 未来候选变量 | 是否适合同步 | 推荐方式 | 原因 / 备注 |
| --- | --- | --- | --- | --- |
| `SkillDef` | `SkillInstanceClass` 之类的实例类型配置 | `否` | 不作为运行时同步对象；它属于静态配置，由定义资产决定 | 架构文档已经明确，这个配置入口应放在 `SkillDef` 上，而不是由 `SkillComponent` 或一次激活执行态临时决定 |

## 9. 当前可以先确认的结论

基于这轮盘点，当前可以先确认下面几件事：

1. 现在有必要先确认未来 `Attribute / State / Buff / Skill` 的**权威快照候选字段**。
2. 当前已经可以把 `Attribute` 的长期同步方向进一步收紧为：服务器保留运行时 `TMap` 容器，客户端长期持有拍平后的 `AttributeSnapshot[]` 权威快照；如果该快照集合长期存在并频繁增删改，优先考虑 `FFastArraySerializer`。
3. 当前更值得保护的边界是：
   - 权威结果字段
   - 本地缓存 / 索引 / 调度结构
   - 对象引用 / 工作集上下文
   这三类数据不要混在一个“统一可序列化运行时结构”里。
4. `AttributeModifier` 当前不应进入客户端长期同步层；归因、击杀、结算这类需求后续应优先基于 `SourceHandle` 设计独立事件结构或汇总结构。
5. 在当前 `TCS` 约束下，属性修改必须经由 `AttributeModifier`，并由 `StateInstance` 托管的 `StateTree` 执行；因此“客户端长期持有 Modifier 快照并据此做本地属性预测”不应作为当前实践方向。
6. 如果接下来继续做本地运行时调整，更稳妥的做法是：
   - 允许优化本地缓存结构和运行时算法
   - 但不要提前把这些本地结构固化成未来网络同步边界
   - 真正的同步边界，后续应由专门快照结构体承担

## 10. 最自然的下一步

如果沿当前方向继续推进，最值得继续细化的不是“现在就改代码”，而是：

1. 把 `Attribute` 的最小权威快照字段集从本表里再压缩成正式候选清单。
2. 把 `State` 的最小权威快照字段集从本表里再压缩成正式候选清单。
3. 把 `Buff` 的最小权威快照字段集从本表里再压缩成正式候选清单。
4. 结合架构文档，把未来 `Skill` 的最小权威快照字段集单独拆成一页更具体的设计草案。