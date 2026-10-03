# [SPEC-08-damage](../spec/09-module-damage.md) — TcsDamage 瞬时流程层设计（伤害/治疗）

- **文档 ID**：`SPEC-08-damage`
- **类型**：SPEC / 模块规格
- **状态**：ACTIVE
- **权威范围**：TcsDamage 瞬时流程：模板、三层值空间、修改器通道、记录流；理由住 LOG-02-effects §D7
- **最后更新**：2026-10-04

> **换根注记（2026-10-01）**：本文 tag 名已随提案 `reroot-gameplay-tag-vocabulary` 换根——旧前缀 `Tcs.Event.*` / `Tcs.Flow.Key.*` / `Tcs.Flow.Template.*` / `Tcs.Attr.*` / `Tcs.Chain.*` 依次成为 `TcsEvent.*` / `DamageFlowKey.*` / `DamageFlowTemplate.*` / `Attribute.*` / `EffectChain.*`；本文正文一律用新名，旧名仅存于本注记与 `log/`、`ledger/`、`evidence/` 等历史文件。

- 日期：2026-09-02
- 状态：**v4（PV 系列 D7-2 收窄：基础伤害值=参数账本解算输入、流程零计算）**（编译层模块名 **TcsDamage**，R0 §9；依据 D7-1~D7-7 已拍板 + 启发文档《伤害修改器设计》全文 + PV 系列 2026-09-11；**插件零公式、流程零预设**）
- 职责一句话：**瞬时流程的机制层——流程解释器 + 步骤注册表 + 流程属性黑板 + 收集事件协议 + 消耗型裁决 + 记录流。流程=数据模板（宿主组装）；标准阶段只是插件自带的步骤库与官方默认模板。**

## 1. 模块边界

- 依赖：TcsCore（句柄/Source/总线/到期堆）、TcsAttribute（provider 接口、聚合语义复用、事务）、**TcsEffect（FEffectStep 基类型+执行器注册宏+链解释器——D4-14 注册制分派）**。
- 被依赖：TcsEditor（组合层）、宿主（delegate 实现、统计/回放消费）——**R3 竖切例外**：TcsIntegration slice 编译依赖本模块（plan2）；全设计口径 TcsIntegration{Skill, Cue}。**TcsSkill 不依赖本模块**——链步骤运行时经注册表分派，编排层不具名领域步骤类型（D4-14）；"技能×伤害"组合在 TcsIntegration/宿主完成。
- 层级：`Core ← Attribute ← Effect ← {Damage, Targeting, State} ← Skill`（**R0 §9 原式，层级序=方向许可**——**Damage/Heal/ModifyFlow 步骤类型+执行器住本模块**，经 UE_DEFINE_EFFECT_STEP_EXECUTOR 自注册；TcsEffect 不依赖本模块）。用户 M9 追问轮确认此方向：真正消费流程的是宿主项目，不需要伤害的项目可不引入本模块，TcsEffect 照常运行。
- **流程管线宿主化（D7-5，用户 M9 轮拍板）**：本模块之于伤害流程 = TcsEffect 之于效果链——流程=数据模板，**阶段构成本身是项目知识**（"不同项目有不同项目的说法"，用户原话），插件不预设；标准十阶段降级为内建步骤库 + 官方默认模板（可整表替换）。

## 2. 类型词汇（对外）

