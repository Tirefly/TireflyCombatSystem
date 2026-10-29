// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/PrimaryAssetId.h"

#include "Asset/TcsAssetLoadPolicy.h"
#include "Handle/TcsSourceHandle.h"
#include "TcsResolveMode.h"

#include "TcsDefinitionSubsystem.generated.h"



/**
 * 定义库就绪信号（**单出口**，`06 §4` 硬规则① / M6 双层引导的最小版）。
 *
 * **为什么必须有它、以及它和 `IsRuntimeReady` 的分工**：`IsRuntimeReady` 是**轮询**（门禁查询点，
 * "现在能注册吗"），本委托是**通知**（"刚刚齐备了"）。只有轮询时，比定义库更早 `BeginPlay` 的
 * 实体就只能**跳过注册**并永久失效（组件今天正是这么做的：未就绪 → `Warning` + 返回，没有补救）；
 * 有了通知，订阅者可以在就绪那一刻补做。两者**MUST 并存**、MUST NOT 互相替代。
 *
 * **动态多播（`DECLARE_DYNAMIC_MULTICAST_DELEGATE`）的理由**：宿主脚本层（C#/蓝图）要能订阅
 * ⇒ 必须反射可达；`UPROPERTY(BlueprintAssignable)` 挂在子系统上即可 `Bind Event to ...`。
 *
 * **触发时机（2026-10-26 起变更为异步）**：预载改为异步后，本委托**不再**在 `Initialize` 内触发，
 * 而在**最后一个异步预载完成的那一帧**触发（预载全为 `OnDemand` 或零资产时仍在 `Initialize` 内）。
 * 订阅者 MUST NOT 假设"订阅时已经错过"——先查 `IsRuntimeReady()` 再决定是否订阅，这是唯一正确用法。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTcsOnDefinitionsReady);



/**
 * 定义库**失败**信号（与 `OnDefinitionsReady` 对称的**单出口**，用户 2026-10-26 裁定第 10 条）。
 *
 * **为什么必须有它**：就绪语义已反转为"**任一步失败即永不就绪**"（见 `IsRuntimeReady` 注释）。
 * 没有失败信号时，宿主的处境是「既收不到就绪、也收不到任何东西」——只能靠轮询超时猜，
 * 而那与「TCS 还没加载完」无法区分。本委托把「不会好了」这件事变成可观测事件。
 *
 * **与 `OnDefinitionsReady` 互斥且各自最多一次**：同一次生命周期内二者**只会有一个**被广播。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTcsOnDefinitionsFailed);



class FReferenceCollector;
class UTcsAttributeDef;
class UTcsAttrModDef;
class UTcsBuffDefAsset;
class UTcsEffectChainDef;
class UTcsEffectTriggerDefAsset;
class UTcsSkillDef;
struct FTcsAttributeDefData;
struct FTcsBuffDef;
struct FTcsEffectChain;
struct FTcsEffectTriggerDef;
struct FTcsSkillDefData;



/**
 * 定义库（**概念名 DefLibrary**，沿用设计词汇）——M1 的宿主，**MUST 为 GameInstance 级**：
 * Const 定义内容跨 PIE 世界共享，图鉴/UI 等**无世界**场景要能查（`MEM-20260902-05` 生命周期分层判据：
 * 先枚举消费场景含无世界者，再定级别）。
 *
 * 职责边界（**只做资产发现、加载与注册，不做执行**——执行归各领域门面）：
 * - **发现与加载彻底分离（2026-10-26 重写）**：`DiscoverDefs` 只收 `FAssetData`（**不加载**），
 *   `LoadDefinitions` 按**生效策略**决定加载什么、怎么加载。旧实现把 `AssetData.GetAsset()`
 *   写在发现里 ⇒ 「发现」这个动作隐含**全量同步加载** ⇒ 任何策略都无法生效
 *   （**只要发现里带加载，策略就永远是死的**）。
 * - **六类 Def 资产**：链 `UTcsEffectChainDef`、触发定义 `UTcsEffectTriggerDefAsset`、
 *   状态定义 `UTcsBuffDefAsset`（2026-10-04 R5 Task 1）、修正器模板 `UTcsAttrModDef`
 *   （2026-10-05 R5 Task 7）、技能定义 `UTcsSkillDef`（2026-10-06 R6 Task 1）、
 *   属性定义 `UTcsAttributeDef`（2026-10-17 `ARCH-04` U-1）——
 *   **六条全部**走加载层统一查询口 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`
 *   （类换过模块后 `IAssetRegistry::GetAssetsByClass` 按**当前类路径**查索引会**静默返 0**，
 *   实测：链资产 12 → 0）。旧注释说"后三条仍走 `GetAssetsByClass`"——那是迁移期的中间状态，
 *   2026-10-26 起**统一走新口**（搬过家的类只会越来越多，两套发现路径没有存在理由）；
 *   二者**都不依赖 `PrimaryAssetTypesToScan` 注册**——该 ini 清单已被 `DEC-2026-10-13-asset-butler`
 *   裁定②否决，替代品是代码自注册宏 `UE_DEFINE_PRIMARY_ASSET_TYPE`（台账 INTEG-3 逐项结账）；
 * - **校验**：双真相（`Chain.ChainId != ChainId`）/ 空 id / 重复登记 → 计入失败清单 + Error，不静默跳过；
 *   触发定义侧另有四条（空 `TriggerTag` / 空 `Def.EventTag` / 空 `Def.EffectChainId` / 身份重复）；
 *   **状态定义侧的三条是身份级**（加载失败或类型不符 / 空 `DefTag` / `DefTag` 重复；
 *   ~~空 `Def.StatusTag`~~ 已于 2026-10-15 删除——该字段并入 `CategoryTags` 容器后**非必填**，
 *   为空是合法配置，拦它属误报，见变更 `merge-status-tag-into-category-tags`）
 *   ——内容级规则（参数键重复、内联触发行缺项、时值来源为空、描述缺项）全归资产 `IsDataValid`，
 *   发现期只判"这条资产能不能被按 tag 寻址"；**修正器模板侧同为身份级三条**（加载失败或类型不符 /
 *   空 `TemplateTag` / `TemplateTag` 重复——`Def.Target`、操作数、值约定的内容规则归资产 `IsDataValid`）；
 *   **技能定义侧同为身份级三条**（加载失败或类型不符 / 空 `DefTag` / `DefTag` 重复——
 *   参数、施法时段、主链、内联触发行与描述的内容规则归资产 `IsDataValid`）；
 *   **属性定义侧同为身份级三条**（加载失败或类型不符 / 空 `DefTag` / `DefTag` 重复——
 *   `Def.BaseValue` / 边界 / 值域模式的内容规则归资产 `IsDataValid`）；
 * - **按名解析**：链定义按 `ChainId` 缓存（`ResolveChain`）、触发定义按 `TriggerTag` 缓存
 *   （`ResolveTriggerDef`）、状态定义按 `DefTag` 缓存（`ResolveStateDef`）、修正器模板按 `TemplateTag`
 *   缓存（`ResolveAttrModDef`）、技能定义按 `DefTag` 缓存（`ResolveSkillDef`）；
 *   **属性定义例外——本类不给公开解析口**（`ARCH-04` U-1 的边界）：它的运行期读口在世界门面
 *   （`UTcsAttributeSubsystem::FindAttributeDef`，`AddAttribute` 的解析源），本类的缓存只是
 *   "发现 → 装配"之间的中转。加一个 GI 侧读口只会成为零消费者设施（本仓"零消费者不预建"）；
 * - **就绪状态机**：`OnDefinitionsReady` **单出口**（幂等，`Initialize` 内置位后广播恰好一次）——M6 双层引导的最小版（06 §4 硬规则①）；与 `IsRuntimeReady` 分工 = **通知 vs 轮询**，两者 MUST 并存；
 * - **装配到世界**：见下条。
 *
 * **装配到世界（跨级方向纪律，2026-09-21）**：本子系统是 GameInstance 级（跨世界存活），而定义**消费方**
 * （`UTcsEffectSubsystem`）是 World 级（每世界重建）——故定义 MUST 在本类**缓存**，并在**每个世界初始化时**
 * 装配进该世界（订阅 `FWorldDelegates::OnPostWorldInitialization`，幂等）。"ready 时对当时的 World 登记一次"
 * 会在关卡切换后丢失全部登记（R3 单地图 PIE 测不出，宿主首次切关卡必撞）。时机已核：`UWorld::InitWorld`
 * 中 `InitializeSubsystems()`（`World.cpp:2447`）**早于** `OnPostWorldInitialization.Broadcast`（`:2601`），
 * 故装配时 EffectSubsystem 已可用。
 *
 * **触发行装配（2026-10-04）**：链装配**之后**，每条触发定义经 `UTcsEffectSubsystem::RegisterTriggerRow`
 * 登记成该世界的触发行，`Source` = 本类自持的来源句柄（`Initialize` 时发放一次）——`effect-trigger` 规格的
 * "定义加载期登记（全局常驻规则）"由此成立。引用链在该世界不可解析时留 Warning 并**仍登记**
 * （宿主可在运行期自行 `RegisterChain`，"跳过登记"会让该规则永久静默失效）。
 *
 * **状态定义也装配到世界（2026-10-04 R5 Task 2；Task 1 时只做缓存）**：状态定义在装配世界时
 * 逐条**登记进该世界的状态门面**（`UTcsBuffSubsystem::RegisterBuffDef`，按值拷一份、不与本类共享
 * 生命周期）——只发现不登记 = 资产零消费者（同触发行那条的理由）。Task 1 只做缓存是因为当时还没有
 * 消费方；"缓存那一步不建任何每世界结构"今天照样成立（故 `SeedWorld` 的判据现看三类定义）。
 * **依赖方向仍是单向的**：本类（`TcsIntegration`）→ `TcsState` 的登记口，门面 MUST NOT 反查本类。
 *
 * **技能定义不装配到世界（2026-10-06 R6 Task 1）**：只缓存内容、由消费方按需 `ResolveSkillDef`
 * 解析，不建任何每世界结构；本轮运行期消费者为零，账本与门禁归后续 Task 2。
 *
 * **属性定义装配到世界（2026-10-17 `ARCH-04` U-1）**：与状态定义同款——装配世界时逐条登记进该世界的
 * **属性门面**（`UTcsAttributeSubsystem::RegisterAttributeDef`）。判据与"状态定义要装配"逐字同源：
 * 属性定义的消费面是 World 级门面（`AddAttribute` 从它的 `DefTable` 解析），而本类是 GameInstance 级
 * ⇒ **只发现不登记 = 资产零消费者**。
 * - **与状态定义的唯一差别是幂等守卫**：`RegisterAttributeDef` 对重复身份是 `ensureMsgf`（**会红字**，
 *   见 `TcsAttributeSubsystem.cpp:88`），而 `RegisterBuffDef` 具名返回。⇒ 本类必须在调用前先查
 *   `FindAttributeDef`——否则宿主（如 `TcsDevBootstrap` 的 `LoadAttributeDefs`）若先登记过，
 *   插件装配就会刷 ensure。**这不是防御性编程，是对既有宿主用法的兼容义务**：宿主自行扫描登记
 *   属性定义是本类补齐之前唯一的合法用法，MUST NOT 因本类补齐而变成错误。
 * - **就绪语义随之变宽**：`IsRuntimeReady() == true` 现在也意味着"属性定义已登记进该世界"。
 *   这是加强不是破坏（此前它**不覆盖**属性定义，见 `ARCH-04` D-5 的第 ② 影响面）——
 *   组件的 `BeginPlay` 门禁因此第一次真的覆盖了属性定义。
 *
 * **MUST NOT 持可变运行态**（Const 面：缓存的是定义，不是运行态；触发行本身住世界级子系统）。
 *
 * 与中央注册表的分工（**不重叠**，三层结构）：本类 = Const 定义（"有哪些定义、定义是什么"）；
 * 各领域子系统登记表 = 定义的运行期副本（"当前世界有哪些定义可用"）；中央注册表
 * （**实体状态机 + 泵接线 + 就绪门禁**，R7/M6 落地——2026-10-04 R5 收窄轮把"中央注册表"这个名字
 * 判给了 `UTcsBuffSubsystem`，故此处不再用它指代未落地的那一层）= 可变运行态 per-entity 桶。
 * 门禁关系：那一层查本类 `IsRuntimeReady()`。
 *
 * **状态门面（`UTcsBuffSubsystem`）是已落地的 per-entity 桶**（R5 Task 2）：它持的是"每个实体有哪些
 * 状态"，不是"世界里有哪些实体"——后者才是上面那一层的职责，故两者不是同一件事。
 */
