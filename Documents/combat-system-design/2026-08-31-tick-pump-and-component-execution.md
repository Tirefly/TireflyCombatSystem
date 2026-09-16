# Tick 泵与组件执行边界（裁决 2 子文档）

> 性质：设计调研记录——**裁决 2（Struct vs UObject 载体）进行中**，本篇为其子结论：**Tick 泵与组件执行边界已定**（2026-08-31）。
> 证据标注：【源码调研】= UE 5.8.0 (Release-5.8, CL 55116800) 本机源码取证；【设计判断】= 架构推理。
> 关联：`2026-08-31-event-dispatch-bus-and-subscriptions.md`（事件分发形态，本篇帧结构引用其总线 Flush）；`2026-08-31-unit-state-relation-and-state-slots.md`（裁决 1）；讨论总日志 `../combat-system-state-tree-research/2026-08-31-combat-skill-system-carrier-discussion.md` §6.4。

## 1. 背景与问题

用户认同"集中统一执行 Tick"，但提出具体形态修正：**ActorComponent 绑定 TickableWorldSubsystem 的 Tick 函数，具体执行逻辑仍在 ActorComponent 内执行**；核心流程若交给 Subsystem，可能导致跨 Actor 的流程阻塞与集中式上帝对象。

## 2. 裁决：采纳"泵 + 组件执行"形态

三句职责划分（与裁决 2"类做词汇、struct 做状态"对齐）：

| 层 | 职责 | 不负责 |
|---|---|---|
| **TickableWorldSubsystem** | 管秩序：pump 顺序（注册序）、缩放 dt 供给、暂停/变速、Tickable 注册表、总线帧末泵 | 执行任何具体战斗逻辑 |
| **ActorComponent** | 管状态与本地执行：持有该单位 Buff 容器/技能游标，TickCombat 内处理本地到期、间隔 tick、游标推进、本地事件入总线 | 跨单位分发（走总线） |
| **词汇类（UClass）** | 管逻辑：步骤/触发器/Handler 的 Execute | 持有状态 |

核心流程进 Subsystem 的真正风险是**上帝对象**（每加一种行为都要改集中式执行器 switch）——泵+组件执行结构性避开：新逻辑 = 新组件代码/新词汇类，Subsystem 永不改。

## 3. 误诊纠正：跨 Actor 阻塞的真面目

【设计判断】无论逻辑在 Subsystem 还是 Component，game thread 上的 tick 都是**顺序的**——单位 A 的重逻辑拖延单位 B，两种形态等价。本形态真正买到的是：

1. **代码组织隔离**（上帝对象风险消除，见 §2）；
2. **故障可归因**（哪个单位异常，栈就在哪个组件）；
3. **本地变更封闭**（本单位 Buff 到期/移除在组件内闭环，全局容器遍历不被中途变异）。

阻塞治理的真实手段两形态通用，见 §5。

## 4. 阻塞治理两护栏

1. **事件帧末泵**：跨单位级联延迟处理（已在事件分发文档定死）——同一规则两层不能表达。
2. **步进预算**：单单位单帧最多推进 N 步，效果链协程化后天然 yield 到下帧，显式预算兜底。

【设计判断】200 单位顺序 tick 为毫秒级，以上足够；并行化（jobify）列为远期，不在当前形态承诺内。

## 5. 落地骨架

```cpp
UINTERFACE(MinimalAPI)
class UCombatTicker : public UInterface { GENERATED_BODY() };
class ICombatTicker {
    GENERATED_BODY()
public:
    virtual void TickCombat(float ScaledDt) = 0; // 组件内：本地到期/间隔/游标/本地事件入总线
};

UCLASS()
class UCombatTickSubsystem : public UTickableWorldSubsystem {
    GENERATED_BODY()
    TArray<TScriptInterface<ICombatTicker>> Tickables; // 注册序即执行序
    float TimeScale = 1.f;                             // 暂停=0，变速=xN
    virtual void Tick(float Dt) override {
        const float ScaledDt = Dt * TimeScale;
        for (const TScriptInterface<ICombatTicker>& T : Tickables)
            if (T.GetObject()) T->Execute_TickCombat(T.GetObject(), ScaledDt);
    }
    // 注册：组件 OnRegister/OnUnregister（或 BeginPlay/EndPlay）；注销延迟到循环后处理
};
```

## 6. 帧结构三拍

1. **泵组件**：逐 Tickable 调 TickCombat(ScaledDt)——本地立即变更（到期/间隔/游标/本地事件入队）；
2. **总线 Flush**：跨单位事件分发（可有界多轮，见事件分发文档 §5 纪律）；
3. **本地清扫**：过期清理、延迟注销落地。

## 7. 四条护栏（防形态劣化）

1. **dt 单点供给**：组件内一律用 `ScaledDt`，禁止自取 `World->GetDeltaSeconds()`——暂停/变速/确定性全靠这条不破。
2. **注销延迟处理**：泵循环内注销（单位死亡）延迟到循环后，与事件泵同一纪律。
3. **无界循环禁令**：单 TickCombat 内禁止无预算 while（步进预算兜底）。
4. **pump 不退化成总线**：组件间交互只走事件总线/查询 API，禁止组件 A 在 TickCombat 里直摸组件 B 的容器——否则耦合以"跨 Actor 阻塞"的形式回来。

## 8. 兼容性核对

- **裁决 2 主体**：组件持 per-unit struct 容器 + 词汇类执行——完全同构；组件只是"容器所有者 + tick 入口"。
- **事件分发文档**：总线仍在 Subsystem 侧，帧末泵职责不变；本篇只确定"本地逻辑在组件内"一层。
- **千人两层架构（未来）**：兵海层走 Mass processors 不经过本 pump，武将层维持组件形态——两层互不污染，pump 形态往前兼容。
- **为何自定义 pump 而非裸 TickComponent**：UE 组件 tick 管理器有间隔/分组，但暂停/变速/定步长（自走棋确定性）与注册序保证需要单点实现——pump 的存在理由。

## 9. 决策状态

**已定**（2026-08-31）：
- 泵 + 组件执行形态与三句职责划分（§2）。
- 阻塞治理两护栏（§4）与落地骨架（§5）、帧结构三拍（§6）、四条护栏（§7）。

**待定**：
- [ ] Tickable 注册表实现细节（延迟注销的挂起列表结构、重复注册防御）。
- [ ] 组件 tick 分组/间隔 LOD（远处/低频单位降频，接口预留 `GetCombatTickInterval()` 之类）。
- [ ] 定步长积累器（fixed timestep accumulator）是否引入——自走棋确定性回放需要时再定。
- [ ] TimeScale 配置入口（GameInstance/世界设置/战斗配置资产）。