### 2.1 流程上下文与三层值空间（M9 轮澄清）
- `FDamageFlowContext`（黑板，可池化）：`Attacker / Instigator / Targets / 分类 Tag 集 / 公式参数初值 / 流程属性黑板 / FlowSource / **CapturedAttrs**`。**黑板即上下文**——天然同构于 FEffectContext。
- **三层值空间**：
  1. **技能参数**（账本，Entry 级持久）——SkillDef 参数+修正链求值，不进流程；
  2. **公式参数初值**：`TMap<FGameplayTag, FTcsParamValue>`（数值为主 + FInstancedStruct 扩展位）——Damage 步骤配置的值（**FTcsParamValue**，PV 系列 2026-09-11 换型——Literal/ParamRef 等源，原 Param() 引用技能参数归 ParamRef）注入，**只读原料**，delegate 按名取用；键=项目词表 tag，**永不用下标**（**2026-09-22 tag 化：原 `TMap<FName, FFlowParamValue>`**）；
  3. **流程属性黑板**（容器=`FTcsFlowAttributes`：键+每键修正链+封闭五带运算（同 M2，Custom op 砍除——D2-7；值域策略逃逸走 IValueDomainPolicy 同款形态），M2 语义复用——**折叠统一调用 TcsAttribute 共享带式折叠纯函数（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器三处共用，勿私建第二份）**，作用域=流程用完即弃）——**伤害计算的工作值**（BaseHitRate/BaseDamage/FinalDamage 等就是临时变量）；内建步骤读写的键名是**标准步骤库的契约**（M8 校验/Explain 认识）——**契约键归框架**（`DamageFlowKey.*` 原生声明，2026-09-22 tag 化），**项目键自由 tag、无注册表**；也是 ModifyFlow 提交的落点。
- **分类 Tag 集**：来源标签（流程启动时由 Damage 步骤配置写入）+ 元素标签（Element 步骤由 delegate 解析后写入，修改器可改）——修改器匹配键（"受火伤+20%"按 Tag 过滤收集事件）、免疫/减伤候选匹配（"免疫火焰"）、FDamageRecord 记录维度；词表归项目，插件只搬运与匹配。
- **AttrCapture**：`FDamageFlowConfig.AttrCaptureList{AttrKey, From: Instigator/Target}`——FlowStart 时从 M2 捕获进 CapturedAttrs 快照；属性读默认 Live、捕获命中读快照（**流程内读取一致性**——中途 buff 过期不追溯）；"本次攻击攻击力+10%"双路径：捕获命中→改 CapturedAttrs（随流程消失，零账本污染）／未捕获→FlowSource 临时修正器挂真实账本。
- `FlowSource`：**每流程唯一 FSourceHandle——作用域修改器的归属锚点**："本次攻击+X%"类临时修改挂 M2 账本、Source=FlowSource，流程结束 `RemoveBySource` 级联摘除（O(1)，复用 D2-2）；替代文章 CopyOnWrite（零拷贝）。

### 2.2 流程模板：标准步骤库 + 官方默认模板（D7-5）
- **流程模板** `FTcsDamageFlowTemplate`（概念名 `FCombatFlowTemplate`；实现名 2026-09-21 定 `FTcsFlowTemplate`，WAIT-8 资产化轮改现名）：有序步骤数组（FInstancedStruct，同 FEffectStep 形状）+ `TemplateId: FGameplayTag`（2026-09-22 tag 化）；**多预设 = 多模板资产**（不同游戏模式/角色配不同模板）。模板选择链：Damage 步骤配置指定 → Def 覆盖 → 全局默认（官方默认 = `DamageFlowTemplate.Default`）。
- **模板重定向** `FFlowRedirect{TargetTemplateId, ReplacementTemplateId}`（D7-7，用户拍板一步到位；字段为 tag）：状态/装备声明换流程，让渡点+重定向栈后挂/高优先+Source 级联回收——**三粒度让渡模式升四粒度**（参数→链→技能→流程）。
- **流程步骤注册表的寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤）**：`FTcsFlowStepExecutorRegistry` 与 Effect 侧**同构、同款改造**——进程级单例不变；动态登记项记录宿主对象 + 登记世界弱引用；失效判据为"对象死 / 世界亡 / 世界不同"；`Find` 带可选世界入参，跨世界失效视为未命中并移除 + Warning；**拒绝门收窄为"同世界活对象重复"**（失效 ⇒ 替换，有效且同世界 ⇒ 拒绝）；补 `Unregister` / `GetDynamicKeys`，门面 `Deinitialize` 按世界撤销。**完整口径见 `04-module-effects.md` §5b 的同名条目**（本文不重复定义，只记落点——避免"同一机制两处定义"）。
- **标准步骤库**（内建 struct+执行器，自注册；官方默认模板 = 下表组装，宿主可整表替换）：

