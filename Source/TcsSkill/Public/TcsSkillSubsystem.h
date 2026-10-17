// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pool/TcsInstancePool.h"
#include "Subsystems/WorldSubsystem.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"
#include "Host/TcsEntityQuery.h"

#include "Def/TcsSkillActivateResult.h"
#include "Def/TcsSkillDefData.h"
#include "Skill/TcsSkillInstance.h"
#include "Skill/TcsSkillInstanceHandle.h"
#include "Skill/TcsChainRedirect.h"
#include "Skill/TcsCooldownState.h"
#include "Skill/TcsSkillEntrySelector.h"
#include "Skill/TcsNumericParamModifier.h"
#include "Skill/TcsSkillEntryHandle.h"
#include "Skill/TcsSkillRegistry.h"
#include "Runtime/TcsEntityBroadcastGuard.h"

#include "TcsSkillSubsystem.generated.h"

class UTcsEventBusSubsystem;
class UTcsClockSubsystem;



/**
 * 技能门面（M5 技能层唯一入口）：世界级子系统，持**per-entity 已学技能账本**与**本世界的技能定义登记表**。
 *
 * 分工：本类 = 门面（世界过滤 / 定义登记 / 授予与撤销 / 账本读取面 / 门禁第一道的判据持有 / 激活入口）；
 * `FTcsSkillRegistry` = 账本登记表（per-entity 桶 + 槽位代际）；`TTcsInstancePool` = 施法运行态池；
 * **`FTcsSkillOps` = 账本引擎函数**（授予 / 撤销 / 按来源级联 / 解析 / 读取的流程逻辑）、
 * **`FTcsCastOps` = 施法引擎函数**（六道门禁裁决 / 运行态建与终结 / 快照构建 / 起链 / 广播）——
 * **桶只存数据与索引，操作全在引擎函数**，这是设计文档 §3.1 的既定分工，同 `UTcsBuffSubsystem` 的处置。
 * 本类的同名方法因此都是**薄壳**（一行转发），保证门面签名稳定而流程改动不碰 UE 生命周期样板。
 *
 * 非 Tickable：账本无时间语义（冷却归 `R6.5`）。
 *
 * **定义怎么进来（单向，与状态门面同款）**：定义库 `UTcsDefinitionSubsystem` 住 `TcsIntegration`，
 * 而 `TcsIntegration` **依赖** `TcsSkill`——本类 MUST NOT 反向 include 它（成环）。故定义经
 * **登记口**（`RegisterSkillDef`）写入；依赖方向单向：定义库写进门，门不反查定义库
 * （同款先例 = `UTcsBuffSubsystem::RegisterBuffDef`）。
 *
 * **文件落点**：门面头与日志通道住 `Public/` 根（`cpp-module-structure` 规格），
 * 领域代码住 `Public/Skill/` 与 `Private/Skill/`。
 */
