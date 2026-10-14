# PLN-R6.5-CC — 消耗策略化（Cost Policy）收窄轮计划

- **文档 ID**：`PLN-R6.5-CC`
- **类型**：PLAN / 轮次计划
- **状态**：**COMPLETED（2026-10-12 收束；2026-10-13 取证补齐）**——Task 0 / 1 / 2 / 3 全部落地并取证；Task 4（收束归档）已完成
- **权威范围**：本轮范围 / §0 裁定 / Task 骨架 / 验收面 / 边界。**不在本文**：`SPEC-04-skill` 正文回写（Task 4 做）、冷却侧（不属本轮）。
- **最后更新**：2026-10-13（**取证补齐**：内容侧消费资产 + PIE 路线 + 三处缺陷修复，见 `EVID-2026-10-13-cost-policy-pie`；**本计划的任务骨架不因此变更**——补齐的是 §验收面 的执行，不是新 Task）

---

## 5. 落地实况（2026-10-12 收束时回写；**本节是"计划 vs 实际"的对账**）

| Task | 状态 | 实际交付 |
|---|---|---|
| **Task 0** 类型层换型 | ✅ | `ETcsSkillCostTiming`（四档，含 `ECT_None` 默认）/ `FTcsSkillCostConfig{Timing, CostMode}` / `FTcsSkillCostPolicy`（中性默认 + `Hidden` + 虚析构）/ `FTcsSkillCostPolicy_ResourceAttr` / `FTcsCostContext`（纯 C++ 非反射）；**删** `ETcsCostPolicy` 三档 + `FTcsCostConfig` |
| **Task 0b** 冷却侧改名（R12） | ✅ | `ETcsCooldownTiming` → `ETcsSkillCooldownTiming`（**只改类型名、值不动**；归档与历史日志旧字面保留） |
| **Task 1** 脚本面宿主插槽 | ✅ | `TcsCostPolicyHost.h`：`UTcsSkillCostPolicyHost` / `ITcsSkillCostPolicyHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + `BlueprintNativeEvent`）+ `FTcsSkillCostPolicy_HostDelegate` 转发器（`TScriptInterface` + **空 Host ⇒ fail-closed** + `Execute_*`） |
| **Task 2** 分派 + 原子性 | ✅ | `ResolvePolicy` / `MakeContext` 唯一解析点；`CanAffordCost` / `CommitCost` 分派；`None` 短路；**空策略 ⇒ fail-closed**；框架用 `BeginBatch`/`Commit` 包策略提交 |
| **Task 3** 装置 + 取证 | ✅ | K 面四条（K1/K2 宿主策略被真实调用、K3 空 Host fail-closed、K5 无残留批）；`g` 判据**取反**；双配置 0/0；UHT 产物核对 |
| **Task 4** 收束归档 | 进行中 | 台账 `COST-3` / `COST-2` 闭合；证据档 `EVID-2026-10-12-cost-policy`；`SPEC-04` / `INDEX` / `GLOSSARY` / `LOG-DECISIONS` 回写 |

### 5.1 ★ 与原计划的三处偏离（MUST 读，防照旧稿派活）

1. **§0.4 D1 的"内建策略也走同一套"被原样实施**，但**命名词根按 R11 统一为 `Policy`**
   （原草稿写过 `FTcsSkillCostMode_ResourceAttr` / `FTcsCostContext` —— 实际是
   `FTcsSkillCostPolicy_ResourceAttr` / `FTcsSkillCostContext`）。
2. **§0.3 纠正二（原子性）在实施期被进一步更正**：我原写"批可同时给全有全无与防重入"，
   **源码核实后改为"批只推迟重算+广播、不提供回滚"** ⇒ "全有全无"的实际保障是**先判后扣**。
   详见 `TcsCostOps.h` 的注释与 `EVID-2026-10-12-cost-policy` §5.2。
3. **§0.2 的"成本组整体作废"已执行**：`FTcsCostGroup` / `ETcsCostCombineMode` /
   `FTcsCostDetail` / `ResolveCostPlan` **一律未建**（用户裁定框架不提供组合代数）。

### 5.2 一处**计划未预见**的实施坑（MUST 留痕）

**`UCLASS` MUST 住头文件**：装置用的宿主策略类最初住 `.cpp`（且一度住匿名命名空间）
⇒ UHT 不为其生成反射代码 ⇒ `C2143 syntax error: missing ';' before '<class-head>'`。
**本轮两次命中**，最终移进 `TcsDevSliceRig_CooldownCost.h`。
**判据**：UCLASS/USTRUCT/UENUM 的**类型名 MUST 在全局作用域、且住头文件**（UHT 只扫头文件）。

### 5.3 验收读数（与冻存基线逐条比对）

- `Tcs.Test.CooldownCost.Run`：**立即面 28/0 + 尾段 31/0**（基线 24/27 + 新增 4 条 K；
  **零 flips、零 FAIL**）
- `Tcs.Test.CooldownCost.Reject`：**7/0**
- R6 回归 `Tcs.Test.SkillLayer.Run`：**24/0**，`[复现性摘要]` 与基线**逐字相同**
- 双配置编译 **0 error / 0 warning**；`openspec validate --all --strict` **42 passed / 0 failed**

### 5.4 **取证补齐（2026-10-13，用户要求；本计划任务骨架不变）**

用户当日要求"PIE 测试也应该执行的，且应该创建一个 Cost 直接消费属性"。**补的是 §验收面 的执行**，
不是新 Task ⇒ **Task 编号与裁定（R1~R13）一概不动**。三处缺口、读数与三处缺陷见
`EVID-2026-10-13-cost-policy-pie`。

- **内容侧消费资产**：`DA_SkillDef_E2E_CostMana`（扣 `Attribute.Mana` **25**）、
  `DA_Mana`（基值 100、上界**动态取 `MaxMana`**）、`DA_MaxMana`；装置新增 **L 面**四条判据。
- **读数**：无头 `Run` **30/0 + 33/0**、`Reject` **7/0**、回归 **24/0**；
  **PIE** `Run` **33/0 + 36/0**（三轮逐字相同）、`Reject` **7/0**、同会话回归 **24/0**、
  **跨 PIE 重跑通过**；**全部日志 `FAIL=0` 且 `ensure=0`**；双配置 **0/0**。
- **★ 一处产品缺陷（本轮抓到并修）**：`FTcsSkillCostPolicy_ResourceAttr` 缺 `BlueprintType`
  ⇒ **脚本层不可达**（与 R4"内建也走同一套"及脚本面插槽目标冲突）。**详见证据档 §3.1/§4**。
- **★ 两处夹具缺陷（本轮抓到并修）**：无头 `ensure` 红字；动态边界引用方未挂实例 ⇒ 扣对又被钳 0。
- **§5.1 第 2 条与 §5.3 的读数口径不变**（那是无头读数；PIE 读数见证据档 §2.2）。

---

## 0. 范围（一句话）

把"消耗"从**写死的内建逻辑 + 档位枚举**改为**策略模式**：`Timing` 只答"何时付 / 要不要付"，"付什么、怎么付"由**策略对象**答（框架自带 `_ResourceAttr` 一个，且它**同样遵循这套方案**）；并补脚本面转发器。同时**闭合台账 `COST-3`**，并**撤销** v1 的"成本组"方向（连带 `COST-2` 改判）。

**本轮的实质 = 把 `D5-10` 早写下的"检查（CanAfford）与提交（Pay）成对走策略"兑现出来**，而不是发明新能力。

---

## 0.1 用户已裁定（本计划据此起草）

| # | 裁定 | 依据 |
|---|---|---|
| **R1** | **改点策略模式**：`ETcsSkillCostTiming{None/OnCastStarted/OnCastCompleted/Manual}` + 策略基类（`CanAfford` / `CommitCost`）+ 内建 `_ResourceAttr` | 用户 2026-10-12 原话（见 §0.2 首段） |
| **R2** | **框架不提供 `All` / `Group` / 多资源原语**——"三消耗"由宿主自己写一个策略一起扣 | 同上："至于什么 All、Group 这些，宿主项目如果有需求，自己去实现就好了，三消耗可以一起塞进一个 CastMode 中" |
| **R3** | **不要 `ETcsCostPolicy` 档位枚举**——直接用策略模式 | 同上："感觉也不需要 `ETcsSkillCostPolicy`，直接用策略模式就好" |
| **R4** | **内建 `ResourceAttr` 也走同一套策略方案**，不写死在代码里 | 同上："TCS 自带一个 ResourceAttr，但也遵循这套方案，而不是直接把 ResourceAttr 写死在代码里" |
| **R5** | **`Timing == None` ⇒ 该技能一定没有消耗**，后面一概不管 | 同上："只要 CostTiming 为 None，那就不用管后面了，因为一定证明这个技能没有消耗" |
| **R6** | **`Custom` 档定名为 `Manual`**（`ECT_Manual` 同款语义） | 用户 2026-10-12："你说得对，还是应该叫 ECT_Manual，语义更准确" |
| **R7** | **现在就加脚本面转发器**（`_HostDelegate`） | 用户 2026-10-12 选择"现在就加脚本转发器（推荐）" |
| **R8** | **框架把策略的 `CommitCost` 包进 `BeginBatch` / `Commit`** | 用户 2026-10-12 选择"框架把策略的 CommitCost 包进 BeginBatch/Commit（推荐）"；并已确认推翻"MUST NOT 用 BeginBatch 包成本"的禁令 |
| **R9** | **单层**（无嵌套组） | 用户 2026-10-12 选择"单层组（推荐）"——**注**：R1 之后"组"概念本身已不成立，本条的**有效残余** = "策略内部想怎么组合由宿主自理，框架不递归" |
| **R10** | **直接开工**（计划 → 实施 → 取证） | 用户 2026-10-12 |

---

## 0.1b 追加裁定（2026-10-12 第二轮复核，已定，**MUST 按此实施**）

| # | 裁定 | 落点 / 判据 |
|---|---|---|
| **R11** | **命名词根统一用 `Policy`**：基类 `FTcsSkillCostPolicy` / 具体 `FTcsSkillCostPolicy_ResourceAttr` / 转发器 `FTcsSkillCostPolicy_HostDelegate` | 与仓内 `FTcsParamSource_*` / `FTcsParamSource_HostDelegate` 的现成命名法一致；避免 `Policy`（基类）与 `Mode`（具体）两个词根并存让后人猜哪个是基类 |
| **R12** | **两侧枚举都加域前缀**：`ETcsSkillCostTiming` **+** `ETcsSkillCooldownTiming`（冷却侧**同批改名**） | 保持这一对姊妹枚举的对称。**本批因此含一处已交付公开枚举改名**（零资产影响，见 §0.7） |
| **R13** | **`COST-2` 记为「✅ 已闭合（改判 = 宿主策略自答）」**（不出表） | R2 已明确否决"框架建多资源原语"，而扩展点由本轮 `COST-3` 交付 ⇒ 留"待触发"会让后人以为还要建 |

> **R12 的范围界定（防误改）**：**只改枚举类型名**（`ETcsCooldownTiming` → `ETcsSkillCooldownTiming`），**枚举值与语义一律不动**（四档原样）；**归档提案与本仓历史日志里的旧字面 MUST NOT 改写**（它们是历史现场——本仓既有"换根注记"纪律同款）。
>
> **R11 的连带**：`FTcsSkillCostConfig` 的字段名取 **`CostMode`**（它答"用哪种模式"）——**类型名统一 `Policy`、字段名保留 `Mode` 是刻意的**：字段描述的是"挂哪个策略实例"，用 `Mode` 读起来是"模式选择"，而类型名表达的是"策略族"。

---

## 0.2 本轮推翻/作废的既有登记（**MUST 留痕，MUST NOT 静默删**）

| 被推翻者 | 原文位置 | 新裁定 | 判据 |
|---|---|---|---|
| **v1 计划的"成本组 / 细节数组 / 组合模式"** | 本档 v1（同日） | **整体作废** | R2：框架不替宿主决定"多条怎么组合"——那是玩法结论 |
| **`ECP_None` 档** | `TcsCostTypes.h` `ETcsCostPolicy` | **随枚举整体删除**；"不消耗"改由 `Timing == None` 表达 | R5 给出**唯一**入口 ⇒ 消掉"空数组 vs 填 None"两个入口的歧义 |
| **`ECP_CustomFragment` 档** | 同上 | **删除**；自定义消耗改由**策略对象**表达 | R1/R3：档位枚举与策略对象并存 = 同一件事两个入口 |
| **`FTcsCostConfig` 类型** | `TcsCostTypes.h` | **退役**，改为 `FTcsSkillCostConfig{Timing, CostMode}` | R1：`Policy` 枚举字段没有存在理由了 |
| **"消费形状是标量 + 键、没有可枚举的多个元素 ⇒ 刻意不对称"** | `TcsCostTypes.h:73-76` | **该判据过期** | 有了策略对象，"消耗什么形状"由策略自决；框架不再对这一层下结论 ⇒ 不对称的**前提**（框架必须用一个枚举表达"没有"）消失 |
| **"`MUST NOT` 用属性侧 `BeginBatch` 包成本"** | `TcsCostOps.h` / `PLN-R6.5-1` | **推翻**（R8） | 原判据是"那个提交点今天不存在"；本轮**就是要造那个提交点**，且多资源下有真实重入窗口（见 D6） |
| **v1 的 `FTcsCostPlan` / 框架级组合一致性** | 本档 v1 | **作废** | "选哪条"成为**策略的内部事务**，框架无需知道 ⇒ 少一层 |
| **`COST-2`（多资源原语）** | 台账 | **改判**：不再是"框架待建原语"，而是"**宿主用策略自答**"⇒ 见 D7 | R2 |

> **本次改点的元教训（建议入 `LOG-DECISIONS`）**：v1 我按"框架要能表达 AND/OR/if-else"设计了三层组合结构，用户一句"宿主自己实现就好了"把它砍成一层策略。
> **判据**：**"多条消耗怎么组合"是宿主的玩法结论，不是框架的形状**——正是主判据（"凡是需要宿主词汇的内置策略，框架一律不提供"）的又一实例。
> **我 v1 的错**：把"框架要能表达"误当成"框架要内置"；正确的做法是**给一个扩展点**（策略），而不是**内置一套组合代数**。

---

## 0.3 ★ 两处我必须先纠正**我自己上一轮**的判断（诚实登记）

### 纠正一：我反对"宿主可替换内建档"（B 案）的理由**不成立**

- **我上一轮说**：让宿主改写内建的 `ResourceAttr` 路径会打破"唯一实现 / TOCTOU 不变量"，并制造"第二真相"。
- **该论证混淆了两件事**：

  | | 我当时的说法 | 真正的不变量 |
  |---|---|---|
  | 内容 | "全局只有**一份函数实现**" | "门禁与提交对**同一次施法**用**同一个权威**" |

- **策略模式完整保留后者**：门禁⑤ 与提交点在**同一个策略对象**上调 `CanAfford` / `CommitCost` ⇒ 判的数与扣的数仍一致，**由构造保证**。
- **而"第二真相"那条反过来打我自己**：把 `ResourceAttr` 写死**同时**另开一个自定义路径 ⇒ **两条会分叉的路**。R4 让内建档**也走同一套**，恰恰**消掉**了它。**用户的方案比我的 A 案更干净。**
- **连带简化**：我 v1 设计的 `FTcsCostPlan`（"门禁-提交计划一致性"）**整个不需要**——"选哪条"变成策略的内部事务。

### 纠正二：我关于 `BeginBatch` 原子性的论断**有一半是错的**（已源码核实）

- **我 v1 说**：用 `BeginBatch`/`Commit` 包成本可同时给"全有全无"与"防重入"。
- **源码核实结果（`TcsAttributePipeline_Cascade.cpp:96-104` / `TcsAttributePipeline.cpp:122-158`）**：

  ```cpp
  // SetBaseValue：写入是【立即】的，批【不】推迟它
  Instance->BaseValue = NewBaseValue;
  Instance->bDirty = true;
  if (Store->BatchDepth == 0 && PushEvalStack(Attribute)) { Recalculate(...); }  // 仅"重算+广播"被推迟
  ```

  | 批**确实**给的 | 批**不**给的 |
  |---|---|
  | ① **重算与广播推迟到最外层 `Commit`**（`BatchDepth` 嵌套计数，内层提交直接返回） | ❌ **回滚 / 撤销写入**——`BaseValue` 当场就改了 |
  | ② **订阅者不会观察到中间态**（批内不广播）⇒ **重入窗口关闭** | |
  | ③ 批内 `EvaluateCurrent` 读旧值、`PeekPending` 只算不写 | |

  ⇒ **"全有全无"不能只靠批**，必须配 "**先全判、再逐条扣**"：先对全部待扣项判 `CanAfford`，全过才进批扣。**两者合起来才是真原子**（对观察者而言）。
- **修正后的 D6 形状**（本轮据此实施）：
  1. **先判**：策略的 `CanAfford` 必须**先**答"整体付不付得起"（对 `_ResourceAttr` 是单条；对宿主的"三消耗"策略由宿主自答——**这是宿主策略的责任，框架无法代判**）；
  2. **后扣**：通过 ⇒ `BeginBatch` → `CommitCost` → `Commit`（**单次 flush + 无中间态可见**）；
  3. **失败**：`CanAfford` 不过 ⇒ **根本不进批**（零写入）——这条才是"全有全无"的**实际保障**，而非批本身。
- **★ 该发现顺带确证了 R8 的必要性**：`BeginBatch` / `Commit` **都没有 `UFUNCTION`**（已核实：`TcsAttributeSubsystem.h:285` / `:288` 前无宏，全文件仅 10 处 `UFUNCTION`）⇒ **脚本宿主根本调不到批**。框架包住，脚本宿主才能拿到"无中间态"这一保障。**这是 R8 的硬判据，不只是便利。**

---

## 0.4 待实施时确认的裁定（**本计划的复核面**）

> 每条给"我推荐 + 判据 + 代价"。**复核方式 = 逐条否决或确认**；被否决的留痕于 `decisions-log`。

### D1 形状（按 R1 起草）

```cpp
// 时机：None = 一定没有消耗（R5）；无 Custom 档（R6 定名为 Manual）
UENUM(BlueprintType) enum class ETcsCostTiming : uint8
{
    ECT_None = 0,            // 无消耗（默认；后面一概不管）
    ECT_OnCastStarted = 1,   // 门禁⑤通过后、建 run 之前
    ECT_OnCastCompleted = 2, // 施法走完全部时段/瞬发完成时
    ECT_Manual = 3,          // 框架不自动提交，宿主调 PayCost
};

