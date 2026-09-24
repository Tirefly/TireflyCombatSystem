# C# 编写 TCS 逻辑的可行性调研（技能 / Buff / 自定义伤害流程）

- 日期：2026-09-23
- 状态：**调研结论**（未落地任何代码；本文是提案输入，不是提案本身）
- 前置：LAC 已接入 UnrealSharp（上游 main @ `46e9c2c1`，UE 5.8，`Script/LegendAutoChessCS` 已实测热重载通过）
- 承接：`2026-09-23-scripting-language-ustruct-research.md`（G-1~G-4 的原始定义与选型结论）
- **本次证据等级高于前次**：前次读 C++ 源码 + UnrealSharp 源码推断"能不能用"；本次**直接读已生成的 glue 产物**（编译器的实际输出）

---

## 0. 问题

> 要让 LAC 能在项目内用 TCS 编写技能逻辑、Buff 逻辑、自定义伤害流程（某些极复杂、无法用 EffectChain 表达的逻辑，只能完全脚本化），现在的 TCS 和未来将要完成的 TCS 是否满足？不满足的话该做哪些事？

---

## 1. 方法论：glue 产物是比源码更硬的判据

TCS 的 C# 绑定已**实际生成**（接入 UnrealSharp 的副产物）：

```
Plugins/Tirefly/TireflyCombatSystem/Intermediate/UnrealSharp/UHT/Editor/Tcs*/  共 76 个 .generated.cs
  TcsAttribute 18 / TcsCore 15 / TcsDamage 20 / TcsEffect 14 / TcsIntegration 4 / TcsNotation 1 / TcsTargeting 4
```

**这条判据同时否掉了此前两个悬置疑问**：

| 悬置疑问 | 实测结果 |
|---|---|
| `USTRUCT()`（非 `BlueprintType`）会不会导出到 C#？ | **会**。`FTcsEffectStep` / `FTcsEffectChain` / `FTcsTriggerContext` / `FTcsFlowTemplate` / 12 个流程步骤全是 `USTRUCT()`，**全部生成了 C# record struct**。`grep BlueprintType` 在 `UnrealSharpManagedGlue` 全库为空 ⇒ **UnrealSharp 不按 BlueprintType 过滤** |
| 纯 C++ 接口（零 `UFUNCTION`）会导出成什么？ | **会导出，但是空壳**（详见 §4） |

**可复用的一行判据**（数"C# 实际能调用几个方法"）：

```bash
grep -rn "public .* [A-Z][A-Za-z]*(" --include=*.generated.cs . \
  | grep -v "ToNative\|FromNative\|GetNativeDataSize\|GetNativeClassPtr\|Marshaller"
```

---

## 2. 三层结论总览

| 层 | 内容 | 结论 |
|---|---|---|
| **第一层·数据面** | C# 定义纯数据 USTRUCT、构造 `FInstancedStruct`、组装数组、塞进链/模板 | ✅ **完全通** |
| **第二层·行为登记面** | 向三张注册表登记执行器/条件求值器/载荷读取器 | ❌ **不通**（G-1/G-2 真身） |
| **第三层·门面调用面** | C# 调 `RegisterChain` / `ExecuteChain` / `ApplyModifier` 等 | ❌ **不通**（无 `UFUNCTION`），且分 A/B/C 三类难度 |

---

## 3. 第一层：数据面（✅ 完全通）

| 能力 | 实证 |
|---|---|
| C# 定义纯数据 USTRUCT 并挂进链 | `FInstancedStruct.Make<T>` 可用；`FTcsEffectChain.Steps` 导出为 `IList<FInstancedStruct>` |
| **C# 组装数组并送进结构体/函数**（此前记忆存疑，本次澄清） | **通，但路径要说清**：C# 侧数组类型是 `IList<T>`（**不是 `TArray<T>`**）。`ArrayCopyMarshaller.ToNative` 需要 `_nativeProperty`，而该值由 glue **自动生成**：`ContainerPropertyTranslator.ExportParameterStaticConstructor` 会写 `CallGetNativePropertyFromName({fn}_NativeFunction, "paramName")`（参数位）或从结构体字段取（字段位）——**即 property 总是来自真实的 UFunction/UStruct，不需要 C# 手填**。⇒ 给 `FTcsEffectChain.Steps` 这类已导出字段赋值、或把 `IList<T>` 作为 `UFUNCTION` 参数传入，**都可行**。反之，纯 C# 里 `new TArray<T>()` 再传出去**不行**（其构造函数强制要求 `nativeUnrealProperty`）——**但这不是必需路径** |
| C# 读事件载荷 | `FTcsOnCombatEvent` 动态多播已导出为 `TMulticastDelegate<FTcsOnCombatEvent>` |
| C# 读触发期上下文 | `FTcsTriggerContext` 全字段导出（`EventTag` / `ClassificationTags: IList<FGameplayTag>` / `Caster`） |

