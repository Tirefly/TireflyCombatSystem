# 项目上下文

## 用途
TireflyCombatSystem（TCS）是一个面向 Unreal Engine 的通用、综合战斗系统框架插件，构建在数据-行为分离（Data-Behavior Separation，定义资产 / 池化实例 / 集中执行）理念之上。旧的 TCS 仓库已冻结（MD-3），本仓库是从零重建（`remake` 分支），当前处于 R3 实施阶段：一个覆盖六个模块（Core / Attribute / Effect / Targeting / Damage / Integration）的纵向切片。插件必须服务多个宿主项目——今天的 LegendAutoChess，以及将来某个第三人称 ARPG——并且必须保持可扩展到服务器权威网络。

## 技术栈
- Unreal Engine 5.8，C++ 插件（`.uplugin` 将在 R3 plan1 Task 0 中重写）。
- 当前已实体化的模块（R3 纵向切片）：`TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`。目标实体化仍为十一个编译模块（R0 §9）：当前七个加上 `TcsState`、`TcsSkill`、`TcsCue` 和 `TcsEditor`。
- 插件级依赖目前在 `.uplugin` 中不存在；模块级依赖由各自的 `Build.cs` 声明。StateTree、GameplayStateTree、GameplayMessageRouter 和 TireflyObjectPool 是历史/目标语境的参照，不是当前插件描述符的依赖。
- 通过 OpenSpec 进行规格驱动开发（2026-09-10 初始化，openspec CLI ≥ 0.23）。

## 项目约定

### 代码风格
- 遵循用户级 `unreal-cpp-style` skill（`C:\Users\TireflyPC\.agents\skills`），包括其中的「模块目录布局」章节。
- **Def 标识与主资产标准（2026-09-17；`PrimaryAssetType` 取值于 2026-09-21 澄清；**标识于 2026-09-22 迁移到 `FGameplayTag`**）**：每个定义在**每条轨道上恰好有一个 id** —— DataTable 行的标识是它的 **`DefTag`/`TemplateTag`（`FGameplayTag`）**，而它的 **RowName（`FName`）只是编辑期定位符**（引擎的 `FTableRowBase` 键类型是 `FName`，无法承载 tag —— 两者是**分工，不是双重事实**：MUST NOT 要求它们同名），而资产的标识是它的 **`[PrimaryAssetType, DefTag.GetTagName()]`**：Def 资产显式声明 `static const FPrimaryAssetType PrimaryAssetType` —— **"声明"是显式的而非继承而来，且其"取值"就是类名**（`UTcsAttributeDef` → `"TcsAttributeDef"`，`UTcsAttrModDef` → `"TcsAttrModDef"`，`UTcsEffectChainDef` → `"TcsEffectChainDef"`；原先"不从类名派生"的措辞意思是"不要依赖引擎默认的 `GetClass()->GetFName()` 回退" —— **不是**"取一个与类名不同的值"）—— 并重写 `GetPrimaryAssetId()`，使 *name* 来自标识 tag 的 `GetTagName()` 而不是资产自身的名字（`FPrimaryAssetId` 的 `name` 槽位是 `FName` —— 这是引擎类型约束）—— 资产文件因此可以重命名或移动而不破坏解析。因此，向消费方注册 Definition 时要显式传入 tag。在 `PrimaryAssetTypesToScan` 中注册类型属于 M6 DefLibrary 轮次（id 在没有它的情况下也可解析）。
- **标识符 = `FGameplayTag` 标准（2026-09-22，用户决策；取代 D2-1 中基于 `FName` 的解读）**：**所有配置引用类标识符都是 `FGameplayTag`** —— 属性名、参数键、黑板键、chain id、flow 模板 id 以及 Def id。`FTcsAttributeName`（原先的 `FName` 包装）被**完全移除**；它通过 `explicit` 构造提供的保护，由引擎的 **`protected explicit FGameplayTag(const FName&)`**（`GameplayTagContainer.h:219`）加上 `RequestGameplayTag(..., ErrorIfNotFound=true)` 取代 —— 也就是说，裸 `FName` 既无法进入接收 tag 的 API，*并且*拼错的 tag 会在解析时触发 ensure 而不是静默降级。**Tag 词汇表契约（2026-10-01 —— 已从本文件迁出）**：归属规则（"谁拥有这个词就由谁声明"）、插件侧根注册表、根命名判据、每个根单一角色的正交规则、深度上限，以及逐词命名约定，现在位于 **`gameplay-tag-governance` 能力**（由变更 `reroot-gameplay-tag-vocabulary` 提出；归档后为 `openspec/specs/gameplay-tag-governance/spec.md`）。引擎级机制（声明位置分类、重命名/重定向、段风格、`DevComment` 撰写）位于用户级 `unreal-gameplay-tags` skill。**本文件 MUST NOT 复述 tag 规则**（每个主题只有一个载体）—— 它只保留 `FGameplayTag` 作为标识符这一标准本身。**`RequestGameplayTag` 会加锁**（`GameplayTagMapCritical`）—— MUST NOT 在热路径上逐帧调用；项目 tag 解析一次后缓存（宿主侧辅助 `TcsDevTags` 是参考形态）。
  - **字段默认值陷阱（2026-09-22，已实际踩中三次）**：`FName` 常量*可以*作为 `UPROPERTY` 默认值；**`FGameplayTag` 不行** —— 原生 tag 在模块加载时注册，因此静态初始化器里的默认值是空的。两个推论：(a) `const FGameplayTag& Alias = Tag_X;` 是**禁止**的 —— `operator FGameplayTag()` **按值**返回，所以引用绑定到静态初始化期间创建的临时对象，该别名将**永远持有空 tag**；应在每个*使用点*（运行时）写 `FGameplayTag(Tag_X)`。(b) 过去默认指向契约键的字段 MUST 在**执行器内部**回退（`Step->Key.IsValid() ? Step->Key : FGameplayTag(Tag_DamageFlowKey_X)`），或在装配期由宿主赋值。