USTRUCT(BlueprintType) struct FTcsSkillCostConfig
{
    ETcsCostTiming Timing = ETcsCostTiming::ECT_None;  // 默认无消耗（与换型前逐字同义）
    FInstancedStruct CostMode;                          // 策略实例（meta=(BaseStruct=…)，见下）
};

// 策略基类（USTRUCT 虚分派；"抽象"由约定：中性默认实现 + meta=(Hidden)）
USTRUCT(meta = (Hidden)) struct FTcsSkillCostPolicy
{
    virtual ~FTcsSkillCostPolicy() = default;
    virtual bool CanAfford(const FTcsSkillCostContext& Ctx, FString* OutReason) const { return true; }
    virtual bool CommitCost(const FTcsSkillCostContext& Ctx, double* OutApplied) const { return true; }
};

// 内建策略（R4：同样遵循该方案）—— 自己带自己的字段（"配置即成员"）
USTRUCT() struct FTcsSkillCostPolicy_ResourceAttr : public FTcsSkillCostPolicy
{
    FGameplayTag ResourceAttr;   // 宿主本体论：框架只给空槽位
    FTcsParamValue Amount;       // 上一批已换型，原样保留
    // CanAfford/CommitCost：原 ResolveEffectiveCost 的"基数求值 ⊕ CostAmount 键折叠"
    // + OutBaseAmount（负基数=漏洞形态 vs 折叠转负=合法免费 MUST 继续分开）原样搬入
};

