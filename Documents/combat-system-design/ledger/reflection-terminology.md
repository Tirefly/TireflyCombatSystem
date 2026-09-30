# TCS「反射」术语规约（消歧）

- **文档 ID**：`LEDGER-terminology`
- **类型**：LEDGER / 规约
- **状态**：LIVING
- **权威范围**：「反射」一词五义的唯一处方措辞与收敛记录
- **最后更新**：2026-09-24

- 建立：2026-09-24（用户质询"为什么 `effect-chain/spec.md` 规定 `FTcsEffectContext` MUST NOT 为反射类型"时暴露）
- 性质：**活文档 / 措辞规约**——规定"反射"相关词汇在 TCS 规格、设计文档与代码注释中的**唯一处方措辞**
- 适用范围：`openspec/specs/`、`openspec/changes/`、`openspec/project.md`、`Documents/combat-system-design/`、`Source/**` 头注释
- 纪律：**新增文本 MUST 用 §3 的处方措辞**；存量文本按"高风险优先"逐步收敛（§5 记录已收敛项与待收敛项）

---

## 1. 问题：一个词，五种含义

全仓勘察（2026-09-24，覆盖 `openspec/` + `Documents/` + `Source/`）统计「反射」词元 **613（md）+ 187（Source）≈ 800 处**，实际承载 **5 种互不相同的含义**：

| 义项 | 含义 | 典型落点 |
|---|---|---|
| **A** | **类型反射可见性**——`USTRUCT()` / `UCLASS()` / `UENUM()` 宏让类型进 UE 反射系统（`StaticStruct()` 可用、UHT 生成代码、可作 `UPROPERTY`） | `反射可见` / `反射化` / `反射类型` / `非反射` / `不可反射` / `不进反射面` |
| **B** | **脚本/蓝图可调用性**——`UFUNCTION()` 让方法可经 `UFunction::Invoke` 调用（脚本层可达、`Execute_`） | `反射面` / `反射可达` / `反射注册入口` / `反射分派` / `反射调用` |
| **C** | **蓝图暴露面 / 可配置性**——`BlueprintType` / `BlueprintAssignable` / `EditAnywhere` 让类型或字段可作蓝图变量、可被编辑器配置 | `A' 反射面` / `蓝图承诺面` / `全反射可配` |
| **D** | **运行期类型分派**——以 `const UScriptStruct*` 作注册表键查表分派 | `反射类型`（作键）/ `按反射类型查表分派` |
| **E** | **序列化 / 复制属性面**（及其他）——引擎 FProperty 表、复制粒度、`反射层视为同一物`、`反射工具箱`（与 UE 反射无关的第四义） | `反射属性` / `反射粒度` / `反射描述符` / `反射工具箱` |

**A / B / C 三义高密度交织**，且已在**规格真相源**里产生两处自相矛盾（见 §4）。

### 1.1 为什么危险（不是"用词不雅"，是会读错规格）

- **同句两义**：`openspec/specs/effect-trigger/spec.md「触发行登记表与订阅生命周期」`「调用成立（**方法反射可见**、形参类型 `FTcsEffectTriggerInstance` **反射可见**）」——前半 B、后半 A。而 `BlueprintNativeEvent` 的形参校验恰恰要求**两者同时成立**；读成同一件事就会漏掉类型侧约束。
- **同词反义**：`openspec/specs/event-bus/spec.md「BP/CS 动态监听面」` 的 `A' 反射面` = **蓝图可绑定**（C）；`openspec/specs/effect-interpreter/spec.md「门面反射面（脚本层可达）」` 的 `门面反射面` = **蓝图不可见**（B，`UFUNCTION()` 无 specifier）。两处都写"R0 §9 蓝图不承诺"，词面无法区分。
- **判据被压扁**：`openspec/specs/effect-step-dispatch/spec.md「步骤执行器插槽」`「键（`UScriptStruct*`）**可反射**，值（`TFunction`）**不可**」——键的"可反射"是 D（类型身份可作查表键），值的"不可反射"是 A（UHT 无法生成属性）。同一形容词指两个维度，而这正是 SCRIPT-2/SCRIPT-8 讨论的**核心判据**。
- **同句正反对举**：`effect-interpreter`（SCRIPT-8 delta）「该 struct 是**非反射**纯 C++ 类型，且含 `FInstancedStruct` `EventPayload`（**反射**）」——前指"无 `USTRUCT` 宏"（A 否定），后指"该字段类型可承载"（A 肯定）。