- **Def 资产命名标准（2026-09-17）**：定义资产命名为 `<Family>Def` —— **不带 `Asset` 后缀**（设计语料已把该概念称为"Def 资产"，所以 `…DefAsset` 是重复）；它们的 DataTable 行是 `<Family>DefTableRow`。全家族：`UTcsAttributeDef` / `UTcsAttrModDef` / `UTcsSkillModDef` / 以及（尚未实现的）`UTcsStateDef` / `UTcsBuffDef` / `UTcsSkillDef`；行为 `FTcsAttributeDefTableRow` / `FTcsAttrModDefTableRow` / `FTcsBuffDefTableRow` / `FTcsSkillDefTableRow`。
- **事件 tag（2026-09-18；命名/归属于 2026-10-01 迁出）**：框架事件 tag 遵循 **`TcsEvent.<Domain>.<EventName>`** 约定（2026-10-01 重定根：原为 `Tcs.Event.<Domain>.<EventName>`），并由**拥有该事件的模块原生声明**（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG_COMMENT`）—— 绝不在 `TcsCore` 中声明（它 MUST NOT 持有任何战斗域词汇）。命名、归属和共享根规则位于 `gameplay-tag-governance` 能力。domain 段是真实的层级节点：同一 domain 的兄弟事件（属性 added/removed 等）挂在 `TcsEvent.<Domain>` 下 —— 注意总线**当前的原生订阅路径是精确匹配 tag**，因此那里的父 tag 订阅要等原生层级匹配（一个待办的总线事项）；BP/CS 动态层已支持部分匹配。
  - **C++ 常量名（2026-09-21；规则于 2026-10-01 迁出）**：原生 tag 常量的命名规则位于 `gameplay-tag-governance` 能力 + 用户级 `unreal-gameplay-tags` skill。重定根后的示例：`TcsEvent.Damage.Recorded` → `Tag_TcsEvent_Damage_Recorded`（原为 `Tag_Tcs_Event_Damage_Recorded`）—— 早先的形式把 `Tcs`+`Event` 合并成一段，曾导致从常量名推导出真实的层级错误。
- 实现类型带 `Tcs` 前缀：`FTcsAttributeId`、`UTcsAttributeSubsystem`、`TTcsInstanceHandle`；枚举值使用缩写前缀。设计文档中的类型名是概念名 —— 执行时以 plan 为准。TcsAttribute 内部刻意并存两个实现命名家族：**modifier 家族**使用短前缀（`UTcsAttrModDef` / `FTcsAttrModInstance` / `FTcsAttrModOperand(Def)` —— `Def` = 模板，`Instance` = 由它实体化出的账本条目），而 **attribute 家族**使用完整单词（`FTcsAttributeName` / `FTcsAttributeInstance` / `FTcsAttributeBounds` / `FTcsAttributeStore` / `ITcsAttributeProvider` / `UTcsAttributeDef` / `UTcsAttributeSubsystem`）。Op band（`ETcsAttributeOp`）是属性级词汇（与 M5 参数链共享），不属于 modifier 家族命名。
- 模块目录布局（口径已收窄）：模块根只放 `Build.cs`、`Module.h`、`Module.cpp`；域代码位于 `Public/` / `Private/` 下，使用 PascalCase 域子目录。
- 日志通道是独立文件：`Public/<ModuleName>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN）+ `Private/<ModuleName>LogChannel.cpp`（DEFINE_LOG_CATEGORY），命名为 `Tcs<ModuleName>LogChannel`；UE category 名遵循 `LogTcs<Module>`（例如 `LogTcsCore`）。写日志的代码包含 LogChannel 头文件，绝不包含 `Module.h`。
- 文件为 UTF-8 无 BOM、LF 行尾。

