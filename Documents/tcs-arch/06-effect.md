# ARCH-06-effect · TcsEffect（效果链解释器与触发执行引擎）

- **文档 ID**：`ARCH-06-effect`
- **类型**：模块档案（现状审核）
- **状态**：生效中（2026-10-17 审）
- **权威范围**：回答「`TcsEffect` 今天实际长什么样、契约是什么、欠了什么」。**不承载决策理由**（旧 `decisions/` 与 `log/` 是理由的真相源），**不承载可执行需求**（`openspec/specs/effect-*` 是契约的真相源）。三者分工见 [`ARCH-INDEX`](INDEX.md) §2。
- **模块规模**：**55 文件 / 7 139 行**（`TcsEffect.Build.cs` 24 + 模块壳 20 ⇒ 业务代码 7 095）
- **需求锚点**：7 份能力 / **1 108 行** / **19 Requirement** / **134 Scenario**（`effect-chain` 107·4·11、`effect-chain-asset` 37·1·4、`effect-interpreter` 366·10·42、`effect-step-dispatch` 113·4·14、`effect-trigger` 393·8·53、`effect-trigger-asset` 52·1·6、`entity-query-contract` 40·1·4）
- **审核顺序**：本档案第 6 篇（`ARCH-01` ~ `ARCH-05` 已审）

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

> **`TcsEffect` 是「事件 → 触发行 → 效果链」的触发执行引擎 + 链原语解释器——系统做流转的流转本体。**

这个「流转本体」的定位不是修辞，它在代码里有三条硬证据：

1. **它对领域零感知**：`TcsEffect.Build.cs:12-23` 的依赖集是 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsAttribute` / `TcsAssetButler`——**七个全在它下方或同层**，没有一个上层领域模块。而它却要执行 `TcsDamage` 的伤害步骤、`TcsTargeting` 的选择步骤、`TcsBuff` 的施加步骤。做法 = **三张进程级注册表 + 三对自注册宏**：类型（步骤 struct / 条件 struct / 载荷 struct）的属主模块自己把执行器喂进来，机制层只按 `const UScriptStruct*` 查表分派。
2. **它是唯一被全仓 6 个模块直接 include 的机制层**：`TcsBuff`(3) / `TcsDamage`(3) / `TcsIntegration`(4) / `TcsSkill`(2) / `TcsTargeting`(2) / `TcsState`(0，仅注释引用)。
3. **它自己只提供「句子」不提供「词汇」**：插件不声明任何链 id、触发规则、条件词、开关词——`ChainId` 住宿主 `EffectChain` 根、`GateTags` 住 `EffectTriggerGate` 根、链内变量住 `EffectChainRunVar` 根、分类词住 `DamageCategory` 根、规则词住 `EffectTriggerDef` 根，**五个根全部由宿主 ini 声明**。

### 1.2 文件组与职责【实测】

| 目录 | 行数 | 职责 | 关键符号 |
|---|---|---|---|
| `Public/`（根） | 728 | 门面（718 行，全仓最大的单个头）+ 日志通道 | `UTcsEffectSubsystem` / `LogTcsEffect` |
| `Public/Chain/` | 1 248 | 链定义、运行态、步骤协议、执行器注册表、6 个链原语 | `FTcsEffectChain` / `FTcsChainRun` / `FTcsStepExecute` / `FTcsEffectStepExecutorRegistry` |
| `Public/Trigger/` | 1 485 | 触发定义、实例、求值器、条件注册表、载荷读取器注册表 | `FTcsEffectTriggerDef` / `UTcsTriggerEvaluator` / `FTcsTriggerConditionRegistry` / `FTcsTriggerPayloadReaderRegistry` |
| `Public/Host/` | 94 | 宿主能力注入契约 | `ITcsEntityQuery` |
| `Private/Chain/` | 966 | 链执行 + 6 个原语执行器 + 事件等待表 | `RunFrom` / `ReleaseRun` / `FTcsChainEventWaitRegistry` |
| `Private/Trigger/` | 1 203 | 触发表 + 求值器 + 三条内置条件 + 载荷读取器 | `FTcsTriggerRegistry` / `TestTriggerCondition*` |
| `Private/Attribute/` | 184 | **属性访问白名单薄壳**（本模块唯一取属性门面处） | `FTcsEffectAttributeAccess` |
| `Private/`（根） | 1 187 | 门面实现，按职责拆 5 个 `.cpp` | `TcsEffectSubsystem{,_Run,_RunAccess,_StepProtocol,_ChainWait,_Trigger,_HostSlots}.cpp` |

**门面的拆分纪律值得记一笔**：`TcsEffectSubsystem.cpp` 已过 300 行，后续每加一组内聚职责就开一个新 `.cpp`（`_Run` / `_RunAccess` / `_StepProtocol` / `_ChainWait` / `_Trigger` / `_HostSlots`），**`_HostSlots.cpp` 的开头还专门写明「不为套用风格而搬动无关代码」**（`RegisterStepExecutor` 留在旧文件里，因为它属 `Chain` 时代、寿命手法不同）。

### 1.3 本模块最反常的一点：**规格覆盖是全仓最好的一档，债却集中在「同一个类的三个同构入口」上**【实测+推断】

**先看好的一面**：7 份能力 / 1 108 行规格对 7 139 行代码 ≈ **1 : 6.4**，是全仓最密的（对照 `ARCH-05` 的 `TcsState`：10 份规格 1 628 行对 2 753 行代码 ≈ 1 : 1.7，但其中 7 份整篇在讲别的模块）。而且 `effect-interpreter` 的 42 个 Scenario 里，**熔断、嵌套预算、三路唤醒、多锚竞态**这些最难写对的纪律全部有对应场景，且代码逐条兑现（见 §2.4）。

**反常的地方**：本模块 12 条债里有 **3 条（D-2 / D-3 / D-11）出自同一个结构**——门面的**三个宿主插槽入口**：

| 入口 | 落地时间 | 有没有「拒绝判在门面」 | 有没有配对撤销口 | 有没有寿命语义 |
|---|---|---|---|---|
| `RegisterStepExecutor` | 2026-09-24（SCRIPT-8） | ❌ **没有**（返回值恒 true） | ❌ **没有** | ✅ 有 |
| `RegisterConditionEvaluator` | 2026-10-06（R-2 后段） | ✅ 有 | ✅ `UnregisterConditionEvaluator` | ✅ 有 |
| `RegisterPayloadReader` | 2026-10-06（R-2 后段） | ✅ 有 | ✅ `UnregisterPayloadReader` | ✅ 有 |

**后写的两个把先写的那个的坑全修了，而先写的那个没回头改。** 这不是疏忽——`_HostSlots.cpp` 的文件头注释与 `RegisterConditionEvaluator` 的注释里，**两处都明确写着「`RegisterStepExecutor` 没有配对撤销口 ⇒ 同一 PIE 无法重臂」**，说明作者完全知道这件事，只是判成了「不改无关代码」。

> **缺陷方向（新增，与 `ARCH-05` 的 G-12 正交）**：`ARCH-05` 抓的是「**代码搬走了、指向它的名字没搬**」——空间方向的滞留。本条是「**新写的入口把老入口的坑修了、老入口没回头**」——**时间方向的滞留**。两者的共同点：**证据就在旁边的注释里，只是没人回头读**。已记为全局待办 **G-15**。

---

## §2 已落地

### 2.1 三张注册表：同一套手法复制三次，且**第三次比第一次更准**【实测】

「机制层不认识领域类型」这件事，`TcsEffect` 用三张**进程级单例注册表**实现（函数局部静态，`D0-4` 单游戏线程）。三者同构到近乎逐字：

| 注册表 | 键 | 登记方 | 注册值 | 自注册宏 | 动态入口 |
|---|---|---|---|---|---|
| `FTcsEffectStepExecutorRegistry` | 步骤 `UScriptStruct*` | 步骤类型的领域模块 | `FTcsStepExecute`（`TFunction`） | `UE_DEFINE_EFFECT_STEP_EXECUTOR` | `Register(..., LifetimeObject, LifetimeWorld)` |
| `FTcsTriggerConditionRegistry` | 条件 `UScriptStruct*` | 条件类型的定义方（含宿主） | `FTcsTriggerConditionTest`（`TFunction`） | `UE_DEFINE_TRIGGER_CONDITION_EVALUATOR` | 同款四参 |
| `FTcsTriggerPayloadReaderRegistry` | **载荷** `UScriptStruct*` | **载荷类型的属主模块** | `FTcsTriggerPayloadRead`（`TFunction`） | `UE_DEFINE_TRIGGER_PAYLOAD_READER` | 同款四参 |

**三条共有的正确设计**：

1. **静态初始化期零 UObject 触达**。注册器构造只写 `PendingEntries`，**不调用 `GetStepStruct()`**（那会触 `StaticStruct()`）。理由写在三处头注释里，且**引擎同款先例**：`FNativeGameplayTag` 用 `UGameplayTagsManager::GetIfAllocated()` 规避同一问题。延迟到首次 `Find` 时才 `ResolvePending()`。
2. **寿命语义两半齐全**（2026-09-29 `DEC-04` 裁定 ⑤）：注册表是进程级（比世界活得久），故动态登记项 MUST 记 ① 宿主对象 `TWeakObjectPtr` ② 登记世界 `TWeakObjectPtr<UWorld>`；失效判据三条（对象没了 / 世界没了 / 世界不符）任一成立即视为未命中 + **移除条目 + Warning**。而**强持有归门面 `UPROPERTY` 数组**——两半互补，缺一都有洞。
3. **拒绝门是「同世界活对象重复」而非「键重复」**。这条是本模块最值得抄的设计：旧口径「键已存在就拒」会让**失效条目毒化后续所有 PIE**（第二次 PIE 撞 `Contains` ⇒ 拒绝 ⇒ 之后一直用旧世界的 stale 指针）。新口径 = 既有条目**已失效则替换**、**有效则拒绝**。

**第三次比第一次更准的一处**（`D-11`，见 §5）：三张表的 `Register` 里，**只有 `FTcsEffectStepExecutorRegistry::Register` 会先调 `ResolvePending()`**（`TcsEffectStepExecutor.cpp:42`），另两张不调（`TcsTriggerCondition.cpp:35-67` / `TcsTriggerPayloadReader.cpp:34-66`）。差别在**幂等性**：`ResolvePending` 是「首次调用即消费 `PendingEntries`」的一次性动作，若不先跑，`Register` 的 `Contains` 检查就**看不见静态自注册项**。

### 2.2 挂起-恢复协议：三路唤醒源 + 一套配平纪律【实测】

这是本模块**技术含量最高**的一块，也是全仓少有的「异步语义被完整落地」的地方。

**运行态池化**：`TTcsInstancePool<FTcsChainRun, FTcsChainRunTag> RunPool`（`TcsEffectSubsystem.h:665`）。`FTcsChainRun` 住 `ChainId`（**不是链定义指针**）+ `PC` + `Context`（黑板）+ `Owner`（门面弱引用）+ `Self`（自身句柄）+ **4 个挂起锚**。

**4 个挂起锚**（`TcsChainRun.h:157-193`）：

| 锚 | 类型 | 谁装 | 谁配平 | 用途 |
|---|---|---|---|---|
| `PendingExpiry` + `bHasPendingExpiry` | `FTcsTimeEntryHandle` + `bool` | 步骤（`WaitDelay` / `WaitEvent`） | 步骤 | 定时唤醒 |
| `PendingEventTag` | `FGameplayTag` | 门面 `ArmEventWait` | 门面命中路 / 步骤超时路 / `ReleaseRun` | 事件唤醒 |
| `bPendingEventHit` | `bool` | **门面**（派发当场） | **步骤** | 区分「命中」与「超时」 |
| `PendingChildRun` / `ParentRun` | `FTcsChainRunHandle` ×2 | 门面 `ArmChildWait` | 步骤 / `ReleaseRun` | 子链唤醒（**双向登记**） |

**配平纪律的一句话**（`TcsChainRun.h:151-155`）：

> 锚由**步骤**在首入时装、在**被唤醒**时配平清除；门面唤醒时**不清除任何锚**——锚的在场与否就是步骤自辨「首入 / 被唤醒」的**唯一判据**。唯一例外是 `ReleaseRun`：运行态释放时无条件全清。

这条纪律的价值在 `WaitEvent` 的执行器里看得最清楚（`TcsStepWaitEvent.cpp:29-155`）——它有**四条分支**，靠两个布尔锚区分：

```
① bPendingEventHit        ⇒ 命中唤醒（撤到期条目）→ TSR_Completed
② bHasPendingExpiry       ⇒ 超时唤醒（摘等待登记）→ TSR_Completed
③ PendingEventTag.IsValid() ⇒ 无效唤醒（脚本层乱调 ResumeRun）→ TSR_Running（继续挂起）
④ 以上皆无                 ⇒ 首入（装锚）→ TSR_Running
```

**分支③是本模块最有价值的一处防御**：`ResumeRun` 是 `UFUNCTION()` 脚本层可调的公开方法，一条**来路不明的唤醒** MUST NOT 骗过步骤。同样的防御在 `RunChildChainStep` 里也有一份（`IsRunActive(Child)` 复核）。

**`ReleaseRun` 是唯一汇聚点**（`TcsEffectSubsystem_Run.cpp:242-288`）：五条结束路径（走完 / 深度熔断 / 步数预算熔断 / 链定义不可解析 / 未知步骤类型）全部经过它。它做三件事，**顺序不可换**：

1. **先 `DropEventWait`**——靠 `PendingEventTag` 认领登记，故 MUST 早于下面的清锚；
2. **清黑板 + 清全部锚**（池 `Free` 不清零，故这里必须显式清，否则槽位内容跨生命周期残留）；
3. **`RunPool.Free` 之后**才 `ResumeRun(ParentRun)`——注释写明理由：父侧被唤醒时会用 `IsRunActive(PendingChildRun)` 复核，**Free 未做则复核答「子链还在跑」⇒ 父重新挂起，而通知已用掉、不会再来 ⇒ 死链**。

### 2.3 两条熔断护栏：**预算共享 + 深度常量**，且注释解释了「为什么不能合并」【实测】

`RunFrom` 用 `FTcsChainEntryScope` 作用域守卫（`TcsEffectSubsystem_Run.cpp:29-56`）实现两件事：

- **预算按最外层进入计**：`Depth == 0` 时才重置 `Steps` 与 `Limit`，嵌套起链（`RunSubChain` / `Branch` 的 `bWait=true` 同步起链）**共用父链预算**。原实现把它当 `RunFrom` 的局部变量 ⇒ 每进一条链各拿一份新预算 ⇒ `A→B→A→B` 自激链既绕过预算、又直接吃调用栈。头注释对这条的定性很准：「**不是性能问题，是崩溃**」。
- **深度上限是固定常量** `TcsMaxChainNestingDepth = 16`（`:20`），**MUST NOT 做成链定义字段**——因为 `MaxStepsPerFrame` 由作者给、可配到 4096，它不能当调用栈护栏。

**作用域守卫的写法值得记**：`RunFrom` 有四条 `return`，手工增减 `ChainEntryDepth` 必漏；用 `FTcsChainEntryScope` 的析构兜住全部路径。这是「不靠纪律、靠语言机制」的正面样本。

### 2.4 事件等待：**订阅计数配对 + 先摘登记再唤醒**【实测】

`FTcsChainEventWaitRegistry`（`Public/Chain/`，135 行）的设计前提写在类注释里，是**两条硬事实**：

1. 总线 `Subscribe` 只收 `UTcsEventHandler*`（传不了 lambda），**派发只传 `(EventTag, Payload)`、不传订阅句柄** ⇒ 每条等待各订一次并不能让回调自辨身份（收益为零、成本为正）；
2. 既有纪律「订阅表不得随数量膨胀」。

⇒ **同一 `EventTag` 的多个等待者共用一条订阅**，运行态**不持订阅句柄**、只持「在等哪个 tag」这一可配平锚。

**一个容易误判的点，类注释专门挡了**（`TcsChainEventWaitRegistry.h:28-31`）：

> 同一 tag 若**同时被触发行与链等待使用**，总线上会有**两条**订阅——两条各属一个 Handler，是必需而非泄漏（一条订阅只对一个 Handler 派发）。人工检查「订阅数不随等待者数量增长」时按本口径读。

**唤醒顺序是硬约束**（`WakeEventWaiters`，`TcsEffectSubsystem_ChainWait.cpp:90-127`）：取等待者快照 → 逐个代际校验 → **先摘登记**（使「同一事件不会二次唤醒同一运行态」不依赖步骤行为）→ 写载荷 + 置 `bPendingEventHit` → `ResumeRun`。全程**按句柄重解析、用完即弃**，因为 `ResumeRun` 会执行步骤、可能起链并扩容池。

### 2.5 触发行求值：四道门 + **主体路由作为门①的一部分**【实测】

`UTcsTriggerEvaluator::HandleEvent_Implementation`（`TcsTriggerEvaluator.cpp:20-126`）的门序：

```
门①  事件 Tag 路由（订阅完成）+ 实例主体匹配（新增，2026-10-05）
门②  ExecutionGate        —— 最廉价先判
门③  GateTags（全部点亮）
门④  Conditions           —— 最贵最后判（可含宿主自定义求值）
     → ExecuteChain
