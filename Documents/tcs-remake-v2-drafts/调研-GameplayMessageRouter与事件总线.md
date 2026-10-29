# 调研 · GameplayMessageRouter 与事件总线实现方案

> **性质**：调研记录，**族外草稿**，不是 V2 结论。
> **来源**：本机 clone `E:/Projects_Unreal/TireflyGameplayUtils/Plugins/GameplayMessageRouter`（30 个文件，含 Intermediate，源码 7 个）；TFunction 实现查 UE 5.8 引擎源码。
> **触发**：用户裁定「脚本侧订阅入口必须有」+「现在这版设计感觉会有严重的性能问题」+「能不能把整个 TFunction 的实现思路都替换掉」。

---

## 0. 三条结论

| # | 结论 |
|---|---|
| **1** | **GMR 不是性能优化方案。** 它的 TFunction 用法是「TFunction 套 TFunction」，**每次注册一次堆分配、每次派发两次间接调用**，比直写一个 TFunction 更重 |
| **2** | **GMR 解决的是「一个派发循环同时服务 C++ 与蓝图」。** 它靠 `Channel + UScriptStruct*` 在**注册期**做类型/频道绑定，让 C++ 侧先过滤再跨语言 |
| **3** | **真正的性能热区不是 TFunction，是「每次派发复制一次订阅数组」**——GMR 有，我们草案也有 |

⇒ 该吸收的是 GMR 的**功能形态**，不是它的**实现**。

---

## 1. GMR 的五个核心机制

### 1.1 频道 + 载荷类型，在注册期绑定

```cpp
// GameplayMessageSubsystem.h:61
TFunction<void(FGameplayTag, const UScriptStruct*, const void*)> ReceivedCallback;
```

底层回调是**类型擦除的**：`UScriptStruct*` + `const void*`。所有载荷类型共用**同一个派发循环**。

上层由模板函数在**注册期**把具体类型打进去：

```cpp
// GameplayMessageSubsystem.h:129-139
template <typename FMessageStructType>
FGameplayMessageListenerHandle RegisterListener(
    FGameplayTag Channel,
    TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback,
    EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch)
{
    auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](
        FGameplayTag ActualTag, const UScriptStruct* SenderStructType, const void* SenderPayload)
    {
        InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
    };
    ...
}
```

**这就是「兼容 C++ 和蓝图」的核心**：派发侧只有一种形状，语言差异被 thunk 吸收在注册期，**不在派发热路径上分叉**。

### 1.2 句柄式生命周期

```cpp
// GameplayMessageSubsystem.h:23-50
USTRUCT(BlueprintType)
struct FGameplayMessageListenerHandle
{
    void Unregister();
private:
    TWeakObjectPtr<UGameplayMessageSubsystem> Subsystem;
    FGameplayTag Channel;
    int32 ID = 0;
};
```

**注意**：句柄是 `BlueprintType`——蓝图能持有、能主动注销。这是脚本侧能正确管生命周期的最小条件。

### 1.3 频道层级匹配（可选）

```cpp
// GameplayMessageTypes2.h:14-23
enum class EGameplayMessageMatch : uint8
{
    ExactMatch,    // A.B 只收 A.B
    PartialMatch,  // A.B 收 A.B 与 A.B.*
};
```

实现在派发侧走 tag 父链：

```cpp
// GameplayMessageSubsystem.cpp:85-121
bool bOnInitialTag = true;
for (FGameplayTag Tag = Channel; Tag.IsValid(); Tag = Tag.RequestDirectParent())
{
    if (const FChannelListenerList* pList = ListenerMap.Find(Tag))
    {
        TArray<FGameplayMessageListenerData> ListenerArray(pList->Listeners);   // ← 见 §2
        for (const FGameplayMessageListenerData& Listener : ListenerArray)
        {
            if (bOnInitialTag || (Listener.MatchType == EGameplayMessageMatch::PartialMatch))
            {
                if (!Listener.bHadValidType || StructType->IsChildOf(Listener.ListenerStructType.Get()))
                {
                    Listener.ReceivedCallback(Channel, StructType, MessageBytes);
                }
            }
        }
    }
    bOnInitialTag = false;
}
```

