# FScopedBatchNotification RAII 事件广播批量化

> 文档定位：后续低优先级调研备忘。
>
> 当前结论：这件事如果还要做，只应讨论“事件广播层的收敛 / 去重”，不再讨论 `State` 槽位激活刷新批处理本身。
>
> 触发条件：先采证确认监听风暴真实存在，再决定是否立项。
>
> 若后续确认推进，模块级落地取舍与实施顺序见 [事件分发优化开发指导（State-Buff-Attribute）](事件分发优化开发指导（State-Buff-Attribute）.md)。

---

## 1. 当前基线

当前版本已经落地的事实：

- `UTcsStateComponent` 已有 `BeginStateSlotActivationBatch()` / `EndStateSlotActivationBatch()`
- `State` 批量移除路径已经接入槽位激活批处理
- `Buff` 侧也已复用这套槽位批处理
- `Attribute` 的数值变化事件已经是数组载荷广播

因此，这份文档不应再把 `UpdateStateSlotActivation` 缺少批处理当成待办问题。

---

## 2. 仍值得保留的问题

当前真正可能还存在问题的，是“事件广播面”而不是“槽位刷新面”：

- `UTcsStateComponent::NotifyStateStageChanged`
- `UTcsStateComponent::NotifyStateRemoved`
- `UTcsStateComponent::NotifyStateApplySuccess`
- `UTcsAttributeComponent::BroadcastAttributeModifierAddedEvent`
- `UTcsAttributeComponent::BroadcastAttributeModifierRemovedEvent`
- `UTcsAttributeComponent::BroadcastAttributeModifierUpdatedEvent`

需要重点观察的场景：

- `RemoveAllStates()`
- `RemoveStatesByDefId()`
- `Buff` 合并淘汰或批量移除
- 对象池回收、角色重置、切图清理

这篇文档剩余的价值，只是记录一个问题：这些路径里，监听者是否会因为大量即时回调而做重复工作。

---

## 3. 不要误做的事

- 不要把“槽位刷新批处理”和“事件广播批处理”混成一件事。
- 不要第一版就做一个统一覆盖 `State + Buff + Attribute` 的总控 RAII Guard。
- 不要在没有澄清事件顺序和查询语义之前，直接把即时广播改成延迟 Flush。

当前 `Buff` 已拆分，后续若真要尝试，也应优先按组件内局部问题处理，而不是设计一个跨组件的总开关。

## 4. 调研思路

先回答值不值得做，而不是先设计 `FTcsScopedBatchNotification` 本体。

建议调研顺序：

1. 先做计数或 Insights 采样，确认热点路径里的回调密度。
2. 审计监听者，确认哪些订阅者依赖“立即回调”。
3. 明确三件语义问题：
	- `ModifierRemoved`、属性值变化、`StateRemoved` 的顺序能不能改。
	- 回调期间的查询 API 应该看到“最新状态”还是“已广播状态”。
	- 中间阶段是否允许被折叠，只保留最终结果。
4. 只有在证据充分时，才考虑做单侧原型：
	- `State` 事件侧
	- 或 `Attribute Modifier` 事件侧

## 5. 当前结论

- 这篇文档可以保留，但只保留为“事件广播收敛”的备忘录。
- 它不是当前默认排期项。
- 只有在采证确认回调风暴确实存在时，才值得单独开题推进。
- 真正进入实现阶段时，应以配套的模块级开发指导为准，而不是直接把这里的 RAII 备忘翻译成统一总控方案。

