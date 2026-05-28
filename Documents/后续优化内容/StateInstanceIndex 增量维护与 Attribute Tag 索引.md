# StateInstanceIndex 增量维护与 Attribute Tag 索引

> 文档定位：TCS 当前版本下的剩余优化评估。
>
> 当前结论：
>
> - `StateInstanceIndex` 批量清理仍有少量价值，但不再是旧文所说的“低风险顺手清理”，因为当前移除回调会观测索引状态。
> - `Attribute` 侧持久化 `Tag` 索引在当前架构下不建议推进；现有 `ManagerSubsystem` 映射已经覆盖了最核心的 Tag 入口需求。

---

## 1. 当前实现基线

### 1.1 `StateInstanceIndex` 现状

`FTcsStateInstanceIndex` 当前维护的并不是旧文里那三项，而是四组数据：

- `Instances`
- `InstancesById`
- `InstancesByName`
- `InstancesBySlot`

写路径仍然是逐条维护：

- `AddInstance()`
- `RemoveInstance()`
- `RefreshInstances()`

`UTcsStateComponent::RemoveAllStates()` 当前流程也比旧文假设更完整：

1. 先收集所有有效 `State`
2. 开启 `BeginStateSlotActivationBatch()`
3. 逐个调用 `RequestStateRemoval()`
4. 在 `FinalizeStateRemoval()` 中依次执行：
   - StopStateTree / 标记 `Expired`
   - `StateTreeTickScheduler.Remove()`
   - `StateInstanceIndex.RemoveInstance()`
   - `RemoveModifiersBySourceHandle()`
   - `NotifyStateStageChanged()` / `NotifyStateRemoved()`
   - 从 Slot 中移除，并请求槽位刷新
5. 最后 `EndStateSlotActivationBatch()`

也就是说，当前 `RemoveAllStates()` 并不只是“把容器清空”，它还承担着**事件语义与查询语义**。

### 1.2 `Attribute Tag` 现状

当前 `Attribute` 数据模型已经和旧文写作时不同：

- `UTcsAttributeDefinition` 只有一个可选的 `AttributeTag`
- `UTcsAttributeManagerSubsystem` 已维护 `AttributeTagToName` / `AttributeNameToTag`
- `UTcsAttributeComponent` 已提供 `AddAttributeByTag()`
- 精确 Tag 入口本质上已经可以走 `Tag -> Name -> Attributes.Find(Name)` 这条链路

因此，旧文里“组件内部需要额外的 Tag 索引来承接 Tag 入口 API”这个前提，已经不成立。

---

## 2. `StateInstanceIndex::ClearAll` 现在还剩多少价值

### 2.1 仍有价值的部分

这部分优化并没有完全失效。`RemoveAllStates()` 在对象池回收、Actor 重置、切图清理等路径上，仍然会做一轮逐条索引移除：

- `Instances.Remove()`
- `InstancesById.Remove()`
- `InstancesByName` 内部数组移除
- `InstancesBySlot` 内部数组移除

如果一个实体身上经常挂很多 `State`，且批量回收频繁，这里确实存在一些重复工作。

### 2.2 旧文低估了语义风险

旧文最大的问题，不是“少算了一个 `InstancesById`”，而是把这件事当成了几乎没有行为影响的容器优化。但当前代码并不是这样：

- `FinalizeStateRemoval()` 会先执行 `StateInstanceIndex.RemoveInstance()`
- 然后才广播 `NotifyStateStageChanged()` / `NotifyStateRemoved()`

这意味着监听者若在回调里调用：

- `GetStatesByDefId()`
- `GetStatesInSlot()`
- `HasStateWithDefId()`

看到的是“移除后的索引结果”。

如果未来在批量路径里抑制逐条 `RemoveInstance()`，改成最后一次性 `ClearAll()`，那么回调中的查询结果会立刻变化。这已经不是纯性能优化，而是**可观察行为变更**。

### 2.3 当前更现实的判断

因此，`ClearAll()` 仍然可以是一个候选方向，但它不再适合被描述为“低风险、顺手一起做”的小清理。