```

**主体匹配被明确归到门①而不是新立门⑤**（`effect-trigger` 规格 `:216` 明文）。实现是一段**零领域知识**的反射比较（`:64-73`）：

```cpp
if (RowSnapshot.Subject.IsValid() && PayloadInfo.Subject.IsValid())
{
    const UScriptStruct* SubjectType = PayloadInfo.Subject.GetScriptStruct();
    if (RowSnapshot.Subject.GetScriptStruct() != SubjectType ||
        !SubjectType->CompareScriptStruct(...))  continue;
}
```

它解决的是台账 `TRIG-6` 的实测缺陷：**内联触发行原本是「事件 Tag 级」规则，一个状态实例登记的行会被世界里任何同 Tag 事件引爆**（PIE 实测「先施主、后施次」时次实例的 `Applied` 引爆了主实例的行，增量 30 期望 25）。修法是「两侧主体精确匹配」——`TcsEffect` **不 include 任何状态句柄类型**，靠 `FInstancedStruct` 的反射比较完成。

**另一处值得记的实现细节**：行数据在进入条件求值前**整份拷贝成值语义快照**（`:51-59`），因为宿主条件回调能登记/摘除行并让 `TArray` 搬移。条件通过后**再校验一次原句柄**（`:107-110`）——回调中已摘除或已被复用的行 MUST NOT 起链。这两条对应规格的两个 Scenario（`effect-trigger:268` / `:273`），逐条兑现。

### 2.6 载荷读取器：把「依赖铁律」变成一处可审计的注册点【实测】

这是本模块**设计上最漂亮的一处**，也是 `ARCH-04` 的 `MEM-20260923-02` 教训（「跨模块数据流条款要过依赖审计」）的正解形态。

**问题**：`FTcsTriggerContext` 需要 `Caster` 与 `ClassificationTags`，但这两个值只能从事件载荷里读——而载荷类型（`FTcsDamageFlowCollectEvent` / `FTcsBuffEventPayload` / 宿主的自定义事件）**全部住上层模块**。`TcsEffect` MUST NOT 认识它们（依赖铁律 `Core←Attribute←Effect←{Damage,…}`）。

**解法**（`TcsTriggerPayloadReader.h:15-25` 的立项理由写得比规格还清楚）：

> 「怎么从载荷里读主体信息」这件事由**载荷类型的属主模块**登记——它可能写 `GetPtr<FTcsDamageFlowCollectEvent>()`，因为它就在那个模块里。

**三处同构的「属主自登记」**（`TcsTriggerPayloadReader.h:183-188` 有一张现成的表）：

| 注册表 | 键 | 登记方 |
|---|---|---|
| `FTcsEffectStepExecutorRegistry` | 步骤 struct | 步骤类型的领域模块 |
| `FTcsTriggerConditionRegistry` | 条件 struct | 条件类型的定义方（含宿主） |
| `FTcsTriggerPayloadReaderRegistry` | **载荷 struct** | **载荷类型的属主模块** |

**今天实际登记了 2 个读取器**（`grep UE_DEFINE_TRIGGER_PAYLOAD_READER` 实测）：

- `TcsDamage/Private/Flow/TcsDamageFlowCollectEvent.cpp:38` —— 伤害收集事件，`Caster ← Attacker` + 分类标签直通；
- `TcsBuff/Private/State/TcsBuffEvents.cpp:36` —— 状态生命周期载荷，`Caster` = 有效 `Instigator` 否则退 `Entity`，`Subject = Make<FTcsBuffInstanceHandle>(Handle)`。

**「空标签集是显式交付，不是遗漏」这句话必须留着**（`TcsTriggerPayloadReader.h:23-25`）：无读取器时标签集为空集，而 `HasAllTags` 在空集上匹配非空 Tag 数组**恒不过**——故内容侧若要用该条件，MUST 有读取器提供标签集。这是一个「默认行为看起来像 bug、其实是契约」的正确样本。

### 2.7 属性访问白名单：一个**只有 6 个方法**的私有薄壳【实测】

`FTcsEffectAttributeAccess`（`Private/Attribute/`，184 行，**纯 C++ 值对象、栈上存活**）是本模块**唯一**取属性门面的地方。它暴露的面被刻意压到最小：

| 方法 | 用途 |
|---|---|
| `Resolve(World)` | 唯一取用点（内部走 `UTcsAttributeSubsystem::Resolve`） |
| `IsLedgerReady(Entity)` | 账本认不认识这个实体 |
| `HasAttribute(Entity, Attribute)` | 该键在不在账本上（**不取值**） |
| `ApplyModifier` / `BeginBatch` / `Commit` | 写面（唯一提交点纪律） |
| `EvaluateCurrent` | 读面 |

**「暴露面即纪律」这句注释**（`TcsEffectAttributeAccess.h:27-28`）：

> 除这些之外 MUST NOT 暴露任何属性 API——需要新能力时先改本类（让改动可审计）。

这条纪律是 `ARCH-04` 的 **D-7**（规格明文禁止「直伸存储」，而 `TcsEffectAttributeAccess::HasAttribute` 正在直伸 `Store->FindInstance`）的所在处——即本模块**唯一违反依赖边界的地方也在这 6 个方法里**。见 §5 D-6。

### 2.8 两个 Def 资产：搬迁完成，重定向已配【实测】

`UTcsEffectChainDef`（`Public/Chain/`，99 行）与 `UTcsEffectTriggerDefAsset`（`Public/Trigger/`，115 行）于 2026-10-15 随 `PLN-REFACTOR-1` Task 8 自 `TcsIntegration` **搬入本模块**，两者都在 `.cpp` 里以 `UE_DEFINE_PRIMARY_ASSET_TYPE` 向 `TcsAssetButler` **向下注册**（这是 `TcsEffect.Build.cs` 依赖 `TcsAssetButler` 的唯一理由）。

**搬迁做对的两件事**：

1. **主资产身份取内容 tag 而非资产名**：`GetPrimaryAssetId()` 覆写为 `[PrimaryAssetType, ChainId.GetTagName()]` / `[..., TriggerTag.GetTagName()]` ⇒ 资产改名、挪目录都不失联。
2. **`IsDataValid` 把 `NotValidated` 提升为 `Valid`**：`UObject::IsDataValid` 基类默认返回 `NotValidated`（`Obj.cpp:6096`），不提升会让「已校验通过」在编辑器里显示为「未验证」，且装置断言 `== Valid` 必然失败。**触发资产做了，链资产没做**（链资产的 `IsDataValid` 直接返回 `Result`，无提升段）——`effect-trigger-asset` 规格 `:20` 明文要求提升，`effect-chain-asset` 规格**未提这条**。这是**规格层面的不对称**，已记 `ARCH-04` 的 `TOOLS-2` ③ 名下（校验矩阵轮），本档案登记为 **D-10**。

### 2.9 门面反射面：**23 个 `UFUNCTION()` 无 specifier**，且「没有反射面」的每一类都写了理由【实测】

`UTcsEffectSubsystem` 的 718 行里公开方法共 **34 个**，其中 **23 个**标了 `UFUNCTION()`（无 specifier），**11 个**是纯 C++ 面：`FindChain` / `UnregisterTriggerRowsBySource` / `ExecuteChain` / `RunChildChainStep` / `ArmEventWait` / `DropEventWait` / `NextTriggerRandomValue` / `GetRunSource` / `GetRunCausedBy` / `SetEntityQuery` / `GetEntityQuery`。口径是「无 specifier」——`BlueprintCallable` 会被 UHT 的蓝图参数校验拒绝（形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等非 `BlueprintType` 的 `USTRUCT()`）。

**四类「没有反射面」的方法，每一类都写了具体理由**，这是本模块文档质量高的直接体现：

| 方法 | 无反射面的理由 |
|---|---|
| `FindChain` | 返回**裸 struct 指针** `const FTcsEffectChain*`——UHT 不支持 struct 指针作反射返回（引擎全仓零先例） |
| `UnregisterTriggerRowsBySource` | 形参 `FTcsSourceHandle` 是**非反射纯 C++ struct**（无 `USTRUCT` 宏） |
| `SetEntityQuery` / `GetEntityQuery` | `ITcsEntityQuery` 三方法零 `UFUNCTION` 且形参含 `TFunctionRef`（头文件自注「C++ 专用面」） |
| `GetRunSource` / `GetRunCausedBy` | **观测读数**（宿主装置与将来的 Explain 面板读它）；脚本可达面归 R8 Explain 轮 |

**两条「传句柄、不传上下文」的落地**：`FTcsEffectContext` 是非反射纯 C++ struct（含 `FInstancedStruct` + `TMap`），故脚本层拿不到它；解法是 6 个按句柄访问器（`GetRunTargets` / `SetRunTargets` / `TryGetRunVariable` / `SetRunVariable` / `GetRunCaster` / `GetRunInstigator`）。规格 `effect-interpreter:149` 明文 **MUST NOT 提供 `FTcsEffectContext` 整体读写口**——整体暴露即等于要求上下文反射化，而访问器路线的全部收益正是**避开**它。

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

按「谁在用、用多少」排序，本模块对外提供 **7 组契约**：

| 契约组 | 符号 | 外部消费者（实测命中数） | 稳定性 |
|---|---|---|---|
| **步骤执行器注册** | `FTcsStepExecute` / `UE_DEFINE_EFFECT_STEP_EXECUTOR` / `ETcsStepResult` | `TcsDamage`(3+2) / `TcsBuff`(1+2) / `TcsTargeting`(1+1) | ★★★ 最稳定（三张表里唯一被三个模块同时使用的） |
| **条件注册** | `FTcsTriggerConditionTest` / `UE_DEFINE_TRIGGER_CONDITION_EVALUATOR` / `FTcsTriggerContext` | 本模块内 3 条内置条件；外部**零**（`TcsSkill` 仅注释引用） | ★★ 稳定，但**无外部使用者** |
| **载荷读取器注册** | `FTcsTriggerPayloadRead` / `UE_DEFINE_TRIGGER_PAYLOAD_READER` | `TcsDamage`(1) / `TcsBuff`(1) | ★★★ 稳定（两个属主模块各一） |
| **链登记与执行** | `UTcsEffectSubsystem` 的 `RegisterChain` / `ExecuteChain` / `FindChain` / `IsChainRegistered` | `TcsIntegration`(2/3/3/1) / `TcsSkill`(3) / `TcsDamage`(1) | ★★★ 稳定 |
| **触发登记** | `RegisterTriggerRow` / `UnregisterTriggerRow(sBySource)` / `FTcsEffectTriggerDef` / `FTcsEffectTriggerInstance` | `TcsBuff`(2/2/4/2) / `TcsIntegration`(3/—/4/1) / `TcsSkill`(—/—/2/—) | ★★★ 稳定 |
| **黑板与运行态** | `FTcsEffectContext` / `FTcsChainRun` / `FTcsChainRunHandle` | `TcsDamage`(3/2/—) / `TcsBuff`(1/1/1) / `TcsSkill`(2/2/17) / `TcsTargeting`(14/1/—) | ★★★ 稳定，但**注意 `TcsTargeting` 对 `FTcsEffectContext` 有 14 处使用**——它是除本模块外最深的绑定者 |
| **实体查询注入** | `ITcsEntityQuery` | `TcsTargeting`(12) / `TcsSkill`(5) / `TcsIntegration`(3) / `TcsBuff`(1) | ★★★ 稳定（**注意：`TcsSkill` 有一份自己的注入点**，见 §5 D-7） |

**「稳定」的判据**（本档案统一口径）：有 ≥2 个外部模块使用 **且** 该符号的语义不依赖本模块内部实现选择。按此判据，上表 7 组全部成立。

### 3.2 MUST NOT 依赖（内部实现细节）【实测+推断】

| 符号 | 为什么外部不该依赖 | 现状 |
|---|---|---|
| `FTcsEffectStepExecutorRegistry` 的 `PendingEntries` / `Lifetimes` / `ResolvePending` | 内部实现；外部只该用宏与 `Find` | ✅ 无外部引用 |
| `FTcsTriggerRegistry`（**纯逻辑类，不是 UObject**） | 它是门面的私有成员，外部没有任何理由持有它 | ✅ 无外部引用 |
| `FTcsChainEventWaitRegistry` / `UTcsChainEventWaitHandler` | 事件等待是门面内部机制 | ✅ 无外部引用 |
| `FTcsChainEntryScope` / `TcsMaxChainNestingDepth` | 匿名 namespace 里的实现细节 | ✅ 无外部引用 |
| `RunChildChainStep` / `ArmEventWait` / `DropEventWait` / `NextTriggerRandomValue` | **头注释明文「宿主与脚本层 MUST NOT 调用」**——解释器与自己的步骤之间的内部协议 | ⚠️ 但它们声明在 `public:` 区（`TcsEffectSubsystem.h:408-478` 的 `StepProtocol` region） |

**最后一行是本模块契约面的一处结构问题**：规格与头注释都写着「宿主与脚本层 MUST NOT 调用」，但这四个方法在 **C++ 访问级别上是 `public`**。它们确实**没有** `UFUNCTION()`（脚本层够不到），但对 C++ 侧的任何模块都是敞开的。`RunChildChainStep` 的注释把这条约束写得很重（「不承担对外兼容承诺」），而**约束只靠注释**。

> 这与 `ARCH-05` 的 **G-14**（「注释里的契约没有校验手段」）同族，但方向相反：G-14 是「注释承诺了、实现没做」；这里是「注释禁止了、访问级别没禁」。已并入 **G-14** 的口径说明。

### 3.3 一条**反向**的边界：本模块对 `TcsAttribute` 的使用面被压到 6 个方法【实测】

`TcsEffect.Build.cs` 依赖 `TcsAttribute`（下层领域模块，**允许**）。但「允许依赖」不等于「随便用」——`FTcsEffectAttributeAccess` 把整个使用面压到 6 个方法（见 §2.7），且注释明文：

> 白名单是**两个消费模块各自的审计面**，MUST NOT 合并（口径见 `attribute-pipeline` 能力的「属性门面的解析点」需求）。

**这条纪律今天被违反了一次**（`HasAttribute` 直伸 `Store->FindInstance`），见 §5 D-6。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

```
  ┌──────────────────────── 事件总线（TcsCore） ────────────────────────┐
  │                        ↑ Subscribe(EventTag, Handler, Immediate)     │
  │                                                                     │
  │   UTcsTriggerEvaluator ──── 四道门 ────► ExecuteChain               │
  │   UTcsChainEventWaitHandler ─ WakeEventWaiters ─► ResumeRun         │
  └─────────────────────────────────────────────────────────────────────┘
        ▲                                          ▲
        │ RegisterTriggerRow                       │ RegisterChain
        │                                          │
  TcsBuff（内联行，Source = CascadeAnchor）    TcsIntegration（定义库装配期）
  TcsIntegration（独立资产行，Source = 定义库）

  起链的三个入口：
    ① TcsIntegration::UTcsCombatEntityComponent::ExecuteChainById  （有组件时）
    ② TcsSkill::TcsCastOps_Chain                                  （施法 MainChainStart）
    ③ 触发行命中                                                  （本模块内部）

  步骤执行器由领域模块自注册（静态初始化期，零调用方向）：
    TcsDamage    → FTcsStepDamage / FTcsStepModifyFlow
    TcsTargeting → FTcsStepSelectTargets
    TcsBuff      → FTcsStepApplyState
    本模块       → WaitDelay / WaitEvent / SetVar / RunSubChain / Branch / ModifyAttribute