| 标准步骤 | 引擎行为 | 项目挂点 |
|---|---|---|
| CollectStart | 发流程开始事件，重置收集 | — |
| PreHit | 发收集事件 → 应用 | 修改命中率/注册免疫候选 |
| Hit | delegate 基础命中率 → 修改器 → 判定 Hit/Miss/Invalid | `ITcsDamageFlowDelegate.GetBaseHitRate` |
| Crit | delegate 基础暴击率 → 修改器 → 判定 | `GetBaseCritRate` |
| Element | delegate 解析元素 → 修改器可改 → 元素标签写入分类 Tag 集 | `ResolveElement` |
| BaseDamage | **接收输入值**（链步骤 DamageBase = 参数账本解算结果——PV-7 2026-09-11：流程零基础值计算）→ 修改器可改 | `CalculateBaseDamage`（**降级逃生口**——D7-2 收窄，宿主特殊公式才实现） |
| AfterDamage | 发收集事件 → 应用（"伤害+50"） | 修改 FinalDamage |
| PreExecute | 发收集事件 → 收集免疫/减伤候选（**只收集不消费**） | 提交消耗型候选 |
| Execute | 免疫/减伤裁决（SortKey 选一）→ 宿主护盾 hook → M2 事务扣血（键=步骤配置的项目词表 AttrKey）→ **成功才消费**（消费**动作**归台账 `DAMAGE-4`） | `ModifyShield`；消费事件 `TcsEvent.Damage.ModifierConsumed` |
| Completed | 发完成事件 + **FDamageRecord 入记录流** | 统计/回放消费 |

- **通用数据步骤（编辑器拼流程零 C++）**：`FlowModify{TargetKey, Op, Operand(FTcsParamValue——Literal/ParamRef 等源，PV 系列 2026-09-11 换型；黑板键引用保留为流程域自身 Operand 选项), Conditions}`（数据化黑板写入——"破甲阶段"=一个 FlowModify）与 `FlowDelegate{TargetKey, Delegate 接口位}`（数据化委托调用，轻量公式挂法）。**MUST NOT 携带消耗策略**——数据步骤只做**纯数值写入**，消耗型提交属"修改器通道"语义（`ModifyFlow` 链原语 / C++ 步骤 / 事件响应）。**每步骤可带 Conditions**（复用 D4-5 条件最小集+Custom Fragment，对流程上下文求值，不过则跳过——"嗜血才跑的额外步骤"用条件表达）。
- **自定义步骤的 C++ 边界（宪法 R0 §8）**：仅两种——①新语义步骤（读宿主独有游戏状态：部位判定/连击计数），新词汇归 C++、自注册宏登记；②新公式（D7-2 插件零公式，delegate=宿主）。除此之外流程组装纯数据。
- 判定代码（Hit/Crit 基础值与判定）= 标准步骤库；**基础伤害值来源 = 链步骤数据配置（参数账本解算——PV-7 2026-09-11，流程零计算）**；命中/暴击基础率等仍为 delegate 挂点——或宿主写自定义步骤整段替代。

