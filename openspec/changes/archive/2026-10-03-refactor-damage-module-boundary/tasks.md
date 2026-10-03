# Tasks: 伤害模块边界整肃

> **本提案（变更创建阶段）只交付**：`proposal.md` / `design.md` / `tasks.md` / `specs/**`（2 个 delta 文件）。
> **以下 1–2 节随提案交付**；**3–8 节是"批准后实现阶段"的清单**。
> **禁止提交**：任何 `git commit` / `git push` / `p4 submit` 都不在本清单内（用户级 `AGENTS.md` 最高优先级）；改动只落工作区。
> **跨提案前置**：`reroot-gameplay-tag-vocabulary` MUST **先归档**，本提案在其后归档（两条需求被两提案同时 MODIFY，见 `proposal.md`「跨提案依赖」）。
> **当前状态（2026-10-03）**：**实现完成 35/35，已归档**（`changes/archive/2026-10-03-refactor-damage-module-boundary/`）。`4.2` / `7.1` 经**编辑器 MCP 实查**为**本就满足**（此前记的"未执行"是误判）；`5.7` 由**资产二进制全扫结构性排除**；`5.6` 由**用户实跑三条测试 + 日志留证**结清。**两条超出本清单的验证未跑**（`proposal.md`「检查点」的 4，与 3 的负对照半）——它们不影响归档，但**⑤ 的删除在当前配置下不可观测**，见 `design.md` D4 与 5.6 注释。

## 1. 契约落地（随提案交付）

- [x] 1.1 `specs/damage-primitive/spec.md`——MODIFIED × **3**：「伤害记录与事件」+「流程委托契约（宿主实现）」+「Damage 链步骤与执行器」（**含审计 `R-2` 的更正**；基准 = 换根后文本）
- [x] 1.2 `specs/damage-step-library/spec.md`——MODIFIED × 1：「标准步骤库（十步）」（基准 = 换根后文本）
- [x] 1.3 `design.md`——五分处置表、D1–D4 决策与备选、**D6（`R-2` 改规格不改实现）**、被否方案、风险与迁移
- [x] 1.4 `openspec validate refactor-damage-module-boundary --strict --no-interactive` 通过  —— **已过（2026-10-03 复验）**：`Change 'refactor-damage-module-boundary' is valid`（exit 0）
- [x] 1.5 **提案评审批准**（**批准前不得进入第 3 节**）  —— **2026-10-03 用户批准**（"可以执行了"），第 3 节据此进入实现
- [x] 1.6 **换根归档确认**：动手前确认 `openspec/specs/damage-primitive/spec.md` 与 `damage-step-library/spec.md` 已含换根后的 tag 名（若换根尚未归档，停止并回报——本提案的 delta 会与之冲突）  —— **已确认（2026-10-03）**：换根已于 2026-10-02 归档（`changes/archive/2026-10-02-reroot-gameplay-tag-vocabulary/`）。生效规格扫描：`TcsEvent.Damage.Recorded` ×2 / `DamageFlowKey.*` ×4 / `DamageFlowTemplate.*` ×1；**旧名（`Tcs.Event.Damage` / `Tcs.Flow.Key` / `Tcs.Flow.Template`）零残留** ⇒ 基线正确，可动手

## 2. 契约门槛复述（本提案不重复的既有义务）

- [x] 2.1 新增的 2 个委托方法 MUST 遵守 `damage-primitive`「流程委托契约（宿主实现）」的全部既有纪律：`UFUNCTION(BlueprintNativeEvent)` + 接口内 `virtual <名>_Implementation` 默认体（**UHT 不生成 stub**）· 形参全反射（只能用 `FTcsDamageFlowContextView`）· C++ 调用点走 `Execute_<名>`（**MUST NOT** 虚表直调，否则脚本层实现被静默跳过）  —— **已逐条核（2026-10-03）**：① 两个新方法均 `UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")` 且**默认体写在接口类内的 `_Implementation` 声明处**（`ImplFound` 分支 ⇒ UHT 不生成 stub）；② 形参三型全部反射（`FTcsCombatEntityHandle` / `double` / `bool` / `const FTcsDamageFlowContextView&`），**无纯 C++ struct 泄进签名**；③ 两个调用点均为 `ITcsDamageFlowDelegate::Execute_<名>`（`TcsFlowStepsCore.cpp` 的 `Execute` 步），零虚表直调。**跨模块实证**：宿主模块的 C++ 实现类 `UTcsDevDamageFormula` **只实现了 5 个旧方法**，新两个自动走接口默认体——`Execute_` 查不到 `UFunction` 时回落原生 `_Implementation`，行为 = 中性默认（见 7.5 的胶水注记）
- [x] 2.2 本提案**不新增**任何公式、不新增任何宿主词汇到框架代码里——新增的两个钩子只做"把决定权交出去"  —— **已核（2026-10-03）**：两个新增实体是**接口方法**（无公式、无词汇），中性默认实现引用的是框架自身的数值契约（候选量 / 吸收量 / 属性当前值）；新增文本里出现的 `DamageFlowKey.*` / `TcsEvent.*` 全部是**既有**框架词汇。**零新增 tag**（`unreal-gameplay-tags` 校验面不受影响）