```

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

| # | 约束 | 代码锚点 | 违反后果 |
|---|---|---|---|
| T-1 | **触发行登记必须早于事件发布** | `FTcsTriggerRegistry::EnsureSubscription`（`TcsTriggerRegistry.cpp:171-190`） | 事件发生时才订阅 = 该事件永久漏掉（无补投） |
| T-2 | **内联行的登记必须早于 `Applied` 广播** | `TcsBuffOps_Trigger.cpp:52` 在广播前调 `RegisterTriggerRow` | 状态自身的行为链漏掉自己的 `Applied` |
| T-3 | **内联行的退订必须早于 `Removed` 广播** | `TcsBuffOps_Trigger.cpp:72` 在广播前调 `UnregisterTriggerRowsBySource` | 已移除实例的行被自己的 `Removed` 引爆 |
| T-4 | **`ReleaseRun` 必须先 `DropEventWait` 再清锚** | `TcsEffectSubsystem_Run.cpp:257` 早于 `:269` | `DropEventWait` 靠 `PendingEventTag` 认领登记，先清则**永久泄漏订阅**（静默） |
| T-5 | **`ReleaseRun` 必须在 `RunPool.Free` 之后才通知父** | `:274` 早于 `:284-287` | 父侧 `IsRunActive` 复核答「还在跑」⇒ 父重新挂起而通知已用尽 ⇒ **死链** |
| T-6 | **`WakeEventWaiters` 必须先摘登记再 `ResumeRun`** | `TcsEffectSubsystem_ChainWait.cpp:110` 早于 `:126` | 「同一事件二次唤醒同一运行态」变成依赖步骤行为 |
| T-7 | **步骤 MUST NOT 在持有 `Context`/`Run` 引用期间起链** | `TcsChainRun.h:104-106`；`TcsStepRunSubChain.cpp:33-37` 取齐数据后才转交 | 池扩容搬移 ⇒ 引用悬空 |
| T-8 | **`RunFrom` 每步入器前必须按句柄重解析** | `TcsEffectSubsystem_Run.cpp:161` / `:233` | 同上 |

### 4.3 一条竖切级的教训：**「每事件一次」这个优化，是靠「读取点在行循环外」保住的**【实测】

`UTcsTriggerEvaluator::HandleEvent_Implementation` 在**进入行循环之前**读一次载荷（`:43`）：

```cpp
const FTcsTriggerPayloadInfo PayloadInfo = ReadPayloadInfo(Payload);
for (const FTcsEffectTriggerHandle& Handle : Candidates) { ... }
```

这不是随手写的顺序。`ITcsTriggerPayloadReader::Read` 的契约里有一条（`TcsTriggerPayloadReader.h:108-110`）：

> **「每事件一次」语义 MUST 保住**：求值器对同一次事件的多行订阅**只读一次载荷**、各行共用同一份结果；本插槽只替换「读取器」这一层，实现方 MUST NOT 依赖「每行被调用一次」。

而 `RegisterPayloadReader` 的转发器（`TcsEffectSubsystem_HostSlots.cpp:128-131`）**必须**把这条再强调一遍，因为宿主读起来会以为「我的读取器被按行调用」：

> `MUST NOT` 因本插槽把读取挪进每行循环——那会让宿主读取器被按行重复调用。

**为什么这条值得单列**：它是**一个性能契约，其成立完全依赖一个循环外的局部变量位置**。任何一次「把 `ReadPayloadInfo` 挪进循环」的重构都会静默破坏它，而且**不会有任何断言失败**——症状只是宿主读取器被调 N 次。规格里有一个 Scenario 专门钉它（`effect-trigger:335`），但那是**测试**，不是**编译期**约束。

### 4.4 一条跨模块的隐性契约：**`CausedBy` 是溯源读数，不是撤销锚点**【实测】

链运行态持两个来源句柄，职责被刻意分开（`TcsEffectContext.h:43-56`）：

| 字段 | 语义 | 写入规则 | 用途 |
|---|---|---|---|
| `RunSource` | **身份** | `ExecuteChain` 未自带时发新号；**子链另发新号** | 「由本次运行产生的东西同源」——同一运行内续杯、跨运行异源可叠层 |
| `CausedBy` | **因果边** | 触发行起链 = 该行 `Source`；子链 = 父链 `RunSource` | **仅溯源读数** |

**硬约束**（`TcsEffectContext.h:54-55` 与 `effect-interpreter:322` 双写）：

> `CausedBy` **MUST NOT 参与撤销或共存判定**。撤销一律按各实例自己的锚点。

这条约束的**实际影响面**已经落地：`TcsBuff` 的级联退订用 `Instance.CascadeAnchor`（不是 `CausedBy`）、`TcsSkill` 的终结回收用 `FTcsCastRun.RunSource`（不是 `CausedBy`）。**今天没有任何按 `CausedBy` 级联的路径**（实测：`GetRunCausedBy` 只有 2 处引用，都在本模块内）。

### 4.5 跨模块：本模块是**被依赖最广**的机制层，但每个消费者的绑定深度差异极大【实测】

| 消费模块 | 引用符号数 | 绑定深度 | 特征 |
|---|---|---|---|
| `TcsTargeting` | 3（`FTcsEffectContext` 14 处 / `ITcsEntityQuery` 12 处 / `ETcsStepResult` 1 处） | **深** | 它把 `FTcsEffectContext` 当**唯一黑板**用（选择器签名直接收它） |
| `TcsDamage` | 5（`FTcsEffectContext` 3 / `FTcsChainRun` 2 / `ETcsStepResult` 3 / 读取器 1 / `UE_DEFINE_EFFECT_STEP_EXECUTOR` 3） | **中深** | 两个步骤 + 一个载荷读取器；但**不认识** `UTcsEffectSubsystem`（只在 `TcsStepDamage.cpp:33` 取 `Run.Owner`） |
| `TcsBuff` | 6（`FTcsEffectTriggerDef` 4 / `UTcsEffectSubsystem` 3 / `ETcsStepResult` 2 / `FTcsEffectTriggerInstance` 2 / 读取器 1 / 步骤宏 1） | **中** | 一个步骤 + 内联触发行登记/退订 |
| `TcsSkill` | 3（`FTcsChainRunHandle` 17 / `FTcsEffectContext` 2 / `FindChain` 3） | **浅但高频** | 它**只持有链运行态句柄**（`FTcsCastRun.ChainRunHandle`），从不读链内容 |
| `TcsIntegration` | 6（链登记 3 / 触发登记 3 / `FTcsEffectChain` 3 / 两个 Def 类 8） | **深** | 定义库装配期唯一同时调 `RegisterChain` 与 `RegisterTriggerRow` 的地方 |
| `TcsState` | **0** | **无** | 只在注释里被提及（`TcsState.Build.cs:15-16` 写明「依赖已去掉 `TcsEffect`」） |

**「`TcsSkill` 只持句柄不读内容」这条值得记**：`FTcsChainRunHandle` 在 `TcsSkill` 里 17 处命中，而 `FTcsEffectContext` 只有 2 处——因为链运行态**不持链身份**（`FTcsChainRun.h:113-114`），`FindChain` 也只答「某条链是否登记」。`TcsSkill` 的注释把这件事说得很清楚（`TcsSkillInstance.h:152`）：

> 从本句柄**无法反查链身份**。

这是**链重定向**（`TcsSkill` 的 `FTcsChainRedirectRegistry`）能在不碰链定义的前提下工作的前提——**重定向发生在起链之前，改的是 id 而不是链内容**。

---

## §5 技术债（期望 vs 实际，逐条带证据）

> **判据**：技术债 = 需求说了 / 设计定了，但**代码做了却做偏了**（或做了一半）。代码存在但不达标。
> 「未实现」（代码根本不存在）见 §6。

### D-1 ★★★ `EventPayload` 把**裸指针载荷**存进了会跨帧存活的黑板——与 `TcsDamage` 的契约正面冲突【实测】

**期望**：黑板里的 `EventPayload` 是「触发事件载荷」，后续步骤可以读它（`TcsStepWaitEvent.h:21-22` 明文：「命中：事件载荷被写进该运行态的 `FTcsEffectContext::EventPayload`，**后续步骤据此读本次事件**」）。

**实际**：这条契约对**任意** `FInstancedStruct` 载荷成立，而载荷的**属主模块**可能给它定了完全相反的生命周期。今天已有的两个载荷里，**有一个就是**：

```cpp
// TcsDamage/Public/Flow/TcsDamageFlowCollectEvent.h
struct TCSDAMAGE_API FTcsDamageFlowCollectEvent
{
    // 流程上下文（派发期有效；订阅方在回调内提交黑板修正即可——收集 ≠ 消费）
    FTcsDamageFlowContext* Context = nullptr;
};
```

而同一文件的类注释把约束写得**更死**（`TcsDamageFlowCollectEvent.h:58-60`）：

> 指针为**进程内瞬态**：立即通道同步派发期有效，消费方 **MUST NOT 跨帧持有**（框架不为它保活）。

**冲突点**：`TcsEffect` 恰恰**跨帧持有**了它——

1. `UTcsTriggerEvaluator::HandleEvent_Implementation:116` 写 `ChainContext.EventPayload = Payload`（按值拷贝，但拷贝的是**指针**）；
2. `FTcsEffectContext` 随 `FTcsChainRun` 住在池里，**链挂起期间一直存活**；
3. `FTcsStepModifyFlow`（`TcsDamage/Private/Chain/TcsStepModifyFlow.cpp:32-34`）在**任意时刻**解引用 `Event->Context`。

⇒ **只要这条链在 `ModifyFlow` 之前挂起过**（`WaitDelay` / `WaitEvent` / `RunSubChain bWait=true` 任一），`FlowContext` 就是一个**已返回的栈帧指针**。

**偏差证据**：

| 证据 | 位置 |
|---|---|
| 载荷自带裸指针 | `TcsDamageFlowCollectEvent.h:69` |
| 载荷明写「MUST NOT 跨帧持有」 | `TcsDamageFlowCollectEvent.h:58-60` |
| `TcsEffect` 把载荷写进长期黑板 | `TcsTriggerEvaluator.cpp:116` |
| 黑板随挂起存活 | `FTcsChainRun::Context`，`TcsChainRun.h:136` |
| 解引用点无生命周期校验 | `TcsStepModifyFlow.cpp:32-34`（只判 `nullptr`，不判是否仍有效） |
| **规格完全没提这件事** | `effect-chain` / `effect-interpreter` 的 `EventPayload` 段落零提及「载荷生命周期」 |

**影响面**：

- 症状是**静默的错误提交**（`Blackboard.Submit` 写到一块已归还的栈内存上），不是崩溃——**最难查的那一类**；
- 触发面很窄：必须「伤害收集事件 → 起链 → 链内先挂起 → 后跑 `ModifyFlow`」。`TcsStepModifyFlow` 是**唯一**消费 `EventPayload` 内层指针的步骤（另一个读者 `TcsBuffSubsystem_Behavior.cpp:69` 读的是自己的 `FTcsBuffEventPayload`，**全值语义、无指针**，安全）；
- 但**这类载荷会越来越多**（宿主自定义载荷、未来的 `FTcsDamageRecord` 等），而约束**只写在载荷侧、没写在黑板侧**。

**是否已登记**：**否**。台账 `TRIG-5` 只登记了 `EventPayloadFilter`（预筛字段零消费者），与此无关。

**建议的判据（不是修法）**：黑板侧应有一条**成文约束**——「`EventPayload` 里放的东西 MUST 是值语义可拷贝的；含裸指针的载荷 MUST 由步骤自己在**同一帧内**消费完，MUST NOT 跨挂起」。这条约束今天既不在 `effect-chain` 规格、也不在 `FTcsEffectContext` 头注释里。

### D-2 ★★ `RegisterStepExecutor` **恒返回 true**——规格要求的「重复登记被拒」负对照不可能通过【实测】

**期望**（`effect-step-dispatch:88` 明文）：

> 拒绝面（ensure + false）：`StepStruct` 空、`Executor` 空、**同类型重复登记**。

**实际**（`TcsEffectSubsystem.cpp:154-196`）：只判了前两条，**没有第三条**。代码直接：

```cpp
FTcsEffectStepExecutorRegistry::Get().Register(StepStruct, MoveTemp(Forwarder), Executor, GetWorld());
RegisteredStepExecutors.AddUnique(Executor);
return true;   // ← 无论注册表是「接受」还是「拒绝并 ensure」，这里都返回 true
```

**为什么这不只是「少了一个分支」**：注册表的 `Register` 返回 `void`（`TcsEffectStepExecutor.h:104`），故调用侧**分不出拒绝与成功**。同一个门面的另两个插槽入口把这个道理写得很清楚——`RegisterConditionEvaluator` 的注释（`TcsEffectSubsystem.h:190-192`）：

> **判在门面、不靠注册表返回值**：注册表的 `Register` 是 `void` ⇒ 调用侧分不出拒绝与成功；而本口返回值是宿主脚本唯一的判据面（「重复登记被拒」这条负对照靠它，否则只能翻 ensure 日志）。

**后写的两个口都按这条实现了**（先 `Find()` 判同键、再 `Register`），**唯独先写的 `RegisterStepExecutor` 没有**。

**偏差证据**：

| 证据 | 位置 |
|---|---|
| 门面无重复登记判断 | `TcsEffectSubsystem.cpp:154-196`（只有两个 `if`，都判空） |
| 返回值恒 `true` | 同上 `:195` |
| 注册表 `Register` 返回 `void` | `TcsEffectStepExecutor.h:104-108` |
| 同门面另两个口的正解写法 | `TcsEffectSubsystem_HostSlots.cpp:59-64`（条件）/ `:118-123`（载荷） |
| 规格要求第三条拒绝面 | `effect-step-dispatch:88` |

**影响面**：宿主脚本层调 `RegisterStepExecutor` 登记一个**已被内置步骤占用**的类型时，会拿到 `true`、以为成功，实际**分派到内置的那个**。症状 = 「我写的执行器不生效」——而返回值、日志（有一条 Log）都指向成功。

**是否已登记**：**否**。

### D-3 ★★ 三个宿主插槽入口**能力不对称**——`RegisterStepExecutor` 缺配对撤销口，同一 PIE 无法重臂【实测】

**期望**：三个插槽是同一族（`effect-step-dispatch` / `effect-trigger` 三处同构描述），理应能力对齐。

**实际**：

| 入口 | 拒绝判在门面 | 配对撤销口 | 寿命语义 |
|---|---|---|---|
| `RegisterStepExecutor` | ❌（D-2） | ❌ **无** | ✅ |
| `RegisterConditionEvaluator` | ✅ | ✅ `UnregisterConditionEvaluator` | ✅ |
| `RegisterPayloadReader` | ✅ | ✅ `UnregisterPayloadReader` | ✅ |

**作者完全知情**——`_HostSlots.cpp:20-22` 的文件头注释：

> **为什么必须有撤销口**（与步骤执行器插槽的关键差异）：`RegisterStepExecutor` 没有配对撤销口 ⇒ 同类型在本世界只能登记一次、**同一 PIE 会话无法重臂**；而条件/载荷两张注册表本就支持撤销 ⇒ 缺了门面口，「重新布置一次探针」会被自己的重复登记门挡住，表现为**装置不可重跑**而非配置错误。

**根因**：`FTcsEffectStepExecutorRegistry::Unregister` 与 `GetDynamicKeys` **都已存在**（`TcsEffectStepExecutor.h:119/129`，且门面 `Deinitialize` 正在用 `GetDynamicKeys`），缺的**只是门面那一个转发方法**——成本约 10 行。

**影响面**：宿主装置「在同一 PIE 内重新布置步骤执行器」办不到；`TcsSkill` / `TcsDamage` 的探针装置若需重臂只能重启 PIE。

**是否已登记**：**否**（台账零命中）。

### D-4 ★★ `FTcsEffectTriggerHandle` 的 `BlueprintType` 在**两份规格里互相否定**【实测】

**代码事实**（`TcsEffectTriggerInstance.h:35-36`）：

```cpp
USTRUCT(BlueprintType)
struct TCSEFFECT_API FTcsEffectTriggerHandle
```

**两份规格各说一半**：

| 规格 | 原文 | 与代码 |
|---|---|---|
| `effect-trigger:347` | 「`FTcsEffectTriggerHandle` MUST 是**反射可见类型**（`USTRUCT()`）」 | ✅ 成立（但只说了 `USTRUCT()`，没说 `BlueprintType`） |
| `effect-trigger:351` | 「**MUST NOT** 加 `BlueprintType`（口径同 `FTcsChainRunHandle`：运行期身份词、非配置数据；R0 §9 蓝图不承诺）」 | ❌ **代码加了** |

**而 `FTcsChainRunHandle` 的规格写的是「MUST 为 `BlueprintType`」**（`effect-chain:86`，2026-09-24 放宽，台账 SCRIPT-8 连带）。⇒ 同一次放宽、同一个理由（UHT 对 `BlueprintNativeEvent` 强制形参蓝图可表达），`effect-chain` 更新了、`effect-trigger` 没更新。

**代码里的理由写得很完整**（`TcsEffectTriggerInstance.h:32-33`）：「口径统一——两条展平句柄不因『是否恰好被插槽用到』而分道扬镳」。**规格是唯一没跟上的那一层**。

**影响面**：读者按 `effect-trigger:351` 的 MUST NOT 去改代码（删 `BlueprintType`），会**立刻破坏** `FTcsEffectTriggerHandle` 出现在 `BlueprintNativeEvent` 签名里的可能性，并让两条句柄分道扬镳。这是「规格比代码严、但严错了」——与 `ARCH-04` 的 G-10 同类，但方向是**规格本身有缺陷**而非实现有缺陷。

**是否已登记**：**否**。

### D-5 ★ `effect-interpreter:135` 的「覆盖范围」清单里，**4 个方法根本不是 `UFUNCTION`**【实测】

**规格原文**（`effect-interpreter:135`）：

> 覆盖范围（本轮）：链登记（`RegisterChain`/`UnregisterChain`/**`FindChain`**）、触发登记（…）、执行（**`ExecuteChain`**/`ResumeRun`/`IsRunActive`）、能力注入（**`SetEntityQuery`**/**`GetEntityQuery`**）；

**实测代码**：这 4 个方法**都没有** `UFUNCTION()` 标记——而且**都是有意的**，各自头注释写了理由：

| 方法 | 代码事实 | 头注释里的理由 |
|---|---|---|
| `FindChain` | 无 `UFUNCTION()` | 返回裸 struct 指针，UHT 不支持 |
| `ExecuteChain` | 无 `UFUNCTION()` | 形参 `FTcsEffectContext` 非反射；脚本走 `ExecuteChainForCaster` |
| `SetEntityQuery` / `GetEntityQuery` | 无 `UFUNCTION()` | `ITcsEntityQuery` 含 `TFunctionRef`，C++ 专用面 |

**同时漏掉的**：实际标了 `UFUNCTION()` 的 `IsChainRegistered`、`RegisterStepExecutor`、`RegisterConditionEvaluator`、`RegisterPayloadReader`、`UnregisterConditionEvaluator`、`UnregisterPayloadReader`、`ExecuteChainForCaster` —— 7 个**不在该清单里**（后 6 个在规格别处另有描述，`IsChainRegistered` 则**全规格零收录**）。

**影响面**：读者按这条清单判断「哪些方法脚本可调」，会得到**4 个假阳性 + 1 个假阴性**。这条清单是 `SCRIPT-1` 批次的历史快照，2026-09-24 之后没再回写。

**是否已登记**：**否**。

### D-6 ★★ 规格明文禁止的「直伸存储」在 `HasAttribute` 里**正在发生**（`ARCH-04` D-7 的承接）【实测】

**期望**（`attribute-pipeline` 能力的「属性门面的解析点」需求）：消费模块 MUST NOT 直伸属性存储，一切经白名单薄壳。

**实际**（`TcsEffectAttributeAccess.cpp:25-29`）：

```cpp
bool FTcsEffectAttributeAccess::HasAttribute(FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute) const
{
    const FTcsAttributeStore* Store = AttributeSubsystem ? AttributeSubsystem->ResolveStore(Entity) : nullptr;
    return Store && Store->FindInstance(Attribute) != nullptr;   // ← 直伸 Store
}
```

同文件的其余 5 个方法**全部**经 `AttributeSubsystem->Xxx(...)`，唯独这一个拿了 `Store` 指针直接调 `FindInstance`。

**为什么它偏了**：`UTcsAttributeSubsystem` **没有**「该键在不在账本上」的公开查询口（只有 `EvaluateCurrent` 取值），而「键不存在」与「值恰为 0」在 `EvaluateCurrent` 里给同一个读数 `0.0`。作者需要区分二者，于是绕过门面拿了 `Store`。

**这不是随手违规**——它是**规格要求的语义**（`effect-trigger:372` 明文要求「属性键在账本上不存在时不静默按 0 比较」）**与门面能力不足**的正面冲突。

**两条修法**（本档案不给结论，只列）：

| 修法 | 改动面 | 代价 |
|---|---|---|
| ① 给 `UTcsAttributeSubsystem` 加 `HasAttribute(Entity, Attribute)` 公开口 | `TcsAttribute` 加一个方法 + 本模块改一行 | 门面多一个方法；与 `ARCH-04` 的 D-8（零调用即删）判据不冲突（本口有真实消费者） |
| ② 让 `EvaluateCurrent` 返回 `TOptional<double>` | 改 `TcsAttribute` 的公开签名 + 全仓 2 个调用点 | 破坏性变更；但一次性消灭「0 vs 不存在」这个陷阱 |

**是否已登记**：`ARCH-04` 的 **D-7** 已登记（从属性侧看是「本模块被直伸」），本档案从效果侧看是**同一个缺陷的另一面**。两篇各自记一次是**有意的**——`ARCH-04` 的读者是属性维护者，`ARCH-06` 的读者是效果维护者，两边都必须知道自己这一半。

### D-7 ★★ `TcsSkill` 与 `TcsEffect` 各有**一份** `ITcsEntityQuery` 注入点——门禁问的和登记问的不是同一个【实测】

**期望**：`ITcsEntityQuery` 是「框架向宿主索取实体能力」的**唯一接口**，注入一次即可。

**实际**：两份 `SetEntityQuery` / `GetEntityQuery`，各住一个门面：

| 门面 | 方法 | 持有的 `UPROPERTY` |
|---|---|---|
| `UTcsEffectSubsystem` | `SetEntityQuery` / `GetEntityQuery`（`TcsEffectSubsystem.h:582/594`） | `TScriptInterface<ITcsEntityQuery> EntityQuery` |
| `UTcsSkillSubsystem` | `SetEntityQuery` / `GetEntityQuery`（`TcsSkillSubsystem.h:668/671`） | 同款一份 |

⇒ **只注入其一**时：`TcsTargeting` 的选择步骤拿不到实体（走效果门面），而 `TcsSkill` 的门禁第一道（`IsEntityReady`）走技能门面——**症状是「门禁恒拒」或「选择恒空」，且两者看起来毫无关联**。

**这条已被登记**：台账 **`WAIT-12`**（2026-10-08 入册，原文：「实体查询注入点分散在两处门面——组件登记处是效果门面那一份，而门禁第一道问的是技能门面那一份 ⇒ 只注入其一时**门禁恒拒、症状误导**」）。

**本档案的补充判断**：`WAIT-12` 记的是**症状**，而**结构面**在于——`ITcsEntityQuery` 是**机制层定义的契约**（住 `TcsEffect/Public/Host/`），却由**两个上层门面各存一份**。修法方向应该是「契约住机制层、注入点唯一」（宿主注入一次，两个门面读同一处），而不是「两边都记得注入」。

**是否已登记**：✅ **是**（`WAIT-12`）。本档案只补结构面的判断。

### D-8 ★ `effect-trigger:122` 的「以 `TTcsInstanceHandle` 作下标句柄」与代码**字面相反**【实测】

**规格原文**（`effect-trigger:122`）：

> `FTcsEffectTriggerHandle`（USTRUCT）**以 `TTcsInstanceHandle<FTcsEffectTriggerTag>` 作下标句柄**（类型区分标签防与链运行态句柄互换）；

**代码实际**（`TcsEffectTriggerInstance.h:36-73`）：`FTcsEffectTriggerHandle` **不内嵌** `TTcsInstanceHandle`，而是**展平**成 `int32 Index` + `int32 Generation` 两个 `UPROPERTY`，只在 `GetInner()` / `SetInner()` 两个转换点与 `TTcsInstanceHandle` 互转。

**规格自己在下一条就否定了上一条**（`:349`）：

> **字段 MUST 展平**（2026-09-24 实测修正，与 `FTcsChainRunHandle` 同根因同修法）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY int32` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`……

⇒ **同一份规格的相邻两条，一条说「用 `TTcsInstanceHandle` 作句柄」、另一条说「MUST NOT 内嵌它」**。前一条是 2026-09-23 的初稿，后一条是 2026-09-24 的实测修正，**修正时没删旧条**。

**影响面**：读者若只读 `:122`（它在「触发行实例与级联退订」需求里，位置更靠前），会以为句柄是模板类型——而那是**已经实测失败过的形态**（C# 侧生成空壳、句柄往返失效）。

**是否已登记**：**否**。`ARCH-05` 的 G-12 记的是「代码搬走、名字没搬」（空间），本条是「规格自我否定、旧条没删」（时间）——**同一族**，已在 G-15 里合并口径。

### D-9 ★ `effect-chain:72` 与 `effect-chain:86` 的 `BlueprintType` 冲突是**同类**的第二次【实测】

`effect-chain:72`（讲 `FTcsEffectContext`）写「MUST NOT 加 `BlueprintType`」，`effect-chain:86`（讲 `FTcsChainRunHandle`）写「MUST 为 `BlueprintType`」。两条**主语不同**（前者是黑板、后者是句柄），故**不算冲突**——但读者极易误读为同一类型。

**真正的缺陷在这一层**：两条都用了「MUST NOT / MUST 加 `BlueprintType`」这个句式，而**判据完全不同**：

| 类型 | 判据 | 结论 |
|---|---|---|
| `FTcsEffectContext` | 它是**配置数据载体**吗？（答：不是，纯运行态） | 不加 |
| `FTcsChainRunHandle` | 它要出现在 **`BlueprintNativeEvent` 签名**里吗？（答：要） | 必须加 |

两条判据正交，而规格把它们写成了同一个句式的两个取值。这正是 `D-4`（`FTcsEffectTriggerHandle` 被误判）的成因——`effect-trigger:351` 照抄了「口径同 `FTcsChainRunHandle`」，而**它恰好是那个必须加的类型**。

**是否已登记**：**否**。建议与 D-4 同批修（先给「`BlueprintType` 加不加」立一条**正交判据**，再逐类型套用）。

### D-10 ★ 链资产的 `IsDataValid` **缺「提升为 `Valid`」段**，而触发资产有【实测】

| 资产类 | `IsDataValid` 有提升段 | 规格要求 |
|---|---|---|
| `UTcsEffectTriggerDefAsset` | ✅ 有（`TcsEffectTriggerDefAsset.cpp:56-60`） | `effect-trigger-asset:20` 明文要求 |
| `UTcsEffectChainDef` | ❌ **无**（`TcsEffectChainDef.cpp:28-51` 直接 `return Result`） | `effect-chain-asset` **未提这条** |

**后果**：链资产校验通过时返回 `NotValidated`，编辑器显示为「未验证」；装置断言 `== Valid` **必然失败**。

**这条已被间接登记**：`ARCH-04` 的 `TOOLS-2` ③ 记「链资产缺这一段」，归属「校验矩阵轮」。本档案**首次给出两侧对照**——同一族的两个类、同一个基类默认值、一个修了一个没修。

**是否已登记**：✅ 间接（`TOOLS-2` ③）。

### D-11 ★★ 三张注册表的 `Register` **幂等性不一致**——只有步骤执行器那张先 `ResolvePending`【实测】

**三张表的 `Register` 第一行**：

| 注册表 | 是否先 `ResolvePending()` | 代码锚点 |
|---|---|---|
| `FTcsEffectStepExecutorRegistry` | ✅ **是** | `TcsEffectStepExecutor.cpp:42` |
| `FTcsTriggerConditionRegistry` | ❌ 否 | `TcsTriggerCondition.cpp:35-67` |
| `FTcsTriggerPayloadReaderRegistry` | ❌ 否 | `TcsTriggerPayloadReader.cpp:34-66` |

**这个差别的实际后果**：`ResolvePending()` 是「把 `PendingEntries` 里的静态自注册项搬进 `Tests`/`Readers`/`Executors`」的一次性动作（`bPendingResolved` 守卫）。若 `Register` 不先跑它，那么**在首次 `Find` 之前**调用 `Register` 时：

1. `Contains(Key)` 检查**看不见静态自注册项** ⇒ 不触发「重复登记」拒绝；
2. `Tests.Add(...)` 写入动态条目；
3. 之后首次 `Find` → `ResolvePending` → 静态项要 `Add` 时撞上已存在的键 ⇒ 走 `ensureMsgf`「重复登记（静态自注册）——保留首个」**并丢弃静态项**。

⇒ **动态登记赢了内置项**，而规格要求的是「保留首个」（内置在前）。

**触发条件很窄**：必须在「任何 `Find` 之前」调 `Register`。今天的三条内置条件与两个载荷读取器都是静态自注册、而 `Register` 只在宿主登记时才调，正常时序下 `Find` 早已跑过 ⇒ **今天不可达**。但这是**时序侥幸**，不是设计保证。

**步骤执行器那张表为什么没这个问题**：因为它的 `Register` 显式加了 `ResolvePending()`。⇒ **三张表里唯一做对的那张，恰好也是唯一有门面动态入口被真实使用的**（`RegisterStepExecutor` 有 `TcsDamage` 的 2 处调用）。

**是否已登记**：**否**。

### D-12 ☆ `FTcsEffectStep`（步骤容器 struct）**全仓零消费者**【实测】

`TcsEffectStep.h:44-52` 定义了 `FTcsEffectStep { FInstancedStruct StepData; }`，规格 `effect-chain:13` 明文收录它（「『把步骤当字段』的容器形状」）。

**实测**：`grep -rn "FTcsEffectStep\b" Source/` **只命中它自己的声明处**——零消费者。

**按「零消费者不预建」判据**，它该删。但按 **G-13 的「契约默认」判据**（设计明文承诺的字段即使零消费者也保留），它是规格**显式收录**的类型，属于「设计承诺的公开形状」。

⇒ **这是 G-13 那对判据的第二个正面冲突样本**（第一个是 `FTcsParamSnapshotEntry::SourceRef`）。与 `SourceRef` 的区别：`SourceRef` 至少有 2 处写入（GC 补引用），`FTcsEffectStep` **连写入都没有**。

**是否已登记**：**否**。并入 **G-13** 一并裁决。

### 债务小结

| # | 缺陷 | 等级 | 类型 | 已登记 |
|---|---|---|---|---|
| **D-1** | `EventPayload` 跨帧持有裸指针载荷，与 `TcsDamage` 契约正面冲突 | ★★★ | **纯代码**（跨模块契约缺失） | ❌ |
| **D-2** | `RegisterStepExecutor` 恒返回 `true`，规格要求的重复登记拒绝不存在 | ★★ | **纯代码** | ❌ |
| **D-3** | 三个插槽入口能力不对称，步骤执行器缺配对撤销口 | ★★ | **纯代码** | ❌ |
| **D-4** | `FTcsEffectTriggerHandle` 的 `BlueprintType` 在两份规格里互相否定 | ★★ | **规格自相矛盾** | ❌ |
| **D-5** | `effect-interpreter:135` 的覆盖范围清单里 4 个方法不是 `UFUNCTION` | ★ | **规格陈旧** | ❌ |
| **D-6** | `HasAttribute` 直伸属性存储（`ARCH-04` D-7 的另一面） | ★★ | **纯代码**（边界违反） | ✅ `ARCH-04` D-7 |
| **D-7** | `ITcsEntityQuery` 有两份注入点，门禁与选择问的不是同一处 | ★★ | **结构** | ✅ `WAIT-12` |
| **D-8** | `effect-trigger:122` 与 `:349` 相邻两条自我否定 | ★ | **规格自相矛盾** | ❌ |
| **D-9** | 「加不加 `BlueprintType`」两条判据正交却写成同一句式（D-4 的成因） | ★ | **规格判据缺陷** | ❌ |
| **D-10** | 链资产 `IsDataValid` 缺提升段，触发资产有 | ★ | **实现不对称** | ✅ `TOOLS-2` ③ |
| **D-11** | 三张注册表 `Register` 幂等性不一致，动态登记可赢内置项 | ★★ | **纯代码**（时序侥幸） | ❌ |
| **D-12** | `FTcsEffectStep` 全仓零消费者 | ☆ | **零消费者** | ❌（并入 G-13） |

**按类型归类**（这是本模块最值得记的一张表）：

| 类型 | 条数 | 编号 |
|---|---|---|
| **纯代码缺陷** | **5** | D-1 / D-2 / D-3 / D-6 / D-11 |
| **规格自相矛盾或陈旧** | **4** | D-4 / D-5 / D-8 / D-9 |
| **实现不对称** | **1** | D-10 |
| **结构问题**（已登记） | **1** | D-7 |
| **零消费者待裁** | **1** | D-12 |

> **与 `ARCH-04` / `ARCH-05` 的方向对照**：
> - `ARCH-04`（属性）：**规格更严、代码更松**——4 条 MUST 全是代码没做；
> - `ARCH-05`（状态）：**代码搬走、引用滞留**——4 条符号/模块/类型/字段全部指错；
> - **`ARCH-06`（效果）：规格与代码各错一半**——5 条纯代码 + 4 条纯规格，且**其中 3 条（D-2 / D-3 / D-11）出自同一个结构（三个同构入口），2 条（D-4 / D-9）出自同一个判据（`BlueprintType` 加不加）**。

**一句话概括本模块的债**：

> 后写的把先写的坑修了，规格记了旧口径没记新口径，而**两边都没回头读自己旁边的注释**。

---

## §6 未实现（设计是否还成立 + 前置条件）

> **判据**：未实现 = 代码**根本不存在**。设计可能有，也可能需要重新核对。

### U-1 ★★★ `Parallel` / `Repeat` / `OnError` 三个原语——**从未存在过**【实测】

**旧文档的说法**（`SPEC-03-effects` §2.1）：

> **余 5 个**：`Parallel` / `Repeat` / `OnError`（R5.5-a）、`Heal`（R5.5-b）、`PlayCue`（R8，随 TcsCue）。

**实测核查**（三类引用名判据，`MEM-20261006-16` 的方法）：

| 名字 | 工作树命中 | `git log --all -S` | 判定 |
|---|---|---|---|
| `FTcsStepParallel` | 0 | **0** | **① 从未存在** |
| `FTcsStepRepeat` | 0 | **0** | **① 从未存在** |
| `FTcsStepOnError` | 0 | **0** | **① 从未存在** |
| `FTcsStepHeal` | 0 | **0** | **① 从未存在** |
| `FTcsStepPlayCue` | 0 | 1（`88f3643`，文档重构提交） | **③ 拟定未建**（只在文档里被提名） |

⇒ **旧文档把它们写成「已设计、待实现」，实际上连 struct 都没落过。** `DAMAGE-2` 台账行的措辞（「三者结构体存在但执行器未实现」）**是错的**——结构体也不存在。

**设计是否还成立**：

| 原语 | 设计还成立吗 | 理由 |
|---|---|---|
| `Parallel` | ⚠️ **需要重新核对** | 设计写「默认不汇合，`bJoin=true` 时 `JoinCount` 计数」。但 `FTcsChainRun::ParentRun` 是**单等待方**（`TcsChainRun.h:190-191` 明文「单等待方即可……将来出现『多父等待』（如 `Parallel` 原语）再改集合」）⇒ 真做 `Parallel` 必须先把 `ParentRun` 改成集合。**这是设计当初就知道的欠账，不是新发现** |
| `Repeat` | ✅ **成立** | 「带单链单帧熔断」的需求已被共享预算覆盖（§2.3）——`Repeat` 本身只需一个游标字段 + 回退 PC |
| `OnError` | ❌ **需要重新设计** | `ETcsStepResult` 只有 `Completed`/`Running` **两档**（`TcsEffectStep.h:24-31`），**没有「失败」档** ⇒ 今天无法表达「这一步失败了」。而 `OnError` 的全部语义建立在「失败」这个概念上。见 U-2 |
| `Heal` | ⚠️ **需要重新核对** | 旧文档把它列在 `TcsDamage` 侧（`R5.5-b`）。但伤害步骤 `FTcsStepDamage` 今天的形态是「委托给伤害流程模板」，治疗是否要同构、还是走负值伤害，**没有裁决记录** |
| `PlayCue` | ⚠️ **前置未成立** | `Source/TcsCue` 模块不存在；`FTcsEffectTriggerDef` 的 `Cues` 字段已于 2026-09-23 删除（`TcsEffectTrigger.h:113-122` 有留痕注释「TcsCue 落地时加回」） |

**前置条件**：`Parallel` 需先改 `ParentRun` 为集合；`OnError` 需先扩展 `ETcsStepResult`（或另立软失败通道）；`PlayCue` 需先有 `TcsCue` 模块。

### U-2 ★★★ 步骤协议**没有「失败」档**——软失败只能「按完成处理 + 日志」【实测】

这是本模块**最深的未实现**，且它影响的不只是一个原语。

`ETcsStepResult` 只有两个值（`TcsEffectStep.h:24-31`）：

```cpp
enum class ETcsStepResult : uint8
{
    TSR_Completed = 0,   // 本步已完成——解释器 PC 前进
    TSR_Running   = 1    // 本步未完成——解释器停驻原 PC
};
```

**而规格明文承认这件事**（`effect-interpreter:355`）：

> `ETcsStepResult` 无中断档，软失败接管归 `OnError`（R5.5-a）。

**实际后果**：今天所有步骤的失败面都是「记日志 + 返回 `TSR_Completed`」。实测统计——

| 步骤 | 软失败分支数 | 处理方式 |
|---|---|---|
| `FTcsStepModifyAttribute` | 4（载荷类型不符 / `Attribute` 无效 / 目标无效 / 实体无账本） | 全部 `TSR_Completed` + Warning 或 Log |
| `FTcsStepWaitEvent` | 4（载荷不符 / 门面不可得 / tag 无效 / 总线不可得） | 同上 |
| `FTcsStepWaitDelay` | 3（载荷不符 / 时钟不可得 / 门面不可得） | 同上 |
| `FTcsStepSetVar` | 2（载荷不符 / `VarKey` 无效） | 同上 |
| `FTcsStepBranch` / `RunSubChain` | 各 2 | 同上 |
| `FTcsStepDamage` / `ModifyFlow` / `SelectTargets` / `ApplyState` | 各 2~4 | 同上 |

⇒ **「这一步失败了」这个概念在协议层不存在**，链永远向前走。

**设计是否还成立**：

「无中断档」是**有意的**——`ARCH-04` 的对照里也提到过，属性管线的「失败零写入」同样是靠调用方自行保证而非事务。但这条设计成立的前提是**`OnError` 会接管**，而 `OnError` **从未存在**（U-1）。⇒ **设计的一个环节悬空着**。

**需要重新核对的三件事**：

1. 「软失败 = 继续」这个默认，对**所有**步骤都合适吗？（`WaitEvent` 总线不可得时按完成处理 = 链静默跳过了一个等待——这可能是对的行为，也可能是错的）；
2. 若要加第三档（如 `TSR_Failed`），`ETcsStepResult` 是 `UENUM(BlueprintType)` 且已出现在 `BlueprintNativeEvent` 签名里（`UTcsStepExecutor::Execute`）⇒ 加值会影响 C#/蓝图侧的枚举映射；
3. 或者不加枚举值、改由步骤写运行态标志（如 `Run.bStepFailed`）+ 解释器检查——代价是**每条 `TSR_Completed` 路径都要查一次**。

**前置条件**：需先裁决「失败是否需要链级语义」。

### U-3 ★★ `EventPayloadFilter`（载荷预筛）**全仓零消费者**——且已被另一条路绕开【实测】

**字段存在**（`TcsEffectTrigger.h:63-65`），规格也收录（`effect-trigger:12`）。

**实测消费者**：`grep -rn "EventPayloadFilter" Source/` 命中 7 处，**全部是声明、注释或 GC 补引用说明**，**零读取**。

**更关键的一点**：它本来要解决的问题（「这一行只对某类载荷生效」）**已经被主体匹配解决了**——`FTcsEffectTriggerInstance::Subject` + 载荷读取器的 `Subject`（§2.5）。台账 `TRIG-6` 的修法注释明确写了这件事（`TcsTriggerPayloadReader.h:51`）：

> 该字段与内容侧 `EventPayloadFilter` 无关。

⇒ **`EventPayloadFilter` 从「留位待用」变成了「被绕过的留位」**。

**设计是否还成立**：

| 读法 | 判断 |
|---|---|
| 作为「廉价字段预筛」（规格原意：先于条件求值，省掉昂贵的条件求值） | ⚠️ **仍成立但优先级下降**——主体匹配已挡掉大部分串扰，剩下的字段级预筛收益有限 |
| 作为「内容侧自筛能力」 | ❌ **已被取代**——主体匹配是机制层提供的，比内容侧配置更可靠 |

**前置条件**：需先裁决「`EventPayloadFilter` 是否还要做」。若不做了，应删除字段（否则是「给策划的假控件」，与 2026-09-23 删 `Cues` 的理由完全一致）。

### U-4 ★★ 链**重定向**在 `TcsEffect` 侧完全未建——它整体住 `TcsSkill`【实测】

旧文档 `SPEC-03-effects` §3 的「起链解析（D5-7 三粒度让渡点之一）」写：

> 链 Id 解析 = 全局定义 → **重定向栈**（状态/装备声明的 `FChainRedirect`，后挂/高优先）→ Custom Fragment；重定向栈解析经注入接口 `IChainRedirectResolver`（宿主/上层提供）。

**实测**：`TcsEffect` 侧**零**重定向设施。重定向**整体住在 `TcsSkill`**（`TcsSkill/Public/Skill/TcsChainRedirect.h` + `FTcsChainRedirectRegistry`），且只服务技能起链路径（`TcsCastOps_Chain.cpp:29`）。

**关键的结构事实**：`FTcsChainRedirectRegistry` 的 `Resolve(Entity, GlobalChainId)` 是**纯 id 变换**，它在**起链之前**把 id 换掉——所以 `TcsEffect` 完全不需要知道重定向的存在。

**设计是否还成立**：

| 原设计 | 现状 | 判断 |
|---|---|---|
| 重定向栈住 `TcsEffect`（机制层） | 住 `TcsSkill`（上层） | ⚠️ **偏离但可能更好**——重定向的「后挂/高优先」语义需要知道「谁挂的」，那是上层知识 |
| 注入接口 `IChainRedirectResolver` | 不存在；`TcsSkill` 有 `ChainRedirectResolver` 字段但**求值未实现**（`TcsCastOps_Chain.cpp:38-42` 明写「归 R6.5-g」） | ❌ **未建，且已有明确归属** |
| Custom Fragment 级 | 不存在 | ❌ 未建 |

**前置条件**：`TcsSkill` 的 `ITcsChainRedirectResolver` 求值（R6.5-g）先落地，才谈得上「重定向是否该下沉」。

### U-5 ★★ 三个「零收录」的门面方法：`IsChainRegistered` / `GetRunSource` / `GetRunCausedBy`【实测】

`grep` 七份规格，三个方法的收录情况：

| 方法 | 规格收录 | 说明 |
|---|---|---|
| `IsChainRegistered` | ⚠️ 只在 `integration-entity` 出现（1 处） | 本模块**自己的规格里零收录**——而它是 `FindChain` 的脚本层替身（`FindChain` 返回裸指针，反射层表达不了） |
| `GetRunSource` | ❌ **零收录** | 头注释写「脚本可达面归 R8 Explain 轮，故本处不加 `UFUNCTION`」——这是**有意的**留白 |
| `GetRunCausedBy` | ❌ **零收录** | 同上 |

**`GetRunSource` / `GetRunCausedBy` 的未实现是「有意留白」**（判据同 `ARCH-01` 的 U-4）：两个字段本身**已落地并有写入方**（§4.4），缺的只是反射面，且归属已明确（R8 Explain）。

**`IsChainRegistered` 不同**：它**已标 `UFUNCTION()`**（脚本层可达）、**有真实消费者**（`TcsDefinitionSubsystem.cpp:204` 的引用链预检），却**在自己的模块规格里零收录**。这是**文档缺口**而非未实现——登记为 **U-5** 而非 D 类。

### U-6 ★ 「打断/取消」——**没有公开的运行态释放入口**，与 `ARCH-01`/`ARCH-05` 同族【实测】

`ReleaseRun` 是**私有**的（`TcsEffectSubsystem.h:656`，`Core` region 的 `private:` 区），五个调用点全在 `RunFrom` 内部。门面公开面**没有** cancel / abort / stop 一类方法。

**这意味着 `InterruptPriority` 字段（`TcsEffectTrigger.h:93-95`）永远无法兑现**——「可打断哪些正在跑的链」需要「打断」这个动作存在。

**已登记**：台账 **`CHAIN-6`**（2026-10-03 入册，原文完整记录了「挂起中的运行态不跑 ⇒ `ReleaseRun` 只在跑的时候被调 ⇒ 今天唯一能释放它的路径是世界反初始化」）。

**本档案的补充**：`CHAIN-6` 只讲了「事件锚分支不可达」。实际影响面更大——**`InterruptPriority` 字段、`OnError` 原语、`Parallel` 的汇合语义三者全部依赖取消能力**。它们是**同一个未实现的三张脸**。

**是否已登记**：✅ **是**（`CHAIN-6`），但影响面被低估了（见上）。

### U-7 ☆ `DAMAGE-2` 台账行的**措辞已假**：「三者结构体存在但执行器未实现」【实测】

台账 `DAMAGE-2` 行（2026-10-05 更新版）写：

> **余 5 个**：`Parallel` / `Repeat` / `OnError`（R5.5-a）、`Heal`（R5.5-b）、`PlayCue`（R8，随 TcsCue）。

而 `SPEC-03-effects` §12 的 R5 收束表写：

> **`Parallel` / `Repeat` / `OnError`** ⛔ **未验** | 三者结构体存在但执行器未实现（归 R5.5-a）

**实测（U-1 已证）**：三者**结构体也不存在**（工作树 0 + `git log --all -S` 0）。

⇒ 「未实现」是对的，「结构体存在」是错的。**这句话会让读者以为「只差一个执行器」**，而实际是「连数据形状都没定」。

**是否已登记**：**否**。这是 `SPEC-03-effects` 的措辞缺陷，而该文档已**冻结为历史真相源**（`INDEX` §1.1）⇒ **不在旧文档上修**，改在本档案登记 + 进 `openspec` 的待办（若无对应能力，则记入 `ARCH-00-overview` 的未实现清单）。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 载体 | 规模 | 本模块的实际情况 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/effect-*` + `entity-query-contract` | 7 份 / **1 108 行** / 19 Requirement / **134 Scenario** | **覆盖完整**：链形状、登记表、黑板、执行协议、三路唤醒、熔断、6 个原语、四道门、条件、读取器、句柄反射性、两个资产类——逐项有主 |
| **实际实现** | `Source/TcsEffect/` | **55 文件 / 7 139 行** | 见 §2 |
| **历史理由** | `Documents/combat-system-design/spec/04-module-effects.md`（223 行，`SPEC-03-effects`）+ `decisions/` + `log/log-02-effects-m4-d7.md` | 223 行模块规格 + 若干决策 | **状态 `ACTIVE`**，但已冻结为历史真相源 |

