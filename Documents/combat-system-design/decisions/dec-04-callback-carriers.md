# 回调载体决策点：`TFunction` 的去留与替换边界

- **文档 ID**：`DEC-04-callback-carriers`
- **类型**：DEC / 决策记录
- **状态**：冻结（决策已定稿并拍板 2026-09-29）；**实现状态 = 裁定 ⑤ 第一批已落地**（`harden-registry-cross-world-lifetime` 已归档为 `2026-09-29-harden-registry-cross-world-lifetime`，A 类值语义改造 + 跨世界寿命兜底已完成并实测；裁定 ③ 已作为记账生效；裁定 ①④ 的其余部分与裁定 ② 的后续批次仍待落地）
- **权威范围**：全仓 `TFunction` / `TFunctionRef` 的**用途分类、可替换性判定与生命周期策略**；**宿主插槽的载体形态**（CDO vs 每执行实例）。**不在本文**——注册表的键与查表逻辑（D4-14 不变）、具体迁移的施工步骤（属提案）
- **最后更新**：2026-09-29

---

## 0. 裁定（2026-09-29，用户拍板：**§7 五项全部接受**）

| # | 裁定 | 生效范围 |
|---|---|---|
| ① | **接受"不全换"的边界**——A 类（注册表的值）改造；B 类（`TFunctionRef`）定案**不换**；C 类（C++ 内部回调）保持不动，其中 `OnConsumed` 重做；D 类（静态自注册载体）**禁止换** | 本文 §3 全节 |
| ② | **A 类的值形态 = 双轨注册值（内置纯函数 + 宿主弱引用 UObject）** | 本文 §5；同时是 `LEDGER-reflection` R-2 跨世界寿命缺陷的兜底形态 |
| ③ | **`SCRIPT-5`（`ITcsEntityQuery` 反射化）就此定案"不换"**，从"待办"改为"已裁决：保持 C++ 专用面" | 本文 §3.2；`LEDGER-deferred` SCRIPT-5、`LEDGER-reflection` R-3 同步改判 |
| ④ | **`FTcsConsumePolicy::OnConsumed` 改为事件/原语语义**，与 `DAMAGE-4`（消耗语义落地）**同批** | 本文 §3.3；`LEDGER-deferred` DAMAGE-4、`LEDGER-reflection` R-4/R-6 同步 |
| ⑤ | **落地顺序：先只做 A 类值语义改造**（它是 R-2 的前置护栏），再在其上落 R-1/R-2 两张新注册表 | 本文 §5 的关键联动 |

> **裁定后的下一步**：按本仓纪律，实施需以 OpenSpec 提案开道（`validate --strict` 通过 → 实施 → 编译验证 → 归档）。本批的提案边界 = ① 值语义改造（含跨世界寿命兜底）。

---

## 1. 为什么会有这份文档

用户对 `TFunction` 这一做法**从 TCS-Remake 开始就持续怀疑**，并在 2026-09-29 提出一个方向性问题：

> 目前 TCS-Remake 中所有涉及 `TFunction` 的用处，能否都换成 UObject-CDO 策略模式，或者按需开启 UObject-InstancePerExecution 策略模式？

这个问题值得落档，因为它不是"某个字段怎么改"，而是**对一种基础做法的质疑**——与 R0 立场书（`DEC-00-constitution`）、PV 载体（`DEC-01-pv`）、折叠与展示（`DEC-02-fold-display`）、AttributeSet 形态（`DEC-03-attribute-set`）同一量级。

**本文的结论是：方向对、范围必须收。** 全量替换在四个不同的地方分别撞上语义、性能、纪律与规格四类硬边界；但其中**真正该改的那一半**（注册表的"值语义"）确实该改，而且它正好是 2026-09-27 发现的跨世界寿命缺陷的根因所在（`LEDGER-reflection` R-2）。

---

## 2. 全量清单（实测，非估算）

扫描范围：`Source/**/*.{h,cpp}`，共 141 个文件。

| 口径 | 数量 |
|---|---|
| `TFunction` / `TFunctionRef` / `TUniqueFunction` 命中行 | **44 行 / 26 个文件** |
| 其中**注释行** | **30 行** |
| 其中**代码行** | **14 行** |