## 3. 撤销框架内定（`FTcsFlowExecute`，`TcsFlowStepsCore.cpp`）

- [x] 3.1 **吸收映射改走委托**：把 `const double Executed = FMath::Max(0.0, Candidate - Absorbed);` 改为经 `ITcsDamageFlowDelegate::Execute_ResolveExecutedDamage(...)` 取值；`Delegate` 为空或未配置时走接口中性默认（= 现状表达式，**零行为变更**）  —— **已落**：表达式保留为**无 delegate 时的初值**（与 `BaseDamage` 步同款写法），有 delegate 时经 `Execute_` 覆写。**口径注记（实施期回填，见 `design.md` Open Question 3）**：`Absorbed` 按现状是**全目标之和** ⇒ 本映射实为**流程级**一次取值，目标句柄取 `Targets[0]`（与既有 `CalculateBaseDamage` 同款）；**逐目标吸收/执行是行为变更，须另开提案**
- [x] 3.2 **致死判定改走委托**：把 `if (AttributeSubsystem->EvaluateCurrent(Target, ResolvedAttrKey) <= 0.0)` 改为经 `Execute_IsLethal(Target, CurrentValueAfterApply, ContextView)`；中性默认 = `<= 0.0`（**零行为变更**）  —— **已落**：判定式改为 `const bool bLethal = DelegateObject ? Execute_IsLethal(...) : CurrentValueAfterApply <= 0.0;`；`EvaluateCurrent` 的取值点、`break` 语义与 `bKill = 1.0` 的累加口径**逐字未动**（仍是"任一目标致死即记账"）
- [x] 3.3 保留 `:194` 的注释原文「记账语义：本次事务提交后目标生命 ≤ 0（**死亡规则仍归宿主**）」——它是本项合规性的关键注脚；补一句"阈值经 `IsLethal` 可覆盖"  —— **已落**：注释原文一字未改，尾部补「——阈值经 `ITcsDamageFlowDelegate::IsLethal` 可被宿主覆盖（中性默认 = `<= 0.0`，即整肃前行为）；框架**仍只记账**、MUST NOT 杀实体」
- [x] 3.4 复核 `Absorbed` 的累加口径不变（`ModifyShield` 语义未动）  —— **已核**：仍是 `for (Target : Context.Targets) Absorbed += Execute_ModifyShield(DelegateObject, Target, Candidate, ContextView);`——**逐目标累加、初值 0.0、语义与调用形态未动**；唯一变化是 `DelegateObject` 提前取出（原为 `Step->Delegate.GetObject()` 现取），**等价改名**；`ContextView` 由"块内局部"提前为"本节共用"，**提取时点未变**（仍在任何 `Execute_` 调用之前）

## 4. 撤销契约覆盖（值域交回宿主，`TcsFlowStepsCore.cpp`）