### 7.2 `SPEC-03-effects` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

| # | 旧文档说什么 | 代码实际 | 归属 |
|---|---|---|---|
| 1 | `FEffectStep` / `FEffectChain` / `FEffectContext` / `FChainRun` / `UCombatEffectSubsystem` | `FTcsEffectStep` / `FTcsEffectChain` / `FTcsEffectContext` / `FTcsChainRun` / `UTcsEffectSubsystem` | **改名滞后**（`Combat* → Tcs*`，`ARCH-01` D-1 同族） |
| 2 | 触发行字段名 `Effects`（§2.2 表内又标了「设计期名 `Effects`，改为 `EffectChainId`」） | `EffectChainId` | **已自纠**（表内说明与代码一致） |
| 3 | 触发行 10 字段含 `Cues` | **已删除**（`TcsEffectTrigger.h:113-122` 有留痕） | **已自纠**（§2.2 表标了 ❌ 已删除） |
| 4 | 「15 个原语，本模块 9 个」 | 本模块实际 **6 个**（`WaitDelay` / `WaitEvent` / `SetVar` / `RunSubChain` / `Branch` / `ModifyAttribute`） | **声称未落地**（余 3 个从未存在，见 U-1） |
| 5 | 「余 5 个：`Parallel` / `Repeat` / `OnError` / `Heal` / `PlayCue`」 | 全部**从未存在** | **声称未落地**（U-1 / U-7） |
| 6 | 「重定向栈住 `TcsEffect`，经 `IChainRedirectResolver`」 | 住 `TcsSkill`（`FTcsChainRedirectRegistry`） | **归属已变**（U-4） |
| 7 | 「黑板含 `CapturedAttrs` 与注入引用」 | **两者都不在黑板**（`TcsEffectContext.h:18-20` 明文分流：前者归 `TcsDamage` 流程上下文，后者归门面注入点） | **已自纠**（代码注释解释了分流） |
| 8 | 「`MaxStepsPerFrame` 熔断上限，**默认 CVar 可调**」 | 默认 64，**是 `UPROPERTY` 链定义字段，不是 CVar** | **实现超文档 / 文档错**（更准确地说：设计改了，旧文档没跟上） |
| 9 | 「`FChainRun` 保存 PC/上下文/**Repeat 游标**/**JoinCount**」 | 无 `Repeat` 游标、无 `JoinCount`（`Repeat`/`Parallel` 从未存在） | **声称未落地**（随 U-1） |
| 10 | 「四种唤醒源：①到期堆 ②事件匹配 ③子链完成 ④**开闸事件**」 | **三路**已落地；④「开闸事件（GateTags 开闸发事件 + `WaitEvent` 语法糖）」**未实现** | **声称未落地**（旧文档 §12 表自己也标了「🟡 部分」） |
| 11 | §5b 三类插槽（选择器/过滤器、步骤执行器、伤害流程） | 三类**都已落地** | ✅ 一致 |
| 12 | §5b「4 张注册表」的寿命语义 | 三张在本模块（步骤/条件/载荷）+ 一张在 `TcsDamage`（流程步骤） | ✅ 一致 |
| 13 | §2.2「条件落地面：三条已实现」 | `HasAllTags` / `Chance` / `AttributeCompare` **三条** | ✅ 一致 |
| 14 | §2.1「本模块 9 个原语」的**实现状态** 2026-10-05 更新为「已落地 10 个」（含领域侧 4 个） | 本模块 6 + 领域侧 4 = **10** | ✅ 一致（10 这个数是对的，9 这个数错了） |