> ⚠ **统计口径更正（自查）**：本文第一版草稿曾报"110 处"，系把 `TFunction<void` 作为独立模式与 `TFunction` 重复计数所致。**真实值 44 行 / 14 行代码**。此更正保留在案，作为"计数即事实"（`CONVENTION` §6.5）的一次实例。

另经全形态扫查确认：`std::function` **0 处代码**；`TUniqueFunction` 0 处；自研委托 `DECLARE_DELEGATE` **0 处**（`TDelegate` 的 18 处命中全部是 `FDelegateHandle` / `FTcsSelHostDelegate` 等名字含 "Delegate" 的假命中，唯一真委托是引擎的 `FWorldDelegates::OnPostWorldInitialization`）。⇒ **本仓没有第二套回调机制，14 行就是全部改造面。**

---

## 3. 角色分类：这 14 行不是一类东西

**这是本文最关键的一张表**——`TFunction` 在本仓承担四种性质完全不同的职责，"能不能换"必须逐类回答：

| 类 | 角色 | 载体 | 是否**存储** | 判定 |
|---|---|---|---|---|
| **A** | **注册表的值** | 4 个类型别名 | **存**（长寿命，跨世界） | ✅ **可换**（仅动态入口那一半） |
| **B** | **接口形参** | `TFunctionRef<void(...)>` ×3 | **不存**（同步调用即弃） | ❌ **不可换**（换了是倒退） |
| **C** | **C++ 内部回调** | 单点 `TFunction` ×3 | 存（但纯 C++ 面） | ⚠️ **不该换**，其中 1 处**规格禁止** |
| **D** | **静态自注册的载体** | 裸函数指针 ×4 族 | 存于**静态初始化期** | 🔒 **禁止换** |

### 3.1 A 类：注册表的值（4 个别名 = 改造主战场）

| 别名 | 定义处 | 值签名 |
|---|---|---|
| `FTcsStepExecute` | `Source/TcsEffect/Public/Chain/TcsEffectStepExecutor.h:18` | `ETcsStepResult(const FInstancedStruct&, FTcsEffectContext&, FTcsChainRun&)` |
| `FTcsFlowStepExecute` | `Source/TcsDamage/Public/Flow/TcsFlowStepExecutor.h:16` | `bool(const FInstancedStruct&, FTcsDamageFlowContext&)` |
| `FTcsTriggerConditionTest` | `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.h:64` | `bool(const FInstancedStruct&, const FTcsTriggerContext&, double)` |
| `FTcsTriggerPayloadRead` | `Source/TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h:61` | `FTcsTriggerPayloadInfo(const FInstancedStruct&)` |

**现状（重要）**：SCRIPT-8 已经**部分完成**了 UObject 化。四张注册表里，两张（步骤执行器）已经支持"值 = UObject"：

- `Source/TcsEffect/Public/Chain/TcsStepExecutor.h:22` —— "本基类**替代 `TFunction` 作注册值**：门面把本对象包成 `TFunction` 转发进既有注册表"
- `Source/TcsDamage/Public/Flow/TcsFlowStepExecutorObject.h:22` —— 同款，流程侧

所以问题不是"能不能换"，而是**"能不能把静态自注册那一半也换成 UObject"** → 见 §3.4 D 类，答案是不能。

### 3.2 B 类：`TFunctionRef` 接口形参（3 处）

| 位置 | 签名 |
|---|---|
| `Source/TcsEffect/Public/Host/TcsEntityQuery.h:48` | `virtual void EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor) = 0;` |
| `Source/TcsIntegration/Public/Entity/TcsPieEntityQuery.h:83` | 覆写声明 |
| `Source/TcsIntegration/Private/Entity/TcsPieEntityQuery.cpp:39` | 实现 |

**判定：不可换。** 三条独立理由，任一条足以否决：

1. **语义方向不同**——`TFunctionRef` 是"宿主提供回调、框架**在遍历过程中同步调用**、**框架不存它**"。换成 UObject 意味着框架必须先构造/持有对象再调用，契约从"**拉**（pull，宿主决定何时读）"变成"**推**（push，框架决定何时喂）"。
2. **热路径与分配**——`ForEach` 式遍历在热路径上。`TFunctionRef` 零分配、零寿命负担；UObject 版要么每次调用 `NewObject`（把分配推进热路径，且与 D0-1 确定性纪律紧张），要么复用单例（引入隐式状态，重入不安全）。
3. **规格已明文"不承诺"**——`TcsEntityQuery.h:32` 自陈："C++ 专用面：参数含 `TFunctionRef`，蓝图不可表达（与 R0 §9'蓝图不承诺'一致）"。