UCLASS()
class TCSSKILL_API UTcsSkillSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 初始化：无每世界共享对象需要预建（账本与登记表按需增长）
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——账本是运行时设施（对齐状态/时钟/总线/属性/效果链门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/**
	 * 反初始化：**三者都要清**——技能定义登记表 / 账本注册表 / 注入的实体查询接口。
	 *
	 * **为什么必须清注入的接口**：`TScriptInterface` 持弱引用语义之外的强持有（本类以 `UPROPERTY` 持有），
	 * 不清则世界拆解后仍握着一个已失效的实现；且跨 PIE 残留会让"未注入"这条降级路径**再也测不到**
	 * （表现为上一世界注入的实现一直生效）。
	 *
	 * **发号器不复位**：`Generation` 的契约是"进程内唯一且永不复用"——复位会让新世界的条目号与上一世界撞车。
	 */
	virtual void Deinitialize() override;

	/**
	 * GC 引用收集（**技能定义登记表 + 施法运行态池的 GC 可见持有**）。
	 *
	 * 为什么必须自己实现（**两处非 UPROPERTY 容器**）：
	 * ① `RegisteredDefs` 是 `TMap<FGameplayTag, TUniquePtr<FTcsSkillDefData>>`；
	 * ② `SkillInstances` 是 `TTcsInstancePool`（元素住 `TArray`）——
	 * 本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它们**；而两者的内容里都有
	 * `FInstancedStruct`（定义的 `CastQueryFragment` / 参数行的数值来源；运行态的参数快照条目
	 * `SourceRef`），其**内层内存可以放宿主自定义 struct 的 `UPROPERTY` 对象引用** ⇒ 不补引用即
	 * **静默回收**（表现为"参数行/查询片段/快照来源里那个对象变成空引用"，不是崩溃）。
	 *
	 * **判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——与 `UTcsBuffSubsystem` 同款。
	 * **手法差异（MUST 守）**：定义侧走 `AddPropertyReferencesWithStructARO`（`FTcsSkillDefData`
	 * **是**反射结构体，有 `StaticStruct()`）；快照侧 MUST 逐条走
	 * `FInstancedStruct::AddStructReferencedObjects`（`FTcsParamSnapshotEntry` **不是**反射结构体，
	 * 没有 `StaticStruct()` 可给）——与状态门面对实例快照的处置逐字同款。
	 *
	 * **账本条目（`FTcsLearnedSkillEntry`）不在本函数范围内**：它按纪律 MUST NOT 持任何 `UObject` 引用
	 * （见 `TcsSkillRegistry.h`），故无需补引用。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#pragma endregion


// 定义登记
#pragma region Definition

public:
	/**
	 * 登记技能定义（**身份由调用方显式传入**——键 = `DefTag`）。
	 *
	 * **为什么身份是形参而不是从 `Def` 里读**：身份归**资产**声明（`UTcsSkillDef::DefTag`——它是
	 * 资产身份、`GetPrimaryAssetId()` 的取值来源、作者期的校验对象），而 `FTcsSkillDefData` 是**定义内容**
	 * （运行期副本）。若为了"从内容里读身份"再往数据 struct 里塞一份 `DefTag`，策划就得在同一个资产里
	 * 把同一个 tag 手填两遍——那是**双真相**。故：身份住资产、内容住数据 struct、
	 * 运行期副本由登记口把两者的身份一次对齐（调用方 = 定义库，其缓存键本就是 `DefTag`，零额外信息）。
	 *
	 * **登记表自持副本**（`TUniquePtr` 持有使 `GetRegisteredSkillDef` 返回的指针地址稳定）。
	 *
	 * 拒绝面（**Error 日志** + 返回 false，不 ensure——调用方是定义库，这里的失败在上游已被拦）：
	 * `DefTag` 无效、同 id 重复登记（**不静默覆写**——口径同链/触发/状态定义登记）。
	 *
	 * @param DefTag 定义身份（资产侧 `UTcsSkillDef::DefTag`）。
	 * @param Def 定义内容（按值拷入登记表）。
	 * @return 返回是否登记成功。
	 */
	bool RegisterSkillDef(FGameplayTag DefTag, const FTcsSkillDefData& Def);

	/**
	 * 注销技能定义（未登记 = Warning + false，不 ensure——正常清理路径）。
	 *
	 * **有该定义的存活条目时不代为撤销**：已建条目持有定义的身份（`DefTag`）而非指针，
	 * 注销只影响"新授予能否解析到定义"（同 `TTcsInstancePool` 的零策略纪律）。
	 *
	 * @param DefTag 定义身份。
	 * @return 返回是否注销成功。
	 */
	bool UnregisterSkillDef(FGameplayTag DefTag);

	/**
	 * 按身份解析本世界已登记的技能定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * **无反射面**：返回**裸 struct 指针** `const FTcsSkillDefData*`——UHT 不支持 struct 指针作反射返回
	 * （同 `GetRegisteredBuffDef` 的取舍）。
	 *
	 * @param DefTag 定义身份。
	 * @return 返回定义；未登记返回 nullptr。
	 */
	const FTcsSkillDefData* GetRegisteredSkillDef(FGameplayTag DefTag) const;

	// 本世界已登记的定义数（观测与装置断言用）
	int32 GetRegisteredSkillDefCount() const;

#pragma endregion


// 授予与撤销
#pragma region Registry

