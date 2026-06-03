# 事件分发优化开发指导（State-Buff-Attribute）

> 文档定位：在确认“事件广播收敛”值得推进之后，作为 `State`、`Buff`、`Attribute` 三个模块的后续开发指导。
>
> 适用前提：基于当前 TCS 实现现状，而不是旧版 `Buff` 未拆分时期的统一 RAII 设想。

---

## 1. 总体指导

当前不建议把三个模块做成一个统一的 `FTcsScopedBatchNotification` 总控系统。更合理的目标是：

- 采用“模块内局部收敛”而不是“跨模块统一收敛”
- 区分“内部运行时联动”和“对外公共事件”两条分发路径
- 内部路径优先保持立即语义，对外路径再评估是否需要批量化 / 去重

用更直接的话说：

- `State` 是上游事件源头
- `Buff` 是挂在 `State` 之上的派生层
- `Attribute` 已经有一半批处理基础，不应再强行套入总控 Guard

因此，后续优化的目标不应是“做一个统一 RAII 类”，而应是“把每个模块自己的事件面收敛到合适的粒度”。

---

## 2. 当前实现对应的关键判断

### 2.1 State

当前 `State` 的公共事件面主要包括：

- `OnStateStageChanged`
- `OnStateDeactivated`
- `OnStateApplySuccess`
- `OnStateApplyFailed`
- `OnStateRemoved`
- `OnStateLevelChanged`
- `OnSlotGateStateChanged`
- `OnStateParameterChanged`

这些事件当前都是逐条即时广播。与此同时，`Buff` 组件还直接订阅了其中的 `OnStateApplySuccess`、`OnStateRemoved`、`OnStateStageChanged`、`OnSlotGateStateChanged` 作为自身运行时联动入口。

这说明 `State` 当前既承担“系统内部联动”又承担“对外通知”两种职责，后续若要优化，必须先把这两层语义拆开。

### 2.2 Buff

`Buff` 的公共事件面主要包括：

- `OnBuffRuntimeDelta`
- `OnBuffRemoved`

但 `Buff` 自身的关键运行时动作并不适合延迟：

- `RegisterBuffInstance()` / `UnregisterBuffInstance()`
- `MarkBuffMergeGroupDirty()`
- 响应 `State` 的阶段变化与 Slot Gate 变化

因此，`Buff` 的优化重点应是“公共事件合并”，而不是“内部维护延后”。

### 2.3 Attribute

`Attribute` 当前已经存在明显的双态：

- `OnAttributeValueChanged` / `OnAttributeBaseValueChanged` 已经是数组载荷广播
- `OnAttributeModifiersAdded` / `Removed` / `Updated` 已改为批量载荷广播
- `OnAttributesReachedBoundary` 已改为批量载荷广播

这说明 `Attribute` 现在已经把公共事件面补齐到了统一风格，后续重点不再是事件面补齐，而是验证消费方是否都迁到了新的批量接口。

---

## 3. 模块级开发指导

## 3.1 State 模块

`State` 模块的目标不是先做批量化，而是先做事件分层。

建议：

- 先拆出“内部原生事件路径”和“对外公共事件路径”
- 内部路径继续保持立即语义，供 `Buff` 和后续运行时模块使用
- 对外公共事件路径再评估是否需要批量化 / 去重

第一阶段建议优先纳入收敛评估的公共事件：

- `StateStageChanged`
- `StateRemoved`
- `StateApplySuccess`
- `StateApplyFailed`

第一阶段不建议一起改的事件：

- `StateLevelChanged`
- `StateParameterChanged`
- `SlotGateStateChanged`

原因很简单：这些事件的频率和语义更离散，当前没有足够证据说明它们也需要批量收敛。

额外指导：

- `OnStateDeactivated` 更适合被视为 `StageChanged` 的派生语义，而不是单独再做一套批量队列
- Flush 边界应优先依附现有结构性批处理，例如最外层 `EndStateSlotActivationBatch()` 或明确的批量移除收束点
- 不能为了“好看”的批量化而改变回调期间的查询语义；当前若监听者在回调里查询状态，应尽量保持与现状一致的可见结果

一句话总结：`State` 先做“分层”，再做“收敛”。

---

## 3.2 Buff 模块

`Buff` 模块不应被设计成第二个“事件源头总线”，它更适合作为派生层做结果合并。

当前已完成：

