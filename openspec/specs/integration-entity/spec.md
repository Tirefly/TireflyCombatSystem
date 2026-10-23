# integration-entity Specification

## Purpose
定义集成层的实体接线：三职责封顶的战斗实体组件、PIE 环境下的实体查询实现，以及 GameInstance 级的定义库（发现、装载、按 tag 解析与**逐世界装配**——把缓存定义登记进该世界的消费方子系统）。

## Requirements

### Requirement: 战斗实体组件（三职责封顶）

`TcsIntegration` MUST 提供 `UTcsCombatEntityComponent`（`UActorComponent` 派生，适配器零逻辑——06 §5），**恰好三职责**：

1. **身份锚**：`BeginPlay` 把所属 Actor 注册为战斗实体（`UTcsAttributeSubsystem::RegisterEntity` 发放句柄）并记录自己的句柄；`EndPlay` 反向注销。挂组件 = 策划声明"本 Actor 是战斗实体"。**门禁**：注册前检查定义就绪（`UTcsDefinitionSubsystem::IsRuntimeReady`）——未就绪留 Warning 并跳过（时序是宿主责任，不 ensure 刷屏）；
2. **查询门面**：`GetCurrent(FGameplayTag) -> double`（转发属性门面求值；**2026-09-22 改造：参数从 `FTcsAttributeName` 改为 `FGameplayTag`**）+ 施加/撤销调试用修正器入口（供宿主与装置；R3 不做 AttributeSet）；
3. **手动触发 API**：`ExecuteChainById(FGameplayTag ChainId)`（**2026-09-22 改造：参数类型 `FName` → `FGameplayTag`**）——组 `FTcsEffectContext`（`Caster` = 自身句柄、`Targets` = 自身句柄、`Instigator` 留空）→ `UTcsEffectSubsystem::ExecuteChain`；返回运行态句柄（全即时链在返回前已走完，句柄活性由 `IsRunActive` 判定）。

组件 MUST NOT 越出三职责；MUST NOT 自 Tick；MUST NOT 认识具体链/步骤类型。

#### Scenario: 组件注册后可查询与触发

- **WHEN** 挂载组件的 Actor 进入 Play，随后调用 `GetCurrent` 与 `ExecuteChainById`
- **THEN** 前者转发属性门面返回当前值、后者起链并返回运行态句柄（链未走完时有效）

#### Scenario: 定义未就绪时跳过注册

- **WHEN** `BeginPlay` 时定义库未就绪
- **THEN** 留 Warning 并跳过注册（不 ensure 刷屏），组件句柄保持无效

### Requirement: 实体查询实现（PIE）

`TcsIntegration` MUST 提供 `ITcsEntityQuery` 的实现 **`UTcsPieEntityQuery`**（**类名不得用 `UTcsEntityQuery`**——该 U 类名已被接口占用）：

- `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>)`：遍历世界中带 `UTcsCombatEntityComponent` 的 Actor 并吐**句柄**（**稳定序**——按组件注册序或 Actor 名排序，同输入同输出）；
- `GetLocation(句柄, FVector&)` / `IsAlive(句柄)`：经**本实现持有的"句柄 ↔ Actor"映射**解析（组件注册时登记、注销时移除）；
- **本实现是宿主侧唯一的"句柄 ↔ Actor"映射点**：机制层（TcsEffect/TcsTargeting/TcsDamage）MUST NOT 依赖该映射，内容资产 MUST NOT 存句柄（授权约束）。
- 注入方式：宿主在 `BeginPlay`/DefLibrary 就绪后调 `UTcsEffectSubsystem::SetEntityQuery`（R3 由测试/宿主接线）。

#### Scenario: 遍历吐句柄且稳定序

- **WHEN** 世界中三个带组件的 Actor 调用 `EnumerateEntities`
- **THEN** 访问者收到三个**句柄**（非 Actor），两次遍历顺序一致

#### Scenario: 句柄解析为定位与存活

- **WHEN** 以合法句柄调 `GetLocation` / `IsAlive`
- **THEN** 分别返回该 Actor 的坐标与"组件仍注册"的存活判定；未注册/已注销句柄返回 false

### Requirement: 定义库（GameInstance 级最小版）