### 7.3 差异的**类型分布**

| 类型 | 条数 | 编号 |
|---|---|---|
| **改名滞后** | 1 | 7.2 #1 |
| **声称未落地** | 4 | #4 / #5 / #9 / #10 |
| **归属已变** | 1 | #6 |
| **已自纠**（旧文档自己发现了并修正） | 3 | #2 / #3 / #7 |
| **文档错**（设计变了文档没跟） | 1 | #8 |
| **一致** | 4 | #11 / #12 / #13 / #14 |

> **这张表本身就是一条结论**：`SPEC-03-effects` 的 **14 项差异里有 3 项是它自己纠的**（#2 / #3 / #7），且纠得**准确**（与代码逐字一致）。
>
> 这与 `ARCH-05` 的发现**同构但结论相反**：`ARCH-05` 说「旧 `SPEC-02-states` 比 `openspec` 更准」，本模块说「旧 `SPEC-03-effects` 有自纠能力，但**它的未实现清单不可信**」（4 项声称未落地里，5 个原语从未存在）。
>
> ⇒ **旧文档族不是「整体过期」，而是「局部自纠 + 局部从不回写」**。自纠的部分（字段改名、字段删除、黑板分流）质量很高；从不回写的部分（原语清单、唤醒源清单）停在设计轮。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

