# ARCH-10-damage · TcsDamage（瞬时伤害流程层）

- **档案 ID**：`ARCH-10-damage`
- **模块**：`TcsDamage`（3 225 行 / 30 文件 ~~3 248 / 31~~ —— 仅 `.h` + `.cpp`；含 `TcsDamage.Build.cs` 23 行为 3 248 / 31）
- **上游依赖**：`TcsCore` / `TcsAttribute` / `TcsEffect`（实测自 `TcsDamage.Build.cs:12-21`）
- **下游消费者**：`TcsIntegration`（声明，**零代码引用**）／ `TcsDev`（装置，真实消费者）／ 宿主
- **审核日期**：2026-10-17
- **审核方法**：`Documents/tcs-arch/INDEX.md` §5（六步）
- **本篇结论**：**它是全仓唯一"规格被一次改造整段掏空"的模块——`damage-step-library` 的标题还写着「十步」，而十步的逐条契约在同一次 tag 化改造里被整段删除，只剩一段泛论。** 于是"规格更严"与"规格漏写"在这里第一次同时发生：**该严的地方没正文可严，该写的地方没人发现漏了**。另有一整类缺陷在本模块第二次出现——**同款代码从 `TcsEffect` 逐字复制过来，缺陷一起复制**。

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

`TcsDamage` 交付**一次瞬时伤害流程的全部机制**，且**只交付机制**：

1. **流程模板与登记表**（`FTcsFlowTemplate` + `UTcsDamageSubsystem::RegisterTemplate`）——阶段构成本身是项目知识（D7-5）；
2. **三层值空间的上下文**（`FTcsDamageFlowContext`：技能参数不进 / 公式参数初值 / 流程属性黑板）；
3. **流程步骤注册表**（与 `TcsEffect` 同构、独立实例）+ 自注册宏 + 脚本插槽；
4. **同步单帧解释器**（`RunTemplate`）；
5. **收集事件协议**（11 个原生 tag + `PublishCollectEvent`）；
6. **十步标准步骤库** + 两个数据步骤 + **官方默认模板**；
7. **宿主委托 7 方法** + 反射视图 + **伤害记录与环形缓冲** + 入库浏览命令；
8. **两个链原语**（`FTcsStepDamage` / `FTcsStepModifyFlow`，均经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 跨模块自注册）。

它**不做任何计算**：命中率 / 暴击率 / 元素 / 基础公式 / 护盾 / 吸收映射 / 致死线，**七处全是宿主委托**。头注释把这条纪律写成「插件零公式」（`TcsDamageFlowDelegate.h:15`），并给出可判定形式：**"普通项目零 delegate"也要能跑通官方默认模板**（`:25-27`）。

### 1.2 文件组与职责【实测】

| 组 | 文件 | 行 | 职责 |
|---|---|---|---|
| 模块壳 | `TcsDamageModule.h/.cpp`、`TcsDamageLogChannel.h/.cpp` | 35 | 标准壳 + 独立日志通道 |
| 门面 | `Public/TcsDamageSubsystem.h` / `Private/TcsDamageSubsystem.cpp` | 615 | 登记表 / 解释器 / 记录缓冲 / 11 个原生 tag / 浏览命令 |
| 上下文 | `Flow/TcsDamageFlowContext.h` | 141 | 纯 C++ 三层值空间（**非反射**） |
| | `Flow/TcsDamageFlowContextView.h` | 95 | 可反射数据面投影（单向） |
| 黑板 | `Flow/TcsFlowAttributes.h` / `Private/…cpp` | 195 | `FTcsFlowAttributes` + 消耗策略三字段 |
| 契约键 | `Flow/TcsFlowKeys.h` / `Private/…cpp` | 112 | 9 个黑板键 + 1 个模板 id（原生声明） |
| 委托 | `Flow/TcsDamageFlowDelegate.h` | 180 | `UINTERFACE` + 7 方法 + 中性默认体 |
| 收集协议 | `Flow/TcsDamageFlowCollectEvent.h` / `Private/…cpp` | 108 | 载荷形状 + 载荷读取器 |
| 记录 | `Flow/TcsDamageRecord.h` | 115 | `FTcsDamageRecord`（12 字段全扁平） |
| 模板 | `Flow/TcsFlowTemplate.h` | 39 | `{ TemplateId; Steps }` |
| 步骤注册表 | `Flow/TcsFlowStepExecutor.h` / `Private/…cpp` | 399 | 注册表 + 双入口 + 寿命语义 + 两个宏 |
| 脚本插槽 | `Flow/TcsFlowStepExecutorObject.h` / `Private/…cpp` | 80 | `UCLASS(Abstract, Blueprintable)` |
| 条件 | `Flow/TcsFlowStepConditions.h` | 104 | 两个谓词 + **inline 求值助手** |
| 十步形状 | `Flow/Steps/TcsFlowSteps.h` | 167 | 10 个 `USTRUCT`（无公共基类） |
| 数据步骤 | `Flow/Steps/TcsFlowDataSteps.h` | 69 | `FTcsFlowModify` / `FTcsFlowDelegate` |
| 十步执行器 | `Private/Flow/Steps/TcsFlowStepsCore.cpp` | 299 | 默认模板的 4 步 |
| | `Private/Flow/Steps/TcsFlowStepsRest.cpp` | 230 | 其余 6 步 + 2 数据步骤 |
| 链原语 | `Chain/TcsStepDamage.h/.cpp` | 133 | 起流程 |
| | `Chain/TcsStepModifyFlow.h/.cpp` | 125 | 向流程黑板提交修正 |

**结构上的第一件事**：**0 个 `UENUM`**（`grep '^UENUM' Source/TcsDamage/` 零命中）。11 个模块里只有它和 `TcsIntegration` 是零枚举——但 `TcsIntegration` 是装配层，而本模块把**所有词都交给了 tag**（9 个黑板键 + 11 个事件 tag + 1 个模板 id，全 `FNativeGameplayTag`）。

这不是省略，是 2026-09-22 那次「标识体系 tag 化」的直接结果。**而正是同一次改造，把本模块的规格掏空了**（见 D-4）。

### 1.3 本模块最反常的一点：**规格的标题承诺了十步，正文一步都没写**【实测】

`damage-step-library/spec.md` 的措辞处处是"十"：

| 位置 | 原文 |
|---|---|
| `Purpose` | 定义伤害流程的标准步骤库（**十步**）… |
| `Requirement` 标题 | 标准步骤库（**十步**） |
| 正文首句 | `TcsDamage` MUST 提供**十阶段**标准步骤库… |
| 官方默认模板节 | 标准**十阶段**只是插件自带的标准件… |

**但正文里没有一步的逐条契约。** 十步各自"做什么、写哪个键、失败怎么办"，**在生效规格里一个字都没有**。

#### 它们曾经在【实测】

`git show a1804ed:openspec/specs/damage-step-library/spec.md`（R3 Task 4 归档时）：

```
- `FTcsFlowCollectStart`：发流程开始事件（原生 Tag `Tcs.Event.Damage.FlowStarted`）+ **重置收集**（`Blackboard.Reset()`）；
- `FTcsFlowPreHit`：发收集事件 → 应用（宿主挂点：修改命中率/注册免疫候选）；
- `FTcsFlowHit`：基础命中率（delegate `GetBaseHitRate`）→ 修改器 → 判定，结果写黑板（键 `Hit`，1/0）；
…（共十行，逐步骤写清"做什么 / 写哪个键 / 谁的挂点"）
- `FTcsFlowExecute`：免疫/减伤**裁决**（按消耗策略的 `SortKey` 选一）→ 宿主护盾 hook → M2 事务扣血（属性键 = **步骤级 `AttrKey` 优先、否则用 `Context.TargetAttrKey`**…）→ **成功才消费**；
- `FTcsFlowCompleted`：发完成事件 + 填充并发布 `FTcsDamageRecord`
```

#### 它们在 2026-09-22 的那次改造里被整段删除【实测】

`git show 539b14e -- openspec/specs/damage-step-library/spec.md`：**17 insertions / 25 deletions**，那十行被替换成一段「执行期行为契约」泛论（讲契约键为何原生声明、为何不许裸字面量）。

**为什么？** 那次改造的主题是「标识体系 tag 化」——修改理由是"键类型 `FName` → `FGameplayTag`"。**十行里每一行都提到键名，于是整段被当成"需要改写的内容"重写了一遍；而重写后的版本只保留了与 tag 有关的那部分，步骤行为本身没有落回来。**

⇒ 后果是一条**双向的债**：

- 规格变松了：「`Execute` 成功才消费」这类**明文 MUST** 在生效规格里消失，只在归档提案里存活（台账 `DAMAGE-4` 记了这一点，但**归因写成"归档时规格被裁"——时点是错的**，归档发生在 09-21、删除发生在 09-22）；
- 规格也变"无法更严"了：**没有正文就没有可被违反的条目**，于是 D-2 / D-5 / D-6 这类"代码偏了规格"的缺陷**在规格侧根本无从判起**——它们的偏差只能靠读代码比对旧提案才能发现。

#### 一次对照：同一批的 `TcsEffect` 没有被掏空

`effect-step-dispatch` / `effect-interpreter` 同期也做了 tag 化，但它们的步骤契约至今在正文里（`effect-interpreter/spec.md:352` 一类逐字段条目）。⇒ **这不是"tag 化的必然代价"，是那一次改造对某一个文件失手**。

### 1.4 依赖面：唯一的声明者用不上，唯一的真消费者是装置【实测】

`TcsDamage.Build.cs:12-21` 声明 `Core / CoreUObject / Engine / GameplayTags / TcsCore / TcsAttribute / TcsEffect`。反查**谁依赖它**：

| 模块 | `Build.cs` 声明 | `Source/` 内代码引用 |
|---|---|---|
| `TcsIntegration` | ✅ `TcsIntegration.Build.cs:22` | **1 处，是注释**（`TcsPieEntityQuery.h:34`「机制层 MUST NOT 依赖本映射」） |
| `TcsDev`（LAC 侧装置） | ✅ | **真实消费者**：3 条模板登记 + 2 个自研步骤 + 1 个公式 delegate + 1 个屏显订阅 + 记录读数 |
| 宿主（内容资产） | — | 走 tag 与 `FInstancedStruct`，**无编译期依赖** |

⇒ 与 `ARCH-09` 的 `TcsTargeting` **同型**：**声明了依赖的那个模块不用它，真正用它的是装置**。区别在于本模块**有门面**——装置能 `GetSubsystem<UTcsDamageSubsystem>()`，所以它不像 `TcsTargeting` 那样"存在不可观测"（见 §3.3）。

---

## §2 已落地

### 2.1 三层值空间：**"第二层不消失，第一层不来"**【实测】

`FTcsDamageFlowContext`（`TcsDamageFlowContext.h:29`）的分区与注释一一对应三层值空间（`:24-27`）：

| 层 | 载体 | 字段 | 状态 |
|---|---|---|---|
| ① 技能参数（账本，Entry 级持久） | **不在本结构** | — | 注释写「不进流程，由 TcsSkill 侧解算后经第 2 层传入」 |
| ② 公式参数初值 | `FormulaParams`（`:52`） | `TMap<FGameplayTag, FTcsParamValue>` | **写入 1 处、读取 0 处** |
| ③ 流程属性黑板 | `Blackboard`（`:55`） | `FTcsFlowAttributes` | 活 |