`TcsIntegration` MUST 提供 `UTcsDefinitionSubsystem`（GameInstance 级），职责：按类发现定义资产 → 校验 → 缓存 → 就绪标记 → 装配到每个世界。

**就绪单出口（M6 双层引导的最小版）**：定义发现 / 加载 / 校验完成时 MUST 经**一个出口**通知外界，**恰好一次**（幂等——重复调用不重复登记），而不是让宿主轮询 `IsRuntimeReady`。义务方是**本类**（它知道定义何时齐备），不是宿主——「时序是宿主责任」只适用于**宿主自己**的接线顺序（见组件门禁那条），**不适用于定义库就该广播的那一次通知**。

> **恢复声明（2026-10-17，`ARCH-11` D-1 / D-3）**：本句与下文两个「就绪」Scenario 由提交 `539b14e`（2026-09-22，标识体系 tag 化）**误删**——同一提交删掉的还有 `damage-step-library` 的十步契约（`ARCH-10` D-2）。判别「写过被删」而非「从未写过」的铁证：被删后正文首句仍残留「就绪**标记**」字样，而「就绪**出口**」没了。
> **代码现状（如实登记，MUST NOT 读成本条已实现）**：`UTcsDefinitionSubsystem` 今天**没有** `OnDefinitionsReady` 这个函数——`Initialize`（`TcsDefinitionSubsystem.cpp:26-56`）在发现全部完成后只置 `bRuntimeReady = true`（`:40`），**不广播任何信号**；头注释 `:53` / `:312` 与 `Initialize` 内注释都写着"单出口 / 广播一次"，但那是**注释里的承诺，代码里没有实现**。⇒ 本条是**规格要求、代码未做**（`G-10` 形态），不是"规格描述现状"。

- **发现**：**两条路径并存，按族分配**——`FTcsPrimaryAssetQuery::GetAssetsForDefClass`（加载层统一查询口，`TcsAssetButler`）用于链 / 触发 / 状态三族；`IAssetRegistry::GetAssetsByClass` 用于修正器模板 / 技能两族（**2026-10-17 订正：原写「五条全走 `GetAssetsByClass`」，实为 3 走新口、2 走旧口**）。两条都不依赖 `PrimaryAssetTypesToScan` 注册（该注册属 M6 轮）。**五条按类发现路径**：
  链资产 `UTcsEffectChainDef`（`DiscoverChainDefs`）、触发定义资产 `UTcsEffectTriggerDefAsset`（`DiscoverTriggerDefs`，**2026-10-04 新增**）、
  状态定义资产 `UTcsBuffDefAsset`（`DiscoverStateDefs`，**2026-10-04 R5 Task 1 新增**）、
  修正器模板资产 `UTcsAttrModDef`（`DiscoverAttrModDefs`，**2026-10-05 R5 Task 7 新增**——该资产族此前**全库零发现路径**，其身份词 `TemplateTag` 因此零解析消费者，即台账 `ATTR-1`）；
  技能定义资产 `UTcsSkillDef`（`DiscoverSkillDefs`，**2026-10-06 R6 Task 1 新增**）；

