# Buff 增量反应语义扩展方案（叠层、时长、 Period）

## 文档目的

本文不是描述当前实现，而是给 TCS Buff 模块下一步扩展“增量反应语义”时的一份完整计划。

本文聚焦的需求不是 Buff 是否存在，而是 Buff 在运行时发生这些增量变化时，系统应该如何统一处理：

1. 叠层上涨后，是否刷新持续时间。
2. 持续时间归零后，是直接移除整个 Buff，还是只掉一层。
3. 如果持续时间归零只掉一层，掉层后是否刷新持续时间。
4. `UTcsBuffDefinition::Period` 在现有 StateTree 架构下，应该如何作为 BuffStateTree 的能力来实现。

本文还要回答一个架构问题：

- 这些能力是否应该继续做成 CDO 策略模式。
- 还是说可以收紧成定义资产上的枚举配置。

本文还有一个边界前提需要先写死：

- 这批新增配置全部都是和“叠层”直接相关的增量语义。
- 因此它们当且仅当 `UTcsBuffDefinition::MaxStackCount > 1` 时才有意义，才应允许配置。
- 对于单层 Buff，这些配置既不应该出现在编辑器里，也不应该在运行时参与行为分支。

## 一、问题重述

当前 TCS Buff 模块已经完成了几件重要收口：

1. `DurationType` / `Duration` / `Period` / `MaxStackCount` / `MergerType` 已经迁入 `UTcsBuffDefinition`。
2. `UTcsBuffInstance` 已经持有 `TotalDuration` / `RemainingDuration` / `Period` / `MaxStackCount` / `StackCount`。
3. `UTcsBuffMerger` 现在只处理 `UTcsBuffInstance`，合并链已经不再借用 `UTcsStateInstance` 名义类型。
4. `UTcsBuffComponent` 已经接管 Duration 跟踪、Merge 编排、MergedOut 收敛和 Buff 专属事件广播。

但当前 Buff 模块仍然缺一块很关键的中层语义：

- Buff 在“层数变化”和“持续时间归零”时，到底要触发什么反应。

现在系统能表达的是：

1. Buff 采用哪种 merger。
2. Buff 当前有多少层。
3. Buff 当前剩余时长是多少。
4. Buff 是否声明了 Period 数值。

现在系统还不能直接表达的是：

1. 叠层增长时，要不要刷新持续时间。
2. 持续时间归零时，是清空整个 Buff 还是只扣一层。
3. 扣层后是否刷新时长。
4. `BuffDef.Period` 这个数值，应该如何在 BuffStateTree 中落成通用、可复用的 Period 能力。

这就是当前 Buff 模块下一步的真实缺口。

## 二、先给结论

结论先写在前面：

1. `UTcsBuffMerger` 应继续保留为 CDO 策略模式。
2. 但“叠层变化 / 持续时间归零”这些增量反应，不应该继续塞进 merger。
3. 这类能力仍然适合做成定义资产上的结构化枚举配置，但配置面需要参考 GAS 的思路进一步收紧。
4. `Period` 不应该继续扩成 `BuffDef` 上的通用反应配置项，而应该交给 BuffStateTree 自己实现。
5. `UTcsBuffDefinition::Period` 只保留为一个默认周期间隔输入，不再承担“叠层时重置 / 叠层时补一次”这类策略表达。
6. 新配置应当只服务于 `MaxStackCount > 1` 的 Buff，不应污染单层 Buff 的资产面。
7. 第一阶段不引入 `ExtendDuration` 这一类扩展时长策略。
8. 如果未来确实出现复杂到枚举表达不动的规则，再额外引入一套独立的 `BuffReactivePolicy` / `BuffLifecyclePolicy` 策略类；不要复用 `UTcsBuffMerger` 去硬扛。

也就是说，推荐方案不是“全做成 CDO”，也不是“一个大枚举塞所有组合”，而是：

- `Merger` 继续是策略类
- `增量反应` 只覆盖 stack / duration，并改成参考 GAS 收紧后的结构化枚举配置
- `Period` 交给 BuffStateTree 自己执行

## 三、为什么不应该把这些能力继续塞进 Merger

表面上看，“层数上涨后刷新持续时间”很容易让人想到直接在 `StackByInstigator` 里做。

但这条路的问题非常直接：