---

## 2. 义项之间的真实关系（判据表）

消歧的前提是承认：**这五个义项彼此独立，不能互相推导**。

| | A 类型可见 | B 方法可调 | C 蓝图可配 | D 可作分派键 |
|---|---|---|---|---|
| **A 蕴含谁** | — | ❌ | ❌ | ✅（有 `StaticStruct()` 才能作键） |
| **B 蕴含谁** | ✅（形参类型必须反射可见） | — | ❌ | ❌ |
| **C 蕴含谁** | ✅（`BlueprintType` 蕴含 `USTRUCT`） | ❌ | — | ❌ |
| **D 蕴含谁** | ✅ | ❌ | ❌ | — |

**三条最常被误推的关系**：

1. **A ⇏ C**：`USTRUCT()` ≠ 可配置。`FTcsChainRunHandle` 就是 `USTRUCT(BlueprintType)` 但零 `EditAnywhere`；`FTcsTriggerContext` 是 `USTRUCT()` 非 `BlueprintType` 且只在栈上存在。**反射可见的类型完全可以禁止进资产**——这两件事由 `UPROPERTY` specifier 与容器位置决定，不由类型宏决定。
2. **A ⇏ B**：类型反射可见 ≠ 方法可被脚本调。方法要 `UFUNCTION()` 才可调；**且 `BlueprintNativeEvent` 还要求形参类型满足蓝图参数校验**（`BlueprintType` 或原生支持类型）——这是 A 与 C 在 B 处的**联合**约束。
3. **"含不可反射成员"是 A 的递归否定**：`TFunction` / `TMap` / 裸 C++ struct 作成员会让**宿主类型**无法加 `USTRUCT()`（UHT 报错）——这是 A 的**传递闭包**，与 B/C 无关。

---

## 3. 处方措辞（唯一真相源）

**新增文本 MUST 按本表选词。禁止使用裸「反射面」「反射可见」「反射层」「反射化」而不标注义项。**

| 义项 | ✅ 处方措辞 | ❌ 停用 | 备注 |
|---|---|---|---|
| **A** | `USTRUCT()` / `UCLASS()` / `UENUM()`（直接写宏名）；需要概括时说 **「类型反射可见」** | 「反射类型」（歧义：A 与 D 共用）、「反射化」（歧义：A 与 B 共用） | 否定式用 **「无 `USTRUCT` 宏」** 或 **「纯 C++ struct」**，不用「非反射」 |
| **B** | **「`UFUNCTION()` 脚本可达」**；需要强调蓝图不可见时写 **「`UFUNCTION()` 无 specifier（蓝图不可见）」** | 「反射面」、「反射可达」（可保留，但 MUST 紧跟宏名）、「反射化」（同 A） | 这是本仓最一致的既有措辞（`TcsEffectSubsystem.h` 17 处模板化） |
| **C** | 直接写 specifier：**`BlueprintType` / `BlueprintCallable` / `BlueprintAssignable` / `EditAnywhere`**；需要概括时说 **「蓝图暴露面」** | 「反射面」（同 B 冲突，最危险） | `A' 反射面` 的 `A'` 是历史代号，新文本不再使用 |
| **D** | **「`const UScriptStruct*` 注册键」** 或 **「按 struct 类型分派」** | 「反射类型」（作键时） | 引擎里这叫 type-based dispatch，与"反射"无关 |
| **E** | 直接说 **「`FProperty` 表」** / **「复制属性集合」** / **「序列化粒度」** | 「反射属性」「反射粒度」 | `反射工具箱`（非目标项）保留原样——它与 UE 反射无关，属独立第四义 |

### 3.1 复合情形的写法

**当一句话同时涉及多个义项时，逐项写宏名，不用概括词**：

- ✅ 「`FTcsChainRunHandle` MUST 为 `USTRUCT(BlueprintType)`——`BlueprintNativeEvent` 的形参校验要求形参类型蓝图可表达，故句柄需同时满足类型反射可见（A）与蓝图暴露（C）。」
- ❌ 「句柄 MUST 可反射——`BlueprintNativeEvent` 要求形参全反射。」