> **发现路径订正（2026-10-17，`ARCH-03` G-7 / `ARCH-11` D-7）**：本条原写「`IAssetRegistry::GetAssetsByClass`…**五条**按类发现路径」，该说法**已假**——`PLN-REFACTOR-1` Task 6（2026-10-15）把**三条**（`DiscoverChainDefs` / `DiscoverTriggerDefs` / `DiscoverStateDefs`）改走 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`，只剩 **两条**（`DiscoverAttrModDefs` / `DiscoverSkillDefs`）仍走旧口。改动理由见 `Asset/TcsPrimaryAssetQuery.h` 头注释：类换过模块后 `GetAssetsByClass` 按**当前类路径**查索引会**静默返 0**（实测两个真实回归：`Slice.Run` 2/4→65/0、`State.Run` 6/9→15/0）。
> ⚠ **两条旧口的形态差异（`ARCH-11` D-7 新发现，MUST 记）**：新旧两口的**异步就绪门禁不同**——旧口用 `IAssetRegistry::IsLoadingAssets()`（`_Skill.cpp:25` / `_AttrModDef.cpp:24`），新口用 `HasInitialScanCompleted`（`TcsPrimaryAssetQuery` 内）；且这两条旧路径**各自重复了 13 行相同的 `IsLoadingAssets` + `WaitForCompletion` 骨架**。⇒ 「该改的只改了三条、且没写为什么留两条」——`TcsIntegration.Build.cs:35` 的注释仍写「五条 `Discover*` 由 `GetAssetsByClass` 改走」，与实际不符。
> **未改的两条不是遗漏（如实登记现状）**：它们**没有**改的理由，代码里也没有注释说明为什么——这正是 `ARCH-11` D-7 登记的债（**规格侧只管事实，理由缺失归代码债**）。
- **校验**：空身份 / 双真相 / 重复登记 → 计入失败清单 + Error，不静默跳过。**2026-09-22 改造：空身份判定从 `ChainId.IsNone()` 改为 `!ChainId.IsValid()`**；
- **按 tag 解析**：`ResolveChain(FGameplayTag ChainId)`（**2026-09-22 改造：参数类型改 tag**）、`ResolveTriggerDef(FGameplayTag TriggerTag)`（**2026-10-04 新增**）
  与 `ResolveStateDef(FGameplayTag DefTag) -> const FTcsBuffDef*`（**2026-10-04 R5 Task 1 新增**）、
  `ResolveAttrModDef(FGameplayTag TemplateTag) -> const UTcsAttrModDef*`（**2026-10-05 R5 Task 7 新增**）、
  `ResolveSkillDef(FGameplayTag DefTag) -> const FTcsSkillDefData*`（**2026-10-06 R6 Task 1 新增**）——均返回缓存内容的裸指针（修正器模板返回**资产对象本身**，因为消费者要的是它能被直接物化；技能定义返回**数据视图**，因为消费者要的是 `FTcsSkillDefData`），未命中返回 nullptr（正常查询路径，不 ensure）；
- **装配到世界**：`OnPostWorldInitialization` 时把缓存定义登记进该世界的消费方子系统（幂等——同一世界只装配一次）。**2026-10-04 扩展**：链装配之后，MUST 把每条触发定义登记为该世界的**触发行**
  （`UTcsEffectSubsystem::RegisterTriggerRow`），`Source` = 定义库自持的来源句柄（`FTcsSourceHandleRegistry::Allocate` 在 `Initialize` 时发放一次）。
  这是 `effect-trigger` 规格"**定义加载期登记**（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"那条的实现落点——**只发现不登记即等于资产零消费者**（策划填了不生效）。装配结果 MUST 进日志（就绪行含触发定义条数，世界装配行含已装配行数；**2026-10-08 补：世界装配行 MUST 另含技能定义已装配条数**）；
- **引用链预检（2026-10-04 新增）**：装配一条触发定义前 MUST 检查其 `Def.EffectChainId` 在该世界是否已登记（`UTcsEffectSubsystem::IsChainRegistered`）。不可解析时 MUST 留一条 **Warning**（含 `TriggerTag` 与链 id）并**仍登记该行**——
  MUST NOT 跳过登记（宿主可在世界运行期自行 `RegisterChain`，"先起世界后登记链"是合法时序，跳过会让该规则永久静默失效），MUST NOT 用 Error（内容缺口不是契约违规，且 `ExecuteChain` 已有自己的拒绝面）。**求值器 MUST NOT 重复该校验**（起链被拒的面归 `ExecuteChain`）；
- **触发定义的失败清单条目（2026-10-04 新增）**：资产加载失败或类型不符 / `TriggerTag` 无效 / `Def.EventTag` 无效 / `Def.EffectChainId` 无效 / `TriggerTag` 与另一资产重复——
  **后两条正是 `RegisterTriggerRow` 的 ensure 拒绝面**，故 MUST 在发现期拦下（MUST NOT 等到装配期让编辑器刷 ensure 红字）；
- **状态定义的发现期校验（2026-10-04 R5 Task 1 新增；四项构成经用户 2026-10-04 确认。★ 2026-10-15 收为三项）**：三个**身份级**判据 MUST 计入失败清单——资产加载失败或类型不符 / `DefTag` 无效 / `DefTag` 与另一资产重复。**原第三项 `Def.StatusTag` 非空校验随 `StatusTag` 并入 `CategoryTags` 一并删除**（判据：`CategoryTags` **非必填**——容器是筛选依据而非身份，为空是合法配置，拦它属**误报**）。
  内容级规则 MUST NOT 在此重复（`Params` 键重复 / `ModifierRows` 空引用 / `Triggers` 内空字段 / `DurationTime` 未配 全归资产 `IsDataValid`，见 `state-def-asset` 能力）——发现期只判"这条资产能不能被按 tag 寻址"；
- **修正器模板的发现期校验（2026-10-05 R5 Task 7 新增）**：三个**身份级**判据 MUST 计入失败清单——资产加载失败或类型不符 / `TemplateTag` 无效 / `TemplateTag` 与另一资产重复。
  同款边界：内容级规则 MUST NOT 在此重复（`Def.Target` 无效 / 数值来源为空 / `OPK_AttributeScaled` 而属性 tag 无效 / 约定列白名单 全归 `UTcsAttrModDef::IsDataValid`，见 `attribute-types` 能力）；
- **修正器模板不参与世界装配（2026-10-05 新增）**：`AttrModDef` 的缓存 MUST NOT 触发任何每世界登记——它的消费形态是**资产直引用**（`FTcsBuffDef::ModifierRows` 持 `TSoftObjectPtr<UTcsAttrModDef>`，物化时 `Get()` / `LoadSynchronous()`），
  故既无"装配进子系统"这一步、也无"登记进状态门面"那一步。**如实记入边界**：`ResolveAttrModDef` 的运行期调用者本轮为**零**，本路径交付的是**身份解析与索引**（资产身份 `[PrimaryAssetType, TemplateTag]` 可寻址、重复身份在发现期被拦），不是运行期取用；
- **状态定义逐世界登记（2026-10-04 R5 Task 2 新增；Task 1 只做到缓存）**：状态定义 MUST 在装配到世界时**登记进该世界的状态门面**（`UTcsBuffSubsystem::RegisterBuffDef`——**2026-10-17 符号订正**：原写 `UTcsStateSubsystem::RegisterStateDef`，三个符号在全仓从未存在，`PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 时登记口随 Buff 专属语义搬走，而本条停在拆分前的名字上；详见 `state-instance-lifecycle` 同条订正与 `ARCH-05` D-4 / `ARCH-07` D-5），与本类缓存**同一份内容**——
  只发现不登记 = 资产零消费者（同触发定义那条的理由；Task 1 只做缓存是因为当时还没有消费方）。装配顺序 MUST 排在链与触发行**之后**（状态门面的存在性由世界子系统机制保证，无需额外门禁）；
  `DefTag` 无效或重复登记（**正常路径下不可达**——发现期已去重）MUST 留 **Warning** 并继续处理其余定义，MUST NOT 中断装配、MUST NOT 用 Error；
