# FScopedBatchNotification RAII 事件广播批量化

> 文档定位：后续独立议题的前置调研记录。不在「Manager API 迁移到 Component」议题范围内。
>
> 触发时机：迁移议题完成 + 回归测试通过之后，新开 brainstorm 推进。

---

## 1. 核心思想

**RAII（Resource Acquisition Is Initialization）** 是 C++ 的经典模式：把“资源获取/释放”绑定到对象的**构造/析构**上。

`FScopedBatchNotification` 就是把“**开始批量收集**”放在构造函数、“**统一广播**”放在析构函数里，靠**作用域自动触发**，避免手动 Begin/End 配对漏写。

---

## 2. 当前问题的具体形态

以 `UpdateStateSlotActivation` 的 8 步流程为例，一次调用可能发生：

```
Step 2: 3 个状态 Merged       → 3 次 NotifyStateRemoved（MergedOut）
Step 3: 4 个状态被 Preempted  → 4 次 NotifyStateStageChanged（Active→Inactive）
Step 4: 2 个新状态 Activate    → 2 次 NotifyStateStageChanged（Pending→Active）
Step 5: SourceHandle 清 Modifier → 每个状态触发 N 次 NotifyModifierRemoved
         → 每次 NotifyModifierRemoved 又触发 NotifyAttributeValueChanged
         → UI 每次都 Invalidate 重绘
```

**典型后果**：

- 同一帧内 UI 刷新 10+ 次，只有最后一次是用户看到的最终值
- 蓝图监听者被反复唤醒，做重复计算
- 回调顺序难以保证原子性（回调 A 看到的是“中间态”，回调 B 看到的是“最终态”）

---

## 3. RAII 批量化的骨架

```cpp
// 1. 守卫对象：构造时“开始收集”，析构时“统一派发”
class TIREFLYCOMBATSYSTEM_API FTcsScopedBatchNotification
{
public:
    explicit FTcsScopedBatchNotification(UTcsStateComponent* InOwner)
        : Owner(InOwner)
    {
        // 嵌套场景：只有最外层守卫真正开启/收尾
        bIsOutermost = (Owner && ++Owner->BatchDepth == 1);
    }

    ~FTcsScopedBatchNotification()
    {
        if (!Owner) return;
        --Owner->BatchDepth;
        if (bIsOutermost)
        {
            Owner->FlushPendingNotifications();   // 统一广播
        }
    }

    // 禁止拷贝/移动，强制栈上 scope 使用
    FTcsScopedBatchNotification(const FTcsScopedBatchNotification&) = delete;
    FTcsScopedBatchNotification& operator=(const FTcsScopedBatchNotification&) = delete;

private:
    UTcsStateComponent* Owner;
    bool bIsOutermost;
};

// 2. Component 侧：收集队列 + 广播路径分流
class UTcsStateComponent : public UActorComponent
{
    friend class FTcsScopedBatchNotification;

protected:
    int32 BatchDepth = 0;

    // 收集容器（按事件类型分开，便于去重/合并）
    TArray<FTcsPendingStageChange>  PendingStageChanges;
    TArray<UTcsStateInstance*>      PendingStateRemoved;
    TArray<UTcsStateInstance*>      PendingStateApplied;

    // 统一派发
    void FlushPendingNotifications();

    // 所有 Notify* 路径改为：若 BatchDepth > 0，入队；否则立即广播
    void NotifyStateStageChanged(UTcsStateInstance* Inst, EStateStage Old, EStateStage New);
};

// 3. 调用点：用大括号创建作用域
void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag SlotTag)
{
    FTcsScopedBatchNotification BatchGuard(this);     // <-- 构造：开启批量
    TGuardValue<bool> Reentrancy(bIsUpdatingSlotActivation, true);

    // ... 原 8 步流程，中途 Notify* 全部进队列 ...

    // BatchGuard 离开作用域 → 析构 → FlushPendingNotifications 一次性发出
}
```

---

## 4. 关键设计点

### 4.1 嵌套安全（深度计数）

如果 `TryApplyState` 内部又调用 `RequestUpdateStateSlotActivation`，会出现嵌套守卫。`BatchDepth` 计数保证**只有最外层**在析构时真正 Flush，内层守卫是 no-op。