1. 这不是 `StackByInstigator` 独有需求，`UseNewest`、`UseOldest` 甚至未来别的 merger 也会需要。
2. merger 的职责应该是“决定谁保留、谁淘汰、叠层如何合并”，而不是“顺便再管时长刷新和 Period 执行”。
3. 如果把反应逻辑写进具体 merger，会导致相同的 Buff 语义在不同 merger 里重复实现。
4. 一旦以后除了 merge 之外，游戏逻辑也会主动调用 `AddStack()` / `RemoveStack()`，那这些路径就绕过 merger 了，行为会不一致。

所以 merger 更适合继续只回答两个问题：

1. 谁是 survivor。
2. survivor 的 stack/max stack 最终是多少。

而“stack 改变后要触发什么副作用”，应该从 merger 中抽离出来，收口到更通用的 Buff 反应层。

## 四、为什么不建议一上来就为这些反应再做一套 CDO 策略

把所有增量反应都做成 CDO，看起来最灵活，但当前阶段并不划算。

### 4.1 这样做的好处

1. 理论扩展性最强。
2. 复杂业务可以直接写代码，而不是被枚举限制。
3. 可以按 Buff 类型写任意特殊行为。

### 4.2 这样做的坏处

1. 对当前已知需求来说，过重。
2. 会额外引入大量很碎的“微型策略类”。
3. 策略之间的执行顺序会立刻变成新问题。
4. 设计师资产面会膨胀：不仅要配 Merger，还要配 StackReactivePolicy、ExpirePolicy、PeriodPolicy。
5. 这些需求目前大多是有限组合，不值得一开始就走最重抽象。

### 4.3 当前阶段更合理的判断

当前这批需求，本质上还是标准 Buff 生命周期策略：

1. 层数上涨后的时长反应。
2. 持续时间归零后的掉层或清除策略。
3. `Period` 作为 Buff 内部逻辑节拍能力的落地方式。

这类规则并没有强到必须用类来表达。

当前阶段直接上 CDO，属于抽象早了。

## 五、为什么也不能只用一个大枚举

另一种极端是做一个总枚举，比如：

- `RemoveWholeBuff`
- `RemoveOneStackAndRefreshDuration`

这种方式的问题是组合爆炸：

1. “层数上涨”与“时长归零”本来就是两种不同触发时机。
2. 如果把它们硬塞进一个总枚举，名字会越来越长，组合会越来越多。
3. Period 既然已经决定交回 BuffStateTree，更不应该再跟这两类通用策略混在一起。

所以真正合理的不是“一个大枚举”，而是：

- 按触发时机拆成几个小配置结构
- 每个结构内部再用小枚举表达几个有限选项

## 六、参考 GAS 后的收紧版方案：只对 Stack / Duration 建模，Period 留给 BuffStateTree

推荐不是直接复刻 GAS，也不是保留本文最初的“五类枚举”方案，而是参考 GAS 的“少量稳定策略轴”思路后，只把真正属于 Buff 通用生命周期的部分留下来。

### 6.1 GAS 值得借鉴的核心不是枚举名，而是收紧方式

GAS 的 `GameplayEffect` 在 Stack 相关层面，核心其实只有几条轴线：

1. `StackingType`
2. `StackDurationRefreshPolicy`
3. `StackPeriodResetPolicy`
4. `StackExpirationPolicy`

它真正值得借鉴的地方在于：

1. “怎么堆叠” 与 “堆叠后如何响应” 被明确拆开。
2. 响应语义被压缩到少数稳定轴线上，而不是做组合爆炸。
3. 单层效果根本不进入这套配置面。

### 6.2 TCS 不应该直接照搬 GAS 的 Period 轴

对 TCS 来说，不能直接把 GAS 的四条轴线全搬过来，原因很明确：

1. TCS 现有的 `UTcsBuffMerger` / `MergerType`，已经承担了比 GAS `StackingType` 更强的“如何合并”职责。
2. GAS 之所以有 `StackPeriodResetPolicy`，前提是 GE 的 Period 本来就是 GE runtime 的内建能力。
3. TCS 当前更合理的边界是：`Period` 继续属于 BuffStateTree 内部逻辑，而不是扩成 `BuffDef` 上的通用策略枚举。

所以 TCS 真正应该借鉴的是：

- 借 GAS 的“收紧原则”
- 但只把 stack / duration 留在通用配置层
- 不把 period 继续做成通用配置项