**引用既有条文时，先判定义项再引用**——不要因为原文写"反射"就沿用同一个词。

---

## 4. 规格真相源中的两处自相矛盾（MUST 修正）

### 4.1 「反射面」在两条规格里指向互补相反的两面

| 位置 | 原文 | 义项 | 实质 |
|---|---|---|---|
| `openspec/specs/event-bus/spec.md「BP/CS 动态监听面」` | `TcsCore MUST 提供 A' 反射面（Lyra GMS 形态）：动态多播 FTcsOnCombatEvent(...)（BlueprintAssignable...）` | **C** | **蓝图可绑定**（暴露面） |
| `openspec/specs/effect-interpreter/spec.md「门面反射面（脚本层可达）」` | `### Requirement: 门面反射面（脚本层可达）` … `UFUNCTION()` 无 specifier | **B** | **蓝图不可见**（脚本可调） |

两条都在 `openspec/specs/`（真相源），都会被后续提案继承。**已按 §3 处方措辞修正**（见 §5）。

### 4.2 「反射可见」在同句内指两个对象

`openspec/specs/effect-trigger/spec.md「触发行登记表与订阅生命周期」`：方法（B）与形参类型（A）都用"反射可见"。**已修正**。

---

## 5. 收敛记录

### 5.1 已收敛（2026-09-24）

> 原则：**只改措辞，不改任何可执行约束**。每处改动都保持原条文的技术含义不变，仅替换为处方措辞或补注义项。

| # | 位置 | 原措辞 | 收敛后 | 性质 |
|---|---|---|---|---|
| 1 | `openspec/specs/effect-chain/spec.md`（效果链上下文·黑板） | `结构 MUST NOT 为反射类型（纯运行态，非配置数据）` | 拆为两条：①不可作配置数据载体（明列 `BlueprintType` 与资产可编辑面）；②不约束类型反射可见性，脚本直接持有须另行评估（指向 SCRIPT-3） | 消歧（拆捆绑） |
| 2 | `openspec/specs/event-bus/spec.md`（BP/CS 动态监听面） | `A' 反射面` | `BP/CS 动态监听面`（保留 `BlueprintAssignable` 原文） | 消歧（C 义） |
| 3 | `openspec/specs/effect-interpreter/spec.md`（门面反射面） | `门面反射面（脚本层可达）` | `门面脚本可达面` | 消歧（B 义） |
| 4 | `openspec/specs/effect-trigger/spec.md`（脚本层可登记） | `方法反射可见、形参类型 FTcsEffectTriggerInstance 反射可见` | `方法为 UFUNCTION() 脚本可达（B）；形参类型 FTcsEffectTriggerInstance 为 USTRUCT() 反射可见（A）——BlueprintNativeEvent 的形参校验要求两者同时成立` | 消歧（同句 A/B） |
| 5 | `openspec/specs/attribute-store/spec.md`（读侧契约） | `三者为反射可见事件` / `反射面覆写` | `三个方法均为 UFUNCTION(BlueprintNativeEvent)`（脚本可覆写） | 消歧（B 的最差措辞） |
| 6 | `openspec/specs/damage-flow/spec.md`（步骤执行器注册） | `注册键 = 步骤 struct 的反射类型` / `首次查询才解析反射类型` | `注册键 = 步骤 struct 的 UScriptStruct*（按类型分派）` / `首次查询才解析 UScriptStruct*` | 消歧（D vs A） |
| 7 | `openspec/specs/damage-step-library/spec.md`（消耗策略） | `纯 C++、不可反射、不可作 UPROPERTY` | `纯 C++、无 USTRUCT 宏、不可作 UPROPERTY` | 消歧（A） |
| 8 | `openspec/specs/clock-expiry-heap/spec.md`（时间源） | `抽象 C++ 接口，非反射——时间源是引擎管道设施而非 Def 配置数据` | `抽象 C++ 接口（无 USTRUCT 宏，不可作 Def 配置数据）——时间源是引擎管道设施` | 消歧（A/C 拆分） |
| 9 | `openspec/specs/attribute-types/spec.md`（账本类型） | `纯 C++ struct，反射面外` / `不进反射面` | `纯 C++ struct（无 USTRUCT 宏，不进 UHT 类型面）` | 消歧（A） |
| 10 | `openspec/specs/instance-handle-pool/spec.md`（句柄反射性） | `反射载荷` / `反射字段`（5 处混用） | `反射载荷` → `FInstancedStruct 载荷`；`反射字段` → `UPROPERTY 字段` | 消歧（A/E） |
| 11 | `openspec/specs/param-value/spec.md`（求值上下文） | `反射可见求值上下文` / `禁止 TFunction 等不可反射成员` | `USTRUCT(BlueprintType) 求值上下文` / `禁止 TFunction 等无 USTRUCT 宏的成员` | 消歧（A） |
| 12 | `Source/TcsCore/Public/EventBus/TcsEventBusSubsystem.h` | `A' 反射面：BP/CS 绑定入口` | `BP/CS 动态监听面（BlueprintAssignable）` | 消歧（C 义，与 #2 同批） |
| 13 | `Source/TcsCore/Public/Parameter/TcsParamTableReader.h` 等 3 处契约接口 | `TcsCore 反射面；宿主可实现` | `宿主可实现（UINTERFACE + UFUNCTION(BlueprintNativeEvent)）` | 消歧（B 义） |