- **"不被装配到世界"那条场景的今日读法（2026-10-04 R5 Task 2 澄清）**：本条描述的是**本类的缓存步骤**——`StateDefs` 缓存本身 MUST NOT 建任何每世界结构（与链 / 触发行"装配即登记"不同）；
  "登记进状态门面"是**第二步、由世界装配路径发起**，不改变第一步的边界。**来源句柄**：本条不为状态定义单独发号（状态实例的来源句柄归状态门面在施加时发放，Task 0 的统一发号器）；
- **技能定义的发现期校验（2026-10-06 R6 Task 1 新增）**：三个**身份级**判据 MUST 计入失败清单——资产加载失败或类型不符 / `DefTag` 无效 / `DefTag` 与另一技能定义资产重复。
  同款边界：内容级规则 MUST NOT 在此重复（`Params` / `BoolSwitches` 键重复、`ModifierRows` 空引用、`Phases` 内 `Duration` 来源为空、`Triggers` 内 `EventTag` / `EffectChainId` 无效、`CastChainId` 无效、描述配置项缺项 全归资产 `IsDataValid`，见 `skill-def-asset` 能力）——发现期只判"这条资产能不能被按 tag 寻址"；
- **技能定义逐世界登记（2026-10-08 R6 Task 2 改判；原「不做世界装配」那条作废）**：技能定义 MUST 在装配到世界时**登记进该世界的技能门面**（`UTcsSkillSubsystem::RegisterSkillDef`），与本类缓存**同一份内容**——
  **改判依据（本条自己的话）**：原文的理由是「它的消费形态是'运行期按 tag 取 Def'（**Task 2 起的账本与门禁**），不是'逐世界登记一次'的常驻规则」——**而 Task 2 的已学技能账本正是那个消费者**：账本的 `GetDef()` / `LevelBase` / `Params` / `BoolSwitches` 全都需要定义内容，而 `TcsSkill` **不能反向依赖 `TcsIntegration`**（`TcsIntegration` 依赖 `TcsSkill`，反查即成环）⇒ 定义 MUST 经**登记口**单向写进门面，门面 MUST NOT 反查本类。
  **同款先例** = 状态定义的逐世界登记（下文那条）：两者都是「消费方是本世界子系统、而定义缓存住 GameInstance 级」的同一形状，处置 MUST 一致。**判据一般化**：「某定义只进缓存、不装配世界」是一条**带前提的裁定**（前提 = 该定义零运行期消费者）——**消费者出现时该前提消失，MUST 同批改本条**，否则规格会从「当时的正确记录」变成「挡住实现的墙」（同族先例 = 台账 `ATTR-1`）。
  **装配顺序 MUST 排在链与触发行之后**（与状态定义同档：只要求「不早于消费方存在」）；`DefTag` 无效或重复登记（**正常路径下不可达**——发现期已去重）MUST 留 **Warning** 并继续处理其余定义，MUST NOT 中断装配、MUST NOT 用 Error。**来源句柄**：本条不为技能定义单独发号（账本条目的学习来源句柄归技能门面在 `GrantSkill` 时发放）。
  **GC 引用收集**：技能定义内容含 `FInstancedStruct`（`CastQueryFragment` / 参数行的数值来源）⇒ 技能门面的 `AddReferencedObjects` MUST 覆盖其登记表（判据 = "**容器是否 GC 可见**"，与值语义/指针语义无关）。