### 6.3 适用前提与可见性约束

这一条应该在文档里明确写死，因为它决定了配置面是否干净：

1. 这批新增项全部都是 stack-related semantic，当且仅当 `MaxStackCount > 1` 时才有意义。
2. 当 `MaxStackCount <= 1` 时，这些项在编辑器中应隐藏，运行时也按默认值忽略。
3. 当 Buff 没有有限 Duration 时，所有 `OnDurationExpired` 相关项都应隐藏并忽略。
4. `UTcsBuffDefinition::Period` 保持为独立字段，它本身不是 stack-only 配置，因此不受这组隐藏规则约束。
5. 隐藏不等于清空。推荐只控制可见性和运行时忽略，不主动重写设计师已经配过的值。

### 6.4 推荐新增的定义层结构

对比 GAS 后，本文原本的“五类枚举”应进一步收紧成“两类枚举类型 + 两个配置结构”。

#### 6.4.1 叠层上涨反应

```cpp
UENUM(BlueprintType)
enum class ETcsBuffDurationRefreshPolicy : uint8
{
    None,
    RefreshRemainingToTotal,
};

USTRUCT(BlueprintType)
struct FTcsBuffOnStackIncreasePolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
    ETcsBuffDurationRefreshPolicy DurationPolicy = ETcsBuffDurationRefreshPolicy::None;
};
```

这块只回答一个问题：

- 当 `NewStackCount > OldStackCount` 时，是否刷新 Duration。

#### 6.4.2 持续时间归零反应

```cpp
UENUM(BlueprintType)
enum class ETcsBuffStackExpirationPolicy : uint8
{
    ClearEntireBuff,
    RemoveSingleStack,
    RemoveSingleStackAndRefreshDuration,
};

USTRUCT(BlueprintType)
struct FTcsBuffOnDurationExpiredPolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
    ETcsBuffStackExpirationPolicy ExpirationPolicy = ETcsBuffStackExpirationPolicy::ClearEntireBuff;
};
```

这块只回答一个问题：

- 当 Duration 走完时，是清掉整个 Buff，还是掉一层；如果掉层，是否刷新 Duration。

同时要明确：

- `UTcsBuffDefinition::Period` 继续保留原字段。
- 但它只表示 BuffStateTree 可读取的默认周期间隔输入。
- 它不再属于这批新增的 stack reaction 配置结构。

### 6.5 为什么推荐这样拆

这样拆比文档最初的五类枚举方案更合理：

1. 新增枚举类型从 5 类收紧到了 2 类。
2. BuffComponent 不再替具体 Buff 作者决定 Period 应该怎么响应叠层和掉层。
3. 配置面明显更干净，也更符合当前 StateTree 主导具体逻辑的架构。
4. 如果某个具体 Buff 确实需要“叠层时重置周期 / 叠层时立刻补一次”，应该在 BuffStateTree 中实现专用节点或专用状态流，而不是继续往 `BuffDef` 的通用配置上加枚举。

## 七、运行时扩展计划

如果定义层新增了这些枚举，运行时至少还要补两块能力：

1. 把 stack / duration 的通用反应真正收口到 `UTcsBuffComponent`
2. 给 BuffStateTree 提供一个可复用的 Period 驱动节点能力

## 7.1 Period 不再走 BuffComponent 侧统一调度

当前这版方案里，Period 不再作为组件侧的通用运行时调度器来实现。

这意味着：

1. 不新增 `PeriodTracker`
2. 不新增 `TickBuffPeriods()`
3. 不在 `UTcsBuffComponent` 上维护 `RemainingTime`
4. 不由 `UTcsBuffComponent` 统一决定何时补一次 Period 或何时重置 Period 相位

原因也很直接：

1. 这会让 BuffComponent 实际上替具体 Buff 作者决定 Period 的节拍语义。
2. 这会在 BuffStateTree 之外再复制一份 Period 相位状态，造成双状态源。
3. 这会把 `Pause` / `HangUp` / Gate 关闭时 Period 是否继续推进的问题提前固化到组件层。

既然当前方向已经明确为“Period 应该交给 BuffStateTree 自己执行”，那 Period 的自然推进也应该一起回到 BuffStateTree 内部。

## 7.2 基于现有 StateTree 架构的 Period 实现方式

这里的关键点是：当前架构其实已经具备了实现 Period 所需要的大部分基础设施。