| 步骤 | 动作 | 结果 |
|---|---|---|
| 1 | 读可执行需求 | 7 份能力 / 1 108 行 / 19 Requirement / 134 Scenario 全读 |
| 2 | 读代码 | **55 文件 / 7 139 行全读**（`.h` 全部 + `.cpp` 全部） |
| 3 | 读旧模块规格 | `SPEC-03-effects` 223 行全读 + `decisions/` 相关 6 篇 + 台账 `CHAIN`/`TRIG`/`DAMAGE` 三族 |
| 4 | 追竖切 | 48 个符号的跨模块命中统计 + 6 个模块的逐处引用核查 |
| 5 | 交叉验证 | 每条结论标置信度；「未实现」项全部反查是否只是搬到别处（**发现 1 例**：链重定向搬到 `TcsSkill`，见 U-4） |

### 8.2 本次审核用到的**三类引用名判据**（`MEM-20261006-16` 的实证应用）

`U-1` 是本档案**唯一**用满三类判据的地方，结果很能说明问题：

| 名字 | 工作树 | `git log -S` | `--diff-filter=A/D` | 判定 | 处置 |
|---|---|---|---|---|---|
| `FTcsStepParallel` / `Repeat` / `OnError` / `Heal` | 0 | **0** | — | **① 从未存在** | 旧文档的「结构体存在」是**假事实**，MUST 订正 |
| `FTcsStepPlayCue` | 0 | 1（文档提交） | 无 A/D | **③ 拟定未建** | 保留原名 + 标「拟名」 |

