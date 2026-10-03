# 设计：触发行独立资产载体

## Context

- **上游**：`PLN-R4`（`plan-r4-trigger-row.md`）Task 2.5。计划的 `Interfaces` 块给出资产类素描（`PrimaryAssetType` / `TriggerTag` / `Def` / `GetPrimaryAssetId` / `IsDataValid`）与两条 DefLibrary 新面（`DiscoverTriggerDefs` / `ResolveTriggerDef`）。
- **既有先例**：链资产 `UTcsEffectChainDef`（`TcsIntegration/Public/Chain/`）与它的 DefLibrary 发现路径（`DiscoverChainDefs` / `ResolveChain` / `SeedWorld`）——本批**照该形态**做第二条路径，而不是另创一套。
- **规格义务**：`effect-trigger` 规格已写"定义加载期登记（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"；`integration-entity` 规格的「定义库」需求今天只覆盖链。
- **业务场景（用户 2026-09-23 确认）**：本资产 = **系统级规则**（全局常驻、与任何 Def 无关）；Buff/Skill 的行为走**内联**（施加时注册、`Source` = 状态实例句柄），内联位在 R5/R6 给那两个 Def 加字段时自然成立。

## Goals / Non-Goals

- **Goals**：①策划零 C++ 配出一条系统级触发规则；②该规则真在世界里生效（不是"存得下、跑不起来"的假控件）；③定义库口径与链资产**同款**（身份 / 双真相 / 失败清单 / 幂等装配 / GC 锚定）。
- **Non-Goals**：SkillDef/BuffDef 内联位（R5/R6）；`EventPayloadFilter` 的字段筛（R4 只存不裁）；链资产 `IsDataValid` 的 `NotValidated` 提升（台账 `TOOLS-2` ③）；宿主"自行登记行"的路径（既有 API，不动）；`TriggerTag` 的改名重定向机制（本批无存量词，不需要）。

## Decisions

### D-11 资产类名 = `UTcsEffectTriggerDefAsset`（**UHT 硬约束**；`<Family>Def` 形态被编译期否决）

**本条是实施期被编译器推翻的一次决策，原文（"改名 `UTcsEffectTriggerDef`，理由 = Def 资产命名标准不带 `Asset` 后缀"）保留在此以防复活**：

- **实测（2026-10-04，Development Editor 首次编译）**：`UTcsEffectTriggerDef` 被 UHT 直接拒绝——
  `Error: Class 'UTcsEffectTriggerDef' shares engine name 'TcsEffectTriggerDef' with struct 'FTcsEffectTriggerDef'`（住 `TcsEffect/Public/Trigger/TcsEffectTrigger.h:50`）。**UHT 按"去前缀后的引擎名"判重**：`U` 类与 `F` 结构体在 UHT 的名字注册表里算同一个名字 ⇒ 与"头文件名必须唯一"是**两条独立的硬门槛**（同族先例：全项目头文件名唯一，见引擎机制事实）。
- **可选项与取舍**：①**改数据 struct 的名字**（`FTcsEffectTriggerDef` → 别的）——否决：它是 Task 1 已归档的公开面，被 `FTcsEffectTriggerInstance` / 登记表 / 求值器 / 规格 / 证据文档共同引用，改名收益为零而回归面最大；②`UTcsEffectTriggerRuleDef` 一类新族名——否决：会把根名（`EffectTriggerDef`）与类名拉开，且"Rule"与设计语料（触发定义）不同词；③**资产类加 `Asset` 后缀**——采纳，即计划 sketch 的原名。
- **标准随之补限定语**（`openspec/project.md` 的「Def 资产命名标准」）：资产类**默认**为 `<Family>Def`；**当 `<Family>Def` 已被同族数据类型占用时**（UHT 引擎名判重），资产类 MUST 加 `Asset` 后缀消歧——本类是该限定语的第一个实例。
- **根名不受影响**：`TriggerTag` 的值仍住 `EffectTriggerDef` 根（根名描述的是**消费角色**，与类名无关）。
- 文件落点 `Source/TcsIntegration/{Public,Private}/Trigger/TcsEffectTriggerDefAsset.{h,cpp}`——与计划 sketch 的文件名一致。

### D-12 定义库**代行登记**：装配期把每条定义注册成该世界的触发行（否则资产零消费者）

计划 sketch 只写了发现与解析两条面——**照此落地则资产是"填了没人用"的假控件**：全库没有任何调用方会把 `TriggerTag` 变成一行触发行，`ResolveTriggerDef` 也无人调用（与 Task 1"零调用方"的处境相同，但那时机制层欠的是下一个 Task，本批欠的是**本 Task 自己的业务闭环**）。而规格已经把"定义加载期登记"写成事实。