### 架构模式
- 数据-行为分离：定义是 UObject Def 资产；实例是按强类型句柄（index + generation，D0-2）寻址的池化 USTRUCT；单个 `TickableWorldSubsystem` tick 泵加可注入的 `FCombatClock` 是过期（expiry）的唯一驱动器（D0-5）。单一游戏线程假设并配断言（D0-4）。Def 资产家族共享**一个**基类 `UPrimaryDataAsset`（`UTcsStateDef` 家族 / `UTcsAttrModDef` / `UTcsSkillModDef`，2026-09-17 决定）—— Def 引用携带由 DefLibrary 解析的 `FGameplayTag` 标识，而 `FPrimaryAssetId` 把 tag 转换为其引擎强制的 `FName` name 槽位；属性词汇的表格行轨道是 `FTcsAttributeDef`（`FTableRowBase`，row name 仍是编辑期定位符）。
- 最小编译集依赖规则（R0 §9）：`Core←Attribute←Effect←{Damage,Targeting,State}←Skill`；表格列是最小编译集，箭头链只是方向许可 —— TcsSkill 针对 Core/Attribute/State/Effect/Notation 编译，组合发生在 TcsIntegration 或宿主中。
- 基于注册表的 step 分发（D4-14）：`TcsEffect` 只依赖 Core/Attribute；域 step 类型 + 执行器从各自模块自注册（`UE_DEFINE_EFFECT_STEP_EXECUTOR`）—— Damage/Heal/ModifyFlow→TcsDamage，SelectTargets→TcsTargeting，ApplyState→TcsState，PlayCue→TcsCue。
- 策略统一（D3-7 v3，取代 2026-09-02 的 EditInlineNew Instanced UObject 决定）：每个策略都是带 C++ 虚派发（StateTree 风格）的反射 USTRUCT 基类，在 Def 资产/step 上以**普通 `FInstancedStruct`** 成员持有（**2026-09-24 切换**：原为 `TInstancedStruct<Base>`；模板形式导出到宿主脚本层时是一个空壳，导致策略字段无法从 C# 配置；类型限制现在来自手写的 `meta=(BaseStruct=...)`）；BP 策略扩展被有意放弃（R0 "不承诺蓝图"）；实例不携带任何策略/载荷/订阅状态。
- 自定义逃逸位规约：每个策略枚举的 `Custom` 固定为值 1；值 0 始终是内置默认 / None。
- 实例数据结构禁止 `TSubclassOf`；热路径永不重查 Def（D2-9 / D5-13）。
- `FTcsParamValue{ FInstancedStruct Source }`（**2026-09-24 起为普通形式**）是 TcsCore 中统一的数值配置载体（PV 系列 2026-09-11，取代 D2-12 的 `FTcsParamScalar`）：抽象基类通过 `virtual double Evaluate(...)`（D3-7 v3 形式）求值，内置 `Literal` / `ParamRef` 源；域专属源位于各自拥有的模块 —— StateLevelArray/Map + InstigatorLevelArray/Map 与 `ITcsEntityLevelProvider` 接口（GetEntityLevel，宿主实现；TargetLevel 源暂缓）位于 TcsState，AttributeScaled 位于 TcsAttribute；基础伤害 = 参数账本解析后喂给伤害流（D7-2 收窄 —— 该流从不计算基础伤害）；属性 modifier 操作数保持独立的 Def/运行时形态（D2-13，账本保持已解析的 double）。
- 参数链与属性 modifier 共享**同一个** band-fold（D5-5 v3，2026-09-14）：op 为 `Add`/`PercentAdd`/`Mul`/`FlatAdd`/`Override`，在行内与跨行都与顺序无关；`Override` band 由三级全序决定 —— `OverridePriority`（modifier 侧，高者胜，唯一的第一仲裁者）→ `OverrideTieBreak`（attribute-Def 侧，封闭的四值策略：max / min / max-abs / min-abs，无自定义策略）→ 带符号的值（打破子策略平局，例如 max-abs 下的 ±5）；胜者替换其余一切（包括 `FlatAdd`）。单个纯 fold 函数位于 TcsAttribute，被 M2、M5 链和 Damage flow 属性复用。Notation 暴露**描述视图策略**（D5-17 v3）：`Def.Descriptions` 持有逐描述条目 `{DescriptionId, TextKey, Views[]}`，其中每个视图槽是一个 `TInstancedStruct<FTcsParamView>` 策略（内置 Value / Series / Range / Attribute）；StringTable 文本只承载槽位名（零语法），所有机器语义都位于结构化、编辑器可校验的配置中 —— `IsCompatible(Probe)` 在保存时拒绝不匹配的源×视图组合，宿主自定义视图无需改动任何公开代码。约定列被限制在白名单内 —— 可在 `Literal`/表格源上配置，在 `ParamRef`/`AttributeScaled` 上禁止（D5-18 v3）。
- 服务器权威网络姿态（D0-1）：模拟核心以无头方式运行，客户端持有镜像状态，预测只覆盖表现层。LAC 本身目前是单机。
- **宿主脚本扩展（2026-09-24，账本 SCRIPT-8 —— ✅ 已落地，提案 `add-host-scripting-slots`）**：TCS **不**内嵌脚本引擎（AS/C#/TS/Luau 由宿主选择 —— D4-17），插件持有**零**脚本相关代码。宿主特定/定制语义（例如"瞄准最近的敌人"、项目专属伤害公式、读取宿主专有状态的 step）由**脚本槽（script slots）**承载：框架提供*插槽*（`UObject` 接口 + `UFUNCTION(BlueprintNativeEvent)`），宿主用**任意 UE 脚本语言**填充。该机制是 **UE 自身的反射派发**（`UFunction::Invoke`），**不是 C# 专属** —— AngelScript / Luau / Puerts(TS) / Blueprint 全都支持，这正是该承诺真正语言无关的原因。**仓库内已验证的先例**：`UTcsEventHandler`（C# 重写可用）与 `ITcsAttributeProvider`（C++ 通过 `Execute_` 抵达 C# 实现）。**已落地的槽家族**：① `ITcsTargetSelectorHost` / `ITcsTargetFilterHost` + 转发器 `FTcsSelHostDelegate` / `FTcsFilterHostDelegate`（之所以需要，是因为 selector 家族使用**虚派发**，而这对脚本定义的结构体在*物理上*不可达 —— 没有 C++ 类型 ⇒ 没有 vtable）；② `UTcsStepExecutor` / `UTcsFlowStepExecutor`（`UObject` 基类，通过 facade 注册并配以 **GC 可见的 `UPROPERTY` 持有** —— 裸 C++ 注册表无法保持对象引用存活，即 WAIT-8 的缺陷形态）；③ `ITcsDamageFlowDelegate` 的 5 个方法重签为接收 `FTcsDamageFlowContextView`（一个**反射的只读投影**）+ `BlueprintNativeEvent` + 调用点切换到 `Execute_` + `RegisterTemplate` 反射化。**实施过程中发现的两条硬约束**（它们修正了账本的原计划）：**普通 C++ 结构体根本无法出现在 `UFUNCTION` 签名中**（UHT：`Unable to find 'struct'`），所以"加个 `UFUNCTION` 就行"是不可能的 —— 签名必须改为反射视图；并且 `BlueprintNativeEvent` 会触发 UHT 的蓝图参数校验，因此连句柄也必须是 `BlueprintType`（`FTcsChainRunHandle` 相应放宽 —— 零承诺面代价，因为消费句柄的 facade 方法保持 `UFUNCTION()` 且不带说明符）。**关键技术：传句柄，不传上下文** —— 槽签名接收 `FTcsChainRunHandle` 等，上下文通过 facade 的逐句柄访问器读写（`GetRunTargets` / `SetRunTargets` / `TryGetRunVariable` / `SetRunVariable` / `GetRunCaster` / `GetRunInstigator`）；这**完全绕开了上下文反射**（反正它也被阻断：`FTcsDamageFlowContext` 深处内嵌了一个 `TFunction`，因此永远无法整体反射）。**划分判据**：框架内部语义 / 热路径 / 引擎级不变量 → **C++**（插件，Authoring layer 3）；定制 / 宿主专有 / 不值得写进插件 / 非热路径 → **槽**（宿主脚本）。两条轨道并存：内置走 C++ 快路径（`TFunction` / 虚派发），槽只为宿主扩展而存在。**C# 重写惯用法因槽载体而异**：`UINTERFACE` 槽需要*两半*（一个仅签名的 partial 声明加一个 `<Name>_Implementation` partial 实现体）；`UCLASS` 槽直接在生成的 `public virtual` 方法上重写。这**不**放松"词汇 = 代码，句子 = 数据"的边界 —— 仍然禁止的是*在数据中生长新语义*（chain 中的内联表达式/脚本片段）。
- **复制安全的结构体纪律（2026-09-20，源码已核实）**：任何要跨网络传输的结构体 MUST 是纯反射数据 —— **不得有 `TFunction` 成员**（UHT 报错，`UhtSession.cs:2645`）、**不得有 `TMap`/`TSet`**（UHT 报错，`UhtMapProperty.cs:274` / `UhtSetProperty.cs:201`；运行时回退会记录错误并且什么都不复制），并且由 `FInstancedStruct` 承载的内部结构体自身也必须复制合法（UHT 无法检查内部类型 —— M8 校验器覆盖此项；`FInstancedStruct` 本身在经典路径与 Iris 路径上都有原生 `NetSerialize`）。通道选择：追加式结果流 → `FFastArraySerializer`；低频事件 → RPC（不可靠 multicast 每次 net update 上限 2 个；可靠通道溢出会断开连接）；复制属性 = 状态语义。PushModel 默认关闭且在 Iris 下禁用；5.8 仍默认使用经典复制。研讨上下文：`Documents/combat-system-design/research/replication-posture.md`。
- TcsCore 不包含任何日志基础设施（D0-6）：日志只用 UE 原生 category；屏幕显示类验收信号来自直接调用 UE API 的测试装置。