- [x] 4.1 把 `const double NewValue = FMath::Max(0.0, Current - Executed);` 改为 `const double NewValue = Current - Executed;`（**删除框架的零下限**）  —— **已落**
- [x] 4.2 **迁移动作（MUST，否则行为回退）**：宿主 MUST 给生命属性配置 `Min = 0` 边界（`ETcsAttributeBoundMode::ABM_Static`/`ABM_Dynamic` + `MinValue`）——否则过量伤害会把生命记为负值。**该动作归 LAC 侧**，见第 7 节  —— **已满足（2026-10-03 经编辑器 MCP 实查，非人工转述）**：`Content/TcsDev/DA_Health.uasset` 的 `Def.Bounds.Min` **本就是 `Mode = ABM_Static` / `StaticValue = 0`**（`Max` 为 `ABM_None`），资产 mtime `2026-10-02 19:13:16`（换根那批资产保存时就在，**早于本次改动**）⇒ **宿主零动作**；此前记的"未执行"是**误判**（把提案里的 MUST 当成了"尚未满足"）。**★ 但缓解只在读侧成立（本次新发现的残留语义）**：`FTcsAttributePipeline::SetBaseValue` **不取值域**（`Instance->BaseValue = NewBaseValue;` 原样写），值域只在 `ComputeFoldedValue` 末端生效 ⇒ `Min = 0` 保证 `EvaluateCurrent` 恒 ≥ 0（`bKill` 与记录读的就是它），但**基础值仍可为负**；后续"抬血"会先填这个坑再上升。按设计这是**末端收口语义**的必然结果（不是缺陷），但 D4 的措辞把它说成了完全缓解——已在 `design.md` D4 补注。**现状暴露度**：**`FormulaChain` 夹具实际就触发了**——该轮 5 条记录（`#2`–`#6`）各执行 25，累计 **125 点打在 100 生命上**，第 5 条（记录号 `#6`）即 25 点过量（见 5.6 的运行期证据）。此前写的"现有夹具不触发"是**错的**
- [x] 4.3 删除处补注释说明"值域收口归属性层的宿主配置（`ABM_None` 是默认 = 不设边界）；本步 MUST NOT 自设下限"  —— **已落**：注释写明三层——① 收口归属性层宿主配置（`TcsAttributePipeline.cpp` 的 `ApplyValueDomain` 只在显式配了边界时钳制）；② `ABM_None` 是默认 = 该侧不设边界；③ 本步自设下限 = **契约覆盖**（比"框架内定"更重的一档）

## 5. 硬编码常量字段与记录形状（**BREAKING**）

