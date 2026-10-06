## MODIFIED Requirements
### Requirement: 定义库（GameInstance 级最小版）

`TcsIntegration` MUST 提供 `UTcsDefinitionSubsystem`（GameInstance 级），职责：按类发现定义资产 → 校验 → 缓存 → 就绪标记 → 装配到每个世界。

- **发现**：`IAssetRegistry::GetAssetsByClass`（不依赖 `PrimaryAssetTypesToScan` 注册——属 M6 轮）；**四条按类发现路径**：
  链资产 `UTcsEffectChainDef`（`DiscoverChainDefs`）、触发定义资产 `UTcsEffectTriggerDefAsset`（`DiscoverTriggerDefs`，**2026-10-04 新增**）、
  状态定义资产 `UTcsBuffDefAsset`（`DiscoverStateDefs`，**2026-10-04 R5 Task 1 新增**）、
  修正器模板资产 `UTcsAttrModDef`（`DiscoverAttrModDefs`，**2026-10-05 R5 Task 7 新增**——该资产族此前**全库零发现路径**，其身份词 `TemplateTag` 因此零解析消费者，即台账 `ATTR-1`）；
- **校验**：空身份 / 双真相 / 重复登记 → 计入失败清单 + Error，不静默跳过。**2026-09-22 改造：空身份判定从 `ChainId.IsNone()` 改为 `!ChainId.IsValid()`**；
- **按 tag 解析**：`ResolveChain(FGameplayTag ChainId)`（**2026-09-22 改造：参数类型改 tag**）、`ResolveTriggerDef(FGameplayTag TriggerTag)`（**2026-10-04 新增**）
  与 `ResolveStateDef(FGameplayTag DefTag) -> const FTcsBuffDef*`（**2026-10-04 R5 Task 1 新增**）、
  `ResolveAttrModDef(FGameplayTag TemplateTag) -> const UTcsAttrModDef*`（**2026-10-05 R5 Task 7 新增**）——均返回缓存内容的裸指针（修正器模板返回**资产对象本身**，因为消费者要的是它能被直接物化），未命中返回 nullptr（正常查询路径，不 ensure）；
- **装配到世界**：`OnPostWorldInitialization` 时把缓存定义登记进该世界的消费方子系统（幂等——同一世界只装配一次）。**2026-10-04 扩展**：链装配之后，MUST 把每条触发定义登记为该世界的**触发行**
  （`UTcsEffectSubsystem::RegisterTriggerRow`），`Source` = 定义库自持的来源句柄（`FTcsSourceHandleRegistry::Allocate` 在 `Initialize` 时发放一次）。
  这是 `effect-trigger` 规格"**定义加载期登记**（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"那条的实现落点——**只发现不登记即等于资产零消费者**（策划填了不生效）。装配结果 MUST 进日志（就绪行含触发定义条数，世界装配行含已装配行数）；
- **引用链预检（2026-10-04 新增）**：装配一条触发定义前 MUST 检查其 `Def.EffectChainId` 在该世界是否已登记（`UTcsEffectSubsystem::IsChainRegistered`）。不可解析时 MUST 留一条 **Warning**（含 `TriggerTag` 与链 id）并**仍登记该行**——
  MUST NOT 跳过登记（宿主可在世界运行期自行 `RegisterChain`，"先起世界后登记链"是合法时序，跳过会让该规则永久静默失效），MUST NOT 用 Error（内容缺口不是契约违规，且 `ExecuteChain` 已有自己的拒绝面）。**求值器 MUST NOT 重复该校验**（起链被拒的面归 `ExecuteChain`）；
- **触发定义的失败清单条目（2026-10-04 新增）**：资产加载失败或类型不符 / `TriggerTag` 无效 / `Def.EventTag` 无效 / `Def.EffectChainId` 无效 / `TriggerTag` 与另一资产重复——
  **后两条正是 `RegisterTriggerRow` 的 ensure 拒绝面**，故 MUST 在发现期拦下（MUST NOT 等到装配期让编辑器刷 ensure 红字）；
- **状态定义的发现期校验（2026-10-04 R5 Task 1 新增；四项构成经用户 2026-10-04 确认）**：四个**身份级**判据 MUST 计入失败清单——资产加载失败或类型不符 / `DefTag` 无效 / `Def.StatusTag` 无效 / `DefTag` 与另一资产重复。
  内容级规则 MUST NOT 在此重复（`Params` 键重复 / `ModifierRows` 空引用 / `Triggers` 内空字段 / `DurationTime` 未配 全归资产 `IsDataValid`，见 `state-def-asset` 能力）——发现期只判"这条资产能不能被按 tag 寻址"；
