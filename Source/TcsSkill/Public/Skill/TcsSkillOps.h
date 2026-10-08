// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Skill/TcsSkillEntryHandle.h"
#include "Skill/TcsSkillRegistry.h"

class UTcsSkillSubsystem;
struct FTcsLearnedSkillEntry;
struct FTcsSkillDefData;



/**
 * 技能账本引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * **为什么必须存在本类**（同 `FTcsStateOps` 的判据）：门面的职责是**世界过滤 + 定义登记 +
 * 门面签名**；授予 / 撤销 / 按来源级联 / 解析 / 读取的流程逻辑按操作种类集中在本类，这样
 * `UTcsSkillSubsystem.cpp` 保持薄壳，而流程改动不与 UE 生命周期样板混在一起。
 *
 * **它不是"零消费者预建"**：本类的消费者**就是**门面的那些操作（状态侧的
 * `FStateOp` / `ECastOp` 等同族）。**访问私有区是它的存在理由之一**——
 * `Registry` / `SourceRegistry` 是门面的私有成员，操作逻辑落在门面类里会让"谁有权改登记表"
 * 与"谁是门面"这两件事混作一团（状态侧同款：`FTcsStateOps` 是 `UTcsStateSubsystem` 的 friend）。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 参数传入。
 * 用 `class` 而非 namespace 是为了对齐设计文档的 `FSkillOps` 命名，并让"这些函数是一族"在
 * 调用点（`FTcsSkillOps::Grant(...)`）一眼可辨。
 *
 * 文件划分：本头 + `TcsSkillOps.cpp`（授予 / 撤销 / 级联 / 注销 / 访问与计数）/
 * `TcsSkillOps_ParamRead.cpp`（等级与参数双表读取）。
 *
 * **导出宏是必需项**（`WAIT-9` 判据："该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）若直接调
 * 本族静态成员，不带 `TCSSKILL_API` 必 `LNK2019`（状态侧 2026-10-05 实测同款）。
 */
class TCSSKILL_API FTcsSkillOps
{
// 授予
#pragma region Grant

public:
	/**
	 * 授予技能——`UTcsSkillSubsystem::GrantSkill` 的实现体。
	 *
	 * **授予前置门禁（硬约束）**：`DefTag` 未在本世界登记 ⇒ 拒绝 + `Warning`（**不 ensure**——
	 * 内容缺口不是契约违规）。**MUST NOT** 建出取不到定义内容的条目：那样三个读取面会全部落空，
	 * 而失败面表现为"读数为 0"而非"没学到"——**静默错误**。
	 *
	 * **重复授予同一 `DefTag` ⇒ 刷新**既有条目（更新 `Level` 与 `LearnSource`），不新建第二条
	 * （同一单位"会同一个技能"只有一条事实）；刷新**不回退**已冻结在他处的等级（运行中升级不追溯）。
	 *
	 * @param Subsystem 技能门面（提供登记表、桶与发号器）。
	 * @param Unit 被授予方实体（无效即拒绝）。
	 * @param DefTag 技能定义身份（须已在本世界登记）。
	 * @param Source 学习来源句柄（无效则由门面发号；同一条目共用它，亦作级联撤销锚点）。
	 * @param OutHandle 输出条目句柄（可选；失败时不写）。
	 * @return 返回是否授予成功。
	 */
	static bool Grant(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FGameplayTag DefTag,
		FTcsSourceHandle Source,
		FTcsSkillEntryHandle* OutHandle);

#pragma endregion


// 撤销
#pragma region Revoke

public:
	/**
	 * 撤销单个条目（脏句柄 = `Warning` + false，不 ensure——时序竞态语义）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回是否撤销成功。
	 */
	static bool Revoke(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle);