### 1.4 蓝图广播：CustomThunk + 通配结构体

```cpp
// GameplayMessageSubsystem.h:208-211
UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging,
          meta=(CustomStructureParam="Message", AllowAbstract="false"))
void K2_BroadcastMessage(FGameplayTag Channel, const int32& Message);
DECLARE_FUNCTION(execK2_BroadcastMessage);
```

蓝图侧可以传**任意** USTRUCT，C++ 侧在 `execK2_BroadcastMessage`（`.cpp:130-145`）里从栈上取出 `FStructProperty*` 与数据指针，再进统一派发。

### 1.5 蓝图监听：AsyncAction + 专用 K2Node

```cpp
// AsyncAction_ListenForGameplayMessage.h:22-36
UCLASS(BlueprintType, meta=(HasDedicatedAsyncNode))
class UAsyncAction_ListenForGameplayMessage : public UCancellableAsyncAction
{
    static UAsyncAction_ListenForGameplayMessage* ListenForGameplayMessages(
        UObject* WorldContextObject, FGameplayTag Channel,
        UScriptStruct* PayloadType, EGameplayMessageMatch MatchType);

    UFUNCTION(BlueprintCallable, CustomThunk, meta=(CustomStructureParam="OutPayload"))
    bool GetPayload(UPARAM(ref) int32& OutPayload);

    UPROPERTY(BlueprintAssignable)
    FAsyncGameplayMessageDelegate OnMessageReceived;
};
```

**每个监听 = 一个 AsyncAction 代理对象 + 一条 C++ 侧 listener 条目**（`.cpp:40-49`）。

⇒ **「只在指定频道上被唤醒」由 C++ 侧完成**，蓝图虚拟机不会收到其他频道的事件。这是它优于「全量多播 + 脚本自己过滤」的地方。

**一个必须知道的约束**：载荷是裸指针，**只在派发期间有效**（`.cpp:95-99`）：

```cpp
ReceivedMessagePayloadPtr = Payload;
OnMessageReceived.Broadcast(this, Channel);   // 蓝图必须在这里面调 GetPayload
ReceivedMessagePayloadPtr = nullptr;
```

⇒ 蓝图里**不能把载荷存起来延后使用**，必须当场拷贝。

---

## 2. 性能实测分析：GMR 比直写 TFunction 更重

### 2.1 TFunction 的真实成本（引擎源码实测）

| 项 | 值 | 出处 |
|---|---|---|
| 内联缓冲 | **24 字节**，16 字节对齐 | `CoreDefines.h:47` / `Function.h:196` |
| `sizeof(TFunction)` | **32 字节**（24 内联 + 8 堆指针） | 推算自 `Function.h:196-197` |
| 超过 24 字节的捕获 | **堆分配** | `Function.h:223,239` |

### 2.2 GMR 的 thunk = 一次堆分配 + 两层间接

```text
RegisterListener 的 ThunkCallback 捕获 = InnerCallback（一个 TFunction，32 字节）
  ⇒ 32 > 24 ⇒ 【堆分配】
  ⇒ 每次派发：外层 TFunction → thunk → 内层 TFunction = 【两次间接调用】
```

**对比我们草案**：直接注册 `TFunction<void(const FTcsEventPayload&)>`，捕获 `[this]`（8 字节）⇒ **内联、零堆分配、一次间接调用**。

| 维度 | 我们草案 | GMR |
|---|---|---|
| 注册期堆分配 | 通常 0 | **每次 1 次** |
| 派发间接调用 | 1 | **2** |
| 每订阅内存 | 32 B | **64 B**（两层） |

⇒ **「TFunction 实现思路有严重性能问题」这个判断，对 GMR 成立，对我们草案不成立。**

### 2.3 GMR 真正的性能热区