- [x] 5.1 `ExecuteFlowCompleted`：`Record.bHit = true;` 改为从契约键读回——与同函数 `bCrit` / `Absorbed` 同款写法（`Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Hit)) > 0.0`）  —— **已落**，写法逐字等同 `bCrit` 行
- [x] 5.2 `TcsDamageRecord.h`：删 `FGameplayTag Element` 字段声明（同时删其上方专述元素的注释行，注释里的"元素"语义改注为"元素经 `ClassificationTags` 承载"）  —— **已落**：字段与旧注释行同批删除，`Identity` region 只余 `FlowId` / `Source` / `Target`；**原位置换上一段**"本结构不含元素字段（BREAKING）+ 正确落点是 `DamageCategory` + `ClassificationTags` + MUST NOT 复活该玩法名词"的注释，防止后来者加回
- [x] 5.3 `TcsFlowStepsCore.cpp`：删唯一写入点 `Record.Element = FGameplayTag();`  —— **已落**
- [x] 5.4 全仓反查零残留：`rg -n "\bElement\b" Source/` 只允许命中**步骤名**（`FTcsFlowElement` / `ExecuteFlowElement`）、**委托方法**（`ResolveElement`）、**事件名**（`TcsEvent.Damage.Element` / `Tag_TcsEvent_Damage_Element`）与注释；**零字段声明、零字段写入、零字段读取**  —— **已验（2026-10-03）**：`\bElement\b` 在 `Source/`（151 文件）共 **11** 命中，逐条归类——注释 5（`TcsFlowStepsRest.cpp:122` 步骤标题 / `TcsDamageFlowCollectEvent.h:58,61` / `TcsDamageFlowContext.h:57` / `TcsDamageFlowContextView.h:73`）· 委托方法 1（`Execute_ResolveElement`）· 步骤局部变量 3（`TcsFlowStepsRest.cpp:135,137,139` 的 `const FGameplayTag Element`——**delegate 解析结果 → 分类集**，非记录字段）· 事件名 2（`Tag_TcsEvent_Damage_Element` 声明与登记）。**`Record.Element` / `FGameplayTag Element;` 声明 / `.Element` 读写：0 命中**
- [x] 5.5 编译验证：Development 与 Shipping 双配置零错误（记录被 `FTcsFlowCompleted` 填充、经总线发布、进环形缓冲、`Tcs.Damage.DumpRecords` 打印——四处编译面都会触到该 struct）  —— **已过（2026-10-03 实跑）**：`LegendAutoChessEditor Win64 Development` = **`Result: Succeeded`**（exit 0）+ `LegendAutoChess Win64 Shipping` = **`Result: Succeeded`**（exit 0）。日志见 `Saved/Logs/UBT-Editor-Development.log` / `UBT-Game-Shipping.log`。**首次构建曾失败一次**（`CS0535`，非本 struct 问题）——根因与处置见 7.5
- [x] 5.6 **载荷兼容性验证**：PIE 里跑一次默认模板，确认 `TcsEvent.Damage.Recorded` 载荷仍能派发到订阅方、`GetRecentRecords` 读回的字段与本次伤害一致  —— **已过（2026-10-03 用户实跑三条，日志留证）**：
  - `Tcs.Test.Slice.Run` = **通过 9 / 失败 0**（检查 4 = 属性定义 4/4；检查 5 = `Attack=30.0 / Armor=5.0 / Health=100.0`；检查 6 = `100.0 → 75.0`；检查 7b 施法者 `100.0 → 55.0`，扣 45.0）
  - `Tcs.Test.Slice.Run EffectChain.FormulaChain` = **通过 9 / 失败 0**（检查 7b `100.0 → 0.0`，扣 100.0）
  - `Tcs.Test.Slice.Reject` = **通过 3 / 失败 0**（2 条登记失败 + 1 条 Error，**均为命令自陈的预期输出**）
  - **载荷派发证据（记录六字段逐一合理）**：`[伤害记录] #1 源 3 → 目标 3：输入 45.000 / 最终 45.000 / 差额 +0.000 / 执行 45.000`；FormulaChain 侧 `#2`–`#5` 全为 `输入 30.000 / 最终 25.000 / 差额 -5.000 / 执行 25.000`，`#5` 起带 `[击杀]` ⇒ **删 `Element` 字段 + `bHit` 改读契约键均未破坏派发**（订阅方 `UTcsDevScreenObserver` 不读这两者，见 7.2 / 7.3）
  - **告警面**：全日志 `Invalid GameplayTag` **0 命中**；测试窗口内除声明为预期的拒绝面 Error 外，仅 1 条无关的 `LogAudioMixer: Timeout` 警告
  - **★ 顺带取得 4.2 残留语义的运行期印证**：FormulaChain 的 `#6` **执行 25.000 却没有对应的 `[属性变更]` 行** —— 健康停在 0、事件未广播（`OldValue` 与钳后的 `NewValue` 同为 0 ⇒ `bChanged=false`）。这正是 `SetBaseValue` 不取值域 + `Min = 0` 在读侧钳住的**直接观测**：**可观测值没有变负（⑤ 的目标达成），而基础值此时已为 −25（残留语义）**
  - **两条边界（诚实标注）**：① `bHit` 由 `true` 变 `false` **运行期无证据**（打屏装置不打印 `bHit`），仍只有代码级证据；② **⑤ 的删除在当前配置下不可观测**——`Min = 0` 恰好遮蔽了它，要观测须临时把 `DA_Health` 的 `Min` 改成 `ABM_None` 再跑（即 `proposal.md` 检查点 3 的负对照半，**未跑**）
- [x] 5.7 **资产面指向性检查**：打开含该事件的蓝图/DataTable 资产（LAC `Content/TcsDev/`）确认无按字段名拼过 `Element` 的残留；必要时在加载日志里核对无字段失配告警  —— **已过（2026-10-03，资产二进制全扫）**：扫 `Content/` 全部 `.uasset` + `.umap`，**含 `DamageRecord` 的资产 = 0 个** ⇒ 从没有任何蓝图 / DataTable 拆过这个 struct ⇒ **不可能存在按字段名拼的 `Element` 引脚**——**结构性排除，比逐个打开更强**。**对照扫描证明手法有效**：同一手法命中 2 个资产（`DA_FormulaChain` / `DA_SliceChain` 含 `DamageFlowTemplate` / `TcsEvent`）。**运行期旁证**：本会话日志 `Invalid GameplayTag` **零命中**（经 MCP `LogsToolset.GetLogEntries` 读活会话日志）
- [x] 5.8 宿主交办核对（写进 LAC 提案范围）：LAC 侧若新增"按元素判定"的需求，**MUST 走 `DamageCategory` 词表 + `ClassificationTags` 匹配**，MUST NOT 要求把字段加回记录  —— **已落**：条文写进 `TcsDamageRecord.h` 的字段区注释（宿主打开该头即见）+ `decisions-log.md` 同日条目；LAC 侧**当前无此需求**（7.3 零引用）