> **只 grep 一次是不够的**：`PlayCue` 与另外四个在**当前工作树里都是 0 命中**，但 `git log -S` 把 `PlayCue`（曾在文档里被提名）与其余四个（从未被提及）分开了。若无这一步，五个会被一并判成「从未存在」——而 `PlayCue` 的处置应该是「前置未成立」而非「设计不存在」。

### 8.3 置信度分布

| 置信度 | 条数 | 说明 |
|---|---|---|
| **【实测】** | 全部 D-1..D-12 / U-1..U-7 | 每条都有代码锚点或可跑命令 |
| **【推断】** | 0 | — |
| **【待核】** | 0 | — |

> **为什么全是实测**：本模块的债全部是「符号在不在、调用有没有、返回值是什么」这类**可 grep 的事实**，没有一条需要推理。唯一的判断题是 **D-1 的影响面**（「触发面很窄」），已在条目内标注为判断而非事实。

---

## §9 结论

### 9.1 三句话

1. **这是全仓设计质量最高、落地最完整的一个模块**——三张注册表的「属主自登记」把依赖铁律变成了可审计的注册点；挂起-恢复协议的 4 个锚、三路唤醒、共享预算 + 深度双护栏、`ReleaseRun` 的唯一汇聚点，每一条都在代码里有对应实现与注释解释「为什么必须这样」。7 份规格 134 个 Scenario 覆盖了它。

