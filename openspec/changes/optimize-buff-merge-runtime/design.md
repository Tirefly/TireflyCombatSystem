## 背景

当前 Buff 合并编排仍然挂在共享宿主 `UTcsStateComponent` 的槽位激活刷新链路上：

```text
UTcsStateComponent::UpdateStateSlotActivation
  -> Broadcast OnPrepareStateSlotActivation
     -> UTcsBuffComponent::HandleOwnerStateSlotActivation
        -> ProcessBuffMerging(StateSlot)
```

而现在的 `ProcessBuffMerging()` 每次都会从头重建合并桶：

1. 扫描 `StateSlot->States`
2. 解析出 BuffInstance
3. 重建临时的 `StateDefId -> TArray<UTcsBuffInstance*>`
4. 对每个发现的组执行 `MergeBuffStateGroup()`
5. 通过共享 removal 链移除 merged-out Buff

这条路径对当前内建 merger 的行为是正确的，但它有三个结构性缺陷：

- 槽位本身没有持久化的合并组运行时状态
- 运行时无法区分“这个组是存在的”还是“这个组是脏的”
- 合并器自己也无法声明“我到底依赖哪些运行时变化”

前面的对比文档已经把问题拆成“最小增量方案”和“完整运行时方案”两类。本 change 采用完整运行时方案作为最终目标，但通过分阶段落地来降低初始落地风险，让运行时先稳定，再逐步放开所有脏源。

## 目标 / 非目标

- 目标：
  - 让 Buff 合并的常态路径不再重复做整槽位 regroup
  - 保持当前合并器输出协议和 merged-out removal 行为不变
  - 让 Buff 合并失效通过脏原因和依赖标记变得显式且可解释
  - 为未来对执行阶段或 gate 敏感的合并器预留能力，而不是再次重开运行时模型
  - 通过两个阶段推进完整方案，使 Phase 1 能先验证缓存 / 索引正确性，再让 Phase 2 扩大失效来源
- 非目标：
  - 重做 `UseNewest`、`UseOldest`、`NoMerge`、`StackByInstigator` 的业务语义
  - 新增编辑器创作入口或 merge 配置资产工具
  - 在本 change 中解决 Skill 重复激活或 Skill 侧冲突策略
  - 在槽位内运行时之外再做跨槽位或全局 Buff 合并缓存

## 关键决策

- 决策：在 `FTcsStateSlot` 上增加槽位内 Buff 合并运行时状态
  - 原因：slot 本身已经是合并编排、gate 状态和运行时状态容器的共享边界，把运行时缓存放在 slot 上最符合局部性，也避免再引入一个平行的全局合并注册表。
  - 考虑过的替代方案：
    - 把所有合并组缓存全都塞进 `UTcsBuffComponent`：放弃，因为运行时最终仍然要按 slot 建模，并与 slot rebuild 保持一致。
    - 只做 per-instance 回指，group 按需懒重建：放弃，因为这本质上还是在回退到重复 regroup。

- 决策：脏原因用 flags，而不是单一枚举值
  - 原因：同一个组在同一帧或同一条回调链里可能因为多个原因同时变脏。用 bitflag 才不会丢信息，也更利于诊断。
  - 考虑过的替代方案：
    - 用最后一次写入覆盖之前的脏原因：放弃，因为这会让行为更难解释，也会掩盖真实失效来源。

- 决策：新增合并器依赖声明，但不修改现有 `Merge()` 输出协议
  - 原因：当前运行时确实需要知道“某个脏原因对这个合并器是否相关”，但实际合并工作仍然可以继续通过现有 input/output 数组表达。
  - 考虑过的替代方案：
    - 立刻把 `Merge()` 重做成新的重上下文 API：放弃，因为这会明显扩大迁移成本，而不是完整运行时模型落地所必需的前置条件。

- 决策：默认依赖声明必须保守
  - 原因：自定义合并器或 Blueprint 合并器可能不会立刻覆盖新的依赖 API。默认宽一点是安全的，至少不会悄悄漏掉失效。
  - 考虑过的替代方案：
    - 默认给一个很窄的依赖集合：放弃，因为一旦合并器忘记显式声明，就可能得到陈旧合并结果。

- 决策：Phase 1 和 Phase 2 共享同一套最终运行时结构
  - 原因：这次不是要先落一个最小增量死胡同，再未来推倒重来。Phase 1 必须直接把持久运行时模型搭起来，Phase 2 只是扩展脏输入和诊断面。
  - 考虑过的替代方案：
    - 先实现最小增量方案，后面再整体替换：放弃，因为这会重复一遍迁移工作，也会推迟最终结构的正确性验证。

## 运行时模型

最终模型应围绕槽位内合并组运行时展开，形态大致如下：