⇒ **"内容那一半"确实已就绪**（与前次结论一致，本次给出更硬的证据）。

---

## 4. 第二层：行为登记面（❌ 不通）

### 4.1 决定性证据：76 个生成文件里只有 4 类可调用方法

```
UTcsEventHandler.HandleEvent                      （BlueprintNativeEvent 覆写）
ITcsAttributeProvider.{GetBaseValue,GetCurrentValue,PeekPending}
ITcsParamTableReader.TryGetNumericParam
UTcsAsyncAction_ListenForCombatEvent.ListenForCombatEvent
```

**无一通向注册表。**

### 4.2 门面子系统的 glue 是"空壳"

- `TcsEffect/TcsEffectSubsystem.generated.cs`：**只有两个属性**（`EntityQuery` / `TriggerEvaluator`），**零个方法**。`RegisterChain` / `RegisterTriggerRow` / `ExecuteChain` / `SetEntityQuery` / `CollectTriggerRowsForTag` 全部未导出。
- `TcsDamage/TcsDamageSubsystem.generated.cs`：零方法。
- `TcsIntegration/TcsCombatEntityComponent.generated.cs`：零方法。

### 4.3 "加个 `UFUNCTION` 就够了吗" —— 必须按形参逐个核，实测分三类

| 类 | 判据 | 实例 | 工作量 |
|---|---|---|---|
| **A 类** | 形参/返回全反射，glue 已在 | `RegisterChain(const FTcsEffectChain&)`、`RegisterTriggerRow(const FTcsEffectTriggerInstance&)`、`RegisterTemplate(const FTcsFlowTemplate&)`、`SetEntityQuery(TScriptInterface<ITcsEntityQuery>)`、`SetTriggerGateTag(FGameplayTag,bool)`、`GetTriggerRowCount()` | **加宏即可**（半天量级） |
| **B 类** | 形参/返回是**无 `USTRUCT` 宏的裸 struct**（实测无 glue） | `ExecuteChainById` / `ExecuteChain`（返回 `FTcsChainRunHandle`、传 `FTcsEffectContext`）、`ApplyModifier(…, const FTcsAttrModInstance&)`、`RemoveBySource(FTcsSourceHandle)` | 需**反射化该 struct** 或**改签名**（用 tag/句柄/int 传参） |
| **C 类** | 签名含 `TFunction` | 三张注册表的 `Register(const UScriptStruct*, TFunction<...>)` | **必须换签名**（G-1+G-2 真身） |

实测无 `USTRUCT` 宏、无 glue 的类型：`FTcsAttrModInstance`、`FTcsSourceHandle`、`FTcsChainRunHandle`。

> **警告**：只看 A 类会得出"加几个宏就通了"的错误乐观结论——**C# 能注册链，但 `ExecuteChainById` 返回 `FTcsChainRunHandle`（B 类），发起不了链**。

---

## 5. ★ 本次新发现：G-1 的"反射化上下文"物理上做不到

### 5.1 `FTcsDamageFlowContext` 深处嵌着 `TFunction`

```
FTcsDamageFlowContext
  └─ FTcsFlowAttributes Blackboard
       └─ TMap<FGameplayTag, TArray<FTcsFlowAttributeSubmit>> Submits    ← 非 UPROPERTY 容器
            └─ FTcsFlowAttributeSubmit
                 └─ FTcsConsumePolicy Consume
                      └─ TFunction<void()> OnConsumed                     ← 不可反射
```

`TcsFlowDataSteps.h:44-46` 的注释**自己写明了**：