**第 ② 层的实际状态值得单独记一笔**：`FormulaParams` 由 `TcsStepDamage.cpp:48` 从步骤拷进上下文，再在 `MakeView()`（`:133`）里拷进视图——**此外全仓无人读它**。

```
$ grep -rn "FormulaParams" Source/TcsDamage
TcsStepDamage.cpp:48        FlowContext.FormulaParams = Step->FormulaParams;   // 写
TcsDamageFlowContext.h:52   TMap<...> FormulaParams;                          // 声明
TcsDamageFlowContext.h:133  View.FormulaParams = FormulaParams;               // 拷给视图
TcsDamageFlowContextView.h:71  TMap<...> FormulaParams;                       // 视图声明
```

⇒ **它唯一的消费者是"宿主脚本在 `CalculateBaseDamage` 里按名取用"**（规格 `damage-primitive:123` 正是这么写的 Scenario）。这条链在 LAC 侧的真实实现（`UTcsDevDamageFormula`）**没有读它**——它读的是属性门面。

**这不是缺陷**（"零消费者不预建"在这里不适用：`FormulaParams` 有规格明文与一个会成立的 Scenario），但它意味着**"三层值空间"的第二层，今天仅有形状、无读者**。已登记：`damage-flow:46` 有字段与键根归属，**没有任何需求说"谁来读"**——见 U-3。

### 2.2 两个"契约默认"的教科书形态：**减法模型与致死线**（全仓最佳实践之一）【实测】

`damage-primitive` 的「流程委托契约」把「框架内定 vs 契约默认」这条判据推到全仓最清晰：

| 表达式 | 判定 | 理由（规格原文） |
|---|---|---|
| `FMath::Max(0.0, ·)` | ✅ **契约默认** | 「零下限只引用数值、方向收敛到'不介入'」 |
| `Candidate - Absorbed` | ❌ **框架内定** | 「宿主只能供'吸收了多少'，无法换成按比例分摊 / 带溢出上限 / 先扣护盾再溢出到生命」 |
| `CurrentValueAfterApply <= 0.0` | ❌ **框架内定** | 「它断言了'生命归零即死亡'这一玩法模型」 |

**处置不是删，是给覆盖点**（`:66`）：两项内定各变成一个 `BlueprintNativeEvent`，默认体保持原行为（`TcsDamageFlowDelegate.h:154-157` / `:174-177`）。⇒ 「框架内定的处置不是删，而是给它一个覆盖点」——这条判据值得抄进别的模块。

### 2.3 值的两档提交纪律：**输入走 Add、结果走 Override**，且理由可判定【实测】

`TcsFlowStepsCore.cpp:48-65` 立了两个助手 `SubmitAddValue` / `SubmitOverrideValue`，理由（`:49-52`）写得比规格还准：

> **输入/基值类走 `Add`**：折叠式 `(0 + ΣAdd) × (1 + ΣPercentAdd) × ΠMul + ΣFlatAdd` 里"基值"天然就是 `ΣAdd` 的一员…
> **MUST NOT 用覆盖带**——覆盖带盖掉一切，会把收集到的修正（如"受火伤 +20%"）整笔抹掉。
> **结果类走 `Override`**：`Executed`/`Absorbed`/`Kill` 是本次执行的事实值，不含修正带语义。

这条纪律**有可判定的对照实验**：把基值改成 Override，收集到的 `PercentAdd` 会整笔失效——不是"数值不对"，是**结构性错**。规格 `damage-step-library:23` 的值域纪律与它同源。

### 2.4 「链侧预写不与流程提交重复计数」：**全档案唯一一条带实测反例的 MUST** 【实测】

规格 `damage-primitive:19-20` 的原文：

> **执行器 MUST NOT 写黑板**：本步骤是**请求方**，只负责把输入填进**上下文请求字段**…
> **理由（实测，MUST NOT 回退）**：黑板会被 `CollectStart` 重置（**收集语义**），而输入是"请求"的一部分、**不随收集清除**。若改由本步骤从黑板读输入，则"链侧预写 + 本步提交"会造成**重复计数**（实测：`10 + 20 = 30`）。

代码侧对应 `TcsFlowStepsCore.cpp:94-97`，注释一字不差地复述了这条理由。**"实测反例 `10 + 20 = 30`"写进规格**是本仓第一次——它把一条"风格约定"升级成"可复现的错误"。

### 2.5 记录是全扁平的，且元素被显式驱逐【实测】

`FTcsDamageRecord`（`TcsDamageRecord.h:33`）12 个字段全部标量或句柄，**无 `TMap` / `TFunction`**（`:27-30` 的纪律），因此"将来可直接上 `FFastArraySerializer`"。

**`Element` 字段的删除是本模块最干净的一次边界整肃**（`damage-primitive:139`）：

> 全库对 `Element` 的操作只有一处且是**置空**…**零消费者**…**MUST NOT 在记录形状里复活"元素"这个玩法名词**——它违反主判据 *"凡是需要宿主词汇的内置策略，插件一律不提供"*。

实测复核：`grep -rn "Record.Element" Source/` **零命中**；`Element` 残留 22 处**全部是步骤名 / 事件名 / 委托方法名**（`FTcsFlowElement` / `TcsEvent.Damage.Element` / `ResolveElement`）⇒ 规格 `:182-185` 的「删字段后零残留」Scenario **真的通过了**。

### 2.6 步骤注册表：**与 `TcsEffect` 同构、独立实例，且逐行同构**【实测】

`FTcsFlowStepExecutorRegistry`（`TcsFlowStepExecutor.h:71`）与 `FTcsEffectStepExecutorRegistry`（`TcsEffectStepExecutor.h`）的关系，实测：**把标识符改个名之后，两份 `.cpp` 的差异只剩 70 行**（`diff` 的 `^[<>]` 计数），且剩余差异**全是注释与日志文案**。

```
$ diff TcsFlowStepExecutor.cpp TcsEffectStepExecutor.cpp | grep "^[<>]" | wc -l
70
```

⇒ 规范明确写了「同构、但独立实例」（`damage-flow:100`：两套流程各自演进，不共享表）。**这条分割是对的**（两类词汇），但它带来一个副作用：**同款缺陷被复制了两遍**——见 D-7。

### 2.7 寿命语义：四条失效判据 + 拒绝门收窄，本模块**完整复刻**【实测】

`damage-flow:111-116` 的六条（记录双弱引用 / 三条失效判据 / 查询侧校验 + Warning / 拒绝门收窄 / 显式移除 / `Deinitialize` 收口）在代码里逐条对上：

| 规格条 | 代码锚点 |
|---|---|
| 记录宿主对象 + 世界弱引用 | `TcsFlowStepExecutor.cpp:156-158`（`FindOrAdd` 双赋值） |
| 失效判据三条 | `:170-172`（`bObjectCollected` / `bWorldGone` / `bWorldMismatch`） |
| 跨世界 MUST NOT 静默 | `:130-134`（Warning 含登记世界与查询世界两个名字） |
| 拒绝门收窄为"同世界活对象重复" | `:44-58` → `DiscardIfStale` |
| 显式移除入口、不可移除静态项 | `:64-82`（`Lifetimes` 无条目即返回 false） |
| `Deinitialize` 收口 | `TcsDamageSubsystem.cpp:95-99` |

**一处 SPEC 与实现的偏差**：规格 `:116` 写「门面 MUST 撤销**本世界**登记的动态条目」，而 `TcsDamageSubsystem.cpp:95` 遍历的是 `GetDynamicKeys()` 的**全部**动态键——**不限世界**。见 D-8。

### 2.8 载荷读取器：**属主自登记，且"每事件一次"有明确位置**【实测】

`TcsDamageFlowCollectEvent.cpp:22-34` 的读取器把 `Attacker` 映射成 `Caster`（`:17-18` 明写「"谁发起的"由触发行侧的 `Instigator` 承载，不是本字段的语义」），`ClassificationTags` 直通。

这是 `UE_DEFINE_TRIGGER_PAYLOAD_READER` **全库第一个登记**（`damage-primitive:264` 记了"此前只有定义、零登记"）。第二个是 `TcsBuff`（`TcsBuffEvents.cpp:36`，注释 `:35` 明写「继 TcsDamage 收集事件读取器之后」）⇒ **先例传播成立**。

### 2.9 收集事件全部原生声明，且**每个词都带 `DevComment`**【实测】

`TcsDamageSubsystem.cpp:21-44` 共 **11 个** `UE_DEFINE_GAMEPLAY_TAG_COMMENT`：8 个阶段挂点（`FlowStarted` / `PreHit` / `Hit` / `Crit` / `Element` / `AfterDamage` / `PreExecute` / `Completed`）+ 1 个记录事件（`Recorded`）+ 1 个消费事件（`ModifierConsumed`）+ 模板 id。

每个 `DevComment` 都写全三件事：「语义 / 由谁声明谁消费 / （若为验证词）退役判据」——`ModifierConsumed` 那条还写明「**本批只声明形状不发布**」（`:44`）。**这是本仓 tag 治理的标配形态**，值得作为模板。

### 2.10 官方默认模板："四步组装"是数据，不是硬编码分支【实测】

`TcsDamageSubsystem.cpp:62-72`：模板在 `Initialize` 里组装成 `FTcsFlowTemplate` 再走 `RegisterTemplate`——**与宿主登记走同一个口**，因此"可整表替换"不是承诺，是结构性成立（`damage-step-library:106`）。

DB 实测的一个细节：**默认模板不含 `Hit` / `Crit` 步**，因此 `bHit` / `bCrit` 恒 `false`。这在 `TcsDamageRecord.h:68-76` 与 `TcsFlowStepsCore.cpp:268-274` 各有注释，规格 `damage-primitive:163-165` 也写了 Scenario。**三处一致，无歧义**。

### 2.11 记录浏览命令：`Tcs.Damage.DumpRecords` 的字段与规格**逐项对上**（除一项）【实测】

`damage-record-inspection` 只有 1 条需求 / 4 个 Scenario，是全 44 个能力里最小的之一。逐项核对：