```cpp
enum class ETcsBuffMergeDirtyReason : uint8
{
    None = 0,
    MembershipChanged = 1 << 0,
    RuntimeValueChanged = 1 << 1,
    ExecutionStageChanged = 1 << 2,
    SlotGateChanged = 1 << 3,
    ForceRebuild = 1 << 4,
};

enum class ETcsBuffMergeDependencyFlags : uint8
{
    None = 0,
    MemberSet = 1 << 0,
    ApplyTimestamp = 1 << 1,
    Instigator = 1 << 2,
    RuntimeStack = 1 << 3,
    ExecutionStage = 1 << 4,
    SlotGateState = 1 << 5,
};

struct FTcsBuffMergeGroupRuntime
{
    FName StateDefId;
    TArray<TWeakObjectPtr<UTcsBuffInstance>> Members;
    ETcsBuffMergeDirtyReason DirtyReasons = ETcsBuffMergeDirtyReason::None;
    ETcsBuffMergeDependencyFlags DependencyFlags = ETcsBuffMergeDependencyFlags::None;
};
```

而 `FTcsStateSlot` 至少应承载：

```cpp
TMap<FName, FTcsBuffMergeGroupRuntime> BuffMergeGroups;
TSet<FName> DirtyBuffMergeStateDefIds;
bool bBuffMergeRequiresFullRebuild = false;
```

具体容器类型在实现时仍可调整，但这里要先锁定的架构结论已经明确：

- 槽位内的持久合并组运行时
- 显式脏状态记账
- 安全的 rebuild fallback

## Merger 依赖声明

`UTcsBuffMerger` 需要一个新的查询接口，例如：

```cpp
virtual ETcsBuffMergeDependencyFlags GetDependencyFlags() const;
```

推荐行为：

- 基类实现返回一个保守上界集合，保证自定义合并器即使暂时不覆盖也仍然正确
- 内建 merger 则覆盖为更精确的声明

预期的内建映射：

- `UseNewest`：`MemberSet | ApplyTimestamp`
- `UseOldest`：`MemberSet | ApplyTimestamp`
- `NoMerge`：`MemberSet`
- `StackByInstigator`：`MemberSet | Instigator | RuntimeStack`

有了这层声明，运行时才能判断“这个脏组在当前 dirty flags 下是否真的需要重新处理”。

## 处理流程

稳定态下的 `ProcessBuffMerging()` 应收敛为：

```text
ProcessBuffMerging(StateSlot)
  -> if full rebuild requested, rebuild slot-local merge groups from StateSlot->States
  -> if no dirty groups remain, return immediately
  -> snapshot dirty ids for this pass
  -> clear only the pass-local dirty set from the slot
  -> for each dirty group runtime
       -> compact invalid weak pointers / stale members if needed
       -> resolve the merger and dependency flags
       -> skip if current dirty reasons do not intersect merger dependencies
       -> execute MergeBuffStateGroup on the cached members
       -> accumulate merged-out Buffs
       -> clear processed dirty reasons that were consumed this pass
  -> remove merged-out Buffs through the existing shared removal chain
```

关键细节：

- 这次处理轮对 dirty ids 的处理必须是“先 snapshot，再清理本轮局部数据”，否则 re-entrant removal 过程中产生的新脏工作可能会被当前轮误清掉，而不是留给下一轮。

## 分阶段落地

### Phase 1：最终运行时模型初始落地

Phase 1 先引入最终结构和安全 rebuild 路径，但只把由成员变更驱动的失效接到正常处理链里。

范围：

- 新增槽位内合并运行时结构
- 新增脏原因 flags 和依赖声明 API
- 让 group rebuild 和 dirty-set 维护在 add/remove 与 slot rebuild 路径上都足够稳健
- 重写 `ProcessBuffMerging()`，让它消费缓存好的组，而不是每轮都整槽位 regroup
- 除了显式 force-rebuild fallback 外，暂时不接运行时敏感脏源

结果：

- 运行时形态本身已经是最终形态
- 可以先验证槽位内缓存的正确性，再继续扩大失效来源

### Phase 2：运行时敏感失效与诊断面

Phase 2 再接剩余脏源和可解释性观察面。

范围：

- 把 Buff stack change 接到 `RuntimeValueChanged`
- 把 stage change 接到 `ExecutionStageChanged`
- 把 slot gate change 接到 `SlotGateChanged`
- 新增有针对性的调试 / 诊断观察面，让开发者能看见“哪些组脏了，以及为什么脏”

结果：

- 运行时正式支持依赖 stage、gate 或运行时数值的合并器
- 合并失效从隐式副作用变成可解释行为

## 风险 / 取舍

- 最大风险是索引一致性：缓存组成员关系绝不能和 `FTcsStateSlot::States` 漂移脱节。
- 脏状态跟踪会在 merged-out removal 和 slot refresh callback 周围引入可重入风险。
- 保守的默认依赖声明有利于正确性，但在内建合并器精确覆盖之前，优化收益会略打折。
- Phase 1 会故意带着一部分供 Phase 2 使用的休眠能力，这是一种有意识的取舍，用来避免把运行时模型重建两遍。

## 手动编辑器测试策略

- 等待开发者手动执行编辑器测试，覆盖 membership-driven merge processing 与 merged-out removal 行为。
- 等待开发者手动执行编辑器测试，确认 forced rebuild 能把 group runtime 和当前 slot state 重新同步。
- 在 Phase 2 中，等待开发者手动执行编辑器测试，覆盖 stack / stage / gate dirty signal 与 merger dependency flags 的交互关系。
- 每个阶段完成后，都用 `TireflyGameplayUtilsEditor Win64 Development` 编译验证一次。