> **对台账的影响**：这正是 `LEDGER-deferred` 的 **SCRIPT-5**（`ITcsEntityQuery` 反射化）——它被"并入 SCRIPT-8"后 SCRIPT-8 又明文"不做"，于是悬了三个轮次。**本文建议就此正式定案为"不换"**，把条目从"待办"改为"已裁决：保持 C++ 专用"，而不是继续挂着（详见 §7 待裁决问题 ③）。

### 3.3 C 类：C++ 内部回调（3 处，其中 1 处是雷）

| 位置 | 形态 | 判定 |
|---|---|---|
| `Source/TcsCore/Public/Clock/TcsExpiryHeap.h:48`（及 `:67`、`:237`） | `TFunction<void(uint64)> OnDue` —— 到期堆条目 | **不该换**：闭包是**瞬态**的（`Push` 传入、到期时 `MoveTemp` 出来立即调）。UObject 化需回答"谁 NewObject、谁保证调用前活着"，纯增负担 |
| `Source/TcsCore/Public/EventBus/TcsEventBus.h:239` | `TFunction<void(FGameplayTag, const FInstancedStruct&)> EventObserver` —— 总线观察者 | **不该换**：同上；总线是 M0 基础设施，不宜依赖 UObject |
| **`Source/TcsDamage/Public/Flow/TcsFlowAttributes.h:30`** | `TFunction<void()> OnConsumed` —— **消耗策略的回调** | **★ 必须单独处理，见下** |

#### ★ `FTcsConsumePolicy::OnConsumed` 是一个"没人用、却挡着路"的死字段

实测三项事实：

**① 零消费者（逐字段 grep 全仓代码）**

| 字段 | 全仓代码命中 |
|---|---|
| `OnConsumed` | **1 处** = 定义处本身（`TcsFlowAttributes.h:30`） |
| `MaxUses` | **1 处** = 定义处本身（`:21`） |
| `Cooldown` | **1 处** = 定义处本身（`:24`） |
| `SortKey` | 5 处（定义 ×2 + 裁决处 `TcsFlowStepsCore.cpp:141/143`） |
| `BestIndex` | 2 处（`TcsFlowStepsCore.cpp:135` 赋值、`:144` 赋值）——**赋值后无任何使用点** |

**② 它是唯一挡住整个黑板反射的东西**

`Source/TcsDamage/Public/Flow/TcsDamageFlowContextView.h:27` 原文：

> `- Blackboard——见上，含 TFunction 物理不可反射；`

`:21-23` 给出了完整因果链与代价评估：

> 后者的黑板 `FTcsFlowAttributes` → `FTcsFlowAttributeSubmit` → `FTcsConsumePolicy::OnConsumed` … **物理不可反射**（UHT 实证）。真前置是"上下文/黑板分层"（台账 SCRIPT-3），**成本高一个数量级**

⇒ 也就是说：**SCRIPT-3（上下文反射化）之所以被"降为可选"、成本被评为"高一个数量级"，根因就是这一个没人用的 `TFunction<void()>`。**

**③ 它同时是网络面的禁忌**

`Source/TcsDamage/Public/Flow/TcsDamageRecord.h:28` 与复制纪律均明文：过网结构 **MUST NOT 出现 `TMap`/`TSet`（UHT 层面不可复制）与 `TFunction`（UHT 报错）**。虽然 `FTcsConsumePolicy` 是运行态而非资产，但"流程上下文要能过网"这件事一旦立项，它会是第一个拦路者。

**⇒ 本文对 C-③ 的建议**：把 `OnConsumed` 从"闭包字段"改为**具名语义**——按 `LEDGER-deferred` DAMAGE-4 的既有方向（"消耗语义落地"本就是待办），让"被消费时要做什么"表达为**事件或原语**（可反射、可复制、可脚本可达），而不是一个内联闭包。**这件事与 DAMAGE-4 同批做最经济**，因为那块业务逻辑本来就要重做。

### 3.4 D 类：静态自注册的载体（**必须保持非 UObject**）

四张注册表各有一套"静态自注册器 + `Registrar` + 裸函数指针"：

