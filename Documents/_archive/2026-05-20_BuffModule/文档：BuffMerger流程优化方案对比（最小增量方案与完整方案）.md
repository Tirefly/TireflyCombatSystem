# BuffMerger 流程优化方案对比（最小增量方案与完整方案）

## 文档目的

本文不扩展具体业务玩法需求，只聚焦当前 TCS 代码里 `BuffMerger` 的执行成本与可行优化路径。

本文的目标有三个：

1. 解释当前 `BuffMerger` 为什么会在槽位刷新时表现为“全量合并”。
2. 给出两个可落地的优化方案：
   - 最小增量方案
   - 完整实现方案
3. 对两个方案做清晰对比，并给出当前阶段的推荐结论。

本文讨论的是“运行时框架优化”，不是某个具体 Buff 业务规则的设计稿。

## 一、当前实现为什么会全量执行

当前 Buff 合并的真实入口不是某个独立的 `BuffMerger::Execute()`，而是 State 主链中的槽位激活刷新扩展点：

```text
UTcsStateComponent::UpdateStateSlotActivation
  -> Broadcast OnPrepareStateSlotActivation
     -> UTcsBuffComponent::HandleOwnerStateSlotActivation
        -> ProcessBuffMerging(StateSlot)
```

当前实现下，只要指定槽位发生一次激活刷新，`UTcsBuffComponent::ProcessBuffMerging()` 就会执行。

而当前 `ProcessBuffMerging()` 的逻辑是：

1. 遍历整个 `StateSlot->States`
2. 按 `StateDefId` 重新分组
3. 对每个分组都调用 `MergeBuffStateGroup()`
4. 汇总 `MergedOutBuffs`
5. 再走统一移除链

这意味着当前成本主要来自两件事：

1. 每次都扫描整个槽位
2. 每次都对所有 DefId 分组都重新执行 merge 判断

因此，当前问题的本质不是“某个具体 merger 算法太慢”，而是：

- 槽位刷新和 Buff merge 之间没有“脏组”概念
- Buff 侧不知道这次刷新到底是谁引起的
- 所以只能保守地把整个槽位里的 Buff 全部重算一遍

## 二、优化目标

本轮优化的目标不是重写 Buff 合并架构，而是先降低当前全量扫描与全量分组的成本。

本轮优化应满足以下要求：

1. 不改变当前 `UTcsBuffMerger::Merge()` 的业务语义
2. 不改变当前 `MergedStates` / `MergedOutBuffs` 的返回协议
3. 不引入 Buff 模块反向侵入 State Core 的新耦合
4. 不把未来可能存在的 stage-sensitive merger 需求提前固化成复杂框架
5. 优先选择可验证、可回退、低风险的路径

## 三、方案 A：最小增量方案

### 3.1 核心思想

最小增量方案的核心不是缓存 merge 结果，而是只记录：

- 这个槽位里，哪些 `StateDefId` 对应的 Buff 组在本轮“脏了”

然后让 `ProcessBuffMerging()` 只处理这些脏组，而不是处理整个槽位的所有组。

### 3.2 需要新增的数据

在 `FTcsStateSlot` 中新增一个运行时字段，例如：

```cpp
TSet<FName> DirtyBuffMergeStateDefIds;
```

它只表达一件事：

- 当前槽位里，哪些 `StateDefId` 的 Buff 分组需要重新执行 merge

这个字段不保存合并结果，不保存 survivor，也不缓存 bucket，只做脏标记。

### 3.3 脏标记时机

最小增量方案只在“成员关系变化”时标脏。

也就是：

1. `TryAssignStateToStateSlot()` 中，状态成功加入槽位后，把它的 `StateDefId` 标脏。
2. `FinalizeStateRemoval()` 中，状态从槽位移除前后，把它的 `StateDefId` 标脏。

这意味着它只关心：

- 哪个 DefId 组里新增了成员
- 哪个 DefId 组里减少了成员

### 3.4 新的 ProcessBuffMerging 流程

最小增量方案下，`ProcessBuffMerging()` 的工作顺序会变成：

```text
ProcessBuffMerging(StateSlot)
  -> 如果 DirtyBuffMergeStateDefIds 为空，直接返回
  -> 拷贝当前 Dirty 集合到 LocalDirtyIds
  -> 先清空 StateSlot 内的 Dirty 集合
  -> 单次遍历 StateSlot->States
     -> 只收集 StateDefId 属于 LocalDirtyIds 的 Buff 状态
  -> 对收集到的每个脏组执行 MergeBuffStateGroup
  -> 汇总 MergedOutBuffs
  -> RemoveMergedOutBuffs
```