| # | 热区 | 证据 | 每 N 次派发的成本 |
|---|---|---|---|
| **1** | **每次派发复制整个订阅数组** | `.cpp:91` `TArray<FGameplayMessageListenerData> ListenerArray(pList->Listeners);` | **1 次堆分配 + memcpy** |
| 2 | tag 父链遍历 | `.cpp:86` | 每层 **1 次 TMap 查找** |
| 3 | 每 listener 的弱指针校验 | `.cpp:97` `ListenerStructType.IsValid()` | 每 listener 1 次 |
| 4 | 每 listener 的类型检查 | `.cpp:105` `StructType->IsChildOf(...)` | 每 listener 1 次 |
| 5 | 两层 TFunction 间接 | `.h:132-138` | 每 listener 2 次 |

**排第一的是 #1，不是 TFunction。** 1000 事件/帧 × 10 订阅 ⇒ **每帧 1000 次堆分配**。

而且我们草案里的「**派发前快照**」做的是同一件事——**一样有这个问题**。

---

## 3. 结论：吸什么，不吸什么

### 3.1 该吸的五条

| # | 吸取 | 理由 |
|---|---|---|
| **A1** | **注册期绑定「频道 + 载荷类型」，派发期不分叉** | 这是「一个循环服务 C++ 与脚本」的唯一干净做法 |
| **A2** | **类型擦除的底层回调**（`UScriptStruct* + const void*`） | 异质载荷共用一个派发循环；UE 的反射类型天然可用作类型键 |
| **A3** | **句柄式生命周期，句柄可蓝图持有** | 脚本侧必须能主动注销，否则只能靠 GC 兜底 |
| **A4** | **蓝图监听走「每监听一个代理对象」，且在 C++ 侧按频道过滤** | 保证蓝图虚拟机**不会收到不关心的频道** |
| **A5** | **`CustomThunk + CustomStructureParam`** | 蓝图侧传任意 USTRUCT 的标准手段，无需为每个载荷类型生成一个 UFUNCTION |

### 3.2 不该吸的四条

| # | 不吸 | 理由 |
|---|---|---|
| **N1** | **TFunction 套 TFunction 的 thunk** | 每次注册一次堆分配、每次派发两次间接调用。**改法见 §4.2** |
| **N2** | **每次派发复制订阅数组** | 真正的热区。**改法见 §4.3** |
| **N3** | **父链遍历作为默认行为** | 每层一次 TMap 查找。层级匹配应当是**显式开启**的能力，不是默认成本 |
| **N4** | **载荷裸指针只在派发期有效** | 对蓝图是陷阱（存下来就悬空）。V2 应当明确载荷的生命周期 |

---

## 4. 可选的 V2 方案（**候选，未裁**）

### 4.1 方案 A：两条注册路径，一套存储

```cpp
// C++ 路径：直接收回调，无需任何派生类型
FTcsEventSubscription Subscribe(FGameplayTag Channel, TFunction<void(const FTcsEventPayload&)> Callback);

// 反射路径：脚本/蓝图，用 UObject + UFunction
FTcsEventSubscription SubscribeReflected(
    FGameplayTag Channel, UScriptStruct* PayloadType,
    UObject* Sink, FName FunctionName);
```

| 优点 | `Subscribe` 满足铁律 3（C++ 不用派生）；两条路径各自最优 |
|---|---|
| 缺点 | 两条路径、两套语义，读者要分清 |

### 4.2 方案 B：统一存储 + 手写函数对象（**替换 TFunction 本体**）

把底层回调从 `TFunction` 换成一个**手写的、保证零堆分配的**类型擦除函数对象：

```cpp
struct FTcsEventSink
{
    void* Object = nullptr;                                  // 捕获物（可内联存放在下方缓冲）
    void (*Invoke)(void*, FGameplayTag, const void*) = nullptr;  // 直函数指针，无虚函数
    void (*Destroy)(void*) = nullptr;
    alignas(16) uint8 Inline[24] = {};                       // 与 TFunction 同尺寸，但由我们保证不溢出
};
```

| 相比 `TFunction` | 收益 |
|---|---|
| 无 `TFunction_OwnedObject` 的虚函数层 | 派发少一次虚调用 |
| 捕获物尺寸由我们编译期断言 | **保证零堆分配**，不会因为捕获变大而静默退化 |
| 无内层 TFunction | 反射路径也只有一层间接 |

| 缺点 | 要自己管拷贝/析构/移动，是**手写基础设施** |
|---|---|