### SCRIPT-8 证据边界

宿主脚本槽家族、反射视图、句柄访问器和胶水形态均已静态实现。UnrealSharp/C# PIE 已验证：模板注册、C# 伤害公式派发、run 句柄访问器、脚本 selector/filter 转发与空 Host 行为、Effect 执行器的同步执行与挂起/恢复执行、Flow 执行器的继续/中止行为，以及已释放/generation 不匹配的句柄。在同一 PIE 世界中，原生 `Obj GC` 发生在探针清空其字段之后、且在这些脚本宿主、两个执行器和伤害委托再次执行之前（`Documents/combat-system-design/evidence/2026-09-28-host-scripting-e2e-pie.md`；原始 `Saved/Logs/LegendAutoChess.log:2656–2707`）。这并未验证跨 PIE 世界的生命周期：动态执行器注册表是进程级的，而被捕获的 UObject 执行器由 world subsystem 持有；R-2 反射待办记录了 TCS 侧待补的补救措施。AS/Luau/TS 往返与脚本世界枚举仍未验证；`ITcsEntityQuery` 仍仅限 C++，因为其契约包含 `TFunctionRef`。

### 测试策略
- **禁止 TDD**（用户级最高纪律）：不写"先失败的测试"步骤。验证 = UBT 编译通过 + 定向人工检查点。
- R3 验收是 `Documents/combat-system-design/plans/plan-r3-vertical-slice.md` 中的七项人工清单，由一个 PIE 测试地图和一个测试装置驱动（plan2 Task 6）。