- **去重键** = 链 `ChainId` / 触发定义 `TriggerTag` / 状态定义 `DefTag` / 修正器模板 `TemplateTag` / 技能定义 `DefTag`（各自与资产身份同键）——同一键的两个资产 = 内容冲突，MUST 拒绝后一个并入失败清单（口径同链的"重复登记"，不静默覆写）；
- **GC 引用收集（2026-10-04 新增，必需；2026-10-05 边界澄清）**：本类 MUST 覆写 `AddReferencedObjects` 并逐条走**四份非 `UPROPERTY` 缓存**（`ChainDefs` / `TriggerDefs` / `StateDefs` / `SkillDefContent`）——它们都是本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它们**，
  而缓存内容的 `FInstancedStruct` 内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用（D4-16 类型不设限）⇒ 不补引用即**静默回收**。判据是"**容器是否 GC 可见**"，与"值语义还是指针语义"无关（同款先例 = `FTcsTriggerRegistry::AddReferencedObjects`）。
  **修正器模板缓存不在此列（MUST 记录理由）**：`AttrModDefs` 缓存的是**资产对象本身**，MUST 照既有两容器模式锚定——`UPROPERTY` 的 `TArray<TObjectPtr<UTcsAttrModDef>>` 保活（同 `StateDefAssets` 的形状），tag→资产 的索引 `TMap` 为非 `UPROPERTY`（它指向的对象已由该数组锚定）。
  故本函数**无需**为它补引用。**这正是"判据 = 容器是否 GC 可见"的应用**：同一类里两种容器、两种处理，不是因为类型不同，而是因为可见性不同。
  定义资产对象本身仍 MUST 以 `UPROPERTY` 锚定（`TUniquePtr` 持内容使 `Resolve*` 返回的指针地址稳定，但重载后资产被回收会让内容失主）。

#### Scenario: 就绪后组件可注册

- **WHEN** 定义发现 / 加载 / 校验完成，单出口被调用（且宿主已在此时挂好组件）
- **THEN** `IsRuntimeReady()` 为真，此后 `BeginPlay` 的组件注册通过门禁；**MUST NOT** 出现"先挂组件后定义齐备 ⇒ 该实体永久未注册且无补救"的窗口

#### Scenario: 链资产自动登记

- **WHEN** 内容目录存在一个合法链资产（`ChainId` 有效），GameInstance 初始化完成
- **THEN** 该世界的 `UTcsEffectSubsystem::FindChain(ChainId)` 可查到该链（发现 → 缓存 → 装配三段连通，**MUST NOT** 停在"缓存里有、世界查不到"）