- **做法**：`SeedWorld` 在链装配之后逐条 `RegisterTriggerRow`，`Source` = 定义库自持的来源句柄（`FTcsSourceHandleRegistry::Allocate`，`Initialize` 发放一次；来源句柄进程内唯一、按值比较，跨世界复用同一枚即可——行本身住世界级子系统、随世界反初始化清空，不存在跨世界残留）。
- **幂等**：沿用既有 `SeededWorld` 判据（同一世界只装配一次）。
- **被否**：①只做载体 + 解析（假控件）；②只做载体、由宿主自行登记（规格已写"定义加载期登记"，把义务推给每个宿主 = 同一段代码在 N 个宿主里重写）；③引入"资产变更后热重装"（零消费者，YAGNI）。
- **代价**：定义库第一次有了"装载以外的副作用"（登记行）——但它仍是 GameInstance 级 Const 面（不持可变运行态；行住世界级子系统）。

### D-13 新根段 `EffectTriggerDef`（否决裸 `EffectTrigger`）

触发定义的**内容身份**需要一个根：资产身份 `[PrimaryAssetType, TriggerTag.GetTagName()]`、去重键、`ResolveTriggerDef` 的查询键三处共用它；它是**宿主内容词汇**（词由宿主的规则清单决定），按治理规则 `EffectTriggerGate` 的同款推导：`EffectTrigger`（限定词，取机制名）+ `Def`（角色名）。

- **被否 `EffectTrigger`**：它会是 `EffectTriggerGate` 的**真前缀**——BP/CS 动态订阅层支持**部分匹配**（本项目 `project.md` 已记该事实），届时"订阅 `EffectTrigger`"会把开关词 `EffectTriggerGate.*` 一并命中；两个语义不同的根互为前缀是治理地雷。`EffectTriggerDef` 与 `EffectTriggerGate` 是**兄弟节点**，互不为前缀。
- **被否"不设身份 tag"**（用资产文件名当身份）：违反 Def 资产标准（改名/挪目录即失联），且 `ResolveTriggerDef` 无从按 tag 查询。
- **被否"复用 `EffectChain` 根"**：链 id 与规则身份是两个消费角色（一角色一根）。
- 顺带钉死**检查词的 `Check` 子段约定**：仅供人工检查的词 MUST 在**功能根**下加 `Check` 段（`<根>.Check.<名>`），MUST NOT 另立根——因为它们的消费路径与正式词完全相同（链 id 仍须 `RegisterChain` 认、规则身份仍须定义库认）。既有 `EffectChain.Check.{SelfSub,Sort,SortMulti,WaitEvent,WaitEvent3}` 已是此形态，但规格只登记了"2 段"⇒ 本批把规格订正为事实。

### D-14 引用链预检 = **Warning + 仍登记**（否决"跳过登记"与"不校验"）

`Def.EffectChainId` 指向的链在那个世界里可能不存在（内容漏配，或宿主在运行期才 `RegisterChain`）。

- **选定**：不可解析 ⇒ 一条 Warning（含 `TriggerTag` 与链 id）+ **仍登记**。理由：宿主"先起世界、后登记链"是合法时序（`RegisterChain` 是公开面），跳过登记会让这条规则**永久失效**且无人知晓；而 Warning 已把内容缺口显式报出。
- **被否"跳过登记"**：把"暂时没链"判成"永久禁用"，且失效是静默的（规则不跑不等于规则不在）。
- **被否"不校验"**：内容漏配要到"事件命中 → `ExecuteChain` 拒绝（Error）"才暴露，且是**每条命中事件一次**的 Error 刷屏——把一次性配置缺口摊成运行期噪声。
- **边界**：本条只覆盖**定义库装配**这一条入口；求值器**不**重复校验（Task 2 已定的口径：起链被拒的面归 `ExecuteChain`）。

### D-15 定义库 MUST 覆写 `AddReferencedObjects`——两份缓存都要走（计划注记此处判据是错的）

计划 Task 2.5 注记写"缓存若用 `TMap<FGameplayTag, TUniquePtr<...>>` 持**定义内容**（值语义 struct）则无需 ARO；但**资产对象本身**必须 `UPROPERTY` 锚定"——**前半句是 Task 2 已经被纠正过的同一处判据错误**（见 `FTcsTriggerRegistry::AddReferencedObjects` 的注释与 `MEM`：是否需要 ARO 与值/指针语义**无关**，只取决于**容器是否 GC 可见**）。