---

## 3. `StateIndex` 更合理的后续方向

如果后续 Profiling 证明 `RemoveAllStates()` 真的在这里有明显成本，更稳妥的路线应当是分层推进：

### 3.1 先把 `ClearAll()` 当成容器原语，而不是直接接到批量移除路径

先给 `FTcsStateInstanceIndex` 提供一个明确的全量清空 API，本身问题不大；但它应该先被理解为：

- 供未来 destructive reset / rebuild 使用
- 不默认等同于“可以直接替换当前 `RemoveAllStates()` 的逐条维护”

### 3.2 若真要优化 `RemoveAllStates()`，需要先定义语义

到那一步时，至少要回答：

1. 批量移除期间，查询 API 应该看到“最新状态”还是“已广播状态”
2. 是否允许回调里查询到尚未物理移除但已逻辑失效的实例
3. 是否需要在 bulk 模式下给查询 API 增加“待移除过滤”逻辑

这类设计一旦引入，就已经不是旧文里描述的单纯容器重置了。

### 3.3 还要考虑整体收益上限

即便 `StateInstanceIndex` 做了批量优化，`RemoveAllStates()` 当前仍然要逐条做：

- `StateTreeTickScheduler.Remove()`
- Modifier 清理
- 事件广播
- Slot 成员移除

因此这项优化即便成立，收益也多半是“局部改善”，不会单靠一个 `ClearAll()` 就把整条路径大幅提速。

---

## 4. `Attribute Tag` 索引为什么不再适合当前版本

旧文对 `Attribute Tag` 索引的想象，默认了两个前提：

1. 组件内部缺少 Tag 入口
2. 一个属性可能天然需要参与较复杂的 Tag 分组查询

当前代码并不满足这两个前提：

- 组件已经有 `AddAttributeByTag()`
- `ManagerSubsystem` 已经负责精确 Tag 与 `AttributeName` 的映射
- 当前每个属性定义只有一个语义性 `AttributeTag`
- 当前没有任何已知的 `GetAttributesByTag()` 调用点或性能热点

对“精确 Tag 找单个属性”这件事来说，持久化一个组件内 `FTcsAttributeIndex` 基本只是**重复存储已有信息**。

---

## 5. 当前更合理的 `Attribute` 路线

当前已经确认并落地的方向，不是上索引，而是补轻量级 Tag 查询便捷 API：

- `HasAttributeByTag()`
- `GetAttributeValueByTag()`
- `GetAttributeBaseValueByTag()`

这些 API 可以直接复用：

- `UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag()`
- 组件现有的 `Attributes.Find()`

这样能先满足大部分“按 Tag 查询单个属性”的需求，而不引入新的写路径维护成本。

---

## 6. 如果未来真出现“按 Tag 分组查询”需求，应该先重审数据模型

只有在以下两个条件同时成立时，才值得重新讨论 `FTcsAttributeIndex`：

1. 游戏层确实出现了“按 Tag / 父 Tag / TagQuery 批量取属性”的明确需求
2. Profiling 证明现有按需遍历真的成了热点

届时优先要回答的不是“索引怎么写”，而是：

- 单个 `AttributeTag` 是否足够，还是需要多 Tag 归属
- 需求是精确 Tag，还是父 Tag / `FGameplayTagQuery`
- 返回结果应该是 `AttributeName`、实例引用，还是快照数据
- 当前单实体属性数量是否大到值得维护持久化索引

在这些问题没有明确之前，旧文里的 `FTcsAttributeIndex` 设计是偏超前的。

---

## 7. 当前建议总结

| 议题 | 当前判断 | 建议 |
|------|----------|------|
| `StateInstanceIndex::ClearAll` | 仍有少量价值，但风险不再是“低且透明” | 低优先级；先 Profiling，再决定是否要引入 bulk 语义设计 |
| `FTcsAttributeIndex` | 对当前 TCS 模型基本不成立 | 先不立项；当前已补 Tag 查询便捷 API，后续仅在出现分组查询需求时再判断是否需要索引 |