> **诚实说明**：这条只省「一次虚调用 + 保证不堆分配」，**不是数量级提升**。真正的数量级问题在 §4.3。

### 4.3 方案 C：干掉「每次派发复制订阅数组」

**先把问题缩小到它真正的范围**：这里**只有一件事**——

> 派发循环正在遍历订阅数组时，如果某个回调**增删了订阅**，迭代会不会失效？

**它不涉及**：订阅的增删语义（那是生命周期管理）、失效订阅的清理（那是惰性摘除）。这两件事在**本问题解决之后一如既往地存在**。

#### 三种解法

| 方案 | 做法 | 派发期分配 | 增删何时生效 | 失效风险 |
|---|---|---|---|---|
| **(a) 快照复制** | 派发前复制整份订阅数组，遍历副本 | **每次派发 1 次堆分配 + memcpy** | 下一拍 | 低（副本天然隔离） |
| **(b) 版本化重试** | 派发前把频道版本号本地取一份；遍历中对齐原数组；若发现版本变了（回调里改过），**从头再遍历一次** | **零分配** | 下一拍 | **中——见下方风险** |
| **(c) 延迟修改** | 派发中所有增删只记录、不执行；派发结束统一执行 | **零分配** | **本拍末尾立即生效** | 低（结构不变） |

#### (b) 版本化重试的展开（你问的这个）

```cpp
void Publish(FGameplayTag Channel, const void* Payload)
{
    int32 DispatchDepth = 0;   // 重入深度
    do
    {
        const uint32 VersionAtEntry = ChannelVersion;
        const int32 Count = Subscriptions.Num();
        for (int32 i = 0; i < Count; ++i)
        {
            if (i >= Subscriptions.Num()) break;   // 已被缩容，退出
            Subscriptions[i].Invoke(Payload);
        }
        ++DispatchDepth;
    }
    while (ChannelVersion != VersionAtEntry && DispatchDepth < MaxRetry);
}
```

**它的核心假设是「回调里的增删是少数，重试一次就收敛」。** 这个假设在多数场景下成立，但**在这里不成立**：

| 风险 | 说明 |
|---|---|
| **重试语义不明** | 若订阅 A 在回调里加了一个订阅 B，重试时 **B 会收到这次事件**——可 B 是在事件发生之后才订阅的。该不该给它？**没有任何正确答案** |
| **重试放大成本** | 若某个回调里有增删（例如「收到伤害 → 给自己挂状态 → 状态注册监听」），每次派发都会重试一遍，成本反而翻倍 |
| **重试上限是两可约定** | 超过 MaxRetry 后放弃？还是接受不一致？两种都不好 |

⇒ **(b) 不适合作为主路径。** 它是一种**乐观并发**策略：收益是省一次分配，代价是引入「新订阅者可能收到旧事件」这个语义空洞。

#### (c) 延迟修改的展开

```cpp
void Publish(FGameplayTag Channel, const void* Payload)
{
    const int32 Count = Subscriptions.Num();   // ← 不复制，直接按此长度遍历
    for (int32 i = 0; i < Count; ++i)
    {
        Subscriptions[i].Invoke(Payload);
    }
    FlushPendingChanges();   // 派发结束后统一应用增删
}
```

**为什么它零分配又安全**：

| 保障 | 机制 |
|---|---|
| 数组在遍历期间不变 | 增删被推到一个**待处理列表**，不在派发中执行 |
| 内存不失效 | 既然不缩容，**指针与元素地址都不会变** |
| 增删语义明确 | 一律「派发结束后生效」，没有歧义 |
| 重入不破坏外层 | 一个**深度计数**记录当前是否在派发中；已在派发中就直接入待处理列表 |

**成本**：一个待处理列表 + 一次深度判断。**没有每次派发的堆分配。**

#### 我上一版的错误

我在 §4.3 把 (b) 加粗成建议，**依据只有「零分配」这一个维度**，没有考虑：

1. **「新订阅者会不会收到旧事件」这个语义问题**——而这恰恰是 (b) 唯一真正的代价；
2. **(c) 同样零分配，且没有语义空洞**——我做结论时根本没有把 (c) 当作独立选项评估。