- 事实：`UTcsDefinitionSubsystem` 的 `ChainDefs` / `TriggerDefs` 都是**非 `UPROPERTY` 成员**，GC 的 `RefLink` 走不到；而两者的内容里都有 `FInstancedStruct`（链 = `Steps`、触发 = `Conditions` / `EventPayloadFilter`），内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用（D4-16 类型不设限）⇒ **静默回收**（表现是"条件里那个对象变成空引用"，不是崩溃）。
- **做法**：覆写 `static void AddReferencedObjects(UObject*, FReferenceCollector&)`，对两份缓存逐条调 `AddPropertyReferencesWithStructARO`（与 `FTcsTriggerRegistry` / `ChainDefs` 同款手法）；资产对象仍以 `UPROPERTY` 数组锚定。
- **链缓存那一半属驱动修**：同一个类、同一种缺陷、同一处覆写的三行循环——不修就是"明知有洞还只补半边"。

### D-16 `IsDataValid` MUST 把 `NotValidated` 提升为 `Valid`（新类做对；链资产的同类缺陷不在此修）

`UObject::IsDataValid` 基类默认返回 `NotValidated`（`Obj.cpp:6096`）⇒ 调 `Super` 起步、无错无警的资产会返回 `NotValidated`，编辑器显示为"未验证"，装置若断言 `== Valid` 必然失败。

- 本类：`Super` 起步 → 无错时把 `NotValidated` 提升为 `Valid`；有错置 `Invalid`。
- 链资产 `UTcsEffectChainDef::IsDataValid` 缺同一段——**归台账 `TOOLS-2` ③**（它是"校验矩阵"轮的事），本批只保证**新类不再复制该缺陷**，避免顺手扩大回归面。

## Risks / Trade-offs

- **每次 PIE 多一次资产扫描**：`DiscoverTriggerDefs` 与链那条同款（`IsLoadingAssets` 时 `WaitForCompletion`）⇒ 编辑器启动/首次 PIE 的成本线性叠加一次按类查询。当前资产数量个位数，可忽略；M6 `PrimaryAssetTypesToScan` 轮会换掉这条路径（台账 `INTEG-3`）。
- **预检 Warning 的误报面**：宿主"运行期才登记链"时，装配期会报一条 Warning 而规则其实可用（D-14 已接受：Warning 非阻塞、且规则仍登记）。
- **GC 补引用漏走**：`FInstancedStruct` 内层只对"带 `WithAddStructReferencedObjects`"的类型递归——宿主若用非反射容器自持对象引用，本机制照不到（既有边界，同 `FTcsTriggerRegistry`）。
- **治理面跨仓**：插件侧登记了 `EffectTriggerDef` 根，但宿主的词与 LAC 侧注册表条目尚未落 ⇒ 在 LAC 补齐前，任何 `EffectTriggerDef.*` 词都解析不到。**处置**：本批的人工检查资产用 `EffectTriggerDef.Check.*`（检查词），并把它与"LAC 侧根登记 + ini 词"登记为 Task 4 的前置动作。
- **`Check` 子段约定是新写进规格的规则**：它约束既有 5 个 `EffectChain.Check.*` 词（昨天刚落地）——本批不删不改这些词，只把形态写清（否则规格的"2 段"与仓库现实不符）。

## Migration Plan

- **无存量迁移**：新资产类、新根段、新发现路径，零已存资产受影响（`Content/` 下没有触发定义资产）。
- **回滚**：删资产类 + 撤 DefLibrary 的两条新面 + 撤销 `SeedWorld` 的触发登记段 + 规格回退（`EffectChain.Check.*` 不受影响）。
- **落地顺序**：提案 validate → 资产类 → 定义库（发现/校验/缓存/解析/装配/ARO）→ Development + Shipping 编译 → PIE 人工检查（正路 / 失败面 / 幂等）→ 规格与计划回写 → 归档。

## Open Questions

1. **LAC 侧根登记（`host-gameplay-tag-registry` 的声明位置清单 + ini 词）何时落**：本批（小改动，但此刻只有一个检查词要用）vs 随 Task 4 的破甲规则词（`EffectTriggerDef.ArmorBreak`）同批？
2. **检查资产是否入库**：`Content/TcsDev/Checks/` 已有 5 个链资产的先例；本次规则资产按其形态入库（`DA_Check_TriggerRule`），还是建完即删、只留日志证据？