// 脚本面转发器（R7）
USTRUCT(BlueprintType) struct FTcsSkillCostPolicy_HostDelegate : public FTcsSkillCostPolicy
{
    TScriptInterface<ITcsSkillCostPolicyHost> Host;  // 空 = 配置缺口（守卫，不崩溃）
};
```

- **`CostMode` 的类型收窄**：用 `meta = (BaseStruct = "/Script/TcsSkill.TcsSkillCostPolicy")`（裸 `FInstancedStruct` + 手写元数据——**不是** `TInstancedStruct<T>`：后者在脚本层导出为空壳，本仓 2026-09-24 已换型）。
- **★ 命名一处（我的取舍，可否决）**：你写的是 `FTcsSkillCostPolicy` + `FTcsSkillCostMode_ResourceAttr`——`Policy` / `Mode` 两个词根并存会让后人猜"哪个是基类"。我建议**统一 `Policy`**：`FTcsSkillCostPolicy_ResourceAttr`（与仓内 `FTcsParamSource_*` / `FTcsParamSource_HostDelegate` 的现成命名法一致）。**字段名仍可叫 `CostMode`**（它答"用哪种模式"），或一并改 `CostPolicy`。
- **★ 时机枚举名（我的取舍，可否决）**：你写 `ETcsSkillCostTiming`，而现名 `ETcsCostTiming`、冷却侧现名 `ETcsCooldownTiming`。**我建议保留 `ETcsCostTiming`**——加了 `Skill` 前缀而冷却侧不加，会**打破这一对的对称**（两者是同批交付的姊妹枚举）。**若你要 `Skill` 前缀，我建议同批把冷却侧也改成 `ETcsSkillCooldownTiming`**（代价：一处已交付公开枚举改名，零资产影响）。

### D2 ★ 策略上下文的形状（**本轮唯一需要新设计的结构**）

- **推荐**：
  ```cpp
  // 纯 C++ 结构（非反射）——只为 USTRUCT 虚分派路径服务
  struct FTcsSkillCostContext
  {
      UTcsSkillSubsystem* Subsystem;
      FTcsCombatEntityHandle Entity;
      const FTcsLearnedSkillEntry* Entry;  // 减免折叠 + 等级都从这里取
      int32 Level;
  };
  ```
- **判据**：策略需要"实体 + 减免修正器 + 等级"三样，而这三样恰好都在 `Entry` 与 `Entity` 上。**`Subsystem` 必须给**（`_ResourceAttr` 要解析 `UTcsAttributeSubsystem`）。
- **为什么纯 C++ 可以**：`USTRUCT` 虚函数**不受 UHT 签名面约束**（只有 `BlueprintNativeEvent` 受）⇒ 上下文**可以**持裸指针与非反射类型。**这是本轮一处刻意的分层**：
  - **C++ 面**（`FTcsSkillCostPolicy` 虚分派）：可用富类型（`FTcsLearnedSkillEntry*`）；
  - **脚本面**（`ITcsSkillCostPolicyHost`）：**MUST** 只用已反射类型 ⇒ 传 `FTcsSkillEntryHandle`，由**转发器**负责"句柄 → 解析条目 → 富上下文"的翻译。
- **一处如实登记**：`FTcsSkillCostContext` 若日后要过网或作 `UPROPERTY`，须反射化——今天**不需要**（策略实例归 Def 资产、求值瞬时不跨帧）。

### D3 ★ 脚本面契约的形参（D8 的硬约束在此生效）

- **推荐**：
  ```cpp
  UINTERFACE(MinimalAPI, Blueprintable) class UTcsSkillCostPolicyHost : public UInterface { GENERATED_BODY() };

  class ITcsSkillCostPolicyHost
  {
      // 检查（纯查询语义：MUST NOT 扣任何东西）
      UFUNCTION(BlueprintNativeEvent)
      bool CanAfford(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, FString& OutReason);

      // 提交（返回是否成功；出参 = 实际扣减量）
      UFUNCTION(BlueprintNativeEvent)
      bool CommitCost(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, double& OutApplied);
  };
  ```
- **判据**：① 形参**不带 `const`**（`BlueprintNativeEvent` 上的 `const` 会让 UHT 生成错误的 thunk 签名——硬规则，先例 `ITcsEntityLevelProvider::GetEntityLevel` / `ITcsParamTableReader::TryGetNumericParam` / `ITcsCastStateQuery`）；② **`Entry` 用 `FTcsSkillEntryHandle`**——`FTcsLearnedSkillEntry` 是**非反射** struct（台账 `SCRIPT-10`），作形参是 **UHT 硬错误**（同 `CQM_Custom` 被迫传 `FTcsCastRunHandle` 而非 `FTcsCastRun`）；③ `FTcsCombatEntityHandle` / `FTcsSkillEntryHandle` 均已反射 ⇒ 可直接用，**无需**选择器族那套"传句柄 + 门面访问器"。
- **一处该接口**做不到**的事（如实登记）**：它**无法**表达"这条自定义消耗要扣多少"的**参数化数值**——脚本宿主要自己持有数值（或从自己的数据里取）。若日后要"脚本策略 + `FTcsParamValue` 数值"，那是**另一层**（届时可让 `_HostDelegate` 也带一个 `FTcsParamValue`）。**本轮不预建。**

### D4 `Timing == None` 与"策略存在"的关系（R5 的精确化）

- **推荐**：`Timing == ECT_None` ⇒ **完全不看 `CostMode`**（不调策略、不发事件、零日志）。`CostMode` 空但 `Timing != None` ⇒ **配置错误 ⇒ fail-closed**（拦住 + `Warning`）。
- **判据**：R5 的原话是"只要 CostTiming 为 None，那就不用管后面了"——**"不用管"** ≠ "空策略 = 不消耗"。后者会让"忘了填策略"表现成"不消耗"（**本仓反复在拦的静默形态**：`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72` 三处措辞几乎逐字相同："不静默通过：静默会让'条件写错'表现成'步骤照跑'"）。
- **⇒ 区分点**：**"声明不消耗"（`Timing = None`，零日志）**与**"想配消耗但没配清"（`Timing != None` 而 `CostMode` 空，拦住 + `Warning`）**——两件事，MUST NOT 混成一类。

### D5 三个提交点的接缝（沿用上一批，逐条不变）

- `ECT_OnCastStarted`：门禁⑤ 通过后、建 run 之前；
- `ECT_OnCastCompleted`：施法走完全部时段 / 瞬发当刻完成时；
- `ECT_Manual`：框架不自动提交，宿主调 `PayCost`（内含 `CanAfford` 重查 = TOCTOU 防护）。
- **`ECT_OnEvent` MUST NOT 补回**（2026-10-12 用户裁定：档位 4→3，理由 = 框架只认识"施法这个事务自身的边界"，"某个特定事件"是项目词汇）。
- **门禁⑤ 恒在 `TryActivate`；每个提交点各自重查一次**（TOCTOU 防护）——**策略化之后这条不变量不变**，只是"重查"改为"再调一次同一个策略的 `CanAfford`"。

### D6 原子性（**按 §0.3 纠正二实施**）

- **形状**：`CanAfford` 全判通过 ⇒ `BeginBatch(Entity)` → `CommitCost` → `Commit(Entity)`；`CanAfford` 不过 ⇒ **不进批**（零写入）。
- **批的边界 MUST 恰好覆盖本策略的扣减**，MUST NOT 圈进施法激活的其它属性改动（如 `MaterializeParamChainRows`）。
- **如实登记（MUST 写进实现注释）**：批给的是"**单次 flush + 订阅者看不到中间态**"（⇒ 关闭重入窗口），**不是回滚**。"全有全无"的实际保障 = **先判后扣**。
- **一处跨实体限制（如实登记）**：`BeginBatch` / `Commit` 形参是**逐实体**的。宿主的"三消耗"若跨实体（施法者付蓝、目标付血），**框架包的那一层覆盖不到第二个实体** ⇒ 该场景宿主须自行组织（且脚本宿主调不到批）。**本轮不做、不入册**（无需求）。

### D7 `COST-2`（多资源）的改判

- **原登记**：待建"成本组"新原语 = `FTcsCostGroup{TArray<FTcsCostConfig>, ERequireMode}`。
- **改判**：R2 ⇒ 多资源**不是框架原语**，而是"**宿主写一个策略**"。
- **处置建议（三选一，请复核时表态）**：
  - **(a) 记录为"✅ 已闭合（改判 = 宿主策略自答）"**（不出表，同本册惯例）；
  - **(b) 保留为"待触发"**，但把**形状**改为"宿主策略示例"（即 `COST-2` 变成"要不要在 LAC 侧提供一个多资源策略样本"）；
  - **(c) 保留原状**，等真有内容需求再裁。
- **我推荐 (a)**：R2 已给出"框架该做什么"的答案（给扩展点，不给组合代数），而**扩展点由本轮 `COST-3` 交付** ⇒ `COST-2` 追的"新原语"已被**明确否决**，留"待触发"会让后人以为还要建。**(b) 的"LAC 侧样本"是另一件事**（样本是宿主内容，不是 TCS 缺口）——若你要样本，我建议**新开一条**而非改造 `COST-2`。

### D8 错误处置面（沿用上一批，逐条不变）

- **配置写错 fail-closed**（无效 `ResourceAttr` / 负基数 / 未持有属性 ⇒ 拦住 + `Warning`）；
- **"不消耗"零日志**（= `Timing == None`）；
- **负基数 vs 减免过度 MUST 继续分开**（`OutBaseAmount` 机制搬入 `_ResourceAttr`，判据原样保留）；
- **"付不起"是正常业务路径** ⇒ `Log` 级、**不是红字**（同 `AttributeCompare` 的处置："不通过 + 一条日志"）；
- **`CustomFragment` 为空**（策略未接上）⇒ **拦 + `Warning`**（**收紧**：换型前该档是**免费放**，见 §0.5）。

---

## 0.5 本轮顺带修掉的一个真缺陷形态（独立于策略化）

**现行 `ECP_CustomFragment` 的降级是"免费放"**：`CanAffordCost` 恒 `true` + `Warning`、`CommitCost` 零扣减。

**而同一个文件下面**（"属性门面不可得"一节）原文写着：

> 资源都没法查就放技能，等于免费放——那是"**配置了消耗却静默不生效**"的最坏形态

⇒ **同一个失败类别，一处拦住、一处放过。**

**MUST NOT 与 `CQM_Custom` 的降级简单类比**（一处真实差异）：`CQM_Custom` 回退到 `DefSwitches` = **一个有定义的行为**；本档回退 `None` = **免费**，**不是一个安全的默认**。

**本轮处置**：策略化之后该档**消失**（真实现取代降级），且 `Timing != None` 而策略为空 ⇒ **fail-closed**（D4/D8）⇒ **洞被堵上**。

---

## 0.6 非目标（本轮**不做**，各有归属）

| 不做的事 | 归属 / 判据 |
|---|---|
| **`All` / `Group` / 多资源组合** | R2：宿主写策略自答（不是框架原语） |
| **递归 / 嵌套组** | R2 同上；`FTcsSkillCostPolicy` 的虚分派天然允许宿主内部任意递归，框架不参与 |
| **多实体成本**（跨实体扣减） | 无需求；且 `BeginBatch` 逐实体 ⇒ 框架包不住（D6 已如实登记） |
| **脚本策略的 `FTcsParamValue` 数值通道** | D3 已登记"另一层"，本轮不预建 |
| **`ECT_OnEvent` 档** | 2026-10-12 已删除，**MUST NOT 补回** |
| **"中性条件层"重构**（v1 D2 的遗留） | v1 的 `Conditional` 已作废 ⇒ 本轮**不需要**条件载体，该项**随之消失** |
| **`R6.5-2` 批次 `e` / `g`**（**★ 2026-10-13：`f` 已删除**——与 `R5.5-f` 同一工作量，收敛为一个编号，见 `GLOSSARY` §2.1） | 与本轮无关，MUST NOT 混编 |
| **冷却侧任何改动** | 已收束 |

---

## 0.7 前提核实（已实测，非推断）

| 前提 | 实测结果 |
|---|---|
| 存量资产是否需迁移 | **不需要**：`Content/` 下 4 个 `DA_SkillDef_*` 逐个字节扫描 `Cost` / `ResourceAttr` / `Cooldown` **全部零命中** |
| `BeginBatch`/`Commit` 是否给"全有全无" | **不给**（**纠正了我自己的论断**）：`SetBaseValue` 的写入**立即**生效，批只推迟"重算 + 广播" ⇒ "全有全无"须靠**先判后扣**（§0.3 纠正二） |
| `BeginBatch`/`Commit` 能否被脚本调到 | **不能**：两者均**无 `UFUNCTION`**（`TcsAttributeSubsystem.h:285` / `:288`）⇒ **确证 R8 的必要性** |
| `USTRUCT` 策略基类能否持富类型形参 | **能**：UHT 签名面约束只作用于 `BlueprintNativeEvent`；C++ 虚函数可自由取 `FTcsLearnedSkillEntry*`（⇒ D2/D3 的两层分工成立） |
| 脚本面转发器先例 | **有**：`FTcsParamSource_HostDelegate`（`TcsParamSourceHost.h`，`UINTERFACE` + `TScriptInterface` + 空 Host 守卫 + `Execute_*`） |
| `FInstancedStruct` 收窄先例 | **有**：`FTcsStateStackPolicy::CustomDecision`（裸 `FInstancedStruct` + 手写 `meta=(BaseStruct=…)`） |
| `openspec` 现状 | **42 passed / 0 failed**、能力 **42**、活跃变更 **0**、台账 **74** |

---

## 1. Tasks

**提案切分建议**（沿用同域先例的粒度）：

| Task | 提案 id | 交付 |
|---|---|---|
| 0 | `add-cost-policy-types` | 类型层换型（`ETcsCostTiming` + `FTcsSkillCostConfig` + 策略基类 + `_ResourceAttr`）+ 删旧枚举/旧类型 |
| 1 | `add-cost-policy-host-slot` | `ITcsSkillCostPolicyHost` + `_HostDelegate` 转发器 |
| 2 | `add-cost-policy-dispatch` | `FTcsCostOps` 改走策略 + 三提交点 + 批（原子性）+ fail-closed |
| 3 | `verify-cost-policy` | 装置扩面 + 双路线取证 |
| 4 | —（收束） | 归档 + 文档回写 + 台账闭合 |

### Task 0 — 类型层换型（`add-cost-policy-types`）

- **交付**：`ETcsCostTiming` 四档（`None` 默认）；`FTcsSkillCostConfig{Timing, CostMode}`；`FTcsSkillCostPolicy`（基类，中性默认 + `meta=(Hidden)`）；`FTcsSkillCostPolicy_ResourceAttr`；`FTcsCostContext`（D2）。**删除** `ETcsCostPolicy` / `FTcsCostConfig` / `ECP_*` 三档。
- **Step**：
  1. `TcsCostTypes.h` 重写（新类型住同文件，避免 UHT"全项目头文件名唯一"）；
  2. `FTcsSkillDefData::Cost` 换型；
  3. **删旧类型并留痕**：`ETcsCostPolicy` / `FTcsCostConfig` 的"刻意不对称"段落**划改**（MUST NOT 静默删——它记录了当时的判据，而该判据本轮过期，**痕迹本身有价值**）；
  4. `_ResourceAttr` 的策略体：**原样搬入** `ResolveEffectiveCost` 的"基数求值 ⊕ `CostAmount` 折叠"与 `OutBaseAmount` 判据（**一处逻辑都不重想**）；
  5. **MUST 确认 `meta=(Hidden)`**：基类不可被 picker 选中（先例 `FTcsParamValueSource` / `FTcsStateStackDecisionFragment`）；**禁 `=0` 与 `PURE_VIRTUAL`**（UHT 为每个 USTRUCT 无条件生成 `TCppStructOps`，纯虚基类报 `C2259`；且 `PURE_VIRTUAL` 在 Development/Shipping 行为不同——最坏的一类失败）；
  6. `TcsCostOps.h/.cpp` 最小改动保编译门（策略化分派在 Task 2）。
- **验收**：双配置编译 0/0；UHT 产物 `NewProp_*` 确认 `CostMode` / `Timing` / `ResourceAttr` / `Amount` 是**真反射字段**（按"假字段"判据）；`openspec validate` 通过。

### Task 1 — 脚本面宿主插槽（`add-cost-policy-host-slot`）

- **交付**：`UTcsSkillCostPolicyHost` / `ITcsSkillCostPolicyHost`（D3 签名）；`FTcsSkillCostPolicy_HostDelegate`（转发器）。
- **Step**：
  1. 接口两头（`UINTERFACE(MinimalAPI, Blueprintable)` + `BlueprintNativeEvent`；**形参不带 `const`**；`Entry` 用 `FTcsSkillEntryHandle`）；
  2. 转发器：`TScriptInterface` 持有 + **空 Host 守卫**（返回中性值，不崩溃/不 ensure——同 `FTcsParamSource_HostDelegate`）；
  3. **MUST 用 `Execute_*` 而非虚表直调**（虚表直调会**静默跳过**脚本层实现，`TcsParamSourceHost.h:116` 的既有判据）；
  4. 转发器负责"`FTcsSkillEntryHandle` → 解析 `FTcsLearnedSkillEntry` → 富上下文"的翻译（D2/D3 的分层落点）。
- **验收**：双配置编译 0/0；C# glue 侧两个方法**有非空壳导出**（判据 = "能导出 ≠ 能往返"，故 Task 3 装置里**必须有**一个脚本/装置侧的往返取值）；`openspec` 通过。

### Task 2 — 策略分派 + 三提交点 + 原子性（`add-cost-policy-dispatch`）

- **交付**：`FTcsCostOps` 全部函数改走策略；`CanAffordCost` / `CommitCost` / `PayCostForEntry` 接策略；`BeginBatch`/`Commit` 包策略提交；**删掉两处降级 `Warning`**。
- **Step**：
  1. **`Timing == None` 短路**（不调策略、不看 `CostMode`、零日志——R5）；
  2. **`Timing != None` 而 `CostMode` 空 ⇒ fail-closed**（拦 + `Warning`——D4，**这是收紧，MUST 写进规格**）；
  3. 策略解析：`CostMode.GetPtr<FTcsSkillCostPolicy>()` **判空**（`FInstancedStruct` 的裸载体形态：`GetPtr<T>()` 单参数版会做 `IsChildOf` 校验、不符返回 `nullptr` ⇒ **一个 `if (!Ptr)` 同时覆盖"载荷缺失"与"类型不符"**；**MUST NOT 用双模板参数重载**——它在 Shipping 下不校验、「this may crash in shipping」）；
  4. 门禁⑤：`Policy->CanAfford(Ctx, &Reason)`；**`OutReason` 用具名原因**（宿主排查最需要的读数）；
  5. 提交：`CanAfford` 通过 ⇒ `BeginBatch` → `Policy->CommitCost(Ctx, &Applied)` → `Commit`（**批的边界 = 紧邻本策略的提交**，注释 MUST 写明 D6 的"先判后扣才是全有全无"）；
  6. `PayCostForEntry`：**内含 `CanAfford` 重查**（TOCTOU 防护统一入口）——自动档 **MUST NOT** 走它（自动档直调 `CommitCost`）；
  7. `BroadcastCostApplied`：载荷 `Amount` = **实际扣减量**（`CommitCost` 的出参），**MUST NOT** 取配置基数（"发出扣了 30 而实际扣了 21"会直接误导订阅者）；
  8. **删两处降级**（`TcsCostOps.cpp` 的 `CanAffordCost` / `CommitCost` 里 `ECP_CustomFragment` 两个分支）与它们注释里的 `SCRIPT-9` / 降级措辞。
- **验收**：双配置编译 0/0。

### Task 3 — 装置扩面 + 双路线取证（`verify-cost-policy`）

- **交付**：`TcsDevSliceRig_CooldownCost` 的 `K` 主判据 + `.Reject` 的 `k` 面。
- **必须钉住的可观测行为（缺一即不算过）**：
  1. **`Timing == None` ⇒ 零扣减 + 零日志**（"不消耗"的判据）；
  2. **`Timing != None` + 空 `CostMode` ⇒ 拦 + `Warning`**（D4 的拒绝面）；
  3. **`_ResourceAttr` 正常扣减**（读数与配置一致）；**`CostAmount` 减免仍生效**（百分比 + 固定值两带）；
  4. **负基数 ⇒ 恒不可付**（拒绝面，`OutBaseAmount` 判据的回归）；
  5. **宿主自定义策略被真实调用**（**这是 `COST-3` 闭合的核心判据**）：装置写一个**最小宿主策略**（住 `TcsDev`），实测 `CanAfford` / `CommitCost` **各被调到**，且**提交量由策略决定**（不是框架）；
  6. **脚本面往返**（若能配 C#）：`_HostDelegate` 的 `Execute_*` 真的走到宿主实现（**MUST NOT 只验"能导出"**——见 Task 1 的判据）；
  7. **批的效果**：策略提交期间**订阅者不收到中间态**（判据 = 批内 `ValueChanged` 广播**恰一次**，且在两属性都改完之后）；
  8. **R6 回归 `SkillLayer.Run` 24/0** 且 `[复现性摘要]` **逐字同基线**。
- **路线**：无头 `UnrealEditor-Cmd`（同一 `TcsDev.dll`）+ 编辑器 PIE，**各取一轮**；双配置编译。

### Task 4 — 收束归档

- **交付**：`SPEC-04-skill` §2/§3.6/§11 回写；`skill-cost` 规格 delta（**MODIFIED 须粘贴完整需求原文**，`openspec` 的既定陷阱）；台账 **`COST-3` 闭合** + **`COST-2` 按 D7 处置**；`INDEX` / `GLOSSARY` / `LOG-DECISIONS`（含 §0.2 的元教训 + §0.3 的两处自我纠正）/ `implementation-log` 同步；**计数声称全库同批回改**（`INDEX` §5 纪律）；证据档 `EVID-2026-10-1X-cost-policy`。
- **验收**：见 §2。

---

## 2. 验收门（本轮"完成"的定义）

- [ ] Task 0~4 全勾；提案归档（或按实际切分）；
- [ ] `openspec validate --all --strict --no-interactive` **0 failed**；活跃变更 **0**；
- [ ] 双配置（`UnrealEditor` / `UnrealGame`）编译 **0 error / 0 warning**；
- [ ] 无头 + PIE **两条路线**读数一致；Task 3 的 8 条可观测行为**全 PASS**；
- [ ] **`COST-3` 闭合的硬判据**：宿主自定义策略的 `CanAfford` / `CommitCost` **各被真实调用一次以上**，且提交量由策略决定；
- [ ] R6 回归 `SkillLayer.Run` 24/0 且 `[复现性摘要]` 逐字同基线；
- [ ] 台账 `COST-3` 闭合 + `COST-2` 按 D7 处置；计数声称全库同批一致；
- [ ] 编码：全部改动文件 LF、无 BOM（`openspec` + `Documents` + `Source`）；
- [ ] **零热补丁、同一基线 DLL**（上一轮的教训：`UnrealEditor-Cmd` 与 PIE 加载**同一个** `TcsDev.dll`，两条路线 MUST NOT 各跑半份代码）。

---

## 3. 风险与如实登记

| 风险 | 处置 |
|---|---|
| 换型触碰已交付公开类型（3 个归档提案 + `skill-cost` 规格 + 装置） | 迁移成本 **0**（§0.7 实测）；引用点逐处改；`openspec` 归档做**逐能力零丢失预检** |
| **"全有全无"若只靠批 ⇒ 是假保障** | §0.3 纠正二已源码核实：批不给回滚 ⇒ 实现 MUST 是**先判后扣**；Task 3 判据 7 从行为面兜底 |
| 脚本宿主**调不到批** ⇒ 它的原子性弱于 C++ 宿主 | 框架包住（R8）⇒ 两条路径**同等**；但跨实体场景框架包不住（D6 已登记） |
| `FInstancedStruct` 双模板参数重载在 Shipping 下不校验类型 | **MUST 用单参数 `GetPtr<T>()`**（`TcsParamSourceHost` / 本仓既有纪律） |
| `meta=(Hidden)` 漏写 ⇒ 基类可被 picker 选中 | Task 0 Step 5 明确要求；UHT 产物核对 |
| `PURE_VIRTUAL` / `=0` 会让 Development 与 Shipping 行为分叉 | **禁**；用中性默认实现（先例 `FTcsParamValueSource`） |
| 策略对象住 Def 资产 ⇒ 运行期不可换 | **这是刻意的**（配置即成员；同 `FTcsStateStackPolicy::CustomDecision`）——若宿主需"运行期换策略"，那是另一项需求 |