### 2.3 伤害修改器 = 触发行 + ModifyFlow（唯一通道，D7-6）
- **修改器没有独立载体类型**（D3-5 推论：状态行为=原语链）：一个"伤害修改器" = 状态/装备的触发行订阅步骤收集事件 → 条件 → `ModifyFlow` 提交。文章"有状态/无状态修改器" = 带/不带条件的触发行；订阅随 Source 生命周期自动退订——"状态在，修改器就在"天然成立。
- 消耗型：提交携带 `{MaxUses/Cooldown/SortKey}`（**2026-09-30 改造**：`FTcsDamageModifierConsumePolicy` 为**纯数据可反射**结构——原 `OnConsumed` 闭包改**事件语义**，消费成功时发 `TcsEvent.Damage.ModifierConsumed`，"被消费时要做什么"由项目经触发行订阅表达）；Execute 步骤裁决 SortKey 选一、成功执行才消费（**消费动作**归台账 `DAMAGE-4`）、未选中者完全不动（D7-4）。
- 聚合：提交落进黑板对应键的修正链，读时按 M2 同式求值——**折叠统一走 TcsAttribute 共享带式折叠纯函数（D5-5 v3，2026-09-14：M2 属性聚合 / M5 参数链 / 本容器三处共用；SortKey 不参与顺序语义，带序由 Op 决定）**。
- 创作糖：一条修改器 = 触发行+单步链（比文章一行重）——M8 编辑器提供"伤害修改器"模板自动生成，**机制唯一、界面给糖**。

**R4 落地状态（2026-10-04 收束回写）——本通道三段全通**：

- ①**订阅侧**：触发行落地（R4 Task 1 数据形状 + Task 2 登记表/求值器/四道门 + Task 2.5 独立资产载体与定义库装配）——见 [SPEC-03-effects](04-module-effects.md) §2.2；
- ②**提交侧**：`FTcsStepModifyFlow` 链原语落地（R4 Task 3，提案 `add-damage-modifyflow-primitive`）——从 `FTcsEffectContext::EventPayload` 取回流程上下文 → 向黑板 `Submit` → 恒 `TSR_Completed`；
- ③**端到端实证**：R4 Task 4（证据 `EVID-2026-10-04-modifier-channel`）——破甲行为**内容资产**（`UTcsEffectTriggerDefAsset`，零 C++ 注册）：订阅 `TcsEvent.Damage.PreExecute` + 单步链 `ModifyFlow{TargetKey = DamageFlowKey.BaseDamage, Op = TAO_Mul, Operand = Literal(0.5)}` ⇒ 提交落在 `Execute` 读黑板之前 ⇒ 扣血 **30 → 15**（记录 `Final` 与生命实际扣量**两个观测面同值**）；对照组（不点灯 ⇒ 门③不通过）扣原值 30、摘行还原、按来源级联摘除各一条。
- **仍未走的面（MUST NOT 外推）**：`Execute` 的**候选裁决**通道（`DamageFlowKey.ExecuteCandidates` + `SortKey` 选一）**无真实候选走过**（台账 `DAMAGE-4`）；`Heal` 原语与治疗流程未落地（`DAMAGE-3` 余）；成功提交**只经 `Op=Mul` 落在 `BaseDamage` 一个键**验过——其他运算带与项目键未验。
- **硬约束（本节 + `TcsStepModifyFlow.h` 头注释）**：`ModifyFlow` **只能用于同步单步链**——链上下文里携带的是流程上下文的**裸指针**，流程是单帧同步体；链若在本步之前挂起（`WaitDelay` / `WaitEvent` / 等子链），流程早已结束、指针悬空。故"一条修改器 = 触发行 + 单步链"不只是创作糖，**也是安全性约束**。（该条为 `PLN-R4` Task 3 注记的明文要求；**头注释那半在 R4 收束时补齐**——Task 3 实施只落了执行器侧校验，头注释与规格两处"明文"当时都未落。）

### 2.4 接口与记录