### 5.2 SCRIPT-8 在飞期的暂缓项（**2026-09-29 阻塞已解除**，逐行判定如下）

> **原阻塞理由**（2026-09-24 记录）：`add-host-scripting-slots` 提案正在修改这些文件（工作树有未提交改动），改同一批文件会造成两处真相冲突；待 SCRIPT-8 收束后按 §3 处方措辞统一。
>
> **2026-09-29 解除**：该提案已归档为 `openspec/changes/archive/2026-09-27-add-host-scripting-slots/`，其 delta 已并入生效规格层（`openspec/specs/`），工作树不再有相关未提交改动。下表逐行给出判定；**归档目录内的旧措辞一律不改**（冻结原则，见 `CONVENTION` §7——归档是历史留痕，改了就成伪造）。

| 位置 | 问题 | 判定（2026-09-29） |
|---|---|---|
| `openspec/changes/archive/2026-09-27-add-host-scripting-slots/specs/effect-step-dispatch/spec.md` | 「键可反射，值不可」（D+A/B 同句）；「`UScriptStruct*` 可作反射形参」 | **不改**——归档冻结；生效版为 `openspec/specs/effect-step-dispatch/spec.md`，以生效版措辞为准 |
| `…/2026-09-27-add-host-scripting-slots/specs/effect-chain/spec.md` | 「反射 + `BlueprintType`」并列（A/C 并置） | **不改**（同上）；生效版 `openspec/specs/effect-chain/spec.md「效果链上下文（黑板）」` 已用处方措辞 |
| `…/2026-09-27-add-host-scripting-slots/specs/effect-interpreter/spec.md` | 「非反射……（反射）」正反对举 | **不改**（同上）；生效版 `openspec/specs/effect-interpreter/spec.md` 已用处方措辞 |
| `…/2026-09-27-add-host-scripting-slots/proposal.md` / `design.md` | 「反射化」「反射可达化」未标义项 | **不改**（同上）——归档提案的标题与正文属历史文本 |
| `TcsEffectSubsystem.h:UTcsEffectSubsystem::UnregisterTriggerRowsBySource` | 「无反射面……非反射……反射层表达不了」一句三义 | **⏳ 待改**（唯一实质残留）——按 §3 处方改写：`无反射面` → `形参含非反射纯 C++ struct（无 USTRUCT 宏），不能作 UFUNCTION 形参`；`反射层表达不了` → `脚本层不可达`。**未在本轮改**：该注释还引用了旧台账编号 `S-1`，须与 `SCRIPT-1` 一并更正，留作独立小项 |
| `Documents/combat-system-design/spec/04-module-effects.md` §5b / `09-module-damage.md` §2.4 / `10-module-targeting.md` | 同批新增文本的「反射面/反射视图/反射化」 | **✅ 已收敛**——三处现行文本已改为处方措辞（`SPEC-03-effects` 等） |
| `RSCH-csharp-authoring` | G-1~G-4 表格「反射化」三义交替、无维度列 | **✅ 已可查**——文件现位于 `research/csharp-authoring.md`（原 `2026-09-23-csharp-tcs-logic-authoring-research.md`）；"加维度列"建议仍待评估，非阻塞项 |
| `Documents/combat-system-design/2026-09-02-r0-rebuild-position-paper.md` | 「反射动态委托」（已自认笼统，见 §6） | 同上 |