### 7.2.1 当前已有的基础能力

1. `UTcsBuffInstance` 本身就是 `UTcsStateInstance` 的派生类，每个 Buff 实例天然都有自己的 `FStateTreeInstanceData`。
2. `UTcsStateInstance` 已经具备 `StartStateTree()` / `RestartStateTree()` / `TickStateTree()` / `PauseStateTree()` / `ResumeStateTree()` 等现成运行时接口。
3. `UTcsStateTreeSchema_StateInstance` 已经把 `StateInstance`、Owner、Instigator 以及相关组件都暴露成了可读取的 StateTree 外部数据。
4. `UTcsStateInstance` 已经有现成的 `SendStateTreeEvent()`，可以把中性事件重新送回当前这棵内层树。

也就是说，Period 缺的不是底层设施，而是一个合适的 StateTree 节拍驱动节点。

### 7.2.2 推荐新增通用节点：`FTcsBuffPeriodDriverTask`

推荐不要在组件里做统一 PeriodScheduler，而是提供一个 BuffStateTree 内可复用的通用 Task。

例如：

```cpp
USTRUCT()
struct FTcsBuffPeriodDriverTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
    float PeriodOverride = 0.f;

    UPROPERTY(Transient)
    float ElapsedTime = 0.f;
};
```

这个 Task 的职责应该尽量小：

1. 从 `StateInstance` 上读取当前 `UTcsBuffInstance`
2. 解析本次驱动使用的周期值：
   - `PeriodOverride > 0` 时，使用 override
   - 否则读取 `BuffInstance->GetPeriod()`
3. 如果周期值 `<= 0`，直接不工作
4. 在 `Tick()` 中累计 `ElapsedTime`
5. 当 `ElapsedTime >= Period` 时，用 while 循环补偿，并逐次发出 `Event.Buff.PeriodTick`

推荐执行链大致如下：

```text
FTcsBuffPeriodDriverTask::Tick
  -> Resolve BuffInstance
  -> Resolve Period
  -> ElapsedTime += DeltaTime
  -> while (ElapsedTime >= Period)
       -> ElapsedTime -= Period
       -> BuffInstance->SendStateTreeEvent(Event.Buff.PeriodTick, Payload)
```

### 7.2.3 `PeriodDriverTask` 只负责产出节拍，不直接执行业务

这里的边界要刻意收紧：

1. `FTcsBuffPeriodDriverTask` 只负责“到点了”这件事。
2. 它不应该直接把伤害、治疗、叠层变化等玩法效果硬写进去。
3. 它最合适的输出，就是一个中性的 `Event.Buff.PeriodTick`。

这样具体 Buff 的作者就可以在 BuffStateTree 里自由决定：

1. 收到 `PeriodTick` 后执行一次伤害
2. 收到 `PeriodTick` 后做一次治疗
3. 收到 `PeriodTick` 后掉层
4. 收到 `PeriodTick` 后只做特效或标签变化

也就是说：

- `PeriodDriverTask` 负责“时机”
- BuffStateTree 其他节点负责“效果”

### 7.2.4 如果具体 Buff 需要“叠层时重置周期 / 叠层时立刻补一次”

这类能力不再继续上升为 `BuffDef` 的通用配置。

推荐做法是：

1. 让具体 Buff 作者在 BuffStateTree 中实现专用节点、专用状态流或专用 Task。
2. 这些专用节点可以直接读取 `BuffInstance->GetStackCount()`、`GetCurrentStage()` 以及其他运行时数据。
3. 如果需要，它们可以自己维护额外的本地相位信息，例如：
   - 上一帧观察到的 stack count
   - 本地 `ElapsedTime`
   - 是否已补发过一次 tick

这样就不会把某个具体 Buff 的 Period 节拍偏好，错误地推广成整套 Buff 系统的通用配置。

## 7.3 把“叠层上涨反应”统一收口到 BuffComponent

这里的核心原则是：

- 不管 stack 是因为 merger 改变，还是因为别的游戏逻辑直接改，最终都应该走同一套反应逻辑。

因此建议不要把反应放在 merger 里，而是放在 `UTcsBuffInstance::SetStackCount()` 写回之后的统一流程里。

### 7.3.1 推荐执行链