UCLASS()
class TCSINTEGRATION_API UTcsDefinitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 初始化：发现定义资产 → 校验 → 缓存 → 标记就绪 → 单出口派发 → 装配到世界（若世界已存在）
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 反初始化：退订世界初始化委托，清空缓存与失败清单（确定性清理）
	virtual void Deinitialize() override;

#pragma endregion


// GC 引用收集
#pragma region GC

public:
	/**
	 * GC 引用收集（**四份定义缓存的 GC 可见持有**，2026-10-06 R6 Task 1 扩展）——**必需**。
	 *
	 * 为什么必须自己实现：`ChainDefs` / `TriggerDefs` / `StateDefs` / `SkillDefs` 都是本类的**非 `UPROPERTY` 成员**
	 * （`TMap<FGameplayTag, TUniquePtr<...>>`），GC 的 `RefLink` 遍历**走不到它们**；而四份缓存的内容里
	 * 都有 `FInstancedStruct`（链 = `Steps`、触发定义 = `Conditions` / `EventPayloadFilter`、
	 * 状态定义 = 参数行的数值来源 / 描述的视图载荷；技能定义还含施法时段、查询片段与内联触发行），
	 * 其**内层内存可以放宿主自定义 struct 的
	 * `UPROPERTY` 对象引用**（D4-16 类型不设限）⇒ 不补引用即**静默回收**
	 * （表现为"步骤/条件/参数行里那个对象变成空引用"，不是崩溃）。
	 *
	 * **判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——计划注记曾以"值语义 struct ⇒ 无需 ARO"
	 * 为由判断不需要，该判据是错的（同 `FTcsTriggerRegistry::AddReferencedObjects` 的纠正）。
	 *
	 * 手法与 `UTcsEffectSubsystem::AddReferencedObjects` 完全同款：逐条调
	 * `FReferenceCollector::AddPropertyReferencesWithStructARO`（引擎 `UDataTable::AddReferencedObjects`
	 * 对 `RowMap` 用的同一招，`DataTable.cpp:300`）。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 *
	 * **属性定义缓存（`AttributeDefs`，2026-10-17 `ARCH-04` U-1）同样不进本函数**——判据同上，且理由更硬：
	 * `FTcsAttributeDefData` 是**纯值 struct**（`double BaseValue` / `FTcsAttributeBounds`（含 `FGameplayTag`）/
	 * `ETcsAttributeValueDomain` / `ETcsAttrOverrideTieBreak`），**没有任何 UObject 字段** ⇒ 没有"包在
	 * `FInstancedStruct` 里的内层对象引用"可漏。资产对象本身由 `AttributeDefAssets`（`UPROPERTY`）保活。
	 * ⇒ 这是**五份内容缓存里唯一不需要本函数的那一份**；把它加进来只会是空转。
	 *
	 * **修正器模板缓存（`AttrModDefs` / `AttrModDefAssets`，2026-10-05 R5 Task 7）不进本函数**——
	 * 同一条判据"容器是否 GC 可见"给出的结论是**不需要**：`AttrModDefs` 虽也是非 `UPROPERTY` 的 `TMap`，
	 * 但它**只持有资产对象本身**（没有包在 `FInstancedStruct` 里的内层对象引用可漏），且每个入索引的
	 * 资产都同时进 `AttrModDefAssets` 这个 `UPROPERTY` 数组 ⇒ GC 经 `RefLink` 已能看见。
	 * 这也划出本轮的边界：**索引容器与保活容器成对出现时，保活责任归后者**。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#pragma endregion


// 就绪
#pragma region Readiness

public:
	/**
	 * 定义库就绪**单出口**（幂等——`Initialize` 内置位后广播**恰好一次**）。
	 *
	 * **义务方是本类**（它知道定义何时齐备），不是宿主——「时序是宿主责任」只适用于**宿主自己**的
	 * 接线顺序，不适用于「定义库就该广播的那一次通知」。
	 *
	 * **订阅者的用法**：在 `BeginPlay` 里先查 `IsRuntimeReady()`，已就绪则直接做；未就绪则订阅本委托、
	 * 在回调里补做，**并在回调后立即退订**（单次信号，不是每帧事件）。回调在**游戏线程、`Initialize` 内**
	 * 同步触发 ⇒ 回调里 MUST NOT 假设世界已装配完成（`SeedWorld` 排在本委托广播**之后**）。
	 */
	UPROPERTY(BlueprintAssignable, Category = "Tcs|Definition")
	FTcsOnDefinitionsReady OnDefinitionsReady;

	/**
	 * 定义库**失败**单出口（与 `OnDefinitionsReady` 对称；两者在一次生命周期内只会有一个被广播）。
	 *
	 * 触发条件 = 任一步失败（资产损坏 / 路径错 / 类型未注册 / 重复登记 / 需要的装配型类型被配成
	 * `OnDemand`）。触发后 `IsRuntimeReady()` **恒为 false 且不再置位**。
	 */
	UPROPERTY(BlueprintAssignable, Category = "Tcs|Definition")
	FTcsOnDefinitionsFailed OnDefinitionsFailed;

	/**
	 * 定义是否已就绪（**门禁查询点**：实体组件注册前查它）。
	 *
	 * **就绪 = 按生效策略该加载的都加载完了 + 全部校验通过 + 装配所需定义已入缓存。**
	 *
	 * **★ 语义已于 2026-10-26 反转（用户裁定第 10 条）**：旧口径是「失败清单非空不影响就绪——
	 * 失败项被跳过、其余可用」；**新口径是「任一步失败即永不就绪」**。
	 *
	 * **为什么反转**：异步预载之后，"部分加载完"不再是瞬时状态而是可长达数秒的窗口。若按旧口径
	 * 在部分可用时就就绪，宿主会在**定义半套**的世界里开始注册实体——随后补进来的定义不会回填，
	 * 表现为"有的 buff 生效、有的不生效"，且随资产加载快慢在不同机器上不同。
	 * 这与「假策略」是同一种病：症状可观测，真因不可定位。
	 *
	 * **⚠ 排障成本随之转移（MUST 读）**：一个拼错的资产路径会让**整个 TCS 停摆**，
	 * 症状是「组件全部不注册」——看起来是 TCS 没工作，实际是内容里有一个坏文件。
	 * 缓解措施是**必需配套**而非装饰：失败时打**一次性、Error 级、含完整失败清单**的日志，
	 * 并广播 `OnDefinitionsFailed`（宿主可主动响应而不是干等）。
	 *
	 * @return 返回是否已就绪。
	 */
	bool IsRuntimeReady() const
	{
		return bRuntimeReady;
	}

	/**
	 * 取失败清单（资产路径 + 失败原因）——供宿主诊断与透传（06 §4 硬规则②"失败清单透传"）。
	 *
	 * @return 返回失败条目列表。
	 */
	const TArray<FString>& GetFailureList() const
	{
		return FailureList;
	}

	// 返回已缓存的技能定义条数，供宿主核对发现结果与就绪日志。
	int32 GetSkillDefCount() const
	{
		return SkillDefs.Num();
	}

	/**
	 * 取某类型在 `OnDemand` 档下的**目录**（该类型的全部 `FPrimaryAssetId`）。
	 *
	 * **为什么是指南针而不是缓存**：`OnDemand` 承诺"就绪时零加载"，故本类**不能**持有这些资产对象
	 * （硬引用 = 已加载）。目录让宿主与诊断工具仍能回答"这个类型下有哪些定义、共几个"，
	 * 而资产本体等到真正解析时才进内存。
	 *
	 * **MUST NOT 建 `DefTag → FPrimaryAssetId` 映射**（裁定第 12 条）：全部 7 个 Def 类的
	 * `GetPrimaryAssetId()` 都是**纯函数**（`return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());`
	 * 无一处读资产内容）⇒ 映射是可被纯函数替代的冗余。
	 *
	 * @param Type 主资产类型。
	 * @return 返回该类型的目录；未登记该类型返回空数组。
	 */
	const TArray<FPrimaryAssetId>& GetOnDemandCatalog(FPrimaryAssetType Type) const;

	/**
	 * 取某类型在 `OnDemand` 档下的**资产总数**（诊断"发现 12 个 / 预载 3 个"与未来 Debug 工具用）。
	 *
	 * @param Type 主资产类型。
	 * @return 返回资产总数；未登记返回 0。
	 */
	int32 GetOnDemandCatalogCount(FPrimaryAssetType Type) const;