## 6. ② 接缝的注释与登记（**零行为变更**）

- [x] 6.1 `TcsDamageRecord.h` 的 `bCrit` 补注释："写入者 `Crit` 步**不在默认模板**，故默认恒 false"  —— **已落**，且同批给 `bHit` 补了对称注释（"读契约键 `DamageFlowKey.Hit`；该步不在默认模板 ⇒ 无提交 ⇒ 折叠初值 0 ⇒ 默认 false"）——两者取值来源同款，注释口径必须一致
- [x] 6.2 `TcsDamageFlowDelegate.h` 的 `ModifyShield` 保留其"宿主 hook；不做护盾系统；默认 0 = 无护盾"注释（该注释自认归属，是本项合规的关键证据）  —— **已核未动**：原文「护盾吸收（宿主 hook；不做护盾系统）。返回本次可吸收的伤害量（默认 0 = 无护盾）。」逐字保留
- [x] 6.3 `DamageFlowKey` 的 `Hit` / `Crit` / `Kill` 三键归属说明登记（"承载宿主判定语义，但仍是步骤间接口 ⇒ MUST NOT 删除"）——落 `TcsFlowKeys.h` 注释  —— **已落**：`TcsFlowKeys.h` 文档块新增一段「**宿主判定语义的键（登记，MUST NOT 删除 —— 2026-10-02 边界整肃）**」，写明三键各自的**生产者/消费者步骤**（`Hit` ③写⑩读 / `Crit` ④写⑩读 / `Kill` ⑨写⑩读）+ 删除禁令 + "取值一律读回、绝不硬编码"
- [x] 6.4 **删除禁令复核**：实现完成后逐条对照 `research/boundary-audit.md` §5 的 32 项，确认**一项未删**（尤以 `FTcsSelSelf` / 空 Filter = 通过 / `GetLocation` / 委托 5 个中性默认 / 官方默认模板四步骨架为重）  —— **已逐项复核（2026-10-03）：一项未删**。**方法**：① 落点比对——32 项中 24 项的落点在本次**未触碰**的文件（`TcsTargeting` / `TcsEffect` / `TcsCore` / `TcsAttribute` / `TcsIntegration`）；② 落点在本次被改文件里的 8 项逐项认领——`G8`（委托中性默认 → **由 5 个变 7 个**，单位元性质不变）·`G9`（空 Delegate ⇒ 原样用输入）·`G10`（键兜底 → 框架契约键）·`G12`（护盾默认 0）·`G13`（`Hit`/`Crit`/`Element` 空 Delegate 降级）·`G14`（空模板 id = 官方默认）·`G15`（默认模板四步骨架，**刻意不组装 Hit/Crit/Element 的行为未动**）·`G16`（`ResolvedAttrKey` 双空字段）；③ 锚点 grep 复核：8 项全部命中。**§5.1 的"十条最易误删"逐条在案**

## 7. 宿主侧迁移交办（**LAC 仓**，不在本仓交付）