| 位置 | 形态 |
|---|---|
| `TcsEffect/Public/Chain/TcsEffectStepExecutor.h:29,43` | `UScriptStruct* (*GetStepStruct)() = nullptr;` + `FTcsEffectStepExecutorRegistrar` 构造 |
| `TcsDamage/Public/Flow/TcsFlowStepExecutor.h:27,40` | 同款 |
| `TcsEffect/Public/Trigger/TcsTriggerCondition.h:79,93` | 同款 |
| `TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h:73,87` | 同款 |

**判定：禁止换。** 判据来自本仓自己的代码注释（`TcsEffectStepExecutor.h:37-40`）：

> 静态自注册器（D4-14；宏展开的载体）：模块静态初始化期构造 → 把本项挂入待解析表。
> **零 UObject 触达**——反射类型延迟到注册表首次查询时才解析。

这是**刻意的纪律**（设计文档注明"依据引擎 `FNativeGameplayTag::GetIfAllocated()` 同款纪律"）。三条理由：

1. **静态初始化期没有世界**，且引擎对象系统是否就绪不保证——在模块静态初始化期取 UObject CDO 是危险操作；
2. `UScriptStruct*` getter 之所以做成**延迟调用**（`GetStepStruct` 是函数指针而非直接指针），正是为了避开"静态初始化期触达反射系统"；
3. **零收益**：内置执行器是**代码**（`&ExecutorFn`，编译期常量地址），把它包成 UObject 只增加间接层与寿命管理负担。用户提出的 "InstancePerExecution" 对内置执行器毫无意义——它们本来就是"每执行一次调用一个纯函数"。

---

## 4. 生命周期策略矩阵（回答"CDO vs InstancePerExecution"）

用户的问题里还有第二个子问题：**按需开启 UObject-InstancePerExecution**。本文的建议是**按角色分派**，而不是全局统一：

| 角色 | 建议策略 | 判据 |
|---|---|---|
| **步骤执行器**（Effect / Flow） | **InstancePerExecution** | 执行可能**挂起**（`TSR_Running` → `ResumeRun`），实例需要承载"挂起期间的状态"；共享 CDO 会串味 |
| **条件求值器** | **CDO 优先** | 纯判定、无状态；是热路径（`TcsTriggerEvaluator.cpp:169` 每候选行每条件一次），InstancePerExecution 会把分配推进热路径 |
| **载荷读取器** | **CDO** | 每事件一次（`ReadPayloadInfo`，各行共用）；无状态。且**当前登记数为 0**（`UE_DEFINE_TRIGGER_PAYLOAD_READER` 全库只有定义、零调用）⇒ **改造它零回归风险**，适合当原型试点 |
| **时钟到期回调** | 保持 C++ 闭包 | 瞬态，无跨帧寿命需求 |
| **事件总线观察者** | 保持 C++ 闭包 | 同上；M0 基础设施不宜依赖 UObject |
| **`OnConsumed`** | **改为事件/原语语义**（见 §3.3） | 闭包形态同时挡住反射与复制 |

**CDO 形态的额外收益**：CDO 天然是"进程内唯一、无寿命问题"的——**用它就不需要解决跨世界寿命问题**（R-2）。这也是为什么"条件求值器/载荷读取器走 CDO"是个好选择：它们无状态，用 CDO 既省分配又天然免疫寿命缺陷。

---

## 5. 建议的统一方案形态

不是"把 `TFunction` 全换成 UObject"，而是**让注册值能表达两种来源**（`LEDGER-reflection` R-2 已拍板的解法 A 的自然延伸）：

```cpp
// 概念示意（非最终形态）
struct FTcsStepExecutorEntry
{
    UScriptStruct* (*GetStepStruct)() = nullptr;   // 键：不变（延迟 getter 纪律保留）
    ETcsStepExecute        BuiltinFn  = nullptr;   // 内置：纯函数（静态初始化期安全）
    TWeakObjectPtr<UTcsStepExecutor> HostObject;   // 宿主：弱引用感知寿命
};
```

**两个真正的增量**（相对于现状）：

1. **弱引用/有效性语义** ⇒ 直接消灭跨世界寿命缺陷（`LEDGER-reflection` R-2）：**对象死了 = 该条登记自然失效**，不需要依赖"谁记得撤销"。这正是用户在 R-2 评审时的裁定顺序——"**优先 TCS 侧兜底，A1（补 `Unregister` + 门面注销）作退路**"——的落地形态。
2. **明确"哪种角色用哪种寿命策略"** ⇒ 即 §4 的矩阵。