> 注：**数据步骤无法携带消耗策略**（`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调，纯 C++ struct 不可反射、不可作 UPROPERTY——UHT 实证）→ 消耗型提交只能来自 C++ 步骤或事件响应。

### 5.2 这条事实改变了 G-1 的形状

- **不是**"给 `FTcsDamageFlowContext` 加个 `USTRUCT` 宏"就行——**只要 `FTcsFlowAttributes` 还持 `TFunction`，它就无法成为反射类型**。
- ⇒ G-1 的必做前置是：**把"黑板"从"上下文"里摘出去**（或给上下文一个反射镜像 + 非反射实体），即**上下文与黑板分层**。
- 此前所有调研都没把 `FTcsFlowAttributes` 的 `TFunction` 接到"反射化可行性"上（它只在 GC 话题里出现过）。

### 5.3 两个上下文的可反射性差异（实测）

| 结构 | 字段构成 | 反射化可行性 |
|---|---|---|
| `FTcsEffectContext` | `FTcsCombatEntityHandle` ×2（反射 USTRUCT）/ `FInstancedStruct` / `TArray<FTcsCombatEntityHandle>` / `TMap<FGameplayTag,double>` | ✅ **字段全反射，可做** |
| `FTcsChainRun` | 含 `TWeakObjectPtr<UTcsEffectSubsystem>` + `TTcsInstanceHandle`（**裸模板，非 USTRUCT**）+ `FTcsTimeEntryHandle` | ⚠️ **第二道坎**（弱引用 + 句柄族需包一层 USTRUCT；先例：`FTcsCombatEntityHandle` 已这么干过） |
| `FTcsDamageFlowContext` | 含 `FTcsFlowAttributes`（→ `TFunction`） | ❌ **当前物理不可能**，须先做 §5.2 的分层 |

---

## 6. 自定义伤害流程：唯一接近可走通的路径，但有一个"看着通实际不通"的坑

### 6.1 好消息：流程侧基础设施比效果链完整得多

- **12 个流程步骤执行器全部已实现**（`TcsFlowStepsCore.cpp` 4 个 + `TcsFlowStepsRest.cpp` 8 个）——对比效果链只有 3/15。
- 12 个步骤 struct **全部导出到 C#**（`TcsFlowCollectStart` / `TcsFlowModify` / `TcsFlowDelegate` / …）。
- 委托出口的调用方式是**纯 C++ 虚函数直接调**（`TcsFlowStepsRest.cpp:80` `Step->Delegate->GetBaseHitRate(...)`，不是 `Execute_` 反射调用）。

### 6.2 ★ 坏消息：`ITcsDamageFlowDelegate` 的 C# wrapper 是空壳

`TcsDamage/TcsDamageFlowDelegate.generated.cs` 实测内容：`[UInterface]` + `Wrap()` + `ITcsDamageFlowDelegateWrapper`——**wrapper 里没有任何方法实现**（因为 `ITcsDamageFlowDelegate` 的 5 个方法在 C++ 侧是零 `UFUNCTION` 的纯虚函数）。

⇒ **C# 类"实现"这个接口在 C# 编译期不报错，但 C++ 侧 `Step->Delegate->GetBaseHitRate(...)` 走的是 C++ 虚表，永远到不了 C#。**

**这是"看起来通、实际不通"的高危形态**——判定方法：读生成的 wrapper 是否为空壳。

### 6.3 ★ 干净对照：TCS 内部就有一组"通 / 不通"的接口样本

TCS 里恰好有两个结构相同、反射标记相反的接口，构成**同代码库内的对照实验**：

| | `ITcsAttributeProvider`（3 个 `UFUNCTION(BlueprintNativeEvent)`） | `ITcsDamageFlowDelegate` / `ITcsEntityQuery`（零 `UFUNCTION`） |
|---|---|---|
| C# 接口里的方法 | ✅ 有 3 个 `[UFunction(FunctionFlags.BlueprintEvent)]` 方法声明 | ❌ **零个方法**（只有 `Wrap()`） |
| C# wrapper | 非空壳 | **空壳** |
| C++ 调用点 | `ITcsAttributeProvider::Execute_GetCurrentValue(...)`（反射路径） | `Step->Delegate->GetBaseHitRate(...)`（**C++ 虚表直调**） |
| C# 能否真被调到 | ✅ **能** | ❌ **不能**（虚表到不了 C#） |

证据行号：`TcsParamSource_AttributeScaled.h:105` / `TcsParamSource_ParamRef.h:45`（`Execute_` 路径）vs `TcsFlowStepsRest.cpp:80/106/130/206`、`TcsFlowStepsCore.cpp:103/148`（`->` 路径）。

**这张对照表把"要做的事"精确化了**：把 `ITcsDamageFlowDelegate` / `ITcsEntityQuery` 改造成 `ITcsAttributeProvider` 的形态（补 `UFUNCTION(BlueprintNativeEvent)` + 调用点改 `Execute_`）。**且 `ITcsAttributeProvider` 是现成的可复制先例**（虽则其形参 `FGameplayTag` 全反射，而 flow delegate 的形参含 `FTcsDamageFlowContext` ⇒ 仍需先解 §5 的上下文问题）。

> 附带发现：`ITcsEntityQuery`（实体查询契约）同样零 `UFUNCTION` ⇒ **C# 也实现不了实体查询**（宿主 `UTcsPieEntityQuery` 是 C++ 实现）。若脚本要读世界（找敌人/取位置），这是另一处需补的反射面。且其形参含 `TFunctionRef<void(FTcsCombatEntityHandle)>`（头文件自己注明"C++ 专用面：蓝图不可表达"）⇒ 属 C 类，须换签名。

---

## 7. 缺口清单（可直接作提案输入）

| # | 缺口 | 性质 | 备注 |
|---|---|---|---|
| G-1a | `FTcsEffectContext` 反射化 | 可做（字段全反射） | `FTcsChainRun` 是第二道坎 |
| G-1b | **`FTcsFlowAttributes` 从上下文摘出 / 反射镜像** | **必做前置** | 否则 G-1 物理不可能（`TFunction OnConsumed`） |
| G-2a | 三张注册表 `Register` 换签名 + `UFUNCTION` | 可做（C 类） | `TFunction` → 反射可见委托 |
| G-2b | 门面 API 补 `UFUNCTION`（A 类） | **最便宜** | 参数已全反射，**加宏即可** |
| G-2b′ | B 类形参反射化（`FTcsChainRunHandle` / `FTcsSourceHandle` / `FTcsAttrModInstance` / `FTcsEffectContext`） | 可做 | 先例：`FTcsCombatEntityHandle` 已是反射 USTRUCT |
| G-2c | `ITcsDamageFlowDelegate` 5 方法 `UFUNCTION(BlueprintNativeEvent)` + 调用点改 `Execute_` | 可做 | 打通"自定义伤害流程"；**先例 = `ITcsAttributeProvider`**（同代码库内现成对照，见 §6.3） |
| G-2d | `ITcsEntityQuery` 反射化（含 `TFunctionRef` 形参换签名） | 可做（C 类） | **脚本读世界**（找敌人/取位置）的通道，当前零 `UFUNCTION` |
| G-3 | 效果链 12/15 原语 | **功能缺口** | 归属：R4 Task 3.5（`SetVar`/`Branch`/`RunSubChain`/`WaitEvent`）+ R5（`Heal`/`ApplyState`/`ModifyAttribute`/`Repeat`/`Parallel`/`OnError`）+ R8（`PlayCue`） |
| G-4 | 4 个策略基类虚分派（`FTcsParamValueSource` / `FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` / `FTcsParamEvaluateContext`） | 架构（可选） | 不影响技能逻辑主体；迁移到注册表分派需独立提案 |

**性价比排序**：G-2b（A 类加宏）→ G-2c（伤害流程）→ G-2b′ + G-1b + G-1a（真前置，含分层设计）→ G-2a → G-3（随轮次）。

---

## 8. 回答"未来 TCS 是否满足"

**不能自动满足。**

- **plan3 对 G-1~G-4 的覆盖 = 0/4**（前次调研已核：plan3 全文零 `UFUNCTION` 新增；它自己的 `FTcsStepModifyFlow` 是又一个 C++ 执行器）。
- **后续轮次（R4 Task 3.5 / R5 / R6 / R8）补的全是 G-3**——即"能做什么"（原语数量），**不补"谁能登记"（G-1/G-2）**。
- ⇒ **技能/Buff 逻辑的脚本化，卡的从来不是原语数量，是登记通道**。即使 15 原语全齐，C# 仍只能"用现成原语拼"，写不了自己的新步骤。

### "完全脚本化"的两种读法（用户原话需要拆开答）

| 读法 | 含义 | 卡在哪 |
|---|---|---|
| **A** | 用 C# **拼现有原语**写复杂技能/Buff | G-2b + G-3（原语数量）；**加宏 + 等 R5/R6 即可** |
| **B** | 用 C# 写**全新的步骤语义** | G-1a/G-1b/G-2a；**且 G-1b 是真障碍（需先做分层设计）** |

---

## 9. 建议的下一步（按性价比，供拍板）—— **✅ 已拍板（2026-09-24）**

**用户拍板结果（2026-09-24，本节建议经用户逐条裁定）**：

| 建议项 | 裁定 |
|---|---|
| 立台账条目 | ✅ **已入册**——新开「脚本化工作流（S 系列）」区段，**S-1 ~ S-5** 五条，总数 27 → 32（见 `deferred-inputs-ledger.md`） |
| 最便宜的一刀（A 类门面加 `UFUNCTION`） | ✅ **接受为下一步**，并**追加 `FTcsChainRunHandle` 反射化**（B 类，避免"能注册但发起不了链"的半通面） |
| G-1b 的独立设计（上下文/黑板分层） | ✅ **出设计提案但不动手**（唯一有真实设计含量的部分，值得单独拍板，不阻塞前面的验证） |
| G-2c 作为"自定义伤害流程"最小闭环 | 未单列，归入 S-4 待触发（形参依赖 S-3 分层结论） |

**用户对脚本化动机的澄清（重要限定）**：三个动机**都占一些**——①摆脱 C++ 编译；②摆脱蓝图脚本的低扩展性（用户本人与策划都偏好 CS 脚本）；③写全新语义。**但明确"不是完全不能 C++"**，原话：

> 如果开发过程中真的出现非常适合放在 TCS 插件基建，或者 LAC 项目 C++ 底层的，肯定是没问题的。

⇒ **脚本化是偏好而非排他原则**。做归属判断时按"适合放哪一层"（插件基建 / 项目 C++ 底层 / CS 脚本），而不是"必须用 C#"。

**已定的下一步（S-1）**：G-2b（A 类门面加宏）+ `FTcsChainRunHandle` 反射化 + **一个三步骤链的 C# 实证**（用现成 3 原语在 `Script/LegendAutoChessCS/` 跑通"选目标 → 造成伤害 → 等待 → 再造成伤害"）。

**实施前待定**：S-1 改的是 TCS 插件源码（门面 API 反射化）⇒ 按分仓纪律**归 TCS 仓**，不落 LAC。

---

### 原始建议（保留备查）

1. **立台账条目**：G-1/G-2 三条全中入册判据（决策已拍板 D4-17 明文承诺"反射面"、代码未落地、不在任何计划 Task 里）。
2. **最便宜的一刀**：A 类门面 API 补 `UFUNCTION`（`RegisterChain` / `RegisterTriggerRow` / `RegisterTemplate` / `SetEntityQuery` / `SetTriggerGateTag`）——**能否先做，取决于是否接受"能注册但发不起来"的中间态**；若要一步到位，须连 B 类的 `FTcsChainRunHandle` 一起反射化。
3. **G-1b 的独立设计**：上下文与黑板分层（含 `FTcsFlowAttributes` 的反射镜像形态）——这是 G-1 的真前置，**应先于"给上下文加 USTRUCT 宏"**。
4. **G-2c 可作为"自定义伤害流程"的最小闭环**（流程侧 12 执行器已全，只差 delegate 出口的反射化）。

> **不建议**：现在就"给所有门面加 UFUNCTION"。A/B/C 三类混做会得到一个半通的面（能注册、发不起、登不了执行器），而故障点离原因很远（C# 报"找不到方法"vs 实际是形参非反射）。

---

## 10. 与既有文档的关系

- 前次调研 `2026-09-23-scripting-language-ustruct-inheritance-research.md`：G-1~G-4 定义、UnrealSharp vs UnrealCSharp 选型、C# 可达性矩阵、fork 同步硬阻断（**已由"改用上游 main"绕开**）。
- 本文：把"能不能用"从**源码推断**升级为**glue 产物实证**；新增 §5（`TFunction` 藏在流程黑板里 ⇒ G-1 物理受限）与 §4.3（A/B/C 三类难度分级）、§6.2（wrapper 空壳）。
- 台账 `deferred-inputs-ledger.md`：G-1/G-2 **尚未成表行**（§9.1）。
- 规格侧明文承诺（欠账的证据）：`openspec/specs/effect-step-dispatch/spec.md:12`（"注册入口 MUST 双份…反射层/脚本层可达"）、`openspec/specs/effect-trigger/spec.md:47`（"反射面欠账（明示，非遗漏）…与步骤执行器注册表同批解决"）、`openspec/specs/param-value/spec.md:46-48`（"MUST 是反射可见 USTRUCT——禁止 TFunction"）。

---

## 11. ★ 实施后的实测结论（2026-09-24，提案 `add-scripting-reflection-surface` 落地）

**门面调用面（§4.2 的"❌ 不通"）已修复并 PIE 实测通过。** 本节记录实施期发现的**三条可复用教训**——它们都是"读源码推断"看不出来、只有真跑才暴露的。

### 11.1 ★★ "能导出 ≠ 能往返"（本次最贵的一条）

**首版实现按"`USTRUCT()` + 非 `UPROPERTY` 内部字段"**（照既有 `FTcsEffectTriggerHandle` 形态）——理由看似充分：`TTcsInstanceHandle<T>` 是模板类型、无法作 `UPROPERTY`，而"空壳结构能当类型用"似乎够完成"接住 → 传回"闭环。

**实测否定了这个推断**：

| 环节 | 实测 |
|---|---|
| 门面反射面 | ✅ 通——C# 成功注册链、起链、链挂起、被到期堆唤醒、跑完（走 `UFunction::Invoke` 反射路径） |
| **句柄往返** | ❌ **不通**——`IsRunActive` 返回 false |

**根因**：绑定产物里 `ToNative`/`FromNative` 是**空函数体**（字段被跳过）⇒ C# 接住句柄时读不到值、传回时写全零 ⇒ 池给 `{Index=0, Generation=1}` 而 C++ 收到 `{0, 0}` ⇒ **代际失配判无效**。

**判据（可复用）**：判"某类型能否被脚本**往返**使用"时，**不能只看它是否被导出**——必须读绑定产物，检查 `ToNative`/`FromNative` 是否含**真实的字段读写代码**。空壳类型能通过编译、能当类型用，但**值过不去**。

**修法**：句柄**展平**为 `UPROPERTY int32 Index` / `UPROPERTY int32 Generation` + `GetInner`/`SetInner` 唯一转换点（无效值 `-1` 与 `0xFFFFFFFF` 位模式相同，往返无损）。先例 = `FTcsCombatEntityHandle`（同为展平的反射句柄）。

**推论**：**模板类型字段不可作 `UPROPERTY`** ⇒ 任何"含模板成员的值类型"要反射化，都必须先展平。这是 TCS 后续反射化工作的通用前置检查项。

### 11.2 门面反射面一打通，既有隐藏缺陷会集中暴露

`FTcsEffectTriggerHandle` 与 `FTcsChainRunHandle` 是**同一形态** ⇒ `RegisterTriggerRow` → `UnregisterTriggerRow` 的往返**同样是坏的**。它此前不可见，唯一原因是**C# 根本调不到这些方法**（无反射面）。

**判据**：打通一扇门时，应主动审计"所有依赖同一形态的既有设施"——缺陷不是新引入的，是新暴露的。本次同批修了两个句柄（同根因同修法）。

### 11.3 脚本层会踩到"对 C++ 调用方不存在"的路径

`UTcsEffectSubsystem::UnregisterChain` 的"有活动运行态"拒绝面原用 `ensureMsgf`——对 C++ 调用方这合理（很少这么写），但**脚本层"起一条挂起链后想注销"是完全正常的调用序列**，产出红字噪音；且同函数内"未登记就注销"用的是 `Warning`，**两种拒绝面口径不一致**。

**已修为 `Warning`**（台账 S-7 登记为口径先例）：**配置错误 → ensure；时序竞态 → Warning**。这与 TCS 内部既有惯例一致（悬空句柄 / 陈旧句柄 / 世界拆解期均"正常竞态不 ensure"）。

### 11.4 实测边界（`TInstancedStruct` 缺口，台账 S-6）

本轮实证**未能**覆盖策略位配置，原因 = §1 的 `TInstancedStruct<T>` 字段导出缺口：

- 原计划的"三步骤链（选目标 → 伤害 → 等待 → 伤害）"**不可实现**——`FTcsStepSelectTargets` 的两个 `TInstancedStruct` 字段在 C# 侧是空壳，构造不出来；
- 改用 `Damage` + `WaitDelay` 两步链后，`FTcsStepDamage.DamageBase`（`FTcsParamValue`，含 `TInstancedStruct`）**仍配不了** ⇒ 实测伤害记录 `Base=0.000`（其默认构造把 Source 初始化为 `Literal(0)`，恰好是"能跑但零伤害"）。

⇒ **S-6 是比 S-1 更靠前的脚本化障碍**：门面通了之后，C# 仍配不了任何带策略的字段。用户 2026-09-24 拍板：换型方案（裸 `FInstancedStruct` + `meta=(BaseStruct=...)`）单列提案下一轮做。

### 11.4 补记：S-6 已解决（2026-09-24 同日）

提案 `switch-strategy-carrier-to-plain-instanced-struct` 已落地并**实测全绿**——换型为裸 `FInstancedStruct` + `meta = (BaseStruct = ...)`：

| 判据 | 实测结果 |
|---|---|
| glue 字段可读写 | ✅ `FTcsParamValue.Source` / `FTcsStepSelectTargets.Selector`·`Filters` 生成真实读写代码（非空壳） |
| **资产兼容性** | ✅ `链定义 2/2 条，失败 0 条`（两个含 `TInstancedStruct` 数据的资产原样加载） |
| **三步骤链**（本节开头说"不可实现"的那条） | ✅ **补跑成功**——`SelectTargets[..]: 候选 1 → 通过 1（Filter 数 0）`，链 `步数=3` → `完成（共 3 步）` |
| **脚本层配数值** | ✅ `DamageBase=25` 生效：`[属性变更] Health：100.000 → 75.000`、`[伤害记录] 输入 25.000 / 执行 25.000` |

**兼容性三重论证**（实测成立，可复用）：①引擎保证 `TInstancedStruct` 与 `FInstancedStruct` 同尺寸且"反射层视为同一物"（`InstancedStruct.h:538`）；②UHT 产出的属性结构逐字段相同；③资产序列化的是**内层类型身份**（`Ar << TObjectPtr<UScriptStruct>`），与容器类型无关。

⇒ **本节 §11.4 之前描述的"实证边界"已消除**：脚本层现在可构造、可配置、值完整往返。**新增的待办**：`BaseStruct` metadata 写错则 picker 静默不限，建议入 M8 校验矩阵（归台账 R8-1）。

### 11.5 最终实证结果

```
第 0 步 OK：注册实体 Id=1 并设 Health=100
第 1 步 OK：拿到 UTcsEffectSubsystem（门面反射面可达）
第 2 步 OK：组装链完成（2 步：Damage + WaitDelay(0.5s)）
第 3 步 OK：RegisterChain 成功（门面反射方法可调用）
第 4 步 OK：ExecuteChainForCaster 返回（句柄 Index=0 Generation=1）   ← 值从 C++ 到 C#
第 5 步 OK：IsRunActive(handle) = true                              ← 值完整传回 C++
第 5.5 步 OK：Health = 100
第 6 步 NOTE：UnregisterChain 被拒（链仍挂起——正确行为）
实证结束：实体注册=通 / 链注册=通 / 句柄往返=通
```

**这意味着：C# 已能完成"注册实体 → 注册链 → 起链 → 接住句柄 → 查询状态"的完整闭环**，且链会被到期堆正常唤醒跑完。下一步的脚本化障碍是 S-6（策略位字段）与 S-2/S-3（行为登记与上下文反射化）。
