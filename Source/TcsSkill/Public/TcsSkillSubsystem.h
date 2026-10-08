// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"
#include "Host/TcsEntityQuery.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsSkillEntryHandle.h"
#include "Skill/TcsSkillRegistry.h"

#include "TcsSkillSubsystem.generated.h"



/**
 * 技能门面（M5 技能层唯一入口）：世界级子系统，持**per-unit 已学技能账本**与**本世界的技能定义登记表**。
 *
 * 分工：本类 = 门面（世界过滤 / 定义登记 / 授予与撤销 / 账本读取面 / 门禁第一道的判据持有）；
 * `FTcsSkillRegistry` = 登记表（per-unit 桶 + 槽位代际）；**`FTcsSkillOps` = 引擎函数**
 * （授予 / 撤销 / 按来源级联 / 解析 / 读取的流程逻辑）——**桶只存数据与索引，操作全在引擎函数**，
 * 这是设计文档 §3.1 的既定分工，同 `UTcsStateSubsystem` / `FTcsStateOps` 的处置。
 * 本类的同名方法因此都是**薄壳**（一行转发），保证门面签名稳定而流程改动不碰 UE 生命周期样板。
 *
 * 非 Tickable：账本无时间语义（冷却归 `R6.5`）。
 *
 * **定义怎么进来（单向，与状态门面同款）**：定义库 `UTcsDefinitionSubsystem` 住 `TcsIntegration`，
 * 而 `TcsIntegration` **依赖** `TcsSkill`——本类 MUST NOT 反向 include 它（成环）。故定义经
 * **登记口**（`RegisterSkillDef`）写入；依赖方向单向：定义库写进门，门不反查定义库
 * （同款先例 = `UTcsStateSubsystem::RegisterStateDef`）。
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
	 * GC 引用收集（**技能定义登记表的 GC 可见持有**）。
	 *
	 * 为什么必须自己实现：`RegisteredDefs` 是 `TMap<FGameplayTag, TUniquePtr<FTcsSkillDefData>>`——
	 * 本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它**；而定义内容里有
	 * `FInstancedStruct`（`CastQueryFragment` / 参数行的数值来源 / 内联触发行），其**内层内存可以放
	 * 宿主自定义 struct 的 `UPROPERTY` 对象引用** ⇒ 不补引用即**静默回收**
	 * （表现为"参数行/查询片段里那个对象变成空引用"，不是崩溃）。
	 *
	 * **判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——与 `UTcsStateSubsystem` 同款。
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
	 * （同 `GetRegisteredStateDef` 的取舍）。
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
	 * 授予技能（把"这个单位会这个技能"写进账本）。
	 *
	 * **授予前置门禁（硬约束）**：`DefTag` 在本世界**未登记** ⇒ **拒绝授予** + `Warning`（**不 ensure**——
	 * 内容缺口不是契约违规）。**MUST NOT** 建出一个取不到定义内容的条目：那样账本的三个读取面
	 * （`GetLevel` / `GetNumericParam` / `IsSwitchSet`）会全部落空，而失败面表现为"读数为 0"而非"没学到"
	 * ——那是**静默错误**。
	 *
	 * 重复授予同一 `DefTag`：**刷新**既有条目（更新 `Level` 与 `LearnSource`），不新建第二条
	 * （同一单位"会同一个技能"只有一条事实）。
	 *
	 * **无反射面**：形参含未反射化句柄（本轮不加 `UFUNCTION`——账本的脚本/蓝图读写面归台账 `SCRIPT-10`）。
	 *
	 * @param Unit 被授予方实体（无效即拒绝）。
	 * @param DefTag 技能定义身份（须已在本世界登记）。
	 * @param Source 学习来源句柄（无效则由本门面发号——同一次授予内该条目共用它，亦作级联撤销锚点）。
	 * @param OutHandle 输出条目句柄（可选；失败时不写）。
	 * @return 返回是否授予成功。
	 */
	bool GrantSkill(
		FTcsCombatEntityHandle Unit,
		FGameplayTag DefTag,
		FTcsSourceHandle Source,
		FTcsSkillEntryHandle* OutHandle = nullptr);

	/**
	 * 撤销单个条目（脏句柄 = Warning + false，不 ensure——时序竞态语义）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回是否撤销成功。
	 */
	bool RevokeSkill(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Handle);

	/**
	 * 按学习来源级联撤销（摘该来源**在该单位**的全部条目）。
	 *
	 * **读数 = 返回实际摘掉的条目数**（观测与装置断言用）。
	 *
	 * **遍历纪律**：先收集命中句柄、再逐个撤销（两段式）——桶的 `ForEach` 期间 MUST NOT 增删条目。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Source 学习来源句柄（无效 = 0 命中，返回 0）。
	 * @return 返回摘掉的条目数。
	 */
	int32 RevokeSkillBySource(FTcsCombatEntityHandle Unit, FTcsSourceHandle Source);

	/**
	 * 注销单位：摘掉该单位全部条目并删除其桶。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回摘掉的条目数（未注册返回 0——正常路径，不 ensure）。
	 */
	int32 UnregisterUnit(FTcsCombatEntityHandle Unit);

	/**
	 * 按句柄取条目（脏句柄返回 nullptr 且不 ensure——时序竞态是正常路径）。
	 *
	 * **无反射面**：返回裸 struct 指针（UHT 表达不了）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回条目；脏句柄返回 nullptr。
	 */
	const FTcsLearnedSkillEntry* GetEntry(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Handle) const;

	/**
	 * 遍历某单位的在册条目（**按桶内槽位下标升序**——稳定序，同输入同输出）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **遍历期间 MUST NOT 增删条目**（会改动槽位/空闲栈）——需要增删时先收集句柄、遍历结束后再动。
	 *
	 * **无反射面**：形参含 `TFunctionRef`（C++ 专用面，同 `ITcsEntityQuery`）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Visitor 访问者（返回是否继续）。
	 */
	void ForEachEntry(FTcsCombatEntityHandle Unit, TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const;

	// 某单位在册条目数（观测与装置断言用；**验收读数取它**，MUST NOT 取"GrantSkill 被调用几次"）
	int32 GetEntryCount(FTcsCombatEntityHandle Unit) const;

	// 全部门面在册条目数（跨单位合计；观测用）
	int32 GetTotalEntryCount() const;

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
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param LevelModifiers 带 `Level` 键的修正器列表（本轮由参数链轮提供；可传空表）。
	 * @return 返回生效等级（脏句柄返回 0）。
	 */
	int32 GetLevel(
		FTcsCombatEntityHandle Unit,
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
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 参数键（`TcsStateParam` 根；该根 `State` 取广义，含技能激活运行态）。
	 * @param OutValue 输出读到的规范值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	bool GetNumericParam(
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		double& OutValue) const;

	/**
	 * 读布尔开关表（与数值参数表**彼此独立**、互不兜底）。
	 *
	 * **未命中落 miss**（返回 false），语义同上。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 布尔参数键（`TcsStateParam` 根）。
	 * @param OutValue 输出读到的开关值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	bool IsSwitchSet(
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		bool& OutValue) const;

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
	 * @param Unit 单位实体句柄。
	 * @return 返回该实体当前是否可操作。
	 */
	bool IsEntityReady(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 引擎函数（`FTcsSkillOps`）需要读写下面三个成员——故开放**最小集**（friend 类不给访问域继承：
	 * 引擎函数访问这些成员靠的是 friend 声明，而非把它们放到 public）。门面只做世界过滤与登记口，
	 * 授予 / 撤销 / 解析 / 读取的流程逻辑全在引擎函数里（设计文档 §3.1 的既定分工，同
	 * `UTcsStateSubsystem` 对 `FTcsStateOps` 的处置）。
	 */
	friend class FTcsSkillOps;

	// per-unit 已学技能注册表（桶 + 槽位代际）
	FTcsSkillRegistry Registry;

	// 技能定义登记表（键 = DefTag；TUniquePtr 持有使解析返回的指针地址稳定）
	TMap<FGameplayTag, TUniquePtr<FTcsSkillDefData>> RegisteredDefs;

	// 学习来源发号器（未声明来源时由本门面发号；进程内唯一、永不复用）
	FTcsSourceHandleRegistry SourceRegistry;

	// 宿主实体查询读口（未注入 = 空接口；`UPROPERTY` 持有以满足 TScriptInterface 的 GC 纪律）
	UPROPERTY()
	TScriptInterface<ITcsEntityQuery> EntityQuery;

#pragma endregion
};