- **修正器模板的发现期校验（2026-10-05 R5 Task 7 新增）**：三个**身份级**判据 MUST 计入失败清单——资产加载失败或类型不符 / `TemplateTag` 无效 / `TemplateTag` 与另一资产重复。
  同款边界：内容级规则 MUST NOT 在此重复（`Def.Target` 无效 / 数值来源为空 / `OPK_AttributeScaled` 而属性 tag 无效 / 约定列白名单 全归 `UTcsAttrModDef::IsDataValid`，见 `attribute-types` 能力）；
- **修正器模板不参与世界装配（2026-10-05 新增）**：`AttrModDef` 的缓存 MUST NOT 触发任何每世界登记——它的消费形态是**资产直引用**（`FTcsBuffDef::ModifierRows` 持 `TSoftObjectPtr<UTcsAttrModDef>`，物化时 `Get()` / `LoadSynchronous()`），
  故既无"装配进子系统"这一步、也无"登记进状态门面"那一步。**如实记入边界**：`ResolveAttrModDef` 的运行期调用者本轮为**零**，本路径交付的是**身份解析与索引**（资产身份 `[PrimaryAssetType, TemplateTag]` 可寻址、重复身份在发现期被拦），不是运行期取用；
- **状态定义逐世界登记（2026-10-04 R5 Task 2 新增；Task 1 只做到缓存）**：状态定义 MUST 在装配到世界时**登记进该世界的状态门面**（`UTcsStateSubsystem::RegisterStateDef`），与本类缓存**同一份内容**——
  只发现不登记 = 资产零消费者（同触发定义那条的理由；Task 1 只做缓存是因为当时还没有消费方）。装配顺序 MUST 排在链与触发行**之后**（状态门面的存在性由世界子系统机制保证，无需额外门禁）；
  `DefTag` 无效或重复登记（**正常路径下不可达**——发现期已去重）MUST 留 **Warning** 并继续处理其余定义，MUST NOT 中断装配、MUST NOT 用 Error；
- **"不被装配到世界"那条场景的今日读法（2026-10-04 R5 Task 2 澄清）**：本条描述的是**本类的缓存步骤**——`StateDefs` 缓存本身 MUST NOT 建任何每世界结构（与链 / 触发行"装配即登记"不同）；
  "登记进状态门面"是**第二步、由世界装配路径发起**，不改变第一步的边界。**来源句柄**：本条不为状态定义单独发号（状态实例的来源句柄归状态门面在施加时发放，Task 0 的统一发号器）；
- **去重键** = 链 `ChainId` / 触发定义 `TriggerTag` / 状态定义 `DefTag` / 修正器模板 `TemplateTag`（各自与资产身份同键）——同一键的两个资产 = 内容冲突，MUST 拒绝后一个并入失败清单（口径同链的"重复登记"，不静默覆写）；
- **GC 引用收集（2026-10-04 新增，必需；2026-10-05 边界澄清）**：本类 MUST 覆写 `AddReferencedObjects` 并逐条走**三份非 `UPROPERTY` 缓存**（`ChainDefs` / `TriggerDefs` / `StateDefs`）——它们都是本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它们**，
  而缓存内容的 `FInstancedStruct` 内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用（D4-16 类型不设限）⇒ 不补引用即**静默回收**。判据是"**容器是否 GC 可见**"，与"值语义还是指针语义"无关（同款先例 = `FTcsTriggerRegistry::AddReferencedObjects`）。
  **修正器模板缓存不在此列（MUST 记录理由）**：`AttrModDefs` 缓存的是**资产对象本身**，MUST 照既有两容器模式锚定——`UPROPERTY` 的 `TArray<TObjectPtr<UTcsAttrModDef>>` 保活（同 `StateDefAssets` 的形状），tag→资产 的索引 `TMap` 为非 `UPROPERTY`（它指向的对象已由该数组锚定）。
  故本函数**无需**为它补引用。**这正是"判据 = 容器是否 GC 可见"的应用**：同一类里两种容器、两种处理，不是因为类型不同，而是因为可见性不同。
  定义资产对象本身仍 MUST 以 `UPROPERTY` 锚定（`TUniquePtr` 持内容使 `Resolve*` 返回的指针地址稳定，但重载后资产被回收会让内容失主）。

#### Scenario: 资产被自动发现并装配