这里有一个实现细节非常关键：

- 必须“先拷贝，再清空”当前槽位的 dirty 集合

原因是 merger 过程中如果又淘汰了一些状态，这些状态进入统一移除链后会再次触发槽位刷新；新的脏标记必须留给下一轮刷新，而不能被当前轮误清掉。

### 3.5 可以顺手加的两个便宜短路

在最小增量方案里，建议顺手补两个低风险短路：

1. 组内数量 `<= 1` 的，直接跳过，不执行 merger
2. 组里首个实例不是 `UTcsBuffInstance` 的，直接跳过

这两个短路都不会改变当前业务语义，但能继续减少不必要的 merger 分发成本。

### 3.6 最小增量方案的优点

1. 改动小，局部闭合
2. 不改 `UTcsBuffMerger` 接口
3. 不需要 BuffInstance 在各种运行时 setter 里反向通知 BuffComponent
4. 不需要新建复杂索引结构
5. 对当前已经存在的 `UseNewest` / `UseOldest` / `StackByInstigator` 很匹配

### 3.7 最小增量方案的缺点

1. 仍然要扫描一次整个 `StateSlot->States`
2. 只是避免“所有组都 merge”，没有避免“整槽位扫描”
3. 默认假设当前 merger 只依赖组成员关系，而不依赖各种外部运行时变化
4. 如果未来 merger 开始依赖阶段、Gate、运行时参数变化，这个方案需要继续扩展脏标记来源

### 3.8 为什么它适合当前 TCS

按当前仓库里的 merger 来看：

1. `UseNewest` 依赖 ApplyTimestamp 与组成员
2. `UseOldest` 依赖 ApplyTimestamp 与组成员
3. `StackByInstigator` 依赖组成员、Instigator、当前 StackCount

它们都不依赖槽位 Gate 是否打开，也不依赖当前实例处于 `Active` / `Pause` / `HangUp` 的哪种阶段。

所以对当前实现来说，“成员关系变化才重算组”是成立的。

## 四、方案 B：完整实现方案

### 4.1 核心思想

完整实现方案不再只记录“哪些 DefId 脏了”，而是进一步把 Buff merge 从“整槽位扫描”演进成“按组维护运行时索引”。

也就是说，不只知道“哪个组脏了”，还直接维护：

- 这个槽位里有哪些 Buff merge group
- 每个 group 当前有哪些成员
- 每个 group 为什么脏了
- 这个 group 的 merger 依赖哪些运行时信息

### 4.2 需要新增的数据结构

完整方案建议引入类似下面的运行时结构：

```cpp
enum class ETcsBuffMergeDirtyReason : uint8
{
    None,
    MembershipChanged,
    RuntimeValueChanged,
    ExecutionStageChanged,
    SlotGateChanged,
    ForceRebuild,
};

enum class ETcsBuffMergeDependencyFlags : uint8
{
    MemberSet,
    ApplyTimestamp,
    Instigator,
    RuntimeStack,
    ExecutionStage,
    SlotGateState,
};

struct FTcsBuffMergeGroupRuntime
{
    FName StateDefId;
    TArray<TWeakObjectPtr<UTcsBuffInstance>> Members;
    ETcsBuffMergeDirtyReason DirtyReason;
    ETcsBuffMergeDependencyFlags DependencyFlags;
};
```

然后在 `FTcsStateSlot` 中维护：

```cpp
TMap<FName, FTcsBuffMergeGroupRuntime> BuffMergeGroups;
TSet<FName> DirtyBuffMergeStateDefIds;
bool bBuffMergeRequiresFullRebuild;
```

### 4.3 完整方案下的脏标记来源

完整方案不再只在 add/remove 时标脏，而是允许从多种来源标脏：

1. 成员变化：状态进入或离开槽位
2. 运行时数值变化：例如某些 merger 若真的依赖 StackCount 或其他运行时值
3. 状态阶段变化：`Active` / `Pause` / `HangUp` 切换
4. 槽位 Gate 变化
5. 强制重建：例如槽位重建、定义重载、缓存失效

### 4.4 完整方案对 Merger 的新要求

完整方案如果要做得完整，最好让 merger 自己声明依赖项。

比如在 `UTcsBuffMerger` 上增加一个查询接口：

```cpp
virtual ETcsBuffMergeDependencyFlags GetDependencyFlags() const;
```

这样框架就能知道：

1. 这个 merger 只关心成员关系
2. 还是也关心运行时 stack
3. 还是也关心执行阶段
4. 还是也关心槽位 Gate 状态