### 4.2 去重 / 合并策略

Flush 前可以做折叠：

- 同一 `StateInstance` 的 `Pending→Active→Inactive` → 最终只发 `Inactive`（或 `Pending→Inactive` 这对净变化）
- 同一 `Attribute` 的多次 `CurrentValue` 变化 → 只发最后一次 `Old→Final`
- 同一 `SourceHandle` 被清 5 个 Modifier → 合并为一个 `ModifiersRemoved(Array)` 事件

这是**“批量”比“延迟”更有价值**的部分——真正减少监听者工作量。

### 4.3 异常 / 早返回安全

RAII 的核心优势：无论函数怎么 `return` 或抛异常，析构必然执行，**不会卡住批量状态**。

手写 `BeginBatch()/EndBatch()` 配对一旦漏写 `End` 就永久堵塞后续广播。

### 4.4 广播顺序契约

Flush 时必须定义明确顺序，例如：

1. 先发所有 `StateRemoved`
2. 再发所有 `StageChanged`
3. 最后发所有 `StateApplied`

避免监听者看到“新状态已 Applied 但旧状态还没 Removed”这种时序错乱。

---

## 5. 对 TCS 的适用性评估

| 维度 | 评估 |
|------|------|
| **收益** | `UpdateStateSlotActivation` / `FinalizeStateRemoval` / `RecalculateAttribute*` 都是高频批量场景，合并后 UI/蓝图监听者工作量可能 ↓ 50%+ |
| **改造面** | 需在 Component 上加 `BatchDepth` + 3~5 个 `Pending*` 队列 + 所有 `Notify*` 路径分流；**所有监听者语义需要复核**（某些监听者可能依赖“立即回调”，不能合并） |
| **风险** | 监听者若在批量期间查询 Component 状态，看到的是“已变更但未广播”的状态——**读写不一致窗口**；需明确：批量期间查询返回最新还是广播前值 |
| **与迁移的耦合** | 迁移本身不引入这个需求；但迁移完成后 `Notify*` 全部在 Component 内，是改造的最佳时机（Manager 时代分散在两处，改造成本更高） |

---

## 6. 建议的时机

**不建议与「Manager API 迁移到 Component」议题合并**，理由：

1. 迁移的“完成定义”是**职责归位**，引入批量化会模糊“这次 PR 改了什么”的判定标准
2. 批量化需要**单独的广播顺序契约 + 去重策略 + 监听者审计**，每一项都可能引出新的 brainstorm
3. 迁移后 `Notify*` 都在 Component 内，**后续独立 PR 的改造面更小**（只改 Component，不动 Manager）

**推荐路径**：

- 本次迁移：`Notify*` 保持现状（立即广播）
- 迁移完成 + 回归测试通过后，**新开 brainstorm**，议题名建议 `"TCS 事件广播批量化与去重"`，输入产物：
  - 广播顺序契约
  - 监听者审计清单
  - 性能基线（用 Unreal Insights 抓一次 `UpdateStateSlotActivation` 的 `Notify*` 调用次数）

---

## 7. 后续 brainstorm 需要回答的问题

先行记录，待新议题启动时逐条展开：

1. **监听者审计**：当前所有 `Notify*` 的订阅者是谁？哪些依赖“立即回调”语义？哪些可以合批？
2. **批量期查询语义**：批量期间调用 `GetAllActiveStates()` 返回最新值还是广播前值？是否需要“快照 View”？
3. **跨 Component 批量**：属性变化可能触发跨 Actor 的回调（例如 UI Widget 监听多个 Actor），`FScopedBatchNotification` 是否需要支持跨 Component 的 Flush 协调？
4. **Attribute 侧对等设计**：`UTcsAttributeComponent` 是否需要同样的守卫？还是复用 `StateComponent` 的作用域？
5. **去重粒度**：对 `NotifyAttributeValueChanged`，折叠策略是“只发最终值”还是“保留中间路径”（某些统计类监听器需要完整路径）？
6. **调试支持**：Flush 时是否提供一个 `TArray<FString>` 的事件日志，便于 Replay / 时序排查？