public:
	/**
	 * 授予技能（把"这个实体会这个技能"写进账本）。
	 *
	 * **授予前置门禁（硬约束）**：`DefTag` 在本世界**未登记** ⇒ **拒绝授予** + `Warning`（**不 ensure**——
	 * 内容缺口不是契约违规）。**MUST NOT** 建出一个取不到定义内容的条目：那样账本的三个读取面
	 * （`GetLevel` / `GetNumericParam` / `IsSwitchSet`）会全部落空，而失败面表现为"读数为 0"而非"没学到"
	 * ——那是**静默错误**。
	 *
	 * 重复授予同一 `DefTag`：**刷新**既有条目（更新 `Level` 与 `LearnSource`），不新建第二条
	 * （同一实体"会同一个技能"只有一条事实）。
	 *
	 * **无反射面**：形参含未反射化句柄（本轮不加 `UFUNCTION`——账本的脚本/蓝图读写面归台账 `SCRIPT-10`）。
	 *
	 * @param Entity 被授予方实体（无效即拒绝）。
	 * @param DefTag 技能定义身份（须已在本世界登记）。
	 * @param Source 学习来源句柄（无效则由本门面发号——同一次授予内该条目共用它，亦作级联撤销锚点）。
	 * @param OutHandle 输出条目句柄（可选；失败时不写）。
	 * @return 返回是否授予成功。
	 */
	bool GrantSkill(
		FTcsCombatEntityHandle Entity,
		FGameplayTag DefTag,
		FTcsSourceHandle Source,
		FTcsSkillEntryHandle* OutHandle = nullptr);

	/**
	 * 撤销单个条目（脏句柄 = Warning + false，不 ensure——时序竞态语义）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回是否撤销成功。
	 */
	bool RevokeSkill(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Handle);

	/**
	 * 按学习来源级联撤销（摘该来源**在该实体**的全部条目）。
	 *
	 * **读数 = 返回实际摘掉的条目数**（观测与装置断言用）。
	 *
	 * **遍历纪律**：先收集命中句柄、再逐个撤销（两段式）——桶的 `ForEach` 期间 MUST NOT 增删条目。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Source 学习来源句柄（无效 = 0 命中，返回 0）。
	 * @return 返回摘掉的条目数。
	 */
	int32 RevokeSkillBySource(FTcsCombatEntityHandle Entity, FTcsSourceHandle Source);

	/**
	 * 注销实体：摘掉该实体全部条目并删除其桶。
	 *
	 * @param Entity 实体实体句柄。
	 * @return 返回摘掉的条目数（未注册返回 0——正常路径，不 ensure）。
	 */
	int32 UnregisterEntity(FTcsCombatEntityHandle Entity);

	/**
	 * 按句柄取条目（脏句柄返回 nullptr 且不 ensure——时序竞态是正常路径）。
	 *
	 * **无反射面**：返回裸 struct 指针（UHT 表达不了）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回条目；脏句柄返回 nullptr。
	 */
	const FTcsLearnedSkillEntry* GetEntry(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Handle) const;

	/**
	 * 遍历某实体的在册条目（**按桶内槽位下标升序**——稳定序，同输入同输出）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **遍历期间 MUST NOT 增删条目**（会改动槽位/空闲栈）——需要增删时先收集句柄、遍历结束后再动。
	 *
	 * **无反射面**：形参含 `TFunctionRef`（C++ 专用面，同 `ITcsEntityQuery`）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Visitor 访问者（返回是否继续）。
	 */
	void ForEachEntry(FTcsCombatEntityHandle Entity, TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const;

	// 某实体在册条目数（观测与装置断言用；**验收读数取它**，MUST NOT 取"GrantSkill 被调用几次"）
	int32 GetEntryCount(FTcsCombatEntityHandle Entity) const;

	// 全部门面在册条目数（跨实体合计；观测用）
	int32 GetTotalEntryCount() const;

#pragma endregion


// 参数链（外部施加）
#pragma region ParamChain

public:
	/**
	 * 外部施加参数修正器（D5-19）：把一批修正器物化后追加进**选择器命中的**每个条目的参数账本。
	 *
	 * **参数链轮的施工入口**：`FTcsSkillEntrySelector` 表达作用域（四档各有字段载体），
	 * `FTcsNumericParamModifier` 是定义侧行。定义自带的行不走这里——它们由
	 * `FTcsCastOps::Activate` 在激活期物化（声明作用域恒为本条目自身）。
	 *
	 * **两个未实现档（`ESS_ByCategoryTags` / `ESS_Custom`）MUST 具名报出"未实现"且零施加**
	 * （分别归台账 `STAT-10` 与 `R6.5-g`）。
	 *
	 * **`Source` 是形参而不是"修正器行自带的来源"**（2026-10-09 订正）：来源**因实例而异**
	 * （同一批修正器可由装备给、也可由天赋给）⇒ 它属于**这一次施加**，不属于那条声明。
	 * **来源无效 ⇒ 本函数拒绝并零施加**（无锚点的条目摘不掉，会违反成对性判据）。
	 *
	 * **无反射面**：形参含未反射化句柄与 C++ 容器（本轮不加 `UFUNCTION`——脚本读写面归 `SCRIPT-10`）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Selector 条目选择器。
	 * @param Modifiers 定义侧修正器行。
	 * @param Source 本次施加的来源锚点（级联摘除用，由调用方声明"这次是谁给的"）。
	 * @return 返回是否至少命中一个条目并施加。
	 */
	bool ApplyParamModifiers(
		FTcsCombatEntityHandle Entity,
		const FTcsSkillEntrySelector& Selector,
		const TArray<FTcsNumericParamModifier>& Modifiers,
		FTcsSourceHandle Source);

	/**
	 * 按来源级联摘除参数修正器（摘该实体全部条目的 `NumericParamModInstances` 中 `Source` 匹配者）。
	 *
	 * **读数 = 返回摘除条数**；`0` 条命中 = 正常路径（**不 ensure**）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Source 归属来源句柄。
	 * @return 返回摘除的修正器条数。
	 */
	int32 RemoveParamModifiersBySource(FTcsCombatEntityHandle Entity, FTcsSourceHandle Source);

#pragma endregion


// 施法运行态的 GC 遍历口
#pragma region SkillInstanceGC

public:
	/**
	 * GC 引用收集专用：遍历在册施法运行态。
	 *
	 * **⚠ 为什么不能直接用 `SkillInstances.ForEach`（2026-08-10 首轮 PIE 实测暴露的真实缺陷）**：
	 * `TTcsInstancePool` 的**每一个**进入点（含 `ForEach` / `Resolve` / `IsValid`）都无条件
	 * `ensure(IsInGameThread())`（`TcsInstancePool.h:29/54/69/91/109`），而**GC 的引用收集不在游戏线程上**
	 * （实测 callstack：`FRealtimeGC::CollectReferencesForGC` → `AddReferencedObjects` → `ForEach`
	 * ⇒ 每次 GC 都留一条 `Ensure condition failed: IsInGameThread()` 红字，**与池空不空无关**）。
	 * ⇒ ARO 内 MUST NOT 走池的公开进入点。
	 *
	 * **本函数因此只做"读连续缓冲"这一件事**：按 `Generations` 的奇偶跳过空闲槽
	 * （与池的 `ForEach` **同一判据**，只是**不带线程断言**），并对每个在册槽回调。
	 * **MUST NOT** 在此做任何改状态的事（GC 期改状态是另一类错误）——纯读。
	 *
	 * @param Visitor 访问者（只读运行态）。
	 */
	void ForEachSkillInstanceForGC(TFunctionRef<void(const FTcsSkillInstance&)> Visitor) const;

#pragma endregion


// 激活
#pragma region Activate

public:
	/**
	 * 尝试激活一个已学技能——六道**具名**门禁（`SAR_*`）+ 建运行态 + 建快照 + 起主链 + 发 `OnCastStarted`
	 * + **瞬发当刻完成**（空时段表）或**排定首段到期条目**（非空时段表）。
	 *
	 * **门禁序**：实体 Ready → 已学 → 冷却 → Instancing 四路分流 → CanAfford
	 * → 定义可解析。**每道具名返回**（修 TCS 报告 09 的门禁内联缺陷）。
	 * **六道今天全部可拒**（③ 冷却与 ⑤ CanAfford 已于 `R6.5-1` Task 1/2 转真——
	 * **MUST NOT 再读成"占位/恒过"**，旧的 `R6.5 占位` 注记时态已到期）。
	 *
	 * **为什么 MUST NOT 收"施法上下文"形参**（设计 §4 的 `Context`）：本 Task **无消费者**——
	 * 链侧 `SelectTargets` 自填目标集、`Instigator` 缺省 = 施法者。收一个无人填的形参即
	 * **"零消费者预建"**；归属 = 出现"宿主指定目标集施法"的真实消费者时。
	 *
	 * **无反射面**：形参含未反射化句柄（本轮不加 `UFUNCTION`——施法面的脚本读写面归台账 `SCRIPT-10`）。
	 *
	 * @param Entity 施法实体。
	 * @param DefTag 技能定义身份。
	 * @param OutRun 输出运行态句柄（可选；失败时不写）。
	 * @return 返回激活结果（具名）。
	 */
	ESkillActivateResult TryActivate(
		FTcsCombatEntityHandle Entity,
		FGameplayTag DefTag,
		FTcsSkillInstanceHandle* OutRun = nullptr);

	/**
	 * 终结一个施法运行态（四路汇聚的唯一入口——回收动作与事件分岔见 `FTcsCastOps::FinishSkillInstance`）。
	 *
	 * **本口发的是 `OnCastInterrupted`**（等价"顶替"档）；**完成**档与**打断**档各有自己的入口
	 * （激活流程末尾的完成终结 / `CancelCast`），三者最终都落到同一个例程上。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回是否确实终结了一个在册运行态。
	 */
	bool TerminateSkillInstance(FTcsSkillInstanceHandle RunHandle);

	/**
	 * **打断一个施法运行态**（`CancelCast`）：先过查询契约，不可打断 ⇒ 拒绝且运行态原样继续。
	 *
	 * 结算序与"止于未来不追溯"的语义见 `FTcsCastOps::CancelCast`。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回是否真的打断了（已终结）。
	 */
	bool CancelCast(FTcsSkillInstanceHandle RunHandle);

	/**
	 * 当前这次施法能不能被打断（D5-1 查询契约）——**打断结算与宿主移动系统询问的唯一合法入口**。
	 *
	 * **宿主 MUST NOT 自行读 `FTcsSkillDefData` 的开关字段代判**（那会在三档之间制造第二真相）。
	 * 三档分派与两处具名降级见 `FTcsCastOps::CanBeCanceled`。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回当前是否可打断（悬空句柄 ⇒ false，不 ensure）。
	 */
	bool CanBeCanceled(FTcsSkillInstanceHandle RunHandle);

	/**
	 * 当前是否允许移动（宿主移动系统询问）。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回当前是否允许移动（悬空句柄 ⇒ false，不 ensure）。
	 */
	bool CanMoveNow(FTcsSkillInstanceHandle RunHandle);

	/**
	 * 取当前时段下标（观测与装置断言用）。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回当前时段下标；悬空句柄返回 `-1`（**与"第 0 段"可区分**——0 是合法下标）。
	 */
	int32 GetCastPhaseIndex(FTcsSkillInstanceHandle RunHandle) const;

	/**
	 * 取本 run **实际起来的链身份**（重定向的验收读数取它）。
	 *
	 * **为什么必须有这个读口**：链运行态不持链身份 ⇒ 从 `ChainRunHandle` 反查不到"这次起的是哪条链"。
	 * 若装置改取"解析函数返回了什么"或"重定向表里有几条"，那是在测解析而不是测起链。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回实际起的链身份；未起链或悬空句柄返回空 tag。
	 */
	FGameplayTag GetCastActiveChainId(FTcsSkillInstanceHandle RunHandle) const;

	// 挂载一条链重定向（`Source` 无效 / 身份无效 ⇒ 拒绝并返回 false）
	bool MountChainRedirect(const FTcsChainRedirectEntry& Entry, FTcsSourceHandle Source);

	// 按来源级联摘除链重定向（返回摘除条数；0 条命中 = 正常路径，不 ensure）
	int32 UnmountChainRedirectsBySource(FTcsSourceHandle Source);

	// 本世界在册链重定向记录数（观测与装置断言用）
	int32 GetChainRedirectCount() const;

	/**
	 * 注入链重定向 Custom 契约（注入点**形状**；其求值归 `R6.5-g`）。
	 *
	 * **`nullptr` 是配置状态不是错误**（同 `SetEntityQuery` 的口径）：未注入时解析路径
	 * **不报**"未实现"——没有东西被拒绝求值。已注入 ⇒ 具名 `Warning` 报未实现并跳过
	 * （MUST NOT 静默忽略，那会让宿主以为它生效了）。
	 *
	 * @param InResolver 宿主实现（可传空以撤销注入）。
	 */
	void SetChainRedirectResolver(TScriptInterface<ITcsChainRedirectResolver> InResolver);

	/**
	 * 按句柄取施法运行态（悬空句柄返回 nullptr 且不 ensure——时序竞态是正常路径）。
	 *
	 * **无反射面**：返回裸 struct 指针（UHT 表达不了）。
	 *
	 * @param RunHandle 运行态句柄。
	 * @return 返回运行态；悬空句柄返回 nullptr。
	 */
	const FTcsSkillInstance* GetSkillInstance(FTcsSkillInstanceHandle RunHandle) const;

	// 在册施法运行态数（观测与装置断言用；**验收读数取它**，MUST NOT 取"TryActivate 被调用几次"）
	int32 GetSkillInstanceCount() const;

#pragma endregion


// 账本参数读取面
#pragma region ParamRead

public:
	/**
	 * 取生效等级（D3-11 终定语义）。
	 *
	 * `EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`：
	 * - `LevelBase` 取自该条目 `DefTag` 对应的**已登记定义**（未登记返回 0）；
	 * - `Σ` 由形参传入的修正器列表给出——**本轮 `Level` 修正键的写入面由参数链轮提供**，
	 *   故本函数取"带 `Level` 键的修正器列表输入"（形参传入），**MUST NOT** 依赖条目结构体持该字段
	 *   （`FTcsLearnedSkillEntry` 按纪律不含参数修正链，见其类型注释）；
	 * - clamp 下界到 0（负值不是合法等级）。
	 *
	 * **无反射面**：形参含未反射化句柄。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param LevelModifiers 带 `Level` 键的修正器列表（本轮由参数链轮提供；可传空表）。
	 * @return 返回生效等级（脏句柄返回 0）。
	 */
	int32 GetLevel(
		FTcsCombatEntityHandle Entity,
		FTcsSkillEntryHandle Handle,
		const TArray<double>& LevelModifiers) const;

	/**
	 * 读数值参数表（按 `DefTag` 解析定义内容，逐行取 `Level` 索引后的规范值）。
	 *
	 * **未命中落 miss**（返回 false）：**MUST NOT** 静默返回 0 当命中——调用方 MUST 用返回值区分
	 * "命中且为 0"与"没配这个键"。出参在 miss 时内容未定义，调用方不得使用。
	 *
	 * **两表互不兜底**：本函数只看数值参数表（`Params`），**MUST NOT** 回落到布尔开关表。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 参数键（`TcsStateParam` 根；该根 `State` 取广义，含技能激活运行态）。
	 * @param OutValue 输出读到的规范值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	bool GetNumericParam(
		FTcsCombatEntityHandle Entity,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		double& OutValue) const;

	/**
	 * 读布尔开关表（与数值参数表**彼此独立**、互不兜底）。
	 *
	 * **未命中落 miss**（返回 false），语义同上。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 布尔参数键（`TcsStateParam` 根）。
	 * @param OutValue 输出读到的开关值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	bool IsSwitchSet(
		FTcsCombatEntityHandle Entity,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		bool& OutValue) const;

#pragma endregion


// 冷却（M5 第二层：D5-15 轨道 / D5-16 可变性 API）
#pragma region Cooldown

public:
	/**
	 * 该技能条目**现在是否在冷却中**（`D5-15`："可施放 = 全部自身轨道清空 + 所属组槽清空"）。
	 *
	 * **本口是 UI 轮询面**（`D5-15`："真值在 Entry，UI 轮询"）——**纯查询语义**：
	 * MUST NOT 改运行态、MUST NOT 发事件、MUST NOT 动账本。
	 *
	 * **无反射面（MUST 记，防后人照 §2.4 草稿补 `UFUNCTION`）**：`PLN-R6.5-1` §2.4 的签名表在
	 * 这些口上写了 `UFUNCTION()`，**本轮刻意不加**——判据：本类**今天零 `UFUNCTION`**
	 * （`GrantSkill` / `TryActivate` / `GetNumericParam` 等全部逐字注明"本轮不加 `UFUNCTION`"），
	 * 且账本与施法面的脚本读写面整体归台账 `SCRIPT-10`（**触发条件型，用户 2026-10-08 裁定
	 * "可以后续安排"**）。单独给冷却加宏会造出"技能面里只有冷却能脚本调"的**孤岛**，
	 * 且形参 `FTcsSkillEntryHandle` 虽已反射，但口径不统一会让 `SCRIPT-10` 落地时**无法一次对齐**。
	 * ⇒ 该缺口统一由 `SCRIPT-10` 收口，**MUST NOT 在此提前破例**。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @return 返回是否在冷却中（悬空句柄 / 定义取不到 ⇒ false，不 ensure）。
	 */
	bool IsOnCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry);

	/**
	 * 取某轨的剩余秒数（UI 径向进度的分子）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @param TrackIndex 轨道下标（对应 `FTcsCooldownPolicy::Tracks` 的序）。
	 * @return 返回剩余秒数（未在冷却 / 越界 / 悬空 ⇒ 0）。
	 */
	double GetCooldownRemaining(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex);

	/**
	 * 取某轨本次冷却的**展示总时长**（径向进展的分母）。
	 *
	 * **不变量 `Remaining <= Total` 恒成立**（否则 UI 会画出超过 100% 的弧）——由
	 * `FTcsCooldownOps::AdjustTrack` 的"`Total` 只增不减"保证。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 * @return 返回总时长（未在冷却 / 越界 / 悬空 ⇒ 0）。
	 */
	double GetCooldownTotal(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex);

	/**
	 * 取该条目的轨道数（**读定义**，不是数"正在跑的轨道"——理由见 `FTcsCooldownOps::GetTrackCount`）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @return 返回轨道数（悬空 / 定义取不到 ⇒ 0）。
	 */
	int32 GetCooldownTrackCount(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry);

	/**
	 * 调整某轨的剩余时长（`D5-16` 承诺的 `AdjustCooldown`）——`Delta > 0` 延长、`< 0` 缩短；
	 * 缩短到不剩 = 清轨。
	 *
	 * **改完 MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）——只改时刻不重挂会让
	 * 堆里的旧时刻继续生效，症状是"改了却不生效"或"提前到期"。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 * @param Delta 剩余时长的增量（秒）。
	 */
	void AdjustCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex, double Delta);

	/**
	 * 清某轨（`D5-16` 承诺的 `ResetCooldown`）：撤到期条目 + 状态整体复位。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 */
	void ResetCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex);

	/**
	 * **起该条目的全部自身轨 + 所属组轨**（`PLN-R6.5-1` §0.2 裁决 ⑤ 新增）。
	 *
	 * **为什么必须有它**（`D5-16` 只承诺了 `Adjust` / `Reset`）：那两枚是"**改**已有冷却"
	 * （改时长 / 清掉），**都不是"开始冷却"**——而 `ECT_Manual` 档的定义恰恰是"框架不挑时机、
	 * 由宿主在自己的逻辑处起冷"。没有本口，手动档**物理上无法兑现**。
	 *
	 * **它与自动档走完全同一条路径**（`FTcsCooldownOps::StartCooldownOnEntry`）——
	 * 手动档的含义是"框架不挑时机"，**不是**"框架不干活"。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @return 返回实际起的轨道条数（0 = 无轨道 / 悬空 / 定义取不到 / 时钟不可得）。
	 */
	int32 StartCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry);