- **WHEN** 内容目录存在一个合法链资产，GameInstance 初始化后检查
- **THEN** 该资产已进缓存且 `IsRuntimeReady` 为真；世界初始化后消费方可按 tag 查到该链

#### Scenario: 非法资产进失败清单

- **WHEN** 某链资产 `ChainId` 无效或其 `Chain.ChainId` 与自身不一致
- **THEN** 该资产被跳过、失败清单含其路径与原因、其余资产仍可用

#### Scenario: 触发定义被装配为触发行

- **WHEN** 内容目录存在一个合法的触发定义资产（`TriggerTag` / `Def.EventTag` / `Def.EffectChainId` 均有效），世界初始化完成后检查世界内的触发行
- **THEN** 该世界多出一行触发行（行数 +1），其 `Source` = 定义库来源句柄；随后发布 `Def.EventTag` 事件时该行触发并执行 `Def.EffectChainId` 指向的链

#### Scenario: 引用的链未登记时留 Warning 但仍登记

- **WHEN** 一条触发定义的 `Def.EffectChainId` 在该世界不可解析（链未登记）
- **THEN** 留一条含 `TriggerTag` 与链 id 的 **Warning**，且该行**仍被登记**（行数 +1）；MUST NOT 出现 Error / ensure

#### Scenario: 触发定义的四类失败进失败清单

- **WHEN** 触发定义资产的 `TriggerTag` 无效，或 `Def.EventTag` 无效，或 `Def.EffectChainId` 无效，或两个资产的 `TriggerTag` 相同
- **THEN** 该资产被跳过并计入失败清单（含路径与原因）、该行**不**装配、其余资产仍可用；MUST NOT 出现 `RegisterTriggerRow` 的 ensure

#### Scenario: 同世界只装配一次

- **WHEN** 同一世界发生多次装配触发路径（世界初始化回调与立即装配）
- **THEN** 触发定义行只登记一次（行数不翻倍）

#### Scenario: 缓存内容的对象引用对 GC 可见

- **WHEN** 某定义资产的 `FInstancedStruct` 内层（链的步骤 / 触发定义的条件 / 状态定义的参数行与内联触发行）持有宿主自定义 struct 的 `UPROPERTY` 对象引用，且该对象在别处无强引用
- **THEN** 该对象不被 GC 回收（定义库 `AddReferencedObjects` 已逐条补引用，四份缓存无遗漏）

#### Scenario: 状态定义只进缓存、不被装配到世界

- **WHEN** 内容目录存在一个合法状态定义资产（`DefTag` 与 `Def.StatusTag` 均有效），GameInstance 初始化后检查（**本步只看缓存那一步**）
- **THEN** 该资产可按 `DefTag` 解析到 `FTcsBuffDef`，且本类的缓存动作**本身**不建任何每世界结构（该世界的触发行数与其它每世界登记表不因它变化）

#### Scenario: 状态定义被逐世界登记进状态门面

- **WHEN** 世界初始化完成后检查该世界的状态门面
- **THEN** 缓存里的每个合法 `DefTag` 都已登记进 `UTcsStateSubsystem`（`GetRegisteredStateDef` 命中且内容与定义库缓存一致）——
  **"只进缓存"（本条）与"登记进门面"（上一条）是同一资产的两步**，缺第二步即资产零消费者；全流程 MUST NOT 出现 Error / ensure

#### Scenario: 修正器模板按身份被发现与解析

- **WHEN** 内容目录首次出现一个 `UTcsAttrModDef` 资产（`TemplateTag` 有效），GameInstance 初始化后检查
- **THEN** 发现路径扫到它、`ResolveAttrModDef(TemplateTag)` 返回该资产、就绪日志含修正器模板计数；且该资产**不产生**任何每世界登记（触发行数与状态门面定义数不因它变化）

#### Scenario: 修正器模板的重复身份被拦在发现期

- **WHEN** 两个 `UTcsAttrModDef` 资产使用同一个 `TemplateTag`
- **THEN** 后一个被跳过并计入失败清单（含路径与原因）、`ResolveAttrModDef` 仍解析到前一个、其余资产仍可用；MUST NOT 静默覆写

#### Scenario: 修正器模板身份无效即无法寻址

- **WHEN** 某 `UTcsAttrModDef` 资产的 `TemplateTag` 无效（空 tag）
- **THEN** 该资产计入失败清单并被跳过（它无法构成 `[PrimaryAssetType, TemplateTag]` 身份，也无从按 tag 寻址）；其余资产仍可用