- [x] 7.1 **生命属性配 `Min = 0`**（第 4.2 项的直接前置；不配则过量伤害记负值）  —— **已满足（2026-10-03 经编辑器 MCP 实查）**：`DA_Health` 的 `Def.Bounds.Min` = `ABM_Static` / `StaticValue = 0`，资产 mtime `2026-10-02 19:13:16` ⇒ **早于本次改动就配好了，LAC 侧零动作**。**方法留档**（下次同类核查照此做，**不要再推给用户去点**）：连 `ue-editor` MCP → `ObjectTools.get_properties(instance={refPath:"/Game/TcsDev/DA_Health.DA_Health"}, properties=["defTag","def"])` → 直读 `def.bounds.min.{mode,staticValue}`。**对照**：`DA_MaxHealth` 同样 `Min = ABM_Static 0`；`DA_Attack` / `DA_Armor` 为 `ABM_None`（不受伤害，无需边界）
- [x] 7.2 `bHit` 语义变更的消费面核对：确认 LAC 侧无"依赖 `bHit` 恒 true"的读取点；若有，MUST 在模板里组装 `FTcsFlowHit` 步或改用其他判据  —— **已核（2026-10-03）：零命中**。LAC `Source/` + `Script/`（排除 `obj/`）对 `bHit` **零引用**；唯一的记录消费者 `UTcsDevScreenObserver` 打屏字段是 `Sequence / Source / Target / Base / Final / Executed / bKill`，**不读 `bHit`** ⇒ 本提案唯一的默认行为变更**在宿主侧无消费面**
- [x] 7.3 `Element` 字段消费面核对（已核为**零引用**，实现后复验一次即可）  —— **复验通过（2026-10-03）**：LAC `Source/` + `Script/` 对 `\.Element\b` 零命中；`FTcsDamageRecord` 仅两处出现，均为**整体载荷**用法（`TcsDevScreenObserver.cpp` 取 `Payload.GetPtr<FTcsDamageRecord>()`、`.h` 的文档注释），**无字段级访问**。唯一形近命中是 `Script/.../GameplayTags.cs` 的 `TcsEvent_Damage_Element`——那是**事件 tag 常量**，非字段
- [x] 7.4 若 LAC 需要自定义吸收模型 / 致死线，**经新委托方法表达（脚本层或 C++ 均可）**，MUST NOT 要求框架回退为硬编码阈值  —— **本次未触发**：LAC 未提出自定义吸收/致死线需求；`UTcsDevDamageFormula` 只实现 `CalculateBaseDamage`（其余走中性默认）。**条文在案**（作为禁令），将来经 `ResolveExecutedDamage` / `IsLethal` 表达
- [x] 7.5 **（实施期新增）UnrealSharp 宿主的 glue 重导要求**：接口声明在插件模块、实现类在宿主模块时，**宿主 glue 镜像不会随接口刷新** ⇒ `CS0535`，宿主 MUST 让**实现模块重新导出** glue  —— **已查明并已处置（2026-10-03）**。**症状**：首次双配置构建失败，`TcsDevDamageFormula.generated.cs(34,82): error CS0535: UTcsDevDamageFormula 不实现接口成员 ResolveExecutedDamage / IsLethal`。**根因**：UnrealSharp 的 glue 导出**按"声明该类型的模块"判脏**，未沿"实现了哪个接口"建立反向依赖边——接口胶水 `TcsDamageFlowDelegate.generated.cs` 本次已刷成 **7 成员**（mtime 07:13:13），而实现类的镜像 `TcsDevDamageFormula.generated.cs` 仍是 **5 成员**的旧文件（mtime 停在 2026-09-30）。**处置**：不碰生成物、不碰宿主源码，只让实现模块重新导出——本次以**零内容的 mtime 触碰** `Source/TcsDev/Public/Dev/TcsDevDamageFormula.h` 验证机制成立（触碰后镜像重写为 7 成员 `L139/188/237/290/343/401/459`，构建转绿）。**边界**：纯 C++ 宿主**不受影响**（无 glue 镜像）；fresh clone / 清理重建**不受影响**（全量重导）⇒ 这是**增量构建**特有的坑，不影响 `proposal.md` 关于"宿主源码零适配"的其余部分。**建议**：该通用事实宜进 `unrealsharp-agent-skill` 的排障节（**已向用户提议，未落**）

## 8. 文档与记录

- [x] 8.1 实现落地后，`log/decisions-log.md` 追加一条边界整肃决策记录（五分处置、两个新钩子、值域交回宿主、`bHit` 语义变更、**`R-2` 规格更正**）  —— **已落（2026-10-03）**：追加一条 8 小节记录，含**实施期发现**（7.5 的胶水失效）与未关闭项
- [x] 8.2 `research/boundary-audit.md` 的 §10「对后续提案的输入」在落地后回填"已按此执行"的追溯注记（**审计报告本体是冻结文档，MUST NOT 改写正文**，只加实施注记）；`R-2` 的处置结论 MUST 一并注明（改规格不改实现）  —— **已落（2026-10-03）**：新增 `### 10.1 实施注记`（6 行对账表 + 2 条实施期回填 + "未发现判据适用错误"），**正文一字未改**；`R-1` 明确标注**仍未排期**
- [x] 8.3 若实现中发现审计结论有误（判据适用错误 / 漏判），MUST 回报并更新审计报告——**不得静默偏离**  —— **已核（2026-10-03）：未发现审计判据适用错误**。五分处置的每一档都经实测站得住（含最有争议的 `S2` 拆分判决：零下限 = 契约默认、减法模型 = 框架内定）。两处实施期发现（7.5 的 glue 失效、3.1 的多目标签名口径）都**不是审计判据问题**而是提案的兼容性承诺与签名口径问题，已分别回填到 `boundary-audit.md` §10.1 与 `design.md` Open Question 3 的对应位置