> **证据状态（2026-09-28 增量）**：DamageFlow 反射视图与 `Execute_*` 静态/glue 证据不变；本次 C# PIE 验证脚本流程步骤返回 false 时中止、true 时两步继续，原生 `Obj GC` 后 Flow executor 和模板中的 C# Damage delegate 均再次执行，公式得到 `Final=7`、Health 100→93（见 `evidence/2026-09-28-host-scripting-e2e-pie.md`）。仅覆盖同一 PIE 世界；跨 PIE 注册表寿命问题与其他语言仍未验证。
- `ITcsDamageFlowDelegate`（UINTERFACE，宿主实现；`ICombatDamageFlowDelegate` 是早期命名，实现类名已统一到 `ITcs…` 前缀）：`GetBaseHitRate / GetBaseCritRate / ResolveElement / CalculateBaseDamage（**降级逃生口**——PV-7 D7-2 收窄后仅宿主特殊公式实现，普通项目零 delegate） / ModifyShield`。
  - **✅ 宿主脚本可达性已打通（静态实现；2026-09-27 C# PIE 子集已验证，提案 `add-host-scripting-slots`）**：5 方法带 `UFUNCTION(BlueprintNativeEvent)`，C++ 调用点改走 `ITcsDamageFlowDelegate::Execute_*` ⇒ 机制形状可由任意 UE 脚本语言承载；本仓当前行为实测只覆盖 UnrealSharp/C#，AS / Luau / TS / 蓝图仍需各自验证。
    - **`Execute_` 是必需的，不是风格选择**：虚表直调（`Step->Delegate->GetBaseHitRate(...)`）会**静默跳过**脚本层实现（脚本覆写走 `ProcessEvent`），表现为"公式不生效"而非崩溃——这正是改造前的形态（`TcsDamageFlowDelegate.generated.cs` 当时是**零方法空壳**，实测存档）。
    - **C++ 实现与脚本实现双轨并存**：`Execute_` 内部先查 `UFunction` 走反射、查不到才回落原生 `_Implementation`（生成代码先例 `TcsAttributeProvider.gen.cpp:ITcsAttributeProvider::Execute_GetCurrentValue`）⇒ 既有 C++ 宿主实现（如 LAC 的 `UTcsDevDamageFormula`）无须改语义，只改签名。
  - **★ 形参不是 `FTcsDamageFlowContext`，而是 `FTcsDamageFlowContextView`（反射只读视图）**：前者是**纯 C++ struct（无 `USTRUCT`）**，出现在 `UFUNCTION` 签名里会让 UHT 报 `Unable to find 'struct' with name ...` ⇒ **不能只"补 UFUNCTION"，必须换签名**（这条修正了台账 SCRIPT-8 的原设想）。
    - 视图只摘**可反射数据面**：参与者句柄（`Attacker`/`Instigator`/`Targets`）、`FormulaParams`、`ClassificationTags`、请求字段（`BaseDamageInput`/`TargetAttrKey`）。
    - **不含黑板**——`FTcsFlowAttributes` 是**纯运行态容器**：存储形状 `TMap<key, TArray<提交>>` 是**嵌套容器**（UHT 层面不可作 `UPROPERTY`），提交项 `FTcsFlowAttributeSubmit` 又是纯 C++ 记录；真前置是"上下文/黑板分层"（台账 SCRIPT-3）——**2026-09-30 更新**：原根因（提交项深处的 `OnConsumed` 闭包）已随消耗策略改造删除，SCRIPT-3 由"物理不可能"降为"逐字段反射化的常规工作量"；但嵌套容器与运行态提交记录仍不可直接反射，故本视图仍不可省。
    - **不含 `Owner` 弱引用**——脚本实现本身是 UObject，可经自身 `GetWorld()` 取门面（LAC 侧实现的机械修复即此路）。
    - **单向投影、无反向写回**：脚本要修正流程值走收集事件协议的 `Submit`，不是改视图（避免双真相）。
  - **中性默认实现住接口声明处**（`virtual <名>_Implementation(...)`）：UHT 检测到声明则**不生成默认 stub**（`UhtFunction.cs:UhtFunctionExportFlags.ImplFound` 的 `ImplFound`）⇒ "普通项目零 delegate"（PV-7）不受影响。先例 = 引擎 `ISequencerAnimationOverride`（`SequencerAnimationOverride.h:ISequencerAnimationOverride`）。
  - **流程模板登记面同步反射化**：`UTcsDamageSubsystem::RegisterTemplate` / `UnregisterTemplate` 加 `UFUNCTION()`（**无 specifier**，口径同 `UTcsEffectSubsystem` 的 SCRIPT-1 批次）——否则脚本"能写公式却注册不了模板"。
  - **流程步骤执行器插槽**：`UTcsFlowStepExecutor`（UObject 基类）+ 门面 `RegisterStepExecutor`——让宿主脚本定义**流程阶段**（"流程阶段构成 = 项目知识"，D7-5 的直接兑现）。详见 `04-module-effects.md` §5b。