这一步是完整方案和最小增量方案最大的架构差异。

### 4.5 完整方案下的处理流程

完整方案下，`ProcessBuffMerging()` 不再需要整槽位重新扫描分组，而是：

```text
ProcessBuffMerging(StateSlot)
  -> 如果没有脏组且不需要 full rebuild，直接返回
  -> 只取 DirtyBuffMergeStateDefIds 对应的 GroupRuntime
  -> 对每个脏组判断是否真的受当前 DirtyReason 影响
  -> 执行 MergeBuffStateGroup
  -> 更新组运行时缓存
  -> 处理淘汰者移除
```

这样它真正做到的是：

- 既不整槽位扫描
- 也不整槽位重建所有 DefId 分组

### 4.6 完整方案的优点

1. 性能上限更高
2. 能真正避免整槽位扫描
3. 为未来可能存在的 stage-sensitive merger 留好扩展点
4. 脏原因与 merger 依赖声明后，框架行为更可解释

### 4.7 完整方案的缺点

1. 改动面显著更大
2. 需要在 State Core、BuffComponent、BuffInstance 之间增加更多运行时协作
3. 更容易引入一致性 bug，例如：
   - group 索引与实际槽位状态不同步
   - dirty 清理时机错误
   - merger 依赖声明不完整
4. 需要重新定义哪些运行时 setter 会触发 merge dirty
5. 现在就做完整方案，明显超出“先优化当前全量执行”的最小目标

## 五、两个方案的直接对比

| 维度 | 最小增量方案 | 完整实现方案 |
|---|---|---|
| 核心思路 | 只引入按 `StateDefId` 的脏组概念 | 维护按组运行时索引和依赖声明 |
| 是否改变 `UTcsBuffMerger` 接口 | 否 | 大概率需要补依赖声明接口 |
| 是否整槽位扫描 | 是，但只处理脏组 | 否，理论上只处理脏组运行时索引 |
| 改动面 | 小 | 大 |
| 一致性风险 | 低 | 中到高 |
| 对当前 merger 的适配度 | 高 | 高 |
| 对未来 stage-sensitive merger 的支持 | 弱，需要后续加入口 | 强，天然可扩展 |
| 适合当前阶段吗 | 是 | 暂时不建议立刻做 |

## 六、对当前 TCS 的推荐结论

当前更推荐先落地最小增量方案，而不是一步做到完整实现方案。

原因有四个：

1. 当前现有 merger 语义简单，主要依赖成员关系而不是阶段/Gate。
2. 当前真实性能浪费点主要是“每次槽位刷新都把所有 DefId 组跑一遍”，最小增量方案已经能解决这个核心问题。
3. 完整方案的主要价值在于支持未来更复杂的 merger 依赖声明，而这在当前代码里还不是现实需求。
4. 当前 TCS 仍处于架构设计与开发实践阶段，更适合先做低风险收敛，再根据实际性能与业务需求决定是否扩大。

## 七、何时应该升级到完整实现方案

只有出现下面这些迹象时，才建议从最小增量方案升级到完整方案：

1. Buff 槽位中同槽位实例数量已经足够多，整槽位扫描本身成为明显热点
2. 新增 merger 开始依赖阶段、Gate、或其他运行时字段
3. 需要在 profiler 中明确区分“哪些 merger 因什么原因被触发”
4. 需要支持比当前 `UseNewest` / `UseOldest` / `StackByInstigator` 更复杂的按组动态策略

## 八、实施顺序建议

推荐顺序如下：

1. 先落地最小增量方案
2. 只为当前代码真实使用到的字段加脏标记
3. 观察 profiler 和实际使用中的新 merger 需求
4. 只有在明确出现 stage-sensitive 或 runtime-sensitive merger 后，再设计完整方案中的依赖声明接口

## 九、验证建议

无论选择哪个方案，都至少应该验证以下场景：

1. 新增同 Def Buff 时，只重算对应 DefId 组
2. 移除或过期某个 Buff 时，只重算对应 DefId 组
3. merger 产生 `MergedOut` 后，延迟刷新链不会丢失下一轮 dirty 信息
4. `UseNewest` / `UseOldest` / `StackByInstigator` 的当前行为与优化前保持一致
5. `TireflyGameplayUtilsEditor Win64 Development` 编译通过

## 十、最终建议

如果目标是“先把当前全量 merge 的明显浪费去掉”，应该实施：

- 最小增量方案

如果目标已经升级成“为未来复杂 merger 体系打底”，才应该考虑：

- 完整实现方案

当前 TCS 更适合前者，不适合现在直接做后者。