2. **它的债不在「不会做」，而在「后做的没回头改先做的」**——12 条债里，D-2 / D-3 / D-11 出自同一个结构（三个同构的宿主插槽入口，后两个把第一个的坑全修了，第一个没回头），D-4 / D-9 出自同一个判据（`BlueprintType` 加不加，同一次放宽只更新了一份规格）。**证据就在旁边的注释里，两处注释都明确写了「另一个口没有这个能力」**。

3. **最危险的一条是跨模块契约的缺位，不是任何单个模块的 bug**——`EventPayload` 允许放任意 `FInstancedStruct`，而 `TcsDamage` 的载荷自带裸指针且明文「MUST NOT 跨帧持有」，`TcsEffect` 却把它存进了会跨帧存活的黑板。**两边都按自己的契约做对了，契约之间没对上**。

### 9.2 一句值得记住的话

> **`ARCH-04` 是「规格写得比代码严」，`ARCH-05` 是「代码搬走了、名字没搬」，`ARCH-06` 是「代码和规格各错一半，且两边的证据都在对方的注释里」。**

### 9.3 与前三篇的对照

| 维度 | `ARCH-04`（属性） | `ARCH-05`（状态） | **`ARCH-06`（效果）** |
|---|---|---|---|
| 规格 : 代码 | 1 : 8.6 | 1 : 1.7（且 7/10 讲错模块） | **1 : 6.4（且逐项有主）** |
| 债的方向 | 规格更严、代码更松（4 条） | 代码搬走、引用滞留（4 条） | **代码 5 条 + 规格 4 条，方向各半** |
| 最严重的一条 | D-1「失败零写入」无候选集无回滚 | D-1「权威层主语错了」 | **D-1「跨模块载荷生命周期契约缺位」** |
| 最严重那条的性质 | **规格对了，代码没做** | **规格讲错了对象** | **两边各自对，契约没对上** |
| 旧文档可信度 | 高（`SPEC-01` 质量好） | **比 `openspec` 更准** | **局部自纠、局部从不回写** |
| 一句话 | 承诺了没兑现 | 地图比代码旧 | **后写的没回头改先写的** |

### 9.4 给下一轮的三条建议（按代价从低到高）

| # | 建议 | 代价 | 收益 |
|---|---|---|---|
| 1 | **给 `RegisterStepExecutor` 补配对撤销口 + 重复登记判断**（D-2 / D-3） | 约 20 行（注册表两个方法都已存在，只缺门面转发） | 三个插槽能力对齐；宿主装置可在同一 PIE 重臂 |
| 2 | **给 `EventPayload` 立一条成文生命周期约束**（D-1） | 0 行代码，改 `effect-chain` 规格 + `FTcsEffectContext` 头注释各一段 | 消灭一类静默的错误提交；且它是**规格缺口**，越晚补波及面越大 |
| 3 | **给「加不加 `BlueprintType`」立一条正交判据**，再逐类型套用（D-4 / D-9） | 1 段规格 + 修 2 处错误条文 | 消灭「同一句式两个相反取值」这个陷阱；`effect-trigger:351` 今天会**误导人去删正确的代码** |

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。`TcsEffect` 首次成篇（**935 行**，本档案迄今最长）。审出 **D-1..D-12 / U-1..U-7**。三条结构性发现：① **规格与代码各错一半**（5 条纯代码 + 4 条纯规格），与 `ARCH-04`（规格更严）/ `ARCH-05`（引用滞留）都不同；② 新增缺陷方向 **G-15「后写的没回头改先写的」**（D-2 / D-3 / D-11 三条出自同一个结构，D-4 / D-9 出自同一个判据，**证据都在旁边的注释里**）；③ 最严重的是 **D-1 跨模块载荷生命周期契约缺位**——`TcsDamage` 的载荷自带裸指针且明文「MUST NOT 跨帧持有」，`TcsEffect` 把它存进跨帧存活的黑板，**两边都按自己的契约做对了、契约之间没对上**。另新增 **G-16**（`DAMAGE-2` 台账行与 `SPEC-03-effects` §12 的「三者结构体存在」是假事实——`git log -S` 双零）与 **G-17**（`IsChainRegistered` 有真实消费者却在自己模块规格里零收录）。**同时首次给出 `SPEC-03-effects` 的 14 项差异清单**（§7.2），并判定旧文档族「**局部自纠 + 局部从不回写**」——3 项自纠准确、4 项声称未落地 |