- 治疗流程：同骨架精简版（无 Hit/免疫；Crit 可选），`IHealFlowDelegate.GetBaseHeal`。
- `FDamageRecord`：`FlowId / Source / Target / 元素 / Hit / Crit / Base / Final / Executed / Absorbed / Kill / 序号 / 时刻`——完成/打断时发 `Damage.Record` 事件（立即，回放依赖序）+ 环形缓冲（统计）。
- 提交原语：**`ModifyFlow{键, 运算, 操作数, 消耗策略?}`（战斗组；步骤类型+执行器住本模块——D4-14）**——触发行订阅阶段收集事件后在响应里提交流程属性修正；`消耗策略`（`FTcsDamageModifierConsumePolicy` = MaxUses/Cooldown/SortKey **纯数据**；原 `OnConsumed` 闭包 2026-09-30 改事件语义）挂在提交上，实现"免疫一次"类效果。

## 3. 关键机制

- **收集 ≠ 消费**（D7-4）：阶段内只收集候选；Execute 裁决（SortKey 选一）→ 成功执行才消费——**消费动作**（扣 `MaxUses` / 起 `Cooldown` / 发 `TcsEvent.Damage.ModifierConsumed`）归台账 `DAMAGE-4`，形状已就位（2026-09-30）；流程失败/打断 → 无人消费。
- **瞬时角色属性修改**（"本次攻击攻击力+10%"）：临时修正器 + **FlowSource**（每流程唯一 FSourceHandle）——O(1) 挂账本、流程结束 `RemoveBySource` 级联摘除；替代文章的 CopyOnWrite。v1 只做当前值通道。
- **流程同步性**：瞬时流程单帧同步完成——**无挂起/唤醒**（区别于链的 WaitEvent）；消耗型 CD 走到期堆（流程外）。
- **免疫多候选裁决**：SortKey/Priority 选一；未选中候选完全不动（文章"三个免疫只消费一个"）。

## 4. 网络姿态落点（NET-1/2）

- 权威侧跑流程；`FDamageRecord` 事件流复制到客户端（战斗日志/统计/UI 数据源）——服务器权威、客户端零预测（瞬时流程无预演问题，文章同款结论）。

## 5. 非目标

不做任何公式（命中/暴击/元素=delegate 挂点或宿主步骤；**基础伤害值=链步骤参数解算输入——PV-7 2026-09-11，流程零计算，流程中途公式贡献=伤害修改器自实现（D7-6 通道）**）；**不预设流程阶段构成**（流程=数据模板，D7-5——标准阶段只是官方默认模板）；不做元素克制表；不做护盾系统（宿主 hook）；不做伤害数字 UI/统计报表（只供数据流与记录）；**不做生产期过程级溯源**（FDamageRecord 是结果快照，不含每键修改器贡献清单——"这个数怎么算出来的"走 M8 Explain 开发期回放，08 §4；千人战场每笔伤害带贡献清单是量灾难）；不做 DoT（那是 M3 周期 + 链，不走瞬时流程）。

## 6. 依据