### 5.3 明确保留（不改）

| 位置 | 保留理由 |
|---|---|
| `反射工具箱`（`DEC-00-constitution` / `SPEC-00-core` / `DEC-2026-09-02-module-map`——后者今在 `HISTORICAL/dec-2026-09-02-module-map-proposal.md`，旧名系历史引用原文） | 指"通用反射工具库"，属独立第四义（E），与 UE 反射无关；改成别的词反而失真 |
| `反射层视为同一物`（`param-value` 等 6 处） | 引擎官方注释引文（`InstancedStruct.h:TInstancedStruct`），**引文不改** |
| `反射粒度` / `反射描述符` / `反射式 StructNetSerializer`（`2026-09-20-replication-posture-research.md` 全篇） | 复制/序列化领域术语（E），内部一致、无歧义 |
| `反射名`（去 F 前缀）/ `反射符号`（UHT 生成符号） | 精确，无歧义 |

---

## 6. 既有正面样本（可直接引用为范式）

| 措辞 | 出处 | 为什么好 |
|---|---|---|
| **`UFUNCTION()` 无 specifier（蓝图不可见）** | `Source/TcsEffect/Public/TcsEffectSubsystem.h`（17 处模板化） | 一句话同时说清"可调"（B）与"蓝图不可见"（C 的否定），零歧义 |
| **「"反射动态委托"原表述过于笼统，落到具体机制即此」** | `DEC-00-constitution` | **全仓唯一一处对"反射"用词做过的显式自我消歧**——后续统一措辞可援引为范式 |
| **`USTRUCT()` / `USTRUCT(BlueprintType)` 直接写宏名** | `openspec/specs/effect-trigger/spec.md「触发载荷读取器」`、`openspec/specs/instance-handle-pool/spec.md「战斗实体身份句柄与发号」`、`openspec/specs/effect-step-dispatch/spec.md「步骤执行器签名与注册表」` | 宏名本身零歧义，是 A/C 的最优写法 |
| **`反射只读视图` / `可反射数据面投影`** | `openspec/specs/damage-primitive/spec.md「流程委托契约（宿主实现）」`、`TcsDamageFlowContextView.h:FTcsDamageFlowContextView` | 明确"这是一个新造的、反射可见的 struct"（A），无 B/C 歧义 |
| **`模板类型字段不可作 UPROPERTY` ⇒ 句柄必须展平** | `RSCH-csharp-authoring`、`LEDGER-deferred` | 说清约束的来源（`UPROPERTY` 承载性，A）而非笼统说"不可反射" |

---

## 7. 与其他文档的分工

| | 本文档 | `deferred-inputs-ledger.md` | `openspec/project.md` |
|---|---|---|---|
| 内容 | **措辞规约**（词怎么用） | 遗留输入（事要做） | 项目口径（架构决策） |
| 生命周期 | 活文档：新文本 MUST 遵守 | 活文档：轮次开工通读 | 活文档：拍板后更新 |
| 触发 | 写任何涉及"反射"的文本前 | 轮次开工 / 新遗留 | 架构决策变更 |

**与 S 系列的关系**：`ledger` 的 SCRIPT-1~SCRIPT-8 记录**能力建设**（哪些面要打通），本文档记录**表述规约**（打通后怎么写）。两者不重叠——SCRIPT-8 落地"脚本插槽"能力，本文档规定该能力在文档里如何被描述。

- **2026-10-01 引用收敛（`CONVENTION` §5 行号禁令）**：本册 **16 行**改写——源码引用 → `路径:符号名`、openspec 引用 → 路径 + 需求名、`r0`/`ledger`/`csharp-research` 等简称 → `DEC-00-constitution` / `LEDGER-deferred` / `RSCH-csharp-authoring`。**§2 术语分义与 §6 收敛表的判定均未变**，只改引用写法。