- 内部注册、注销、脏标记、合并重建相关逻辑继续保持立即执行
- 不要让 `Buff` 的核心运行时语义依赖延迟后的公共 `Buff` 事件
- 公共事件侧已经收口为两类结果事件：`OnBuffRuntimeDelta` 与 `OnBuffRemoved`
- 旧的逐条 `OnBuffStackChanged` / `OnBuffMaxStackCountChanged` / `OnBuffPeriodChanged` / `OnBuffDurationRefreshed` 公共事件接口已经移除

当前批量事件语义：

- `OnBuffRuntimeDelta`：叠层、最大叠层、周期、持续时间等运行时变化
- `OnBuffRemoved`：终态移除事件

当前合并规则：

- 同一批内多次叠层变化，只保留 `Old -> Final`
- 同一批内多次最大叠层变化，只保留 `Old -> Final`
- 同一批内多次周期变化，只保留 `Old -> Final`
- 多次 Duration 刷新，只保留最终剩余时间
- 若同一批内已经发生 `Removed`，则该 Buff 前面的运行时变化直接失效，只保留终态移除事件

当 `State` 侧完成“内部事件 / 公共事件”分层后，`Buff` 优先接到 `State` 的内部立即路径，而不是继续依赖延迟后的公共动态委托。

一句话总结：`Buff` 只合并公开结果，不延后内部维护。

---

## 3.3 Attribute 模块

`Attribute` 模块是三个模块里最适合优先深化的，因为它已经有批量提交骨架。

当前已完成：

- 沿用现有 `BatchId + 提交阶段统一广播` 的方向完成了 `Modifier` / `Boundary` 事件批量化
- 旧的单条 `Modifier` / `Boundary` 公共事件接口已经移除，统一收口到批量事件面
- `SetAttributeBaseValue` / `SetAttributeCurrentValue` / `ResetAttribute` 也改成了“静默求最终态 + 统一补发最终差异”的方式，避免局部路径和批处理主路径语义分裂

建议的公共事件顺序契约：

1. 先发 Modifier 批量变化
2. 再发 BaseValue / CurrentValue 的变更数组
3. 最后发 Boundary 批量结果

当前实现说明：

- `Modifier` 变更路径已经按这套顺序对外广播
- `BaseValue` / `CurrentValue` / `Boundary` 的提交阶段顺序也已对齐为“值变化在前，边界结果在后”
- 下一步不应再继续改 `Attribute` 事件面本身，而应转向清理消费方和推进 `Buff` 模块

这样做的好处是：

- 监听者先知道“为什么变”
- 再看到“值怎么变”
- 最后再看到“是否命中边界”

额外指导：

- 不要为了统一风格，把 `Attribute` 再外包进跨模块 RAII Guard
- 当前索引维护、工作集推导、提交阶段已经相对清晰，后续应在这个基础上补齐剩余事件，而不是推倒重来

一句话总结：`Attribute` 应该补齐现有批量体系，而不是重新设计一套总控机制。

---

## 4. 推荐实施顺序

建议顺序如下：

1. 先改 `State` 的事件分层
2. 再补 `Attribute` 的 Modifier / Boundary 批量事件
3. 最后收敛 `Buff` 的公共运行时变化事件

当前进度：

- `State` 第一阶段分层已完成
- `Attribute` 的 Modifier / Boundary 批量事件已完成
- 下一步主线应转到 `Buff`

这个顺序不能反过来。原因是：

- `State` 是源头，不先分层，`Buff` 就一直挂在公共动态委托上
- `Attribute` 最独立，且已有一半批处理骨架，适合作为第二步
- `Buff` 的最终事件形态，取决于 `State` 是否已经把内部联动和公共通知拆开

---

## 5. 后续调研与验证清单

在继续推进后续模块之前，至少先完成以下核对：

1. 对这些路径做计数或 Unreal Insights 采样：
   - `RemoveAllStates()`
   - `RemoveStatesByDefId()`
   - `Buff` 合并淘汰 / 批量移除
   - 对象池回收、角色重置、切图清理
2. 审计当前监听者，确认哪些订阅者依赖“立即回调”语义
3. 明确查询语义：回调期间查询 API 看到的是“最新状态”还是“已广播状态”
4. 明确顺序契约：`ModifierRemoved`、属性值变化、`StateRemoved`、`BuffRemoved` 之间哪些顺序不能改
5. 第一版只允许做单侧或双侧局部原型，不允许一口气同时重写三个模块

---

## 6. 非目标

以下内容不应被混入这个议题：

- 重做 `State` 槽位激活批处理
- 设计一个统一覆盖 `State + Buff + Attribute` 的系统级 RAII Guard
- 一次性重写全部公共事件面
- 顺手修改网络同步语义或调试 UI 语义

这份指导的目标只限于：在保住当前运行时语义的前提下，收敛高频公共事件的分发成本。