⇒ **这是我的推理缺陷：拿单一维度（分配次数）当决策依据，漏掉了语义维度。**

#### 修正后的建议

| 优先级 | 方案 | 理由 |
|---|---|---|
| **推荐** | **(c) 延迟修改** | 零分配 + 语义明确 + 无悬空风险 |
| 可选 | (a) 快照复制 | 实现最简单、最不容易写错；代价是每次派发一次分配。**若要 (c) 的正确性但不想写待处理列表，就用它** |
| 不推荐 | (b) 版本化重试 | 省一次分配，换来一个没有正确答案的语义空洞 |

> **但最终选 (a) 还是 (c)，取决于我们是否信任「每帧千次派发」这个规模预期。** 这个预期目前**没有实测数据**（见 §5 的 Q3 说明）。在拿到数据之前，**按最保守的方案选 (a) 也完全站得住**。

### 4.4 方案 D：频道结构

| 选项 | 派发成本 |
|---|---|
| 单层 `TMap<FGameplayTag, TArray<Sink>>`，精确匹配 | 1 次 map 查找 |
| 支持层级匹配（显式开启） | 每次派发多 N 次 map 查找 |
| 预计算：注册期把层级关系摊平 | 派发仍 1 次查找，代价转移到注册期 |

---

## 5. 裁决结果（2026-10-26）

| # | 问题 | 裁决 |
|---|---|---|
| **Q1** | 底层回调用什么 | **(b) 手写零堆分配函数对象** |
| **Q2** | 注册路径 | **(a) 两条路径**：C++ 直收回调 / 反射路径收 UObject + UFunction |
| **Q3** | 派发期订阅变更 | **(c) 延迟修改 + 软删标记**（零分配，且注销立即生效） |
| **Q4** | 频道层级匹配 | **(b) 显式开启** |
| **Q5** | 载荷生命周期 | **(c) 受控借用 + 明确契约** |

### Q3 的现状说明

用户倾向 **(a) 快照复制**，理由是要保留三个特性：**快照派发 / 惰性摘除 / 订阅生命周期闭合**。

**必须澄清一点**：这三个特性与 Q3 的三种解法**不在同一层面**——

| 特性 | 归谁 | 与 Q3 的关系 |
|---|---|---|
| **快照派发**（派发期遍历稳定） | **就是 Q3 本身** | 是同一个问题 |
| **惰性摘除**（失效订阅顺带移除） | 失效清理策略 | **与 Q3 无关**，三种解法都要做 |
| **订阅生命周期闭合**（施加/撤销配对注册注销） | 使用纪律 | **与 Q3 无关**，三种解法都要做 |

⇒ 选 (a) 或 (c) **都完整保留后两个特性**。差别只在「派发中遍历的稳定性怎么保证」。

**我上一版把 (b) 加粗是错的**，依据只有「零分配」，漏掉了 (b) 的语义空洞（回调解中新增的订阅会收到本次事件）。已按 §4.3 修正：**(c) 延迟修改同样零分配，且无语义空洞**。

**最终裁决（2026-10-26）**：用户提问「已失效的订阅者还会收到广播吗？」——在**朴素 (c)**（只锁长度）里**会**。

⇒ 精化为 **软删标记**：删除拆成「逻辑删除（立即可见）」+「物理摘除（深度归零时执行）」。

| 保证 | 机制 |
|---|---|
| 遍历期不缩容 | 锁定 Count |
| 派发中注销的**立即不再收到** | 遍历时检查 bPendingRemoval |
| 派发中新增的**本拍不生效**（含嵌套内层） | 进 PendingAdds，不进 Slots |
| 物理摘除与应用新增 | 深度归零时统一执行 |

**行为对比**：**(c)+软删 严格优于 (a) 快照**——(a) 的快照里仍留着已注销的订阅，本次派发它还会收到。

**关于「为什么不在派发前 flush」**：入口处待处理列表天然为空（上一轮归零已清），在那里 flush 是空操作，且会掩盖「某次没走到归零」的缺陷。**Flush 的时机只有一个：深度归零。**

---

*本文件为调研记录，族外草稿，零提交。*