```text
UTcsBuffInstance::SetStackCount
  -> 写回 StackCount
  -> BuffComponent->HandleBuffStackCountChangedInternal(this, OldStackCount, NewStackCount)
     -> 如果 New > Old
        -> 按 Definition.OnStackIncreasePolicy 执行 Duration 反应
  -> BuffComponent->NotifyBuffStackChanged(...)
```

### 7.3.2 为什么要放在 SetStackCount 之后统一处理

因为这样能保证：

1. `StackByInstigator` 修改 stack 时，能自动触发反应。
2. 未来如果 `UseNewest` / `UseOldest` 也会改 stack，也能自动触发反应。
3. 未来如果蓝图或 C++ 业务直接调用 `AddStack()`，仍然是同一套语义。

这就是把“merge 算法”和“stack 反应”真正解耦。

## 7.4 把“持续时间归零反应”从 ExpireBuffInstance 前移出来

当前 `TickBuffLifecycles()` 的逻辑是：

- 时长归零 -> `ExpireBuffInstance()` -> 走统一移除链。

如果要支持“归零只掉一层”，这个逻辑必须改成先问策略，再决定是否真的移除。

### 7.4.1 推荐新增统一入口

建议新增：

```cpp
void HandleBuffDurationExpired(UTcsBuffInstance* BuffInstance);
```

当前 `TickBuffLifecycles()` 在发现 `RemainingDuration <= 0` 后，不再直接 `ExpireBuffInstance()`，而是调用这个函数。

### 7.4.2 推荐的处理规则

#### 情况 A：`ExpirationPolicy == ClearEntireBuff`

保持当前行为：

```text
HandleBuffDurationExpired
  -> RequestStateRemoval(BuffInstance, Expired)
```

#### 情况 B：`ExpirationPolicy == RemoveSingleStack`

#### 情况 C：`ExpirationPolicy == RemoveSingleStackAndRefreshDuration`

建议这样统一处理：

1. 如果当前 `StackCount <= 1`
   - 不要走 `SetStackCount(0)` 让它以 `StackDepleted` 理由离场
   - 直接按 `Expired` 理由请求移除
2. 如果当前 `StackCount > 1`
   - 先减 1 层
    - 如果 `ExpirationPolicy == RemoveSingleStackAndRefreshDuration`，刷新 `RemainingDuration`
    - 不再由组件侧决定 Period 应该怎样跟随掉层变化

这样可以保持移除原因语义清楚：

- 真正由时长归零导致的离场，仍然是 `Expired`
- 不会因为内部实现细节让最后一层误显示成 `StackDepleted`

## 八、Merger、BuffComponent 与 BuffStateTree 的职责边界

推荐把职责边界明确成下面这样：

### 8.1 `UTcsBuffMerger` 负责什么

1. 决定谁保留。
2. 决定谁淘汰。
3. 决定 survivor 的 stack / max stack 最终如何收敛。

### 8.2 `UTcsBuffComponent` 负责什么

1. 执行 stack 改变后的统一反应。
2. 执行 duration 归零后的统一反应。
3. 维护 Duration 跟踪、合并编排、移除收敛等宿主级运行时职责。
4. 把最终移除请求重新送回 State 统一移除链。

### 8.3 BuffStateTree 负责什么

1. 消费 `UTcsBuffDefinition::Period` 这个默认周期间隔输入。
2. 通过 `PeriodDriverTask` 或专用 Task 维护自己的本地 Period 节拍。
3. 在收到 `Event.Buff.PeriodTick` 后执行真正的玩法逻辑。
4. 如果某个具体 Buff 想在叠层变化时重置或补发 Period，也由 BuffStateTree 自己决定。

### 8.4 这样拆的直接好处

1. `UseNewest` / `UseOldest` / `StackByInstigator` 不需要各自重复写“时长刷新规则”。
2. 以后新增 merger，也不用再重复实现相同的 stack 反应逻辑。
3. Period 不再由通用组件替具体 Buff 作者做决定。
4. 业务侧如果直接调用 `AddStack()`，行为仍然和 merge 路径保持一致。

## 九、推荐的调用顺序

为了避免后续实现时互相覆盖，建议先把几个关键顺序定死。

### 9.1 Stack 增长反应顺序

推荐顺序：

1. 先完成 `StackCount` 写回
2. 再执行通用 Duration reaction
3. 最后广播 `OnBuffStackChanged`

原因：