**关键联动**：R-2 若照抄 SCRIPT-8 模式新增两张注册表，会把同一缺陷**复制成三份**（调研已明确警告）。故**R-2 的落地必须与本文的"值语义"改造同批**，否则是在扩散缺陷。

---

## 6. 迁移代价与影响面

| 项 | 影响 |
|---|---|
| **A 类改造面** | 4 张注册表的内核 + 4 个门面注册入口。查表逻辑与键**零改动** |
| **B 类** | 不动（本文明文定案"不换"） |
| **C 类** | 只动 `OnConsumed` 一处，且与 DAMAGE-4 同批 |
| **D 类** | 不动（纪律保留） |
| **编译验证** | Development **+ Shipping**（注册表含 `UCLASS`/`UPROPERTY` 面，且 `IsDataValid` 类 `WITH_EDITOR` 面按既有纪律必跑 Shipping） |
| **回归网** | `Tcs.Test.Slice.Run`（10/10）与 `Tcs.Test.Slice.Reject`（3/3）应原样通过——**双轨并存意味着 C++ 快路径行为不变** |
| **规格影响** | `effect-step-dispatch` / `damage-flow` / `effect-trigger` 的"注册入口双份"相关需求需按新值形态回写；`param-value` 不受影响 |
| **跨world 缺陷的实测** | 按 R-2 既有裁定："**先实测，但只做记录**"——不因实测结果改变设计（TCS 应直接兜底，不交给宿主项目） |

---

## 7. 待用户裁决的问题

> **⚖ 本节已结案（2026-09-29）**：五项全部被接受，裁定见 **§0**。保留下方原文作**决策留痕**——它记录了"当时有哪些选项、为什么这样建议"，是 §0 裁定的依据面。后续修订 MUST 改 §0，不改本节。

| # | 问题 | 本文建议 |
|---|---|---|
| ① | **是否接受"不全换"的边界**？即 A 类改造 + C-③ 重做，B 类定案"不换"、C 类其余不动、D 类禁止换 | 建议接受；全换会同时撞上语义（B）、纪律（D）、热路径分配、网络规格（C-③）四类边界 |
| ② | **A 类的值形态**：采"双轨注册值（纯函数 + 弱引用 UObject）"，还是别的形态（如注册值类型擦除、或注册表改两张表） | 建议双轨 + 弱引用；它同时是 R-2 寿命缺陷的兜底形态，且与"优先 TCS 侧兜底"的既有裁定一致 |
| ③ | **SCRIPT-5（`ITcsEntityQuery` 反射化）是否就此定案"不换"**，从"待办"改为"已裁决：保持 C++ 专用面" | 建议定案。它已悬三个轮次，且 §3.2 的三条理由不会因时间改变 |
| ④ | **`FTcsConsumePolicy::OnConsumed`** 是否按"改为事件/原语语义"处理，并与 DAMAGE-4 同批 | 建议是。它是"零消费者 + 挡住 SCRIPT-3 + 挡网络"的三重负担，且业务逻辑（消耗语义）本来就要重做 |
| ⑤ | **落地顺序**：与 R-2/R-1 反射插槽合成一批（"脚本通道收口"），还是先只做 A 类值语义改造 | 建议**先做值语义改造**（它是 R-2 的前置护栏），再在其上落 R-1/R-2 两张新注册表 |

---

## 8. 依据

- **代码实测**（本文全部结论的一手来源）：`Source/**/*.{h,cpp}` 141 文件全量扫描；关键行号已在各节就地标注。
- `LEDGER-reflection`（`ledger/reflection-backlog.md`）：R-2 调研（跨世界寿命缺陷的发现与解法 A/A1）、SCRIPT-2/SCRIPT-3/SCRIPT-5 的定位
- `LEDGER-deferred`（`ledger/deferred-inputs-ledger.md`）：SCRIPT-1/SCRIPT-5/SCRIPT-8 条目、DAMAGE-4（消耗语义落地）
- `SPEC-03-effects` 的 D4-14（注册制分派）/ D4-17（语言无关执行器双入口）；`SPEC-08-damage` 的 D7-4（消耗型修正器）
- `SPEC-04-skill`、`research/csharp-authoring.md`：SCRIPT 系列的判据来源
- `CONVENTION` §6.5「计数即事实」（本文 §2 的统计口径更正即其实例）