	/**
	 * 按学习来源级联撤销（摘该来源**在该单位**的全部条目）。
	 *
	 * **两段式**：桶的 `ForEach` 期间 MUST NOT 增删（会改动槽位与空闲栈）⇒ 先收集命中句柄，
	 * 遍历结束后再逐个撤销（同 `FTcsTriggerRegistry::UnregisterRowsBySource` 的手法）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Source 学习来源句柄（无效 = 0 命中，返回 0）。
	 * @return 返回摘掉的条目数（观测与装置断言用）。
	 */
	static int32 RevokeBySource(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSourceHandle Source);

	/**
	 * 注销单位：摘掉该单位全部条目并删除其桶。
	 *
	 * **账本条目零外部资源**（不挂修正器、不挂触发行、无时间条目）⇒ 无需逐条撤销流程，
	 * 直接删桶即可——**这正是"只持一个 `LearnSource`"的收益**（没有需要配对摘除的东西）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @return 返回摘掉的条目数（未注册返回 0——正常路径，不 ensure）。
	 */
	static int32 UnregisterUnit(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit);

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄取条目（脏句柄 / 单位未注册返回 nullptr——**正常查询路径，不 ensure**）。
	 *
	 * 门面按句柄取条目时句柄里没有单位段，故 MUST 由调用方给出单位（同 `FTcsStateOps::Find`
	 * 的处境；本模块的桶按单位定位，不做跨桶扫描——句柄的 `Index` 只在单位内有意）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回条目；脏句柄返回 nullptr。
	 */
	static const FTcsLearnedSkillEntry* Find(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle);

	/**
	 * 遍历某单位的在册条目（**按桶内槽位下标升序**——稳定序，同输入同输出）。
	 * 访问者返回 false 即提前终止。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Visitor 访问者（返回是否继续）。
	 */
	static void ForEachEntry(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor);

	// 某单位在册条目数（观测与装置断言用；**验收读数取它**，MUST NOT 取"授予被调用几次"）
	static int32 CountEntries(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit);

	// 全部在册条目数（跨单位合计；观测用）
	static int32 CountAllEntries(UTcsSkillSubsystem& Subsystem);

#pragma endregion


// 读取面
#pragma region ParamRead

public:
	/**
	 * 取生效等级（D3-11 终定语义）：`EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`。
	 *
	 * `LevelBase` 取自该条目 `DefTag` 对应的**已登记定义**（未登记按 0 处理，不猜）；
	 * `Σ` 由形参传入——**本轮 `Level` 修正键的写入面由参数链轮提供**，故本函数取"带 `Level` 键的
	 * 修正器列表输入"，**MUST NOT** 依赖条目结构体持该字段（`FTcsLearnedSkillEntry` 按纪律不含
	 * 参数修正链，见其类型注释）。脏句柄返回 0。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param LevelModifiers 带 `Level` 键的修正器列表（本轮由参数链轮提供；可传空表）。
	 * @return 返回生效等级（钳下界到 0）。
	 */
	static int32 GetLevel(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle,
		const TArray<double>& LevelModifiers);

	/**
	 * 读数值参数表（按 `DefTag` 解析定义内容、逐行求值并转规范值）。
	 *
	 * **未命中落 miss**（返回 false）：**MUST NOT** 静默返回 0 当命中——调用方 MUST 用返回值区分
	 * "命中且为 0"与"没配这个键"。出参在 miss 时内容未定义，调用方不得使用。
	 *
	 * **两表互不兜底**：本函数只看数值参数表（`Params`），**MUST NOT** 回落到布尔开关表。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 参数键（`TcsStateParam` 根；该根 `State` 取广义，含技能激活运行态）。
	 * @param OutValue 输出读到的规范值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	static bool GetNumericParam(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		double& OutValue);

	/**
	 * 读布尔开关表（与数值参数表**彼此独立**、互不兜底）。
	 *
	 * **未命中落 miss**（返回 false），语义同上。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @param Key 布尔参数键（`TcsStateParam` 根）。
	 * @param OutValue 输出读到的开关值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	static bool IsSwitchSet(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle Handle,
		FGameplayTag Key,
		bool& OutValue);

#pragma endregion
};