1. 通用生命周期配置应当先把 Buff 调整到稳定状态。
2. 外部事件监听者与 BuffStateTree 的专用逻辑，都应该看到已经稳定后的 stack / duration 结果。

### 9.2 BuffStateTree 的 PeriodTick 与 Duration 生命周期的顺序

推荐顺序：

1. 先 Tick BuffStateTree（包括 `PeriodDriverTask`）
2. 再推进 `TickBuffLifecycles`

原因：

1. 常见游戏语义下，如果某一帧同时到达最后一个 Period Tick 和时长结束，通常更符合直觉的是“先触发最后一次 Period，再结束 Buff”。
2. 这样更接近常见 DoT/Hot 的玩法预期。

### 9.3 Duration 归零且选择掉层时的顺序

推荐顺序：

1. 先减层
2. 再按策略刷新 Duration
3. 如果具体 Buff 想让掉层影响 Period，由 BuffStateTree 自己处理本地相位
4. 不额外再走一次“stack 增长反应”

这里要注意：

- 掉层不是 stack 增长，不应该复用 `OnStackIncrease` 那套逻辑。

## 十、为什么推荐“结构化枚举 + BuffStateTree 内 PeriodDriver”，而不是“第二套策略类”

### 10.1 这套需求的本质是生命周期配置，而不是算法多态

merger 的问题是：

- 同组 Buff 里谁赢谁输。

而这里的问题是：

- 某个 Buff 在增量变化后应该怎么响应。

前者本质上更像算法策略。
后者本质上更像生命周期配置。

所以它们就不该用同一种抽象来承载。

### 10.2 当前需求的自由度其实不高

现在已经明确的真实选项很有限：

1. 叠层上涨时，时长刷新或不刷新。
2. 持续时间归零时，移除整 Buff、只掉一层，或掉一层并刷新时长。
3. Period 的节拍与效果继续留在 BuffStateTree 内部。

前两类属于有限集合，适合枚举。
第三类则更适合留在 StateTree 节点和状态流里表达。

### 10.3 如果以后确实遇到复杂规则，再加第二层策略类

如果未来出现下面这些真实需求，再考虑新增单独的 `UTcsBuffReactivePolicy`：

1. 叠层增长时，Duration 不是简单恢复到 `TotalDuration`，而是要按 Instigator 属性计算。
2. 归零掉层时，不是固定掉 1 层，而是按 SourceHandle 或标签决定掉多少层。
3. Period 触发时机不再是固定选项，而是带复杂条件、概率或自定义过滤。

但即使到那一步，也建议：

- 新增独立策略类
- 不要把这些职责再挂回 `UTcsBuffMerger`

## 十一、推荐的实施阶段

## 11.1 第一阶段：先补数据建模和执行骨架

目标：先把框架语义建完整，不着急做复杂玩法。

建议任务：

1. 新增 `ETcsBuffDurationRefreshPolicy`。
2. 新增 `ETcsBuffStackExpirationPolicy`。
3. 在 `UTcsBuffDefinition` 上新增 `FTcsBuffOnStackIncreasePolicy`。
4. 在 `UTcsBuffDefinition` 上新增 `FTcsBuffOnDurationExpiredPolicy`。
5. 补上与 `MaxStackCount` / `DurationType` 绑定的编辑器可见性与运行时忽略规则。
6. 新增可复用的 `FTcsBuffPeriodDriverTask`。
7. 约定统一的 `Event.Buff.PeriodTick` 事件契约。
8. 明确 `UTcsBuffDefinition::Period` 作为 BuffStateTree 默认输入字段的用法。
9. 新增 `HandleBuffStackCountChangedInternal()`。
10. 新增 `HandleBuffDurationExpired()`。

## 11.2 第二阶段：把现有运行时链路接到新骨架上

建议任务：

1. `TickBuffLifecycles()` 中把直接 `ExpireBuffInstance()` 改成 `HandleBuffDurationExpired()`。
2. `UTcsBuffInstance::SetStackCount()` 写回后调用 `HandleBuffStackCountChangedInternal()`。
3. 周期型 Buff 的 BuffStateTree 采用 `WhileActive` TickPolicy，并在常驻 Active 状态中挂入 `FTcsBuffPeriodDriverTask`。
4. 具体 Buff 如果希望 stack 变化影响 Period，相应逻辑在该 BuffStateTree 的专用节点里完成。

## 11.3 第三阶段：验证基础组合是否正确