### Git 工作流
- 活跃分支：`remake`。旧实现冻结在旧仓库/分支上，仅供参考。
- 提交只在用户明确授权后发生；任务边界是等待用户评审的停止点。
- 提交信息风格沿用既有仓库约定：`【ADD】…` / `【MOD】…` / `【DEL】…`。

## 领域上下文
- **唯一事实来源**：`Documents/combat-system-design/` —— 从 `INDEX.md`（文档索引 + 当前进度）和 `decisions/dec-00-constitution.md` §9（模块实体化）入手。冲突解决顺序：用户实时决策 > 设计文档 > plan 文档 > 会话启动提示词。
- R3 计划：`2026-09-02-r3-plan1-core-attributes.md`（TcsCore + TcsNotation 外壳 + TcsAttribute）与 `2026-09-02-r3-plan2-damage-chain.md`（TcsEffect + TcsTargeting + TcsDamage + TcsIntegration）。M3 states / M5 skill / M7 cue / M8 编辑器工具已设计但不在 R3 切片内。
- 核心域概念（各自都有正式模块文档 01–11）：属性聚合管道（band、事务、read-registers-dependency、SCC 环检测）、带五轴 `FStateStackPolicy` 的状态中心注册表、效果链（15 个原语、触发器行求值器、异步四唤醒源）、伤害流模板（解释器 + 标准 step 库 + 官方默认模板）、带多轨冷却的技能施放账本、targeting selector/filter 策略（默认 Self / EventTarget），以及 TcsNotation 层（ValueConvention + 通过 `FText::Format` 占位符绑定描述文本）。
- 宿主集成契约：双层引导（GameInstance DefLibrary + World driver），`CombatEntity` 组件作为三职责适配器。

## 重要约束
- 确定性是一条轻量级纪律（D0-1），以单 tick 泵作为时间的唯一驱动器。
- Mass/ECS 内存布局工作明确不在当前范围内（未来的 TcsMass）；按 `2026-09-02-mass-连续内存数据布局备忘.md` 用 1000 单位压力测试再审。
- 开工前加载 agent skill：`unreal-development-workflow`（执行哲学）、`unreal-cpp-style`、`unreal-cpp-compile`（引擎路径探测 + UBT 构建）。
- 对于新能力、破坏性变更、架构调整或重大性能工作，在实施之前先在本 `openspec/` 目录中创建并验证 OpenSpec proposal。

## 外部依赖
- 宿主项目：LegendAutoChess —— 本仓库作为 git 子模块挂载在 `Plugins/Tirefly/TireflyCombatSystem`。
- 引擎插件：StateTree（仅决策/编排 —— 绝不执行 buff/skill）、GameplayStateTree、GameplayMessageRouter、TireflyObjectPool（同级 Tirefly 插件）。
- 预期 Unreal Engine 安装在 `E:\UnrealEngine\` 下；按 `unreal-cpp-compile` skill 探测确切路径。