#pragma endregion


// 消耗（M5 第二层：D5-10 的 Policy × Timing）
#pragma region Cost

public:
	/**
	 * **按条目提交消耗**（`ECT_Manual` 档的专用入口 + 宿主主动重试口）。
	 *
	 * **为什么必须有它**（`D5-10` 的提交口不在任何既有清单里）：`ECT_Manual` 档的定义是
	 * "框架不挑时机、由宿主在自己的确定逻辑处调"——没有本口，手动档**物理上无法兑现**。
	 * 同族地，冷却侧的对应物是 `StartCooldown`（`PLN-R6.5-1` §0.2 裁决 ⑤）。
	 *
	 * **自动档 MUST NOT 走它**（`OnCastStarted` / `OnCastCompleted` / `OnEvent` 直调
	 * `FTcsCostOps::CommitCost`）：自动档的提交点**已经知道**自己该不该提交，
	 * 走本口会多一层"按 Timing 判断"的分支——那正是"同一件事两个入口"。
	 *
	 * **本口内部重查一次 `CanAffordCost`**（TOCTOU 防护：门禁⑤ 通过到此刻之间资源可能已被花掉）。
	 *
	 * **无反射面**：同冷却是族——脚本读写面整体归台账 `SCRIPT-10`（见 `IsOnCooldown` 的注释）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @return 返回是否提交成功（`ECT_None` 档恒 true；付不起 / 悬空 / 定义取不到 ⇒ false）。
	 */
	bool PayCost(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry);

	/**
	 * 该条目**现在**付得起吗（纯查询语义：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件）。
	 *
	 * **与门禁⑤ 同一判据**（同一实现 `FTcsCostOps::CanAffordCost`）——宿主据此可预先灰掉按钮，
	 * 而**不应该**用它替代真正的激活（门禁⑤ 在 `TryActivate` 内仍会判一次）。
	 *
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目句柄。
	 * @return 返回是否付得起（悬空 / 定义取不到 ⇒ false）。
	 */
	bool CanAfford(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry);