- 启发文档《伤害修改器设计》（用户提供全文，wx_article.txt）：阶段化骨架、事件实时收集、流程由流程选择执行修改器、流程属性（FormulaAttr）、瞬间属性修改、有状态修改器、黑板即上下文——七个机制全部落位（四个同构、一个被句柄设计超越、一个真新增）。
- 拍板：D7-1~D7-4（2026-09-02 问答框，含"插件零公式"原则——用户："具体项目中如何实现 damage/heal 还都不确定，插件直接提供 effect 流程不合适"）；**D7-5~D7-7（2026-09-02 M9 追问轮）**：用户否定预设阶段表（"Damage 阶段具体都有什么，不同项目有不同项目的说法"）→ 流程管线宿主化；伤害修改器唯一通道确认（用户追问后统一模型拍板）；模板重定向用户拍板一步到位（否决 YAGNI 缓增补）；**D5-5 v3（2026-09-14：流程属性容器 = TcsAttribute 共享带式折叠器三处共用之一，折叠勿私建）**。

## 7. 验收钩子

竖切扩展：火球术链 → Damage 原语 → **官方默认模板** → 项目 delegate（一条简单公式）→ Health 事务 → Record 事件可见；三免疫实例注入后仅消费一个；临时修正器（本次攻击 +10% 攻击力）流程后还原。M9 轮新增人工检查：数据步骤（FlowModify 实现破甲）零 C++ 拼流程；模板重定向挂/摘（状态声明 FFlowRedirect）；修改器模板生成的触发行+单步链生效。

**R4 收束时的实证状态（2026-10-04）**——逐条对账，**未打勾的仍是留白，不得读成已验**：

| 验收钩子 | 状态 | 证据 / 留白 |
|---|---|---|
| 流程骨架（官方默认模板 + 宿主公式 delegate + Health 事务 + `FDamageRecord` 记录流） | ✅ **R3 竖切已验** | `PLN-R3-2` Task 7 验收（7 项人工检查全过）+ 入库命令 `Tcs.Damage.DumpRecords`；R4 两轮 PIE 里的 `[伤害记录]` 行为同一路径 |
| **"修改器 = 触发行 + 单步链"生效** | ✅ **已实证（R4 Task 4）** | 走**内容资产**路径、零 C++ 注册：`EVID-2026-10-04-modifier-channel`（破甲 `30 → 15`、对照组、摘行还原、按来源级联摘除）。**注意区分**：M8 的"伤害修改器模板**自动生成**"这层糖**未实现**，本次是手工建两条资产——"机制唯一、界面给糖"里的前者已证、后者未做 |
| 三免疫实例注入后仅消费一个 | ⛔ **未验** | 候选裁决通道（`DamageFlowKey.ExecuteCandidates` + `SortKey`）**无真实候选走过**，消费动作整体未实现 ⇒ 台账 `DAMAGE-4`（R5/M4a） |
| 临时修正器流程后还原（`FlowSource` 级联摘除） | ⛔ **未验** | `AttrCapture` 整套无填充者/读取者 ⇒ 台账 `WAIT-7`；`FlowSource` 的 `RemoveBySource` 级联复用 M2 既有能力，但"流程结束即摘"未作为验收项跑过 |
| 数据步骤（`FlowModify`）**零 C++ 拼流程** | ⛔ **无证据** | R4 的破甲走的是**链原语** `ModifyFlow`，**不是**流程内数据步骤；`FTcsFlowModify` 执行器 R3 已落地，但"编辑器里零 C++ 拼一条流程"从未作为验收项跑过（该面只在 R3 用宿主 C++ 自研步骤验过"流程可整表替换"） |
| 模板重定向挂/摘（`FFlowRedirect`） | ⛔ **未落地** | 台账 `STAT-3`（归 R5/M3 状态轮——消费者是"状态/装备声明换流程"） |

## 8. 修订记录