#### Scenario: 新世界初始化后链仍可查（跨世界装配）

- **WHEN** 定义库已就绪（链已缓存），此后**一个新世界**完成初始化
- **THEN** 该世界的 `UTcsEffectSubsystem::FindChain(...)` 能查到**全部**已缓存链——无需重新扫描、无需重新就绪
- **判据**：定义库住 GameInstance（跨世界存活）而 `UTcsEffectSubsystem` 每世界重建 ⇒ 装配 MUST 发生在**每个世界的初始化时**，MUST NOT 只在就绪时对当时的 World 登记一次（那会在关卡切换后丢失全部链）

#### Scenario: 跨世界装配的实现时序锚点

- **WHEN** 检查装配路径的挂载方式
- **THEN** 两条路径 MUST 都在：① `Initialize` 末尾对**当时已存在**的 GameInstance 世界立即装配（PIE 里世界可能先于子系统建好）；② `FWorldDelegates::OnPostWorldInitialization` 对**此后每个新世界**装配。装配 MUST 幂等（同一世界只装配一次）
- **判据来源**：这两条时序锚点原写在头注释里（`TcsDefinitionSubsystem.cpp` 的 `SeedWorld` 段与 `WorldInitDelegateHandle` 处），**规格曾把它们记成一条 Scenario，后被 `539b14e` 一并删除** ⇒ 今天只在下述代码里存活。恢复它是因为「删了验证、留了结论」会让读者以为跨世界装配无需验证
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

- **WHEN** 内容目录存在一个合法状态定义资产（`DefTag` 有效；`CategoryTags` 可空），GameInstance 初始化后检查（**本步只看缓存那一步**）
- **THEN** 该资产可按 `DefTag` 解析到 `FTcsBuffDef`，且本类的缓存动作**本身**不建任何每世界结构（该世界的触发行数与其它每世界登记表不因它变化）

#### Scenario: 状态定义被逐世界登记进状态门面

- **WHEN** 世界初始化完成后检查该世界的状态门面
- **THEN** 缓存里的每个合法 `DefTag` 都已登记进 `UTcsBuffSubsystem`（`GetRegisteredBuffDef` 命中且内容与定义库缓存一致）——
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

#### Scenario: 技能定义按身份被发现与解析

- **WHEN** 内容目录首次出现一个 `UTcsSkillDef` 资产（`DefTag` 有效），GameInstance 初始化后检查（**本步只看缓存那一步**）
- **THEN** 发现路径扫到它、`ResolveSkillDef(DefTag)` 返回该资产的内容、就绪日志含技能定义计数；且本类的**缓存动作本身**不建任何每世界结构（该世界的触发行数与其它每世界登记表不因它变化）——
  **「登记进技能门面」是第二步、由世界装配路径发起**（见下文那条），不改变本步的边界（同状态定义「只进缓存」那条的今日读法）

#### Scenario: 技能定义的四类失败进失败清单

- **WHEN** 技能定义资产的 `DefTag` 无效，或两个资产的 `DefTag` 相同，或资产加载失败 / 类型不符
- **THEN** 该资产被跳过并计入失败清单（含路径与原因）、不被缓存、其余资产仍可用；MUST NOT 出现 Error / ensure（发现期只判身份，不判内容）

#### Scenario: 技能定义被逐世界登记进技能门面

- **WHEN** 世界初始化完成后检查该世界的技能门面（内容目录存在一个合法技能定义资产）
- **THEN** 缓存里的每个合法 `DefTag` 都已登记进 `UTcsSkillSubsystem`（`GetRegisteredSkillDef` 命中且内容与定义库缓存一致），且世界装配日志含技能定义条数——
  **「只进缓存」（发现那一步）与「登记进门面」（本步）是同一资产的两步**，缺第二步即资产零消费者；全流程 MUST NOT 出现 Error / ensure

#### Scenario: 未登记定义的技能授予被拒

- **WHEN** 对某个 `DefTag` **未登记进本世界技能门面**的技能调用 `GrantSkill`
- **THEN** 授予被拒绝（留 **Warning**、不 ensure），且该实体的**账本条目数不变**——
  MUST NOT 建出一个取不到定义内容的条目（那会让参数读取面全部落空，而失败面表现为「读数为 0」而非「没学到」，属静默错误）