至少验证下面这些典型组合：

1. `MaxStackCount == 1` 时新增配置是否隐藏且运行时被忽略
2. `StackByInstigator + 叠层上涨刷新时长`
3. `UseNewest + 叠层上涨不刷新时长`
4. `DurationExpired -> ClearEntireBuff`
5. `DurationExpired -> RemoveSingleStackAndRefreshDuration`
6. 带 `Period` 的 Buff 在 BuffStateTree 内挂入 `FTcsBuffPeriodDriverTask` 后，是否能稳定发出 `Event.Buff.PeriodTick`
7. `Pause` / `HangUp` / Gate 关闭导致 BuffStateTree 不 Tick 时，Period 是否自然跟着停止推进
8. 同一帧 Period 与 Duration 同时到点时，是否先触发最后一次 Period 再 Expire
9. 如果某个具体 Buff 想在叠层时重置或补发 Period，是否可以只通过 BuffStateTree 专用节点实现，而不依赖新的 BuffDef 通用配置

## 十二、几个典型配置示例

### 12.1 常见叠毒 Buff

需求：

1. 每次叠层刷新时长
2. 时长结束只掉一层
3. 周期伤害由 BuffStateTree 自己执行

推荐配置：

- `MaxStackCount > 1`
- `MergerType = StackByInstigator`
- `OnStackIncrease.DurationPolicy = RefreshRemainingToTotal`
- `OnDurationExpired.ExpirationPolicy = RemoveSingleStackAndRefreshDuration`
- BuffStateTree 的常驻 Active 状态挂入 `FTcsBuffPeriodDriverTask`
- 周期伤害逻辑通过消费 `Event.Buff.PeriodTick` 实现
- 如果这类毒 Buff 希望“叠层时重置自己的周期相位”，在该 BuffStateTree 的专用节点里处理，不走通用 BuffDef 配置

### 12.2 常见护盾覆盖 Buff

需求：

1. 新护盾覆盖旧护盾
2. 不需要 stack refresh 语义
3. 到时直接移除

推荐配置：

- `MaxStackCount = 1`
- `MergerType = UseNewest`
- 不配置任何新增 stack reaction 选项
- Duration 到时沿用当前单层 Buff 的直接移除行为

### 12.3 叠层时额外爆一次的印记 Buff

需求：

1. 叠层时刷新时长
2. 每次叠层立刻补一次 Period 执行
3. Period 自然 Tick 仍然保留

推荐配置：

- `MaxStackCount > 1`
- `MergerType = UseOldest` 或 `StackByInstigator`
- `OnStackIncrease.DurationPolicy = RefreshRemainingToTotal`
- `OnDurationExpired.ExpirationPolicy = ClearEntireBuff`
- BuffStateTree 内使用专用的印记 Task / 状态流实现“叠层时立刻额外执行一次”
- `FTcsBuffPeriodDriverTask` 仍然只负责自然 Period 节拍，不负责这类专用引爆逻辑

## 十三、最终推荐

如果目标是给 TCS Buff 模块补上“叠层 / 时长 / Period”这批增量语义，当前最合理的路线是：

1. `UTcsBuffMerger` 继续只负责合并算法。
2. 增量反应语义放进 `UTcsBuffDefinition` 的结构化枚举配置，但仅对 `MaxStackCount > 1` 的 Buff 生效。
3. 配置面应参考 GAS 的收紧方式，把枚举类型控制在少数稳定语义轴，不保留原始五类枚举方案。
4. `Period` 不再进入这批 BuffDef 通用枚举配置；`UTcsBuffDefinition::Period` 只保留为 BuffStateTree 可读取的默认输入。
5. `UTcsBuffComponent` 只负责 stack / duration 这两类通用生命周期反应，不负责 Period 调度。
6. BuffStateTree 通过 `FTcsBuffPeriodDriverTask` 或专用 Task 负责 Period 节拍和具体玩法执行。
7. 当前不要把这批需求再做成第二套 CDO 策略，也不要引入 `ExtendDuration`。
8. 真到以后出现复杂自定义公式，再新增独立 `BuffReactivePolicy`，也不要复用 merger。

一句话总结：

- merger 是“谁留下”的策略
- 增量反应只覆盖 stack / duration 这两类通用生命周期配置
- period 是 BuffStateTree 自己的节拍与执行能力

这两件事应该拆开。