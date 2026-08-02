# 设计：OngoingAttrMod 依赖链惰性重算（后续提案）

> 状态：搁置。该能力在 AttributeModifier 重构完成后，以独立 OpenSpec change 讨论和实现；当前不进入 AttributeModifier 重构 proposal。
>
> 关联：[设计：Modifier操作数与AttributeOperation模型（待审核）.md](设计：Modifier操作数与AttributeOperation模型（待审核）.md)。

## 1. 背景

当前 Ongoing AttributeModifier 的动态 Operand 采用拉取式更新：AttributeComponent 在既有重算入口执行时读取最新外部值。StateParam、Attribute 或其他依赖值变化本身不会主动触发 Attribute 重算。

这种方式简单且避免重入，但会使依赖值变更与 Ongoing 结果更新之间存在延迟。后续需要支持“仅在真实依赖变化时重算受影响 Ongoing”的惰性依赖链能力，而不是无条件每帧重算。

## 2. 目标与非目标

### 目标

- 外部依赖变更时只标脏，不在 setter 或事件回调中同步递归重算。
- 一个父 Ongoing ModifierInstance 的多 Operation 保持整体标脏、整体计算、整体原子提交。
- 同一帧多次依赖变更合并为一次重算。
- 首版仅支持目标 AttributeComponent 内的 Attribute 依赖，以及目标本地 BuffInstance 可访问的 StateParam 依赖。
- 对跨 Component、业务 UObject 或装备字段等首版未观察的依赖，保留显式请求重算入口。

### 非目标

- 不在首版建立跨 Actor 全局依赖图或全局重算调度器。
- 不将 StateParam 变化改为同步执行 Attribute 重算。
- 不使用有限次数固定点迭代掩盖 Modifier 间循环依赖。
- 不为任意业务对象建立 SourceHandle 全局解析或对象注册表。

## 3. 核心原则

```text
依赖值提交变化
  -> 增加 Revision
  -> 查询反向依赖索引
  -> 标记受影响父 Ongoing ModifierInstance 为 Dirty
  -> 请求 Component 延迟 Flush

安全时机 Flush
  -> 合并 Dirty 集合
  -> 构造依赖闭包和 Working Snapshot
  -> 稳定排序 / 拓扑排序
  -> 原子重算受影响父实例
  -> Clamp、范围传播、最终事件广播
```

`MarkDirty` 不等于立即重算。它只记录失效；实际计算必须在 Component 可控的安全时机集中执行，避免同一帧多次变更重复计算、事件重入或半提交状态。

## 4. 依赖记录

Evaluator 只能经只读 `AttributeEvaluationContext` 读取可观察值。Context 在每次读取时自动收集 DependencyKey，避免 Custom Evaluator 手工维护依赖声明。

```text
DependencyKey
  AttributeCurrentValue[TargetComponent, AttributeId]
  StateParamEffectiveValue[SourceBuffInstance, ParamTag]

DependencyRevision
  每次生产者成功提交真实值变化时递增。

OngoingModifierDependencyRecord
  ParentModifierInstanceId -> DependencyKey[]

ReverseDependencyIndex
  DependencyKey -> ParentModifierInstanceId[]

DirtyOngoingModifierSet
  等待下一次 Flush 的父实例集合。
```

依赖精度固定到父 Ongoing ModifierInstance，而不是单个 Operation。多 Operation Modifier 的原子性禁止单独刷新其中一条 Operation。

## 5. 自引用与循环

重算当前父实例时，TCS 在虚拟 Ongoing 重建中排除该父实例的全部旧结果，再读取快照。这不会建立当前实例指向自身的依赖边。

不同父 Ongoing ModifierInstance 之间形成环时，例如 A 读取 AbilityPower 写 MaxHealth，B 读取 MaxHealth 写 AbilityPower：

- 首版必须检测依赖图循环。
- 新 Apply 或本次更新必须失败并保留上一次已提交结果。
- Development / Editor 输出包含 ParentModifierInstanceId、OperationId 与依赖链的诊断。
- 不使用“最多 N 次迭代”的不确定性兜底。

## 6. StateParam 契约调整

当前“StateParam 值变化只更新自身，不主动触发下游”应在后续提案中收紧为：

```text
StateParam 值变化不得同步执行下游 Attribute 重算。
StateParam 值变化可以发布依赖失效通知，供 AttributeComponent 标脏。
实际 Attribute 重算只能由 AttributeComponent Scheduler 在安全时机执行。
```

这保持惰性和无重入原则，同时允许依赖链自动收敛。

## 7. 首版范围与显式回退

自动观察的首版依赖：

- 目标 AttributeComponent 内 Attribute 的 CurrentValue。
- 目标本地 BuffInstance 暴露的 StateParam effective 值。

不自动观察的依赖：

- Instigator 或其他 Actor 的 Attribute。
- 非目标 Component 的 Attribute。
- 装备、天赋、场景 UObject 或任意业务对象字段。

对未观察依赖，业务层在值变化后调用显式失效入口，例如：

```cpp
AttributeComponent->RequestOngoingModifierRecalculation(SourceHandle);
```

该入口只标脏匹配 SourceHandle 的 Ongoing 实例，仍由后续 Flush 统一处理。

## 8. 后续待确认

- Scheduler 的精确 Flush 时机：批次末尾、Component Tick、还是两者结合。
- 同一 Component 内依赖闭包、拓扑排序与跨父实例循环检测的具体算法。
- 生产者 Revision 的存储位置和网络同步边界。
- Attribute / Contribution / StateParam 依赖变化的统一通知接口。
- Ongoing 来源 StateParam 失效时的最终策略：保留旧结果、移除 Ongoing 或本轮失败。
- 与 Buff Period、StateTree reselect、AttributeRange 传播的执行顺序。
- `RequestOngoingModifierRecalculation` 的 Blueprint 公开面、错误结果和批量语义。

## 9. 当前重构的兼容边界

当前 AttributeModifier 重构不实现 DependencyKey、Revision、反向索引、Dirty 集合或 Scheduler。它只需确保：

- Ongoing 的 EvaluatorContext 与持续实例身份可承载未来依赖记录。
- Ongoing 重算可以由一个受控 Component 内部入口触发。
- 当前重算维持拉取式惰性行为，不承诺依赖值变化的自动传播。
- 不将任何临时全帧 Tick 或跨 Actor 轮询作为过渡方案。