#pragma endregion


// 解析
#pragma region Resolve

public:
	/**
	 * 按 id 解析链定义。
	 *
	 * **本组 5 个解析口自 2026-10-26 起支持异步**（用户裁定第 13/14 条）：`OnDemand` 与
	 * 「`PreloadSelected` 未选中的资产」在就绪时**零加载**，它们只能靠解析口按需加载。
	 *
	 * **MUST NOT 是 `const`**：`Async` 解析要在加载完成时把内容写进缓存，写缓存就不能 const。
	 * 这不是"顺手去掉 const"，而是「按需加载」这条能力的直接后果——只读的缓存口无法兑现 `OnDemand`。
	 *
	 * @param ChainId 链 id。
	 * @param Mode 加载模式（默认 `Async`）。
	 * @param OnLoaded 加载完成回调（**按值传**——非 const 引用不能带默认值绑临时量；
	 *        语义是"加载完成时调用它"，已在内存时**同步**触发）。
	 * @return `Sync` 模式或已在内存时返回链定义；`Async` 且不在内存时返回 nullptr。
	 */
	const FTcsEffectChain* ResolveChain(
		FGameplayTag ChainId,
		ETcsResolveMode Mode = ETcsResolveMode::Async,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	/**
	 * 按身份解析已缓存的触发定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveChain` 同款：返回**缓存内容的裸指针**（`TUniquePtr` 持有使地址稳定），
	 * 故无反射面（UHT 不支持 struct 指针作反射返回）——宿主脚本层的登记入口是
	 * `UTcsEffectSubsystem::RegisterTriggerRow`（它自己带定义数据，不需要本方法）。
	 *
	 * @param TriggerTag 触发身份（`EffectTriggerDef` 根）。
	 * @return 返回触发定义；未缓存返回 nullptr。
	 */
	const FTcsEffectTriggerDef* ResolveTriggerDef(
		FGameplayTag TriggerTag,
		ETcsResolveMode Mode = ETcsResolveMode::Async,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	/**
	 * 按身份解析已缓存的状态定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveTriggerDef` 同款（裸指针 + 不 ensure）；状态定义的消费者是 `UTcsBuffSubsystem`
	 * 的施加路径（R5 Task 2 起）——**本类不做世界装配**，状态定义在哪个世界用由消费方按需取。
	 *
	 * @param DefTag 定义身份（状态词表根）。
	 * @return 返回 buff 定义；未缓存返回 nullptr。
	 */
	const FTcsBuffDef* ResolveStateDef(
		FGameplayTag DefTag,
		ETcsResolveMode Mode = ETcsResolveMode::Async,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	/**
	 * 按身份解析已缓存的修正器模板（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveStateDef` 同款（裸指针 + 不 ensure）。**消费角色 = 发现/索引路径自身**：
	 * `ModifierRows` 今天走**资产直引用**（`TSoftObjectPtr<UTcsAttrModDef>`，物化器只 `Get()`/`LoadSynchronous()`
	 * 取对象），故本方法的**运行期调用者为零**——它存在是为了让模板身份词有解析锚点（台账 `ATTR-1` 的关闭依据）。
	 * 与 `StateDef` 在 R5 Task 1 的处境同形（"只入缓存、暂无消费者"），不是预建根。
	 *
	 * @param TemplateTag 模板身份（`AttrModDef` 根）。
	 * @return 返回修正器模板资产；未缓存返回 nullptr。
	 */
	const UTcsAttrModDef* ResolveAttrModDef(
		FGameplayTag TemplateTag,
		ETcsResolveMode Mode = ETcsResolveMode::Async,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

	/**
	 * 按身份解析已缓存的技能定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * 与 `ResolveStateDef` 同款：返回内容副本的裸指针，`TUniquePtr` 持有使地址稳定；
	 * 指针在定义库反初始化时失效。技能定义只缓存、不做世界装配；本轮运行期消费者为零。
	 *
	 * @param DefTag 技能定义身份（`SkillDef` 根）。
	 * @return 返回技能定义数据；未缓存返回 nullptr。
	 */
	const FTcsSkillDefData* ResolveSkillDef(
		FGameplayTag DefTag,
		ETcsResolveMode Mode = ETcsResolveMode::Async,
		FStreamableDelegate OnLoaded = FStreamableDelegate());

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 一个 Def 类型的**发现结果**（发现与加载分离的载体，2026-10-26）。
	 *
	 * **为什么要分离**：今天 `Discover*` 里直接 `AssetData.GetAsset()`——「发现」这个动作隐含了
	 * **全量同步加载**，任何策略都无法生效（策略管加载，而加载被写死在发现里）。
	 * **只要发现里带加载，策略就永远是死的。** ⇒ 发现阶段只收 `FAssetData` 与策略，
	 * 由 `LoadDefinitions` 统一按策略决定"加载哪些、怎么加载"。
	 */
	struct FDiscoverResult
	{
		// 主资产类型（策略查询键）
		FPrimaryAssetType Type;

		// 该类型在 UE_DEFINE_PRIMARY_ASSET_TYPE 里的声明值（配置未覆盖时的回落值）
		ETcsAssetLoadPolicy DeclaredPolicy = ETcsAssetLoadPolicy::OnDemand;

		// 发现到的全部资产数据（**未加载**——只有 AssetRegistry 的元数据）
		TArray<FAssetData> Assets;
	};

	// 收集一个 Def 类型的发现结果（只取 FAssetData，MUST NOT 加载资产）
	void DiscoverDefs(UClass* DefClass, FDiscoverResult& OutResult);

	// 按生效策略加载全部发现结果并装配缓存（全部异步；失败项进清单）
	void LoadDefinitions(TArray<FDiscoverResult>& InResults);

	// 异步预载完成/失败的总出口（置就绪或置失败，二者只发其一）
	void OnPreloadFinished(bool bInSucceeded);

	/**
	 * 发起一批异步预载（**唯一的批量加载点**）。
	 *
	 * **必须在这里做未注册自查**：实测（R-2）未注册类型会让 `LoadPrimaryAssetsWithType` 拿到空数组
	 * ⇒ `CreateCombinedHandle` 对空子句柄**同步执行 `OnComplete`** 并返回 `nullptr` ⇒
	 * 「代理立刻被调用」+「句柄 nullptr」+「零 Error 零 Warning」。只挂 `OnComplete` 的调用方会读成
	 * 「加载完成、共 0 个资产」⇒ 计入成功 ⇒ 一路走到就绪。
	 *
	 * @param InAssetManager 资产管理器（调用方已取，非空）。
	 * @param InIds 要加载的主资产身份。
	 * @param InResult 该类型的发现结果（装配与诊断用）。
	 * @param OutError 发起失败的原因（空表示已发起）。
	 * @return 是否成功发起（不代表加载成功）。
	 */
	bool PreloadPrimaryAssetIds(
		UAssetManager* InAssetManager,
		const TArray<FPrimaryAssetId>& InIds,
		const FDiscoverResult& InResult,
		FString& OutError);

	/**
	 * 查重：同类型下 `PrimaryAssetName`（即 `DefTag`）重复而路径不同 ⇒ 计入失败清单并拒绝登记。
	 *
	 * **为什么是"拒绝登记"而不是"打 Warning"**：引擎已在 `TryUpdateCachedAssetData`
	 * （`AssetManager.cpp:1656-1690`）打过 Warning + 编辑器通知 + `ensureMsgf` 三件套 ⇒ 我们再做
	 * 一次是重复劳动，且分不清是谁打的。**引擎没说、而我们必须说的是**：引擎的 `FindOrAddAsset`
	 * 对同名是**覆盖**，而扫描顺序**无保证**（引擎源码自注 `// Temporary performance fix…`）
	 * ⇒ 同一个 `DefTag` 在不同次启动可能解析到不同资产。
	 * **"随机生效一个"比"全部失败"更糟**——后者能查出来，前者表现为"我改的资产没生效"的间歇性谜题。
	 *
	 * @param InResult 该类型的发现结果。
	 * @return 无重复返回 true；发现重复返回 false（已写入 FailureList）。
	 */
	bool CheckDuplicates(const FDiscoverResult& InResult);

	// 判定某类型是否是「装配型」（SeedWorld 会逐世界登记它的定义 ⇒ MUST NOT 走 OnDemand）
	bool IsSeedingType(FPrimaryAssetType Type) const;

	// 逐类型装配器（**输入是已加载的资产对象**，做身份校验与入缓存；失败项进 FailureList）。
	// 与发现分离：这里不做发现、不做批量加载，只看"这一个对象能不能按身份入缓存"。
	void AssembleChainDef(UObject* InAsset, const FString& InAssetPath);
	void AssembleTriggerDef(UObject* InAsset, const FString& InAssetPath);
	void AssembleStateDef(UObject* InAsset, const FString& InAssetPath);
	void AssembleAttrModDef(UObject* InAsset, const FString& InAssetPath);
	void AssembleSkillDef(UObject* InAsset, const FString& InAssetPath);
	void AssembleAttributeDef(UObject* InAsset, const FString& InAssetPath);

	// 把某类型的一批已加载资产逐个装配（PreloadAll / PreloadSelected 的异步完成路径用）
	void AssembleLoadedType(const FDiscoverResult& InResult);

	// 按类型分发到一个装配器（键 = 主资产类型）
	void AssembleAsset(FPrimaryAssetType Type, UObject* InAsset, const FString& InAssetPath);

	// 按 tag 在目录里找身份并按需加载资产（`OnDemand` 与"未被选中的资产"的唯一加载入口）。
	// @return `Sync` 或已在内存时返回资产对象；否则返回 nullptr 并登记一次加载。
	UObject* EnsureDefinitionLoaded(
		FPrimaryAssetType Type,
		FGameplayTag Tag,
		ETcsResolveMode Mode,
		FStreamableDelegate OnLoaded);

	// 目录查找：按 `PrimaryAssetName`（= `DefTag.GetTagName()`）在该类型的目录里找身份
	bool FindCatalogId(FPrimaryAssetType Type, FGameplayTag Tag, FPrimaryAssetId& OutId) const;

	// 装配已缓存定义进指定世界（幂等：同一世界不重复登记；链在前、触发行在后；状态 / 技能 / 属性定义按序登记）。
	// 判据：**凡消费面是 World 级门面的定义族都必须装配**——只发现不登记 = 资产零消费者。
	void SeedWorld(UWorld* World);

	// 世界初始化回调（新世界装配入口）
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	// 已装配的世界集合（幂等判据——同一世界不重复装配）。
	//
	// **为什么是集合而不是单槽（2026-10-17 改，`ARCH-11` D-2 / §3.4）**：单槽判据 `SeededWorld.Get() == World`
	// 只在「世界严格串行更替」时成立——一旦同一进程内**同时**存在两个世界（PIE 多客户端 / 编辑器预览世界 +
	// 游戏世界），单槽会让**第二个世界**重复装配、并让**第一个世界**的幂等判据失效。
	// 而 `SeededWorld` 同时是本类唯一能枚举「我装配过哪些世界」的凭据 ⇒ 反初始化时要按它逐世界撤销
	// 触发行（见 `Deinitialize`），单槽天然做不到。⇒ 两个用途都要求集合。
	// `TWeakObjectPtr` 的语义是**不强持有世界**（世界先死 = 条目自动失效，下次装配时顺手剔除）。
	TArray<TWeakObjectPtr<UWorld>> SeededWorlds;

	// 链定义缓存（Const 内容；键 = ChainId）——`TUniquePtr` 持有使解析返回的指针地址稳定
	TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>> ChainDefs;

	// 触发定义缓存（Const 内容；键 = TriggerTag）——持有形态与失效判据同链缓存
	TMap<FGameplayTag, TUniquePtr<FTcsEffectTriggerDef>> TriggerDefs;

	// 状态定义缓存（Const 内容；键 = DefTag）——持有形态与失效判据同上
	TMap<FGameplayTag, TUniquePtr<FTcsBuffDef>> StateDefs;

	// 技能定义缓存（Const 内容副本；键 = DefTag）——持有形态与失效判据同上，不参与世界装配
	TMap<FGameplayTag, TUniquePtr<FTcsSkillDefData>> SkillDefs;

	// 属性定义缓存（Const 内容副本；键 = DefTag）。**唯一不给公开解析口的定义缓存**——读口在世界门面
	// （`UTcsAttributeSubsystem::FindAttributeDef`），本容器只是"发现 → 装配"的中转；加 GI 侧读口
	// 就是零消费者预建（`ARCH-04` U-1 的边界）。
	// `FTcsAttributeDefData` 是**纯值 struct**（`double` / `FTcsAttributeBounds` / 两个枚举，零 UObject）
	// ⇒ **不需要** `AddReferencedObjects` 补引用——这是五份内容缓存里唯一不需要的那一份。
	// 持有形态与其余四份一致（`TUniquePtr`）：公开头只前向声明 `FTcsAttributeDefData`，
	// 而 `TMap` 的值类型若按值置入则要求完整类型（析构在头里隐式生成）⇒ 用 `TUniquePtr` 既保住
	// 「公开头不拉领域类型」这条纪律，又与既有四份同形。
	TMap<FGameplayTag, TUniquePtr<FTcsAttributeDefData>> AttributeDefs;

	// 修正器模板缓存（键 = TemplateTag）。**持有资产对象本身而非内容副本**——模板的"内容"就是资产
	// （`Def` 行 + 身份），没有可分离的 const 值类型可拷；故本容器是索引，GC 可见性由下面的资产数组承担。
	TMap<FGameplayTag, TObjectPtr<UTcsAttrModDef>> AttrModDefs;

	// 定义资产缓存（GC 锚定：缓存的是定义内容，但资产对象本身也需存活以免重载）
	UPROPERTY()
	TArray<TObjectPtr<UTcsEffectChainDef>> ChainDefAssets;

	// 触发定义资产缓存（GC 锚定，理由同上）
	UPROPERTY()
	TArray<TObjectPtr<UTcsEffectTriggerDefAsset>> TriggerDefAssets;

	// 状态定义资产缓存（GC 锚定，理由同上）
	UPROPERTY()
	TArray<TObjectPtr<UTcsBuffDefAsset>> StateDefAssets;

	// 修正器模板资产缓存（GC 锚定，理由同上——`AttrModDefs` 索引非 UPROPERTY，靠本数组保活）
	UPROPERTY()
	TArray<TObjectPtr<UTcsAttrModDef>> AttrModDefAssets;

	// 技能定义资产缓存（GC 锚定；内容副本的内层对象引用另由 AddReferencedObjects 收集）
	UPROPERTY()
	TArray<TObjectPtr<UTcsSkillDef>> SkillDefAssets;

	// 属性定义资产缓存（GC 锚定，理由同上）。内容副本本身无对象引用（见上），故本数组是**唯一**保活点。
	UPROPERTY()
	TArray<TObjectPtr<UTcsAttributeDef>> AttributeDefAssets;

	// 来源句柄分配器（进程内原子发号；本类只在自己 `Initialize` 时用一次）
	FTcsSourceHandleRegistry TriggerSourceRegistry;

	// 触发行来源句柄（装配期登记触发行时的 `Source`——"定义库来源"这一级联退订锚点）
	FTcsSourceHandle TriggerSeedSource;

	// `OnDemand` 档的**目录**（键 = 主资产类型；值 = 该类型的全部 FPrimaryAssetId）。
	// **只存身份，不存资产对象**——硬引用意味着已加载，与「不预载」矛盾。
	// 不建 DefTag → Id 映射：GetPrimaryAssetId() 是纯函数，映射是可被替代的冗余（裁定第 12 条）。
	TMap<FPrimaryAssetType, TArray<FPrimaryAssetId>> OnDemandCatalog;

	// 异步预载句柄（**本类必须持有至反初始化**，裁定第 9 条）：
	// 句柄释放即触发卸载，已"预载"的资产会变回未加载。
	TArray<TSharedPtr<FStreamableHandle>> PreloadHandles;

	// 尚未完成的异步预载批次数（归零 ⇒ 走到就绪/失败的单出口）
	int32 PendingPreloadBatches = 0;

	// 本次生命周期内已出现过失败（失败即永不就绪；也不再广播就绪）
	bool bLoadFailed = false;

	// **就绪前**已完成初始化的世界（待装配集合，裁定第 8/15 条）。
	// **弱引用的理由**：本类是 GameInstance 级，世界可能在等待期间被销毁 ⇒ 强引用会拖住世界。
	TArray<TWeakObjectPtr<UWorld>> PendingWorlds;

	// 失败清单（资产路径 + 原因）
	TArray<FString> FailureList;

	// 就绪标记（`OnDefinitionsReady` 的幂等依据）
	bool bRuntimeReady = false;

	// 世界初始化委托句柄（反初始化时退订）
	FDelegateHandle WorldInitDelegateHandle;

#pragma endregion
};