#pragma endregion


// 门禁第一道：实体可操作
#pragma region EntityReady

public:
	/**
	 * 注入实体查询实现（门禁第一道的判据来源）。
	 *
	 * **未注入 = `nullptr` 是配置状态不是错误**（同 `ITcsParamTableReader` 可空的口径）：
	 * 门禁据此**降级为"只判句柄有效"**，且**放行**——MUST NOT 因"能力未注入"把一切施法拦死、
	 * MUST NOT ensure、MUST NOT 留红字。
	 *
	 * **为什么住门面而不是全局单例**：实体是**世界内**的存在（同一份数据在 PIE 与 Game 世界各自独立），
	 * 注入物按世界持有；跨世界不共享是特性不是缺陷（同 `SetEntityLevelProvider` 的理由）。
	 *
	 * 注入物以 `UPROPERTY` 持有（`TScriptInterface` 的 GC 纪律：非 `UPROPERTY` 的接口指针
	 * 会被 GC 静默回收，表现为"读口突然变空"）。
	 *
	 * @param InEntityQuery 实体查询实现（宿主组件或宿主侧 UObject；可传空以撤销注入）。
	 */
	void SetEntityQuery(TScriptInterface<ITcsEntityQuery> InEntityQuery);

	// 取当前注入的实体查询（未注入返回空接口）
	TScriptInterface<ITcsEntityQuery> GetEntityQuery() const
	{
		return EntityQuery;
	}

	/**
	 * 取事件总线（施法事件广播用；世界拆解期返回 `nullptr`——**非确保**，调用方按丢弃处理）。
	 *
	 * 同款先例 = `UTcsBuffSubsystem::GetEventBus`（状态门面为广播状态事件而持的同一读口）。
	 *
	 * @return 返回总线；世界无该子系统时返回 nullptr。
	 */
	UTcsEventBusSubsystem* GetEventBus() const;

	/**
	 * 取时钟泵门面（时段到期的入堆与取消口；世界拆解期返回 `nullptr`——调用方须容忍）。
	 *
	 * **为什么是公开口**：时段入堆/撤堆发生在引擎函数（`FTcsCastOps`）里，它本来就够得着门面私有区；
	 * 但"取另一个子系统的指针"是任何调用方都要做的事，藏起来只会让下一处再往 friend 名单加一个名字。
	 * 返回裸指针即"可能为空"的显式声明。同款先例 = `UTcsBuffSubsystem::GetClockSubsystem`。
	 *
	 * **门面只转交读口，MUST NOT 持有到期堆**：`FTcsExpiryHeap` 是泵私有，消费者一律经本口拿到的
	 * `UTcsClockSubsystem` 调 `PushExpiry` / `CancelExpiry`（`AdvanceTo` 保持泵私有）。
	 *
	 * @return 返回时钟泵；世界无该子系统时返回 nullptr。
	 */
	UTcsClockSubsystem* GetClockSubsystem() const;

	/**
	 * 门禁第一道：实体是否可操作。
	 *
	 * **判据来源 = 注入的 `ITcsEntityQuery::IsEntityReady`**，**MUST NOT** 用 `IsAlive` 代替——
	 * 两者是**正交轴**：`IsAlive` 答"宿主认为它活着吗"（死亡触发的被动 / 复活技能应当为假但**可放**），
	 * `IsEntityReady` 答"框架能不能在它身上操作"（待销毁尸体应当为假）。`IsAlive` 的 PIE 实现是
	 * "映射里还有这个句柄"而非"活着"（`TcsPieEntityQuery.cpp:71`，其类注释自认"框架不认识死亡"）⇒
	 * 拿它当门禁会**双向误判**。
	 *
	 * **降级纪律**：未注入实现时只判"句柄有效"并放行（见 `SetEntityQuery`）。
	 *
	 * **MUST NOT 反查 `TcsIntegration`**（依赖方向单向——`TcsIntegration` 是终端模块，反查会成环）。
	 *
	 * @param Entity 实体实体句柄。
	 * @return 返回该实体当前是否可操作。
	 */
	bool IsEntityReady(FTcsCombatEntityHandle Entity) const;

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 引擎函数（`FTcsSkillOps` / `FTcsCastOps`）需要读写下面的成员——故开放**最小集**（friend 类不给
	 * 访问域继承：引擎函数访问这些成员靠的是 friend 声明，而非把它们放到 public）。门面只做世界过滤、
	 * 定义登记与注入面，账本与施法的流程逻辑全在引擎函数里（设计文档 §3.1 的既定分工，同
	 * `UTcsBuffSubsystem` 对 `FTcsBuffOps` 的处置）。
	 */
	friend class FTcsSkillOps;
	friend class FTcsCastOps;
	friend class FTcsParamChainOps;
	friend class FTcsCooldownOps;
	friend class FTcsCostOps;

	// per-entity 已学技能注册表（桶 + 槽位代际）
	FTcsSkillRegistry Registry;

	/**
	 * **实体级**共享组冷却槽（`D5-15` 的"公共冷却 = 共享组轨道"）。
	 *
	 * **为什么是"实体 → 组 → 槽"两层而不是一张平表**：公共冷却的作用域是**实体**
	 * （"同一个实体不能连续放任意技能"），组标签只在**实体内**比较——两个实体各有自己的
	 * GCD 线，互不影响。平表会让 A 实体的冷却拦住 B 实体的技能（跨实体串味），
	 * 或逼出一个"复合键 = 实体+组"的手工拼装（那是在重造一层 TMap）。
	 *
	 * **键取 `FTcsCombatEntityHandle`**（已反射、有 `GetTypeHash`），不是 `int64`——
	 * 与 `Registry` / `Stores` 的键控口径一致。
	 *
	 * **零 GC 负担**：槽只含两个 `double`、一个 tag、一个句柄，**无任何 `UObject` 引用**
	 * ⇒ `AddReferencedObjects` MUST NOT 为它新增遍历。
	 *
	 * **清理归属**：条目级撤销**刻意不动它**（组槽会自行到期解除；替它解除会**替同组的其它技能**
	 * 解除公共冷却）——完整清理由 `ReleaseEntityCooldowns`（注销实体）与门面 `Deinitialize` 承担。
	 */
	TMap<FTcsCombatEntityHandle, TMap<FGameplayTag, FTcsCooldownGroupState>> CooldownGroups;

	/**
	 * 施法运行态池（一次激活一个实例）。
	 *
	 * **元素住 `TArray` ⇒ 扩容即搬移**：调用方 MUST NOT 跨"可能触发池扩容的调用"缓存
	 * `FTcsSkillInstance*`（同 `FTcsChainRun` 的持有纪律）。
	 *
	 * **GC 纪律**：池是本类的非 `UPROPERTY` 成员，而运行态里的 `ParamSnapshot` 可持对象引用
	 * ⇒ `AddReferencedObjects` MUST 覆盖它（见该函数注释）。
	 */
	TTcsInstancePool<FTcsSkillInstance, FTcsSkillInstanceTag> SkillInstances;

	// 技能定义登记表（键 = DefTag；TUniquePtr 持有使解析返回的指针地址稳定）
	TMap<FGameplayTag, TUniquePtr<FTcsSkillDefData>> RegisteredDefs;

	// 学习来源发号器（未声明来源时由本门面发号；进程内唯一、永不复用）
	FTcsSourceHandleRegistry SourceRegistry;

	/**
	 * 链重定向栈（D5-7 链级让渡；账本侧）。
	 *
	 * **零 GC 负担**：记录只含 tag / 句柄 / 整数，**无任何 `UObject` 引用**（见该类型的类注释）
	 * ⇒ `AddReferencedObjects` MUST NOT 为它新增遍历。
	 */
	FTcsChainRedirectRegistry ChainRedirects;

	/**
	 * **在飞终结标记集**：终结例程已进入广播窗口的 run 句柄。
	 *
	 * **它修的是 Task 3 留下的真实缺陷**：`FinishSkillInstance` 的事件广播是**当场同步派发**
	 * ⇒ 订阅者可在回调内**重入终结同一句柄**。Task 3 只挡住了"双次归还槽位"这一半
	 * （靠归还前复核池句柄是否仍有效），**"打断事件发两次"当时仍然存在**
	 * （`TcsCastOps.cpp` 的旧注释自认，其归属即本轮）。
	 *
	 * **体例 = 共享层的统一重入守卫**（`TTcsEntityBroadcastGuard`，2026-10-14）：
	 * 它取代了状态侧的 `InRemovalBroadcastHandles` 与技能侧原本这份 `InFinishBroadcastHandles`
	 * ——**一份实现、两个实例**（各持自己句柄类型）。语义不变：进出广播窗口**成对**标记 / 解除
	 * ⇒ 递归终结同句柄**返回 false 且静默**，既不重复广播、也不重复归还槽位。
	 * **判据 = 订阅者重入一次，`Interrupted` 仍为 1。**
	 *
	 * **为什么是"进出成对"而不是"终结即加入、永不删"**：本集是**窗口**（广播期间），
	 * 不是"已终结"的历史——广播结束即须移除，否则池槽位复用给新 run（同 `Index`、新 `Generation`）
	 * 时不会误伤（句柄含代际，本已不撞），但**同一句柄的后续合法终结**会被永久拒绝。
	 */
	TTcsEntityBroadcastGuard<FTcsSkillInstanceHandle> FinishBroadcastGuard;

	// 宿主链重定向 Custom 契约（未注入 = 空接口；`UPROPERTY` 持有以满足 TScriptInterface 的 GC 纪律）
	UPROPERTY()
	TScriptInterface<ITcsChainRedirectResolver> ChainRedirectResolver;

	// 宿主实体查询读口（未注入 = 空接口；`UPROPERTY` 持有以满足 TScriptInterface 的 GC 纪律）
	UPROPERTY()
	TScriptInterface<ITcsEntityQuery> EntityQuery;

#pragma endregion
};