| 规格要求（`:10-15`） | 代码（`TcsDamageSubsystem.cpp:338-398`） |
|---|---|
| 默认 10、钳到 `[1, 容量]` | ✅ `:348-353` |
| 每行含 序号/FlowId/源/目标/Base/Final/差额/Executed/Absorbed/暴击/击杀/**时刻** | ❌ **缺"时刻"** —— `:383-387` 的 `Printf` 没有 `Timestamp` |
| 差额 = `Final - Base` | ✅ `:381` |
| `LogTcsDamage` `Display` + 屏显各一份 | ✅ `:389-394` |
| 空缓冲明确提示 | ✅ `:359-364` |
| MUST NOT 逐键归因 | ✅ 只打差额 |

⇒ **一项缺失**，见 D-10。

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

| 契约 | 符号 | 消费者 | 稳定性依据 |
|---|---|---|---|
| 两个链原语 | `FTcsStepDamage` / `FTcsStepModifyFlow` | 内容资产（`FInstancedStruct`）、装置 | `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册，`TcsEffect` 零改动 |
| 黑板读写面 | `FTcsFlowAttributes::Submit` / `Read` / `FindSubmits` | **宿主自研步骤**（装置 `TcsDevFlowSteps.cpp` 就是这么用的） | 头注释 `:81-83` 明写"无导出宏会 `LNK2019`"——**已验证过** |
| 9 个黑板契约键 | `Tag_DamageFlowKey_*` | 宿主步骤、装置、C#（`GameplayTags.cs:22-31` 有 10 个常量） | 原生声明 + 导出宏 |
| 3 个模板 id | `Tag_DamageFlowTemplate_Default` | 官方默认 + 宿主 | 同上 |
| 11 个事件 tag | `Tag_TcsEvent_Damage_*` | 触发行（载荷读取器）、装置屏显（`TcsDevScreenObserver.cpp:63`） | 同上 |
| 宿主委托 | `ITcsDamageFlowDelegate`（7 方法） | 宿主 C++ / C#（glue 实测有 7 个 `public` 方法） | 全 `BlueprintNativeEvent` + 中性默认体 |
| 视图 | `FTcsDamageFlowContextView` | 宿主脚本 | `USTRUCT(BlueprintType)`，7 个 `BlueprintReadOnly` |
| 记录 | `FTcsDamageRecord` | 订阅 `Recorded` 的消费者 | `USTRUCT(BlueprintType)` + 全扁平 |
| 记录读数 | `GetRecentRecords` | 装置（`TcsDevSliceRig.cpp:647`） | 非 `UFUNCTION`（见 D-9） |

### 3.2 MUST NOT 依赖（内部实现细节）【实测】

| 项 | 为何不可依赖 |
|---|---|
| `FTcsDamageFlowContext` 整体 | **纯 C++ struct，非反射**——不能作 `UFUNCTION` 形参，不能序列化，**MUST NOT 跨帧持有**（`:22`） |
| `FTcsFlowAttributes::Submits` 存储形状 | `TMap<tag, TArray<提交>>`——嵌套容器，UHT 不可反射。`FindSubmits` 返回的是**只读指针**，持有它会随下一次 `Submit` 失效（`TArray` 扩容） |
| `Context.Owner` | 弱引用，随世界销毁失效。脚本侧 MUST 走自身 `GetWorld()`（`TcsDamageFlowContextView.h:33-34`） |
| `FTcsFlowStepExecutorRegistry` 是进程级单例 | 只有 `Find` 带世界校验；`Register` 的拒绝门也是世界敏感的——**跨世界直接登记会静默替换** |

### 3.3 一条与 `ARCH-09` 对照出来的结构差异：**本模块"存在可观测"，但"登记不可枚举"**【实测】

`ARCH-09` 的 `TcsTargeting` 因为零 `UCLASS`，运行期只能靠别人活着。本模块相反：`UTcsDamageSubsystem` 是标准 `UWorldSubsystem`，`DoesSupportWorldType` 收窄到游戏世界（`TcsDamageSubsystem.cpp:75-81`），`GetSubsystem` 随时可问。

**但它的两张表达面都不可枚举**：

| 想问什么 | 能问吗 | 缺口 |
|---|---|---|
| 登记了哪些模板？ | ❌ | 无 `GetTemplateIds()` / 无迭代口；`FindTemplate` 要求先知道 id |
| 注册了哪些步骤执行器？ | ❌ | `FTcsFlowStepExecutorRegistry` 只有 `GetDynamicKeys()`（只含动态项），静态自注册项在私有 `Executors` 里 |
| 这个步骤类型注册了吗？ | ✅ | `Find` |

⇒ 与台账 `TOOLS-6` 记的「`FTcsStepModifyFlow` 的跨模块自注册只能**反证**」**同一条**。已登记，见 U-4。

### 3.4 一条反直觉的边界：**`Context.Owner` 是步骤取世界的唯一通路，而它不在反射视图里**【实测】

`FTcsDamageFlowContext::Owner`（`:107`）的类型是 `TWeakObjectPtr<UTcsDamageSubsystem>`。规格 `damage-primitive:111` 明确把它排除在视图外，理由给的是「脚本层经自身 `GetWorld()` 取门面」。

**这条理由在脚本步骤（`UTcsFlowStepExecutor`）上成立**——它是 `UObject`，有 `GetWorld()`。
**但它在"宿主 C++ 自研步骤"上也成立**——因为 C++ 步骤拿到的是**完整的** `FTcsDamageFlowContext`，能直接读 `Owner`。

⇒ 视图里放不放 `Owner` 是正确取舍。**代价是：脚本步骤要发收集事件（`PublishCollectEvent`），就必须先 `GetWorld()->GetSubsystem<UTcsDamageSubsystem>()`**——而 `PublishCollectEvent` **没有 `UFUNCTION`**（`TcsDamageSubsystem.h:167` 是裸的 `void`），glue 里也没有它。见 D-9。

### 3.5 一处刻意为之的"重复字段名"：`FTcsFlowPreExecute::CandidateKey` 与 `FTcsFlowExecute::CandidateKey`【实测】

两个 struct 都有 `CandidateKey`（`TcsFlowSteps.h:128` / `:150`），语义是「⑧ 收集落点 ↔ ⑨ 裁决读取」。**名字相同是必要的**——它们必须配对，配错则候选永远读不到。

但代码里**只有 ⑨ 读这个字段**：

```
TcsFlowStepsCore.cpp:131   const FGameplayTag CandidateKey = Step->CandidateKey.IsValid() ? … // ⑨ 读
TcsFlowStepsRest.cpp:169   // 候选（免疫/减伤）落进 Step->CandidateKey——⑨ 按 SortKey 裁决、成功才消费
```

⑧ 的 `CandidateKey` **一次都没被读过**——它只发事件，落点由**响应方自己决定往哪个键提交**。⇒ 见 D-5。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

```
内容资产（链）──► TcsEffect 解释器 ──► FTcsStepDamage 执行器（住本模块）
                                          │
                                          ├─ 取 UTcsDamageSubsystem（经 Run.Owner → World）
                                          ├─ 构 FTcsDamageFlowContext（Caster/Instigator/Targets）
                                          └─ RunTemplate ──► 十步标准库
                                                              │
                              ┌───────────────────────────────┼──────────────────┐
                              ▼                               ▼                  ▼
                     UTcsAttributeSubsystem            UTcsEventBusSubsystem   UTcsClockSubsystem
                     （Execute 扣血）                  （11 个收集事件）        （记录时刻）
                                                              │
                                                              ▼
                                                     触发行 → 单步链 [ModifyFlow]
                                                              │
                                                              └──► 回写黑板（同一上下文）
```

**两条方向相反的回边**（这是本模块竖切最重要的形状）：

1. **正向**：`FTcsStepDamage` → `RunTemplate` → 十步 → 扣血；
2. **反向**：收集事件（`PublishCollectEvent`）→ 总线立即通道 → 触发行 → `FTcsStepModifyFlow` → **回到同一个 `FTcsDamageFlowContext` 的黑板**。

**第 2 条回边依赖一个裸指针**：`FTcsDamageFlowCollectEvent::Context`（`TcsDamageFlowCollectEvent.h:69`）。这是 `ARCH-06` 的 **D-1**（跨模块载荷生命周期契约缺位）在本模块的落点——`TcsStepModifyFlow.h:29-32` 把危险写成了明文 MUST NOT：

> **硬约束：只能用于同步单步链**——链上下文里携带的是流程上下文的**裸指针**…链若在本步之前挂起，流程早已结束、指针悬空 ⇒ 本原语 MUST NOT 出现在含挂起步骤的链上。

⇒ **两端各写了一份正确契约，中间那条"谁保证同步"没有任何机制**。若宿主在 `PreExecute` 响应里起一条含 `WaitDelay` 的链，`ModifyFlow` 会用悬空指针写黑板。**这不是本模块独有的疏漏**（`ARCH-06` 已作为跨模块缺口登记），但**本模块是唯一能在代码里制造这个悬空的地方**——因为它持有 `&Context`（`TcsDamageSubsystem.cpp:279`）。

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

| # | 时序事实 | 锚点 | 后果 |
|---|---|---|---|
| 1 | `CollectStart` **重置黑板**在设计上先于 `BaseDamage` | `TcsFlowStepsCore.cpp:76` | 链侧预写的黑板值**全部丢失**；输入只能走 `BaseDamageInput` |
| 2 | `ClassificationTags` **只有 `Element` 步写**，且 `CollectStart` 不清它 | `TcsFlowStepsRest.cpp:139` / 上下文无清理点 | **跨流程污染**：上下文是每次流程新建（`TcsStepDamage.cpp:44`）⇒ 实际安全，但**没有任何一行代码保证"新建"** |
| 3 | `Element` 步在默认模板里**不存在** | `TcsDamageSubsystem.cpp:64-67` | `FlowStarted` / `PreHit` / `Hit` / `Crit` 事件上按分类匹配**恒不通过**（注释 `TcsDamageFlowCollectEvent.h:57-61` 已写明） |
| 4 | `Execute` 读黑板**早于**写 `Executed`/`Absorbed` | `TcsFlowStepsCore.cpp:132` vs `:213-214` | `Completed` 读到的必然是本次结果；但**宿主在 `PreExecute` 里提交 `Executed` 会被覆盖** |
| 5 | 记录在 `Completed` 步**每个目标一条** | `:262-284`（`for Target` 里 `AppendRecord`） | 多目标时**一次流程产生 N 条记录**，`Sequence` 各不相同但 `FlowId` 相同 |
| 6 | `FlowSource` 在 `RunTemplate` 起手分配 | `TcsDamageSubsystem.cpp:227` | 每流程一个，但**全仓无人用它级联摘除**（见 D-3） |

**第 5 条值得单独展开**：`damage-primitive:150` 的 Scenario 写「环形缓冲可读回**同一笔记录**」（单数），而代码多目标时写 N 笔。规格里**没有一处**提到多目标记录数——见 D-6。

### 4.3 一条竖切级的教训：**"每目标一条记录"与"流程级单次映射"混在同一个函数里**【实测】

`ExecuteFlowExecute` 里三个"逐目标"与两个"流程级"交错：

| 量 | 粒度 | 锚点 |
|---|---|---|
| `ModifyShield` | **逐目标求和**（`Absorbed += `） | `:159-163` |
| `ResolveExecutedDamage` | **流程级一次**（只传 `Targets[0]`） | `:172-179` |
| 扣血 `SetBaseValue` | **逐目标** | `:192-205` |
| `IsLethal` | **逐目标，任一为真即 `bKill`** | `:224-235` |
| `Kill` 键 | **流程级单值** | `:238` |

⇒ **`Executed` 是流程级单值，却要分摊到 N 个目标各自的扣血**（`:201`：每个目标都减同一个 `Executed`）。3 个目标、`Executed=10` ⇒ **总扣血 30**。代码注释 `:170-171` 说「`Absorbed` 本就是全目标之和 ⇒ 本映射是**流程级**的一次取值」——**理由成立，但它没说"执行量因此被重复施加到每个目标"**。

见 D-6。

### 4.4 跨模块：本模块是 `Arch-06 D-1` 的唯一制造者，也是 `TcsDev` 的最大客户【实测】

| 关系 | 模块 | 性质 |
|---|---|---|
| 被 `TcsEffect` 注册表托管 | `TcsEffect` | 两个执行器自注册进去，`TcsEffect` 不认识任何本模块类型 |
| 向 `TcsEffect` **注入**裸指针 | `TcsEffect`（总线） | 唯一跨模块的风险面（4.1 的回边） |
| 依赖 `TcsAttribute` 的事务 | `TcsAttribute` | `BeginBatch / EvaluateCurrent / SetBaseValue / Commit` |
| 被 `TcsDev` 重度使用 | LAC 装置 | 3 条模板 + 2 个自研步骤 + 1 个公式 + 1 个屏显 + 记录读数 |
| 被 `TcsIntegration` 声明 | — | **零代码引用** |

---

## §5 技术债（期望 vs 实际，逐条带证据）

### D-1 ★★★ `Hit` / `Crit` 两步用 `> 0.0` 当"有无提交"的哨兵，于是**宿主无法表达"命中率 0"与"暴击率 0"**【实测】

**期望**：`FTcsFlowHit` / `FTcsFlowCrit` 的设计是「delegate 给基础率 → 收集事件让修改器改写 → 折算成 `Hit` / `Crit` 键」。规格 `damage-primitive:169-170` 明写 Scenario：

> **WHEN** 模板组装了 `FTcsFlowHit` 步且宿主响应方把 `DamageFlowKey.HitRate` 改写为 `>= 1.0`
> **THEN** `bHit = true`；未达 1.0 时 `bHit = false`

**实际**（`TcsFlowStepsRest.cpp:87-89`）：

```cpp
const double Modified = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_HitRate)) > 0.0
    ? Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_HitRate))
    : HitRate;
SubmitFlowRestOverride(Context.Blackboard, FGameplayTag(Tag_DamageFlowKey_Hit), Modified >= 1.0 ? 1.0 : 0.0);
```

`HitRate` 的合法值域是 `[0,1]`（`TcsDamageFlowDelegate.h:60` 的注释：「基础命中率（[0,1]）」）。**`0` 在值域内**，但这里把它当成了"没有人提交过"的哨兵。

**偏差的完整链条**：

| 宿主想表达 | 提交 | `Read` 返回 | `> 0.0` | `Modified` | 结果 |
|---|---|---|---|---|---|
| "这一击必定落空" | `HitRate` = 0 | 0.0 | false | **回落到 delegate 的 1.0** | **`bHit = true`（必中）** |
| "把命中率压到 0"（致盲/无敌） | `TAO_Mul` 0 | 0.0 | false | 同上 | **必中** |
| 不提交 | — | 0.0（折叠初值） | false | delegate 值 | 正确 |

⇒ **"提交了 0"与"没提交"完全不可区分，且失败方向是"更宽松"**。宿主越是努力把命中率压到 0，越会得到必中。**症状与"正常命中"一模一样**，无任何日志。

**同一个文件里有一份正确的对照**：`ExecuteFlowExecute` 读候选用 `FindSubmits`（`TcsFlowStepsCore.cpp:137`）——它区分"键存在但数组空"与"键不存在"，`BestIndex` 用 `INDEX_NONE` 哨兵而非值域内的值。⇒ **正确手法在同模块内已有先例，只是没回头改这两步**。

**影响面**：所有带 `Hit` / `Crit` 步的模板。默认模板不含这两步（`TcsDamageSubsystem.cpp:64-67`），所以**今天的装置跑不到**——它只在宿主组装了 `Hit` 步之后才发作，此时排障成本极高（读数正确、无告警、逻辑反直觉）。

**是否已登记**：❌ 否。台账 `DAMAGE-4` 记的是"消费动作未实现"，与本条无关。

**修法（一行级）**：把哨兵从"值 > 0"改成"键有提交"——

```cpp
const TArray<FTcsFlowAttributeSubmit>* Submits = Context.Blackboard.FindSubmits(HitRateKey);
const double Modified = Submits ? Context.Blackboard.Read(HitRateKey) : HitRate;
```

---

### D-2 ★★★ 规格的十步契约在 2026-09-22 的一次**无关改造**里被整段删除，而标题、Purpose、正文首句与默认模板节**共四处**仍宣称"十步/十阶段"【实测】

**期望**：`damage-step-library` 是十步行为的唯一权威（"规格是契约层"），每步"做什么 / 写哪个键 / 失败怎么办"应当可被 `validate` 与人工对账。

**实际**：生效规格正文里**没有任何一步的逐条契约**。

**证据一（曾经存在）**：`git show a1804ed:openspec/specs/damage-step-library/spec.md` 有十行逐步骤定义（含 `Execute` 的「成功才消费」与「两者皆空 → 不扣血 + Warning」）。

**证据二（何时消失）**：`git show 539b14e -- openspec/specs/damage-step-library/spec.md` ——**17 insertions / 25 deletions**，那十行被替换成一段「执行期行为契约」泛论。该提交的主题是「标识体系 tag 化改造」（`FName` → `FGameplayTag`）。

**证据三（为什么没被发现）**：那次改造的每个文件都要改键名；这个文件里**每一行都提到键名**，于是整段被当作"待改写内容"重写。重写后的版本只保留了与 tag 有关的部分（"键为何原生声明"、"不许裸字面量"），**步骤行为本身没有落回来**。`openspec validate --strict` 只校验 Requirement / Scenario 结构——**它看不见"正文变短了"**。

**证据四（残留的四处"十"）**：

| 位置 | 原文 |
|---|---|
| `Purpose`（`:4`） | 定义伤害流程的标准步骤库（**十步**）… |
| Requirement 标题（`:8`） | 标准步骤库（**十步**） |
| 正文首句（`:12`） | `TcsDamage` MUST 提供**十阶段**标准步骤库… |
| 默认模板节（`:106`） | 标准**十阶段**只是插件自带的标准件… |

⇒ **规格在四处承诺了十步，却在正文里一步都没写**。这不是"规格更松"（`ARCH-04` 的方向），也不是"引用滞留"（`ARCH-05`）——它是**结构还在、内容被删空**。

**为什么这条排 ★★★**：它是本模块其余几条规格债的**母债**。D-4（`CandidateKey` 零读取）、D-5（多目标无规格）之所以只能靠"读代码对比旧提案"才判得出，**正是因为规格里没有可被违反的条目**。

**同时订正一处台账归因**：`DAMAGE-4` 行写「**归档时规格被裁**——当前生效的 `damage-step-library/spec.md` 的「标准步骤库（十步）」只剩概括段」。归档发生在 **2026-09-21**（`add-tcsdamage-steps-and-primitive`，归档后规格 `a1804ed` **仍有十行**），删除发生在 **2026-09-22**（`539b14e`）。⇒ **时点记错了**，这直接影响"该谁负责修"：不是归档流程的问题，是**tag 化改造的单文件失手**。

**是否已登记**：🟡 部分——`DAMAGE-4` 记了症状（"十步逐条行为契约整体丢失"），但归因（时点 + 成因）错误。

---

### D-3 ★★ `FlowSource` 的三处注释都写着"流程结束 `RemoveBySource` 级联摘除"，而**全仓对它的使用是"分配后就再也没人看"**【实测】

**期望**（`TcsDamageFlowContext.h:69-71`）：

> 每流程唯一的来源锚点（`RunTemplate` 起流程时分配；作用域修改器挂 M2 账本时以它为 Source，**流程结束 `RemoveBySource` 级联摘除**——09 §2.1/§3）

**实际**：

```
$ grep -rn "FlowSource" Source/TcsDamage
TcsDamageSubsystem.cpp:227   Context.FlowSource = FlowSourceRegistry.Allocate();   // 分配
TcsDamageSubsystem.cpp:232-233   UE_LOG(... "FlowSource=%llu", ...);               // 打日志
TcsDamageSubsystem.cpp:287-288   UE_LOG(Verbose, ...);                              // 打日志
TcsFlowStepsCore.cpp:265     Record.FlowId = Context.FlowSource.Id;                // 记录里当 FlowId
TcsDamageFlowContext.h:71    FTcsSourceHandle FlowSource;                          // 声明
```

**`RemoveBySource` 调用点：0**。`grep -rn "RemoveBySource" Source/TcsDamage` 只命中两处**注释**（`TcsDamageSubsystem.cpp:226` / `TcsDamageFlowContext.h:70`）。

⇒ `FlowSource` 的实际用途只有一个：**当 `FTcsDamageRecord::FlowId` 的取值**（`TcsFlowStepsCore.cpp:265`）。"级联摘除锚点"这层语义**从未被实现，也从未有对象挂上去**——因为整个设计里的挂账动作（"作用域修改器挂 M2 账本时以它为 Source"）本身不存在（`AttrCapture` 未落地，见 U-3）。

**规格侧**：四份能力规格里**完全没有对 `FlowSource` 的任何需求**——只有 `damage-primitive:112` 说「视图 MUST NOT 含 `FlowSource`」（因为"没有脚本消费场景"）。

⇒ **注释承诺了一个规格从未要求的机制**，而且这句承诺写得像既成事实。这是 `G-14`（注释里的契约无人校验）的**第二个形态**：不是"契约未实现"，是"**注释描述了一个不存在的行为**"。

**影响面**：任何读这份头注释的人都会以为"起流程 = 自动清理作用域修改器"，从而**不给自己的挂账写清理**。今天无害（因为挂账本身不存在），一旦 `AttrCapture` 或"本次攻击 +10%"落地，就会变成**真实的泄漏**。

**是否已登记**：❌ 否。台账 `WAIT-7` 记的是 `AttrCapture` 整套未落，没有记 `FlowSource` 的注释与实现脱节。

---

### D-4 ★★ `FTcsFlowPreExecute::CandidateKey` **一次都没被读过**——⑧ 配了不生效，⑨ 读的是自己的字段【实测】

**期望**：`TcsFlowSteps.h:126-128` 写：

> 候选落点键（默认 `ExecuteCandidates`——**⑨ 裁决步骤按此读回**）

而 `FTcsFlowExecute::CandidateKey`（`:148-150`）写：

> 免疫/减伤候选键（默认 `ExecuteCandidates`——**与 ⑧ 配对**）

⇒ 两处注释都暗示"⑧ 写进 ⑧ 的键，⑨ 从 ⑨ 的键读"，**即两个字段必须一致才能工作**。

**实际**：⑧ 的执行器（`TcsFlowStepsRest.cpp:161-172`）**只发事件，从不读 `Step->CandidateKey`**：

```cpp
// 候选（免疫/减伤）落进 `Step->CandidateKey`——⑨ 按 SortKey 裁决、成功才消费
BroadcastFlowRestCollect(Context, Tag_TcsEvent_Damage_PreExecute);
```

那句注释**描述了它没有做的事**：候选落进哪个键，**完全由订阅方（宿主/触发行）自己决定**——响应方拿到 `FTcsDamageFlowCollectEvent` 后自行 `Submit(某键, …)`，⑧ 无从干涉。

⇒ 三个后果：

1. **⑧ 的 `CandidateKey` 是死字段**（配了不生效，且**没有日志**）；
2. **"配对"是配置纪律而非机制保证**——⑨ 配 `A` 而订阅方写 `B`，候选静默丢失（`FindSubmits` 返回 nullptr）；
3. `BestIndex` 本身**赋值后无使用点**（台账 `DAMAGE-4` 已记）——所以第 2 条今天还看不出来。

**是否已登记**：🟡 半——`DAMAGE-4` 记了「`BestIndex` 赋值后无任何使用点」，**没记"⑧ 的 `CandidateKey` 零读取"**。两者是同一条通道的两端，应当合并登记。

---

### D-5 ★★ 多目标语义**从未被写下**，而代码里同时存在"每目标一条记录"「`Executed` 重复施加」与"流程级单次映射"【实测】

**期望**：规格对 `Context.Targets` 只有一句（`damage-primitive:18`）：「`Targets` 取 `Context.Targets`——**消费目标集，不内嵌选择器**」。**没有任何一处**说明多目标时：记录产出几条？`Absorbed` / `Executed` / `Kill` 是每目标还是流程级？扣血量是"每个目标各扣 `Executed`"还是"总量分摊"？

**实际**（`TcsFlowStepsCore.cpp`）：

| 量 | 粒度 | 锚点 | 冲突面 |
|---|---|---|---|
| 记录 | **每目标一条** | `:262-284` | 规格 `damage-primitive:150` 的 Scenario 写「可读回**同一笔记录**」（单数） |
| `ModifyShield` | 逐目标求和 | `:159-163` | — |
| `ResolveExecutedDamage` | **流程级一次**，只传 `Targets[0]` | `:172-179` | 与"逐目标求和"的 `Absorbed` 语义错配 |
| 扣血 | **逐目标各减 `Executed`** | `:192-205` | ⇒ **N 个目标总扣 `N × Executed`** |
| `IsLethal` | 逐目标，**任一为真即 `bKill=true`** | `:224-235` | 与"每目标一条记录"冲突：**所有目标共享同一个 `bKill`** |

**最刺眼的一处**：`Executed` 与 `bKill` 是**流程级单值**，而记录是**每目标一条**。于是

- 目标 A 死、目标 B 没死 ⇒ **A 与 B 两条记录的 `bKill` 都是 `true`**；
- 3 个目标、`Executed=10` ⇒ 每个目标各扣 10（总 30），而每条记录的 `Executed` 都写 10。

代码注释 `:170-171` 承认了一半：「多目标时与 `BaseDamage` 步同款取首个目标：`Absorbed` 本就是全目标之和 ⇒ 本映射是**流程级**的一次取值，不是逐目标取值」——**它论证了映射粒度，但没有回答"这个执行量要不要在每个目标身上各施加一次"**。

**影响面**：默认模板四步里 `Execute` 与 `Completed` 都在，**任何多目标链（`SelectTargets` + `Damage`，本仓主力形态）都会走到这条未定义路径**。今天装置只跑单目标（`Self` 选择器），所以**没暴露**。

**是否已登记**：❌ 否。台账无任何多目标条目。

---

### D-6 ★★ 规格把"脚本步骤修正流程值"的落点指向 `PublishCollectEvent`，而**那个口没有 `UFUNCTION`**——脚本打不到【实测】

**期望**：四处承诺了这条逃生路：

| 位置 | 原文 |
|---|---|
| `TcsFlowStepExecutorObject.h:29-31` | 「脚本步骤若要修正流程值，MUST 走收集事件协议（`UTcsDamageSubsystem::PublishCollectEvent` + 响应内 `Submit`）」 |
| `TcsDamageFlowContextView.h:38-39` | 「宿主若要修正流程值，走收集事件协议提交黑板修正」 |
| `TcsDamageFlowContext.h:120-121` | 「宿主修正流程值走收集事件协议的 `Submit`」 |
| `TcsDamageSubsystem.cpp:194` | 「视图是只读投影——脚本步骤要修正流程值走收集事件协议的 `Submit`」 |

**实际**：`UTcsDamageSubsystem` 只有 **3 个 `UFUNCTION`**：

```
$ grep -n "UFUNCTION()" Source/TcsDamage/Public/TcsDamageSubsystem.h
87:    bool RegisterTemplate(const FTcsFlowTemplate& Template);
98:    bool UnregisterTemplate(FGameplayTag TemplateId);
122:   bool RegisterStepExecutor(const UScriptStruct* StepStruct, UTcsFlowStepExecutor* Executor);
```

`PublishCollectEvent`（`:167`）与 `GetRecentRecords`（`:189`）都是**裸 C++ 方法**；glue 产物 `TcsDamageSubsystem.generated.cs` 里也只有 3 个 `public` 方法（`:81` / `:112` / `:156`，实测）。

⇒ **脚本步骤要"修正流程值"，唯一的正规通路是它打不到的那个口。**

**脚本步骤实际还剩什么能力**：

| 能力 | 可达？ |
|---|---|
| 读上下文（参与者 / 目标 / 公式参数 / 分类 / 请求字段） | ✅ 视图 |
| 返回 `false` 中止流程 | ✅ |
| 向黑板提交修正 | ❌ 无口 |
| 发收集事件（让别人的响应去提交） | ❌ 无口 |
| 读记录 | ❌ 无口 |

⇒ 规格 `damage-flow:135-136` 的 Scenario 写「流程行为由脚本决定（含返回 false 中止流程）」——**"由脚本决定"实际只剩"改自己的返回值"**。

**是否已登记**：❌ 否。台账 `SCRIPT-8` 记的是"插槽形状"，未记这个洞。

---

### D-7 ★★ `Deinitialize` 撤销**全部**动态条目，而规格要求"**本世界**"——且与 `TcsEffect` 的同款实现**逐字相同**（同款缺陷复制）【实测】

**期望**（`damage-flow:116`）：

> **`Deinitialize` 收口**：门面 MUST 撤销**本世界**登记的动态条目…

**实际**（`TcsDamageSubsystem.cpp:95-99`）：

```cpp
for (const UScriptStruct* DynamicStepStruct : FTcsFlowStepExecutorRegistry::Get().GetDynamicKeys())
{
    FTcsFlowStepExecutorRegistry::Get().Unregister(DynamicStepStruct);
}
```

`GetDynamicKeys()`（`TcsFlowStepExecutor.cpp:84-95`）遍历的是 **`Lifetimes` 全表**——不限世界。⇒ **任何一个世界先退出，就会把另一个世界仍有效的动态登记一起撤销。**

**这不是本模块独有的**：`TcsEffectSubsystem.cpp:42-61` 的写法**逐字相同**（三张注册表各一段同款循环）。`TcsEffect` 的规格与它自己的实现同样不匹配。

**影响面**：单 PIE 会话无感。**双世界（客户端 + 服务器 PIE，或 `Game` + `GamePreview` 并存）时**，先退出的那个会把后者的宿主执行器登记清空 ⇒ 表现为"步骤类型写漏"（`RunTemplate` 走 Error 中止），而**真正的原因是另一个世界退出了**。规格 `:113` 本该警告的正是这一形态：「MUST NOT 静默按'未登记'处理（否则'世界已更换'会被表现成'步骤类型写漏'）」——**规格自己预言了这个症状，却在自己的 `Deinitialize` 条款上犯了它。**

**是否已登记**：❌ 否。

---

### D-8 ★ `Tcs.Damage.DumpRecords` 少打印规格要求的"时刻"字段【实测】

**期望**（`damage-record-inspection:11`）：每行 MUST 至少含「序号 / `FlowId` / 源句柄 / 目标句柄 / Base / Final / 差额 / `Executed` / `Absorbed` / 暴击 / 击杀 / **时刻**」。

**实际**（`TcsDamageSubsystem.cpp:383-387`）：`Printf` 的形参表里**没有 `Record.Timestamp`**——

```cpp
TEXT("  #%-4lld %-5lld #%-5lld #%-5lld %-9.3f %-9.3f %+-9.3f %-9.3f %-9.3f %-4d %d"),
Record.Sequence, Record.FlowId, Record.Source.Id, Record.Target.Id,
Record.Base, Record.Final, Delta, Record.Executed, Record.Absorbed,
Record.bCrit ? 1 : 0, Record.bKill ? 1 : 0);
```

表头（`:371`）同样没有"时刻"列。

⇒ 规格 4 个 Scenario 里没有一个覆盖"每个字段都在"（`Scenario: 打印最近记录` 只写「每行含 Base / Final / 差额 / Executed **等**字段」——**"等"字把这条要求放行了**）。

**修法**：`Printf` 加一个 `%8.3f` 与对应形参，表头加「时刻」列。**约 3 行**。

**是否已登记**：❌ 否。

---

### D-9 ★ 记录时刻在时钟门面不可得时**静默写 0.0**，无日志【实测】

```cpp
// TcsFlowStepsCore.cpp:259-260
const UTcsClockSubsystem* Clock = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UTcsClockSubsystem>() : nullptr;
const double Now = Clock ? Clock->GetClock().Elapsed : 0.0;
```

同一函数里的另一个降级有日志（`:253-256`：门面不可得 → Warning「记录未产出」），而**时钟不可得时完全静默**，记录照样产出、`Timestamp` 恒 0。

⇒ 读者看到 `Timestamp = 0.000` 时的第一反应是"第一帧的伤害"，而不是"时钟门面挂了"。

与 `ARCH-09` 的 **G-25**（"降级后与正常路径不可区分者 MUST 至少一条 Warning"）同型。**本模块内还有两处同型**（D-1 的"必中"、D-4 的死字段）。

**是否已登记**：❌ 否。

---

### D-10 ★ 规格把记录的两个字段写成 `uint64`，代码是 `int64`（UHT 约束），规格未订正【实测】

**规格**（`damage-primitive:137`）：`uint64 FlowId` / … / `uint64 Sequence`。
**代码**（`TcsDamageRecord.h:44` / `:108`）：`int64 FlowId` / `int64 Sequence`，注释 `:42` 写明理由：「`UHT 不支持 uint64 作属性类型——句柄 Id 恒为正，无符号性差异无实际影响」。

⇒ **代码是对的（有理由），规格是错的**。这是 `ARCH-04` 以来又一次「规格写了一个物理上做不到的形状」。

**是否已登记**：❌ 否。

---

### D-11 ★ 两处注释仍写"项目键自由 `FName`"，而 tag 化已过去近两个月【实测】

| 位置 | 原文 |
|---|---|
| `TcsFlowStepsCore.cpp:22` | 「黑板保留键（…项目自定义键自由 **FName**）」 |
| `TcsFlowAttributes.h:78` | 「键 = 项目词表（…项目键自由 **FName**，无注册表）」 |

而**同一批代码里所有键都是 `FGameplayTag`**，规格 `damage-flow:66` 也写对了（「宿主自定义键…类型同为 `FGameplayTag`」）。

⇒ 与 `ARCH-07` 的 G-18（一次改名没有单一规则）**同族但更轻**：不是改名遗漏，是**注释在改名时没被扫到**。两处都在"解释设计意图"的长注释里——**越是解释性的注释，越容易在改造后被遗漏**。

**是否已登记**：❌ 否。

---

### 债务小结

| # | 一句话 | 档 | 类型 | 是否已登记 |
|---|---|---|---|---|
| **D-1** | `HitRate`/`CritRate` 用 `> 0.0` 当空值哨兵 ⇒ 无法表达"命中率 0"，且失败方向是"必中" | ★★★ | **纯代码** | ❌ |
| **D-2** | 规格十步契约在 tag 化改造里被整段删除，四处仍宣称"十步" | ★★★ | **纯规格** | 🟡 归因错 |
| **D-3** | `FlowSource` 注释承诺"流程结束级联摘除"，`RemoveBySource` 调用点 = 0 | ★★ | **注释 vs 代码** | ❌ |
| **D-4** | ⑧ 的 `CandidateKey` 零读取，注释却写"⑨ 按此读回" | ★★ | 双方 | 🟡 半 |
| **D-5** | 多目标语义（记录条数 / `Executed` 重复施加 / `bKill` 共享）从未被写下 | ★★ | 纯规格 | ❌ |
| **D-6** | 脚本步骤"修正流程值"的正规口 `PublishCollectEvent` 无 `UFUNCTION` | ★★ | 双方 | ❌ |
| **D-7** | `Deinitialize` 撤销全部动态条目 vs 规格"本世界"，且与 `TcsEffect` 逐字相同 | ★★ | 纯代码 | ❌ |
| **D-8** | `DumpRecords` 少打印规格要求的"时刻" | ★ | **纯代码** | ❌ |
| **D-9** | 记录时刻在时钟不可得时静默 0.0 | ★ | **纯代码** | ❌ |
| **D-10** | 规格记录字段 `uint64` vs 代码 `int64` | ★ | 纯规格 | ❌ |
| **D-11** | 两处注释仍写"项目键自由 `FName`" | ★ | **注释滞留** | ❌ |

**分布**：**纯代码 4 条**（D-1 / D-7 / D-8 / D-9）、**纯规格 3 条**（D-2 / D-5 / D-10）、**双方 2 条**（D-4 / D-6）、**注释类 2 条**（D-3 / D-11）。

⇒ **这是九篇以来第一次"纯代码 ≥ 纯规格"**（前八篇里只有 `ARCH-08` 接近，而它方向相反）。成因也不同：`ARCH-08` 是**规格单方面掉队**，本模块是**规格被人为掏空之后，代码失去了对照物，于是偏了也没人发现**——D-1 与 D-7 都属于"规格里没有这一条，所以谁也没去查"。

---

## §6 未实现（设计是否还成立 + 前置条件）

### U-1 ★★ `Heal` 执行器（台账 `DAMAGE-3` 余项）——**设计仍然成立，但依据已经变了**【实测+推断】

**缺什么**：`FTcsStepDamage` 有对应的治疗步骤吗？**没有**。全仓 `Heal` 相关：`grep -rn "Heal" Source/` 只命中 `Health` 这个词的一部分（`TcsStepDamage.h:45` 一类注释），**无 `FTcsStepHeal` 类型、无执行器**。

**旧设计怎么说**（`SPEC-08-damage` §2.4 `:87`）：「治疗流程：同骨架精简版（无 Hit/免疫；Crit 可选），`IHealFlowDelegate.GetBaseHeal`」。

**设计还成立吗**：🟡 **一半**。

- 成立的部分：**"同骨架"** 这个判断是对的——`RunTemplate` + 十步库 + 黑板 + 记录全部可复用，治疗确实只需一个链原语 + 一份模板；
- 不成立的部分：**`IHealFlowDelegate` 该不该是一个独立的接口**。旧设计定这条时，`ITcsDamageFlowDelegate` 只有 5 个方法；今天它有 7 个，其中 `ModifyShield`（护盾吸收）与 `ResolveExecutedDamage`（吸收→执行量）**对治疗同样适用**（"治疗被减疗吸收"完全同构）。⇒ **独立接口会重复这两个方法**，而共用接口又要把"伤害"这个域名词留在里面。

**前置条件**：先裁定"治疗是共享 `ITcsDamageFlowDelegate` 还是独立接口"。这条裁不掉，`Heal` 就落不了。

**台账状态**：`DAMAGE-3` 余项，归属 `R5.5-b`。**记录准确**。

---

### U-2 ★★ 消耗动作（台账 `DAMAGE-4`）——**形状已就位，动作零实现，且"成功才消费"的规格已消失**【实测】

**缺什么**：`FTcsFlowExecute` 算出 `BestIndex`（`TcsFlowStepsCore.cpp:135-147`）后**直接丢弃**——无"标记已消费 / 扣 `MaxUses` / 起 `Cooldown` / 发 `ModifierConsumed`"任何一步。

**逐字段实测**（与台账一致）：

| 字段 | 消费者 |
|---|---|
| `SortKey` | ✅ 1 个（`:141` 的比较） |
| `MaxUses` | **0** |
| `Cooldown` | **0** |
| `BestIndex`（局部量） | **赋值后无使用点** |

`Tag_TcsEvent_Damage_ModifierConsumed` 声明了但**从不发布**（`TcsDamageSubsystem.cpp:43-44` 的注释明写「本批只声明形状不发布」）。

**设计还成立吗**：✅ **成立，而且比旧设计更容易做了**。2026-09-30 的改造把 `OnConsumed` 闭包换成事件语义后，"成功才消费"只剩三件纯数据操作（扣次数 / 起 CD / 发事件），**没有任何闭包/回调的形状问题**。

**但有一个新的前提**：D-2 已经把「成功才消费」这条 MUST 从生效规格里删掉了（它只活在归档提案 `2026-09-21-add-tcsdamage-steps-and-primitive` `specs/damage-step-library/spec.md:15`）。⇒ **落消费动作之前，必须先把这条 MUST 恢复到 `specs/`**，否则做完了也没有可对照的契约。

**前置条件**：① 恢复十步契约（D-2）；② 有真实的"收集型候选"生产者。

**台账状态**：`DAMAGE-4`，归属 `R5.5-c`。**症状记录准确，归因需订正**（见 D-2）。

---

### U-3 ★★ `AttrCapture` 整套（台账 `WAIT-7`）——**设计仍成立，但它与 `FlowSource` 是同一件事的两半，而两半都没落**【实测+推断】

**缺什么**：`FTcsDamageFlowContext::CapturedAttrs`（`:75`）**无填充者、无读取者**——实测 `grep -rn "CapturedAttrs" Source/` 只有 2 处，都在本模块（字段声明 + 视图排除说明），**且都是注释/声明，无读写**。

**旧设计怎么说**（`SPEC-08-damage` §2.1 `:31`）：

> `AttrCapture`：`FDamageFlowConfig.AttrCaptureList{AttrKey, From: Instigator/Target}`——FlowStart 时从 M2 捕获进 `CapturedAttrs` 快照；属性读默认 Live、捕获命中读快照（流程内读取一致性）；"本次攻击攻击力+10%"双路径：捕获命中→改 `CapturedAttrs`（随流程消失，零账本污染）／未捕获→FlowSource 临时修正器挂真实账本。

**设计还成立吗**：🟡 **一半，而且发现旧文档自己改了另一半的口径**。

- `SPEC-08-damage` `:32` 的"★ 2026-10-08 补注"说：**技能侧**的同款声明已整体删除（三条判据：零消费者 + 与流程属性黑板/FlowModify 机制重叠 + 技能侧已被参数快照占满），但**"本行（伤害流程侧）不受影响"**。
- 而那条判据里最重的一条——**"与流程属性黑板 `FTcsFlowAttributes` + `FlowModify` 机制重叠"**——**对伤害侧同样成立**：`FTcsFlowAttributes` 本身就是"流程用完即弃"的工作值，`FTcsFlowModify` 就是"往它里面写"。`CapturedAttrs` 是**同功能的第二套**（`TMap<FGameplayTag, double>` vs `FTcsFlowAttributes` 的 `TMap<tag, TArray<提交>>`）。

⇒ **伤害侧的 `CapturedAttrs` 应当按与技能侧相同的判据重新裁一次**。旧文档明确禁止把技能侧删除读成"本条已交付"，但它**没有回答"同样的判据对伤害侧为什么就不成立"**。

- `FlowSource` 那一半（"未捕获→临时修正器挂真实账本"）依赖 D-3 记的级联摘除，**同样没落**。

**前置条件**：① 先裁"伤害侧的 `CapturedAttrs` 是否与 `FTcsFlowAttributes` 重复"；② 若判重复，本条应改为"删除 `CapturedAttrs` 字段"而非"实现 `AttrCapture`"。

**台账状态**：`WAIT-7`。**记录准确**（含 2026-10-08 的范围澄清），**缺一条"伤害侧是否同判"的待裁项**。

---

### U-4 ★★ 「模板登记不可枚举」与「步骤注册表不可枚举」——**台账 `TOOLS-6` 记了一半**【实测】

**缺什么**：

| 想问 | 有没有口 |
|---|---|
| 登记了哪些模板 | ❌ 无 `GetTemplateIds()` |
| 注册了哪些步骤执行器（含静态项） | ❌ `Executors` 私有，只有 `GetDynamicKeys()` |

**设计还成立吗**：✅ 成立，而且**成本极低**——`Templates` 是 `TMap`，加一个 `TArray<FGameplayTag> GetTemplateIds() const` 是三行；注册表加一个 `GetRegisteredKeys()` 同理。

**台账状态**：`TOOLS-6` 记了**步骤注册表**那一半（「若 Explain 面板要做'已登记步骤清单'，须先给两张执行器注册表补只读枚举 API」），**没记模板登记表**。⇒ 建议 `TOOLS-6` 补一句。

---

### U-5 ★ `Heal` 之外的旧文档符号：`FFlowRedirect` / `ETcsAttrCaptureFrom` / `FDamageFlowConfig` ——**三者全部双零**【实测】

**实测**：`grep -rn "FFlowRedirect|ETcsAttrCaptureFrom|FDamageFlowConfig|AttrCaptureList" Source/` ⇒ **零命中**；`git log --all -S` 对前两者**也零命中**（从未存在过，连数据形状都没定）。

| 符号 | 旧文档出处 | 双零？ | 台账 |
|---|---|---|---|
| `FFlowRedirect` | `SPEC-08-damage` §2.2 `:37` | ✅ 工作树 0 + `git log -S` 0 | `STAT-3`（归属 `R5.5-d`） |
| `ETcsAttrCaptureFrom` | 同 §2.1 `:31` | ✅ 双零 | `WAIT-7` 的一部分 |
| `FDamageFlowConfig` | 同 §2.1 `:31` | ✅ 双零 | 无 |

⇒ `FFlowRedirect` 与 `ETcsAttrCaptureFrom` **已在台账**；**`FDamageFlowConfig` 从未入账**（它是 `AttrCaptureList` 的宿主结构，随 `WAIT-7` 一起消失即可，不必单列）。

**`FFlowRedirect` 的设计还成立吗**：🟡 **需要重新核**。它要解决的是"状态/装备声明换流程"（四粒度让渡：参数 → 链 → 技能 → **流程**）。而今天 `FTcsStepDamage::FlowTemplateId`（`TcsStepDamage.h:39`）**已经能在链步骤上指定模板**——即"换流程"最简单的那一半**已经用配置表达了**。剩下的一半是"状态/装备**自动**换"（运行时让渡 + 优先级 + 级联回收），那需要"让渡栈"。**这条应当按"是否还有真实内容需求"重裁，而不是照旧排期。**

**台账状态**：`STAT-3` 记录准确（含 2026-10-05 的改判）。

---

### U-6 ★ 官方默认模板**只有一种**，而旧设计说"多预设 = 多模板资产"（台账 `WAIT-8`）【实测】

**缺什么**：`UTcsDamageFlowTemplateDef : UPrimaryDataAsset` 不存在（`grep -rn "FlowTemplateDef" Source/` 零命中）。今天所有模板都是**C++/内容里手搓 `FTcsFlowTemplate` 再 `RegisterTemplate`**（装置 `TcsDevBootstrap.cpp:240-295` 三条全是这么建的）。

**设计还成立吗**：✅ **成立**，而且**成本比旧设计低**——旧设计要"DefLibrary 发现 → 登记进子系统"一整套；今天 `FTcsPrimaryAssetQuery`（`ARCH-03` 的成果）已经就位，模板资产可以直接走它。

**但有一条要重新核**：旧设计写「模板选择链：Damage 步骤配置指定 → **Def 覆盖** → 全局默认」。第二步"Def 覆盖"**今天无处可放**——`FTcsStepDamage` 只有一个 `FlowTemplateId`，没有"技能 Def 覆盖它"的通路。若这条设计还要，就需要在 `TcsSkill` 侧加配置位；若不要，旧文档应标注为已简化。

**前置条件**：裁定"Def 覆盖模板 id"是否还要。

**台账状态**：`WAIT-8`，归属"等第一个真实内容需求"。**记录准确**，GC 面已修（`AddReferencedObjects`，见 §2.10 与 U-7）。

---

### U-7 ★ `AddReferencedObjects` 的 GC 保活**从未被实测**（台账 `WAIT-8` 的边界半）【实测】

`TcsDamageSubsystem.cpp:104-120` 的实现是对的（逐模板 `AddPropertyReferencesWithStructARO`，理由写在 `TcsDamageSubsystem.h:46-65`，含引擎出处 `DataTable.cpp:300` 与 `InstancedStruct.cpp:506`）。

**但台账 `WAIT-8` 明确记着**：

> **⚠️ 未实证**：GC 场景（无其他强引用的 delegate + 手动 `obj gc`）**未跑**——宿主当前用 `UPROPERTY` 强引用绕过（现成冗余保险），R3 无合适夹具。

⇒ **今天仍无夹具**。这条保活的成立依据是**引擎源码机制**而非实测。

**设计还成立吗**：✅ 成立（且 `TcsEffect` 同款）。**前置条件**：一个"模板里的 delegate 无其他强引用 + 强制 GC + 再跑一次流程"的夹具。

**台账状态**：`WAIT-8`。**记录准确。**

---

### 补充：`ConditionChance` 的随机源**永远拿不到真随机**——这是"有意留白"还是"漏了接线"？【实测】

`ShouldRunFlowStep(Conditions, Context, RandomValue = 0.0)`（`TcsFlowStepConditions.h:63-66`）的第三参**有默认值 0.0**，而**全部 12 个调用点都是两参调用**（`TcsFlowStepsCore.cpp` 4 处 + `TcsFlowStepsRest.cpp` 8 处，实测）：

```
$ grep -rn "ShouldRunFlowStep(Step->Conditions, Context)" Source/TcsDamage
12 处，全部两参
```

⇒ `RandomValue` **恒为 0.0**，于是 `FTcsConditionChance` 的判定恒为 `0.0 >= Probability` ⇒ **`Probability > 0` 时恒不通过（跳过该步），`Probability == 0` 时恒通过**。

**规格怎么说的**（`damage-step-library:74`）：

> `FTcsConditionChance`（**概率型条件显式声明随机源**：宿主未注入随机源时退化为确定性判定，保证可复现——D0-1 的例外必须显式）

⇒ **规格把"未注入"写成了合法降级**，于是代码恒不注入也是"符合规格"的。但对照 `TcsEffect`：它的同款条件是 `EvaluateTriggerConditions(..., Facade->NextTriggerRandomValue(), ...)`（`TcsTriggerCondition.cpp:344` / `TcsBranch.cpp:43`）——**有真实随机源，且注释说明"取随机值会推进门面的随机流"**。

⇒ **同一族的两个条件求值器，一个接随机源、一个不接**，而**规格把不接的那个写成了设计**。这是 `G-15`（同族入口判据不一致）的本模块形态，而且比 `G-15` 更进一步：**规格为缺陷提供了合法性**。

**分类**：严格说这**不构成"未实现"**（规格允许），而应当记为 **§6 的一条"待重裁"**：`FTcsConditionChance` 要么接上 `UTcsDamageSubsystem` 的随机流（与 `TcsEffect` 对齐），要么**从规格里删掉"概率型条件"这个说法**——因为一条永远走固定分支的概率条件没有存在意义。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 载体 | 体量 | 状态 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/damage-{primitive,flow,step-library,record-inspection}/` | 4 能力 / **635 行** / 19 Requirement / **74 Scenario** | **`step-library` 正文被掏空（D-2）** |
| **实际实现** | `Source/TcsDamage/` | 30 文件 / 3 225 行 | 4 条纯代码债 |
| **历史理由** | `Documents/combat-system-design/spec/09-module-damage.md`（`SPEC-08-damage`） | **135 行** / ACTIVE | 见 §7.2 |

**一个值得记的比例**：本模块是**规格覆盖率最高**的模块之一（74 个 Scenario 对 3 225 行代码，平均 44 行代码 1 个 Scenario；`TcsSkill` 是 11 052 行 / 8 能力），但**覆盖率与有效性在这里首次脱钩**——74 个 Scenario 里**没有一个能发现 D-1**（因为 `HitRate = 0` 这个输入从未出现在任何 Scenario 里），也**没有一个能发现 D-5**（没有任何 Scenario 使用多于一个目标）。

⇒ 这与 `ARCH-09` 的 **G-23**（Scenario 网格缺角）**同型但更重**：`TcsTargeting` 是"该写的组合没写"，本模块是"**该写的组合没写，且写了的那些覆盖不到真实缺陷**"。

### 7.2 `SPEC-08-damage` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

| # | 旧文档声称（行） | 代码实际 | 判定 |
|---|---|---|---|
| 1 | `FDamageFlowContext`（`:25`） | `FTcsDamageFlowContext` | **改名滞后** |
| 2 | `FTcsDamageFlowTemplate` → `FTcsFlowTemplate`（`:36` 已自注） | `FTcsFlowTemplate` | ✅ **已自纠** |
| 3 | `FFlowRedirect{TargetTemplateId, ReplacementTemplateId}`（`:37`） | **双零**（工作树 0 + `git log -S` 0） | **声称未落地**（台账 `STAT-3`） |
| 4 | `FDamageFlowConfig.AttrCaptureList` + `ETcsAttrCaptureFrom`（`:31`） | **双零** | **声称未落地**（`WAIT-7`） |
| 5 | `IHealFlowDelegate.GetBaseHeal`（`:87`） | **双零** | **声称未落地**（`DAMAGE-3`） |
| 6 | `FTcsConsumePolicy`（`:89` 已自注改名） | `FTcsDamageModifierConsumePolicy` | ✅ **已自纠** |
| 7 | `OnConsumed` 闭包（`:60` / `:89` / `:93` / `:134` 四处） | 已删，改事件语义（`:134` 有自注） | ✅ **已自纠** |
| 8 | `FDamageRecord` 含**元素**字段（`:88`） | 字段已删（`TcsDamageRecord.h:54-59`） | ❌ **未自纠**（§2.5 的清理已完成，旧文档未回写） |
| 9 | 十步表（`:41-52`）说 `Hit` 判定「Hit/Miss/**Invalid**」三态（`:45`） | 无三态枚举，`bHit` 是 `bool`（`>= 1.0` 折算） | ❌ **未自纠** |
| 10 | 十步表说 `Element` 步「元素标签写入分类 Tag 集」+「**修改器可改**」（`:47`） | 写入 ✅；**"修改器可改"无实现**（`TcsFlowStepsRest.cpp:131-141` 只写入，无读回覆盖） | ❌ **未自纠** |
| 11 | `ITcsDamageFlowDelegate` 5 方法（`:75`） | **7 方法**（`:76` 有自注"5 方法带 UFUNCTION"，但 `:75` 的列表没加） | 🟡 **半自纠**（`§2.4` 的 ★ 段与 `§7` 表都写了 7，但 `:75` 那行仍列 5） |
| 12 | `FTcsDamageFlowTemplateDef : UPrimaryDataAsset`（`:36` 的"模板资产"） | **零命中** | **声称未落地**（`WAIT-8`） |
| 13 | 网络姿态 `NET-1/2`（§4 `:98-100`） | `openspec` **零对应需求** | **设计有、规格无** |
| 14 | §7 验收钩子表（`:117-124`）6 行逐条对账 | **5 行状态仍准确**（✅ 2 / ⛔ 3），第 6 行"模板重定向"与 #3 同 | ✅ **准确** |

**统计**：**14 项里 5 项自纠准确**（#2 / #6 / #7 / #14 与 #11 的半自纠）、**3 项声称未落地但台账已记**（#3 / #4 / #5 / #12 中的四条，台账都已覆盖）、**3 项旧文档未自纠**——不是"未落地"，而是"**已变了但没写**"（#8 / #9 / #10）。

⇒ **就地订正率 = 5/14 ≈ 36%**，介于 `SPEC-04-skill` 的 7/14 与 `SPEC-06-targeting` 的 3/8 之间。

### 7.3 差异的**类型分布**（与 `ARCH-05` 起的六类判据对照）

| 判据类 | 本模块命中 | 具体 |
|---|---|---|
| **符号名**（`ARCH-05`） | **1** | #1 `FDamageFlowContext` |
| **模块名**（`ARCH-05`） | **0** | — |
| **类型名**（`ARCH-05`） | **2** | #2 `FTcsDamageFlowTemplate`（已自纠）/ #6 `FTcsConsumePolicy`（已自纠） |
| **字段位置**（`ARCH-05`） | **0** | — |
| **时态词**（`ARCH-08` `G-22`） | **1** | `SPEC-08` `:12` 的「v4（PV 系列 D7-2 收窄…）」——它仍是当前状态描述，**不是**过期时态 ⇒ 记 0 更准。真正命中的是 `DAMAGE-4` 台账行里的时点错误（见 D-2） |
| **设计已推翻**（`ARCH-09` `G-29`） | **2** | #9 的三态判定 / #10 的"修改器可改"——**都是设计里写过、代码里从未实现、也没有任何记录说明为什么不做了** |

⇒ **本模块首次出现"类型名类 2 命中、但 2 项都已自纠"**——这是九篇里第一次某类判据"命中但无害"。原因很简单：`SPEC-08-damage` 的 §8 修订记录**逐条记了改名**（v4 增补 2 / 3 / 4 三条），是旧文档族里修订记录写得最实的一篇之一。

**这与 §8 的规模判据一致**：135 行 > 150 行的门槛附近，修订记录质量"中上"。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

| 步 | 动作 | 产出 |
|---|---|---|
| 1 | 读 4 份能力规格（635 行 / 19 需求 / 74 Scenario） | §2 的需求锚点、§5 的规格侧证据 |
| 2 | 读 `Source/TcsDamage/` 全部 30 个文件（3 225 行） | §1.2 文件表、§3 契约面 |
| 3 | 读旧文档 `SPEC-08-damage`（135 行，**先读 §8 修订记录**） | §7.2 的 14 项差异清单 |
| 4 | 追竖切：本模块 7 个公开符号在 11 个模块里的使用点 | §1.4 / §4.1 |
| 5 | 交叉验证：`git log -S` / `git show <commit>:<file>` / glue 产物 / 台账逐行 | D-2 / U-5 / D-6 的确证 |
| 6 | 读旧文档收束节（`SPEC-08` §8 修订记录 + §7 验收钩子表） | §7.2 的订正率 |

### 8.2 本次审核用到的**七类判据**（`ARCH-05` 四类 + `ARCH-08` 第五类 + `ARCH-09` 第六类 + 本篇第七类）

| # | 判据类 | 本模块命中 | 依据 |
|---|---|---|---|
| 1 | 符号名 | 1 | `FDamageFlowContext` |
| 2 | 模块名 | **0** | 本模块没经历过拆分 |
| 3 | 类型名 | 2（均已自纠） | `FTcsDamageFlowTemplate` / `FTcsConsumePolicy` |
| 4 | 字段位置 | **0** | 同上 |
| 5 | 时态词 | 1（台账行，非规格） | D-2 的 `DAMAGE-4` 时点错误 |
| 6 | 设计已推翻 | **2** | `Hit` 三态 / `Element` 的"修改器可改" |
| 7 | **正文被删空**（新） | **1** | D-2：规格结构在、内容没了 |

**第 7 类的定义与判据**：

> **凡规格的标题 / Purpose / 正文首句仍在描述某组内容，而该组内容在正文里找不到逐条条目者**，记**「结构在、内容被删空」**。
> 判据：拿掉所有 `Requirement` / `Scenario` 结构之后，正文里还剩几条**可被违反的**陈述？若「标题承诺的条目数」远大于「正文里逐条写出的条目数」，即命中。
> 成因（本模块实测）：**大范围重写改造里，含目标内容的整段被当作"待改写内容"重写，改写后只保留与改造主题相关的那部分。**

**为何它不能归入已有的六类**：

- 不是 #1~#4（引用类）：**没有任何符号引用错**——规格里的符号全对；
- 不是 #5（时态词）：**没有"本 Task / 本轮"这类相对时态**；
- 不是 #6（设计已推翻）：**设计没被推翻**，是**没被写**。

⇒ 与 `G-22`（写了不存在的状态）**方向相反**：那条是"多写了一个已过去的状态"，本条是"**少写了本该有的内容**"。

### 8.3 置信度分布

| 置信度 | 条数 | 条目 |
|---|---|---|
| **【实测】** | 10 | D-1 / D-2 / D-3 / D-4 / D-6 / D-7 / D-8 / D-9 / D-10 / D-11（全部有 `grep` / `git` / glue 直证） |
| **【实测+推断】** | 4 | D-5（代码行为实测，"应当如何"的判据是推断）/ U-1 / U-3 / U-5 |
| **【推断】** | 3 | U-6（"成本比旧设计低"是推断）/ §4.2 #2 的跨流程污染（实际安全）/ §6 的 `ConditionChance` 影响面 |
| **【待核】** | 0 | — |

**本篇 11 条债里 10 条是纯实测**——因为它们的证据都在单文件内（`grep` 一次即得），不需要跨模块推理。

### 8.4 一次方法论层面的收获：**"规格被删空"需要主动去查，它不会以任何形式报错**

本篇审到 D-2 的路径值得记下来，因为它是**唯一一条"必须去读 git 历史才能发现"的债**：

1. 读 `damage-step-library/spec.md`，看到标题「标准步骤库（十步）」；
2. 往下找十步——**没有**；
3. 第一反应是"规格质量差"（`ARCH-08` 的结论形态）；
4. 但 `Purpose` 与正文首句都写"十步" ⇒ **不像"从没写过"，像"写过又没了"**；
5. `git log --follow` 该文件 → 四次提交；
6. `git show a1804ed:<file>`（归档那次）→ **十行俱在**；
7. `git show 539b14e -- <file>` → **17 insertions / 25 deletions**。

⇒ **判据（应当进 `INDEX` §5）**：

> **凡规格出现"标题/Purpose 承诺 N 项，正文里少于 N 项"，MUST 跑 `git log --follow` 与 `git show <commit>:<file>` 确认它是"从未写过"还是"写过被删"。**
> 两者的处置完全不同：
> - **从未写过** = 规格欠账（补写即可，`ARCH-08` 的形态）；
> - **写过被删** = **回归**（要先查清是谁在什么改造里删的，因为**同一个改造可能还删了别处**）。
>
> 判别点：**正文里是否还残留"待填"的痕迹**——本模块残留了四处"十"，是"删"的铁证；`ARCH-08` 那些条目则从头到尾没提过步数。

**这条判据的代价**：一次 `git log --follow` + 一次 `git show`，约 2 分钟。**而不查的代价**是一条 ★★★ 债被误记为"规格质量差"，从而按"补写"处置——补写会补成**猜测的样子**，而不是**当初定下的样子**。本模块若按"补写"处置，"成功才消费"这条 MUST **大概率补不回来**（因为没人记得它曾在规格里）。

### 8.5 一次「同款代码复制」的系统性发现：**`TcsEffect` 与 `TcsDamage` 是全仓唯一一对"逐行同构"的注册表**

实测：`diff` 两份 `.cpp` 只有 70 行差异，且全是注释与日志文案（§2.6）。

**这条同构带来一个可预测的后果**：**任何一方的缺陷都会同时存在于另一方**。

| 缺陷 | `TcsDamage` | `TcsEffect` |
|---|---|---|
| `Deinitialize` 撤销全部动态条目（D-7） | ✅ 有 | ✅ **有**（`TcsEffectSubsystem.cpp:42-61`，逐字相同） |
| 拒绝门 / 失效判据 / 寿命语义 | ✅ 正确 | ✅ 正确（同一份实现） |
| 缺枚举面（U-4） | ✅ 有 | ✅ 有（`TOOLS-6` 已记） |

⇒ **`ARCH-11-integration` 审核时应当拿这条规律去查 `TcsIntegration`：它有没有复制 `TcsSkill` / `TcsEffect` 的某段实现？** 若有，D-7 那一类缺陷很可能第三次出现。

---

## §9 结论

### 9.1 三句话

1. **本模块是全仓唯一"规格被一次无关改造掏空"的模块**——`damage-step-library` 的四处仍宣称"十步"，正文里一步都没有；删除发生在 2026-09-22 的 tag 化改造（`539b14e`，−25/+17），而台账把它误记为"归档时被裁"。
2. **规格一空，代码就失去了对照物**——D-1（`> 0.0` 当空值哨兵，导致"命中率 0"不可表达且静默变必中）与 D-7（`Deinitialize` 撤销全部世界）都是"规格里没有这一条，所以谁也没去查"，而**前者是本模块唯一的 ★★★ 纯代码缺陷**。
3. **它也是全仓第一次出现"同款代码逐字复制"**——`TcsDamage` 与 `TcsEffect` 的步骤注册表 `diff` 只剩 70 行注释差异，于是**缺陷也一起复制**（D-7 两边有，U-4 两边有）；同时**规格侧只有一边掉队**（本模块的十步契约被删，`TcsEffect` 的步骤契约还在）。

### 9.2 一句值得记住的话

> **`TcsDamage` 的规格写得比代码好——它只在 2026-09-22 那天被人顺手擦掉了一半，然后就没人再看它。**

**展开**：

- **"规格写得比代码好"** 不是修辞：`damage-primitive` 的「契约默认 vs 框架内定」判据（§2.2）、「链侧预写不与流程提交重复计数」带实测反例 `10+20=30`（§2.4）、记录里拒收"元素"这个玩法名词（§2.5）——这三条是全 44 个能力里论证质量最高的段落。
- **"被顺手擦掉一半"** 是字面意义的：那次改造要改 21 个原生词、几十个文件；这个文件恰好"每一行都提到键名"，于是整段被当作待改写内容重写，改写后只保留了与 tag 有关的部分。
- **"没人再看它"** 有结构性原因：`step-library` 的正文变成了一段泛论，**它读起来像一个"概述章"**，于是后面的审核者（包括本档案的前九篇）看到它时不会觉得"缺了什么"，只会觉得"这段写得比较概括"。

### 9.3 与前十篇的对照

| 篇 | 模块 | 主方向 | 纯代码债 | 纯规格债 |
|---|---|---|---|---|
| `ARCH-04` | `TcsAttribute` | **规格更严、代码更松** | 少 | **4** |
| `ARCH-05` | `TcsState` | 代码搬走、引用滞留 | — | — |
| `ARCH-06` | `TcsEffect` | **各错一半** | 5 | 4 |
| `ARCH-07` | `TcsBuff` | 纯规格为主 + 1 条崩溃级 | 1 | 多 |
| `ARCH-08` | `TcsSkill` | **代码干净、规格单方面掉队** | **1** | **8** |
| `ARCH-09` | `TcsTargeting` | 判据从未被写下 | **1** | 4 |
| **`ARCH-10`** | **`TcsDamage`** | **规格被掏空 ⇒ 代码失去对照物** | **4** | **3** |

⇒ **本模块是九篇以来第一次"纯代码 ≥ 纯规格"**，而**成因与 `ARCH-08` 正好相反**：

- `ARCH-08`（`TcsSkill`）：代码写对了、规格没跟上 ⇒ 1 条代码债 / 8 条规格债；
- `ARCH-10`（本模块）：规格被删空、代码随之偏 ⇒ 4 条代码债 / 3 条规格债。

**一个可直接用的推论**：**规格的单方面缺失会传染成代码缺陷**，而且**只在一个方向传染**——`ARCH-06`（各错一半）里规格与代码是同批写的，缺陷各自独立；本模块是规格**先失去内容、后失去约束力**，于是代码在无人对照的情况下漂移。

### 9.4 给下一轮的四条建议（按代价从低到高）

| # | 动作 | 代价 | 收益 |
|---|---|---|---|
| 1 | **恢复十步契约到 `specs/`**（D-2）——从归档提案 `2026-09-21-add-tcsdamage-steps-and-primitive` 的 `specs/damage-step-library/spec.md` 取回十行，按当前 tag 名订正后并回 | **约 40 行**，一次 `git show` + 一次编辑 | 立刻让 D-4 / D-5 / U-2 变成"有契约可对照"的条目；**这是本模块所有规格债的前置** |
| 2 | **D-1 的一行修法**：`> 0.0` 改为 `FindSubmits != nullptr` | **2 行 × 2 处** | 关掉本模块唯一的 ★★★ 代码缺陷；同模块已有正确先例（`FindSubmits`） |
| 3 | **补多目标语义到规格 + 定 `Executed` 的施加粒度**（D-5） | 一次裁定 + 约 15 行规格 | 多目标链是本仓主力形态；今天走的是未定义路径 |
| 4 | **给 `PublishCollectEvent` / `GetRecentRecords` 加 `UFUNCTION()`**（D-6） | **2 个宏** | 兑现四处注释共同承诺的"脚本修正流程值"通路 |

**一条不做的事**：**不要"顺手"重写 `damage-step-library`**。D-2 的教训正是"一次顺手的大范围重写把内容擦掉了"——恢复应当走 `git show` 取回原文 + 逐处订正，**不做重构**。

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。`TcsDamage` 首次成篇（**3 225 行 / 30 文件**；本篇档案 `ARCH-10-damage`）。审出 **D-1..D-11 / U-1..U-7**（另加 §6 一条 `ConditionChance` 的待重裁）。三条结构性发现：① **规格被一次无关改造掏空**——`damage-step-library` 的十步契约在 2026-09-22 的 tag 化改造（`539b14e`，−25/+17）里被整段删除，而 Purpose / 标题 / 正文首句 / 默认模板节**四处**仍宣称"十步"；台账 `DAMAGE-4` 的归因（"归档时被裁"）**时点错误**（归档 `a1804ed` 时十行俱在）。② **规格一空，代码失去对照物**——D-1（`HitRate`/`CritRate` 用 `> 0.0` 当空值哨兵）与本模块唯一的 ★★★ 纯代码缺陷直接相关，而它在 74 个 Scenario 里**没有任何一个输入能触发**。③ **`TcsEffect` 与 `TcsDamage` 的步骤注册表是全仓唯一一对逐行同构实现**（`diff` 只剩 70 行注释差异）⇒ D-7 的 `Deinitialize` 撤销全部世界的缺陷**两边都有**。新增缺陷方向 **G-30「结构在、内容被删空」**——第 7 类判据，判据 = 标题承诺 N 项而正文少于 N 项时 MUST 跑 `git log --follow` 区分"从未写过"与"写过被删"。**旧文档判定**：`SPEC-08-damage`（135 行）**14 项里 5 项自纠准确 / 3 项旧文档未自纠的非未落地项（#8 元素 / #9 三态 / #10 修改器可改）/ 4 项声称未落地但台账已记**，就地订正率 5/14 ≈ 36%，介于 `SPEC-04-skill`（7/14）与 `SPEC-06-targeting`（3/8）之间。**§8.2 的七类判据里首次出现"标题承诺 vs 正文条目数"这一类**。 |