- v1（2026-09-02）：初版（D7-1~4 折入）。
- v2（2026-09-02，M9 追问轮）：§1 依赖方向对齐 D4-14 翻转后形状（本模块依赖 TcsEffect，Damage/Heal/ModifyFlow 步骤类型+执行器住本模块）；ModifyFlow 归属标注。
- v3（2026-09-02，M9 追问轮重构）：**流程管线宿主化**（D7-5——阶段表降级为标准步骤库+官方默认模板，流程=数据模板；通用数据步骤 FlowModify/FlowDelegate + 每步骤 Conditions）；§2.1 重写为三层值空间（技能参数/公式参数初值 Map/流程属性黑板契约键名）+ 分类 Tag 集/FlowSource/CapturedAttrs 定位澄清；§2.3 修改器唯一通道（D7-6）；D7-7 模板重定向（四粒度让渡）。
- v4（2026-09-11，PV 系列 D7-2 收窄）：**基础伤害值来源 = 链步骤数据配置（StateParam 参数账本解算——复合运算由参数链承载），结果输入流程，流程零基础值计算**（用户否决流程域拼装/表达式源/双字段三案）；`CalculateBaseDamage` 从 ★主公式 **降级逃生口**（普通项目零 delegate）；公式参数初值/FlowModify Operand 换型 FTcsParamValue（黑板键引用保留为流程域自身选项）；流程中途公式贡献 = 伤害修改器自实现（D7-6 通道，不开 BaseDamage 公式口）。
- v4 增补（2026-09-15，D5-5 v3 交叉引用补齐）：§2.1/§2.3 流程属性黑板的折叠口径显式化——统一调用 TcsAttribute 共享带式折叠纯函数（三处共用之一），实现落点指向补齐；顺带移除无出处的旧词"三域三制"。
- v4 增补 2（2026-09-23，标识体系 tag 化改造回写）：§2.1 公式参数初值键 `TMap<FName, …>` → **`TMap<FGameplayTag, FTcsParamValue>`**；黑板容器名对齐实现（`FFlowAttributes` → **`FTcsFlowAttributes`**）；**契约键归框架**（`DamageFlowKey.*` 原生声明）与项目键自由 tag 的分工写明；§2.2 模板身份补 `TemplateId: FGameplayTag`、模板类型名对齐实现（`FTcsDamageFlowTemplate`，WAIT-8 轮改名）；重定向字段为 tag。落点 = 提案 `switch-identifiers-to-gameplay-tags`（2026-09-22 归档）。
- v4 增补 3（2026-09-30，消耗策略形状改造 + `ModifyFlow` 落地回写；提案 `add-damage-modifyflow-primitive`）：`FTcsConsumePolicy` → **`FTcsDamageModifierConsumePolicy`** 且改为**纯数据可反射结构**（`USTRUCT` + 三字段 `UPROPERTY`；去 `TFunction OnConsumed` 闭包，"被消费时要做什么"改**事件语义** = 原生事件 tag `TcsEvent.Damage.ModifierConsumed` + 项目经触发行订阅；消费**动作**仍归台账 `DAMAGE-4`）；§2.2 标准步骤表 Execute 行 / §2.3 消耗型 / §2.4 提交原语 / §3 收集≠消费 四处 `OnConsumed` 口径同步；§2.4 视图「不含黑板」的理由改写为**嵌套容器 + 运行态提交记录**（原 `TFunction` 根因已消除 ⇒ SCRIPT-3 降级为常规工作量）；**顺带校正**：§2.2 数据步骤 `FlowModify` 字段表原写 `SortKey`（实现无此字段，以源码为准改为 `Conditions`）并补「MUST NOT 携带消耗策略 = 职责划分」。
- v4 增补 4（2026-10-04，R4 收束回写）：**§2.3 补"修改器通道三段全通"的落地状态**（订阅侧触发行 / 提交侧 `FTcsStepModifyFlow` / 端到端实证 `EVID-2026-10-04-modifier-channel`）+ **硬约束明文**（`ModifyFlow` 只能用于同步单步链——安全性约束，不只是创作糖；头注释那半本次补齐）；**§7 验收钩子逐条对账**（两条已实证、四条仍未验/未落地，明确标注不得外推）。落点 = `PLN-R4` Task 5。
