// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillOps.h"

#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"

#include "Def/TcsBoolSwitchRow.h"
#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"
#include "Skill/TcsParamChain.h"

#include "TcsSkillSubsystem.h"



// 等级
int32 FTcsSkillOps::GetLevel(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	const TArray<double>& LevelModifiers)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		// 脏句柄 = 时序竞态（不 ensure）；返回 0 而不是条目等级的"合理猜测"——调用方应先用句柄有效性把关
		return 0;
	}

	// `LevelBase` 取自**已登记定义**（未登记说明条目是在注销前建的；按 0 处理，不猜）
	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);

	// **账本优先（2026-10-09 参数链接线）**：条目 `NumericParamModInstances` 里已有 `Level` 键修正器时，
	// 生效等级由**账本**折叠得出（`clamp(0, LevelBase + Σ)`，"运行中升级不追溯"靠激活期快照实现）。
	// 这是 Task 4 落地的写入面——`FTcsParamChainOps::EvaluateEffectiveLevel` 与它**同式**
	// （都经唯一的折叠调用点，初值同为 `LevelBase`）。
	//
	// **业务场景（谁会走到这一支）**：技能自己配了 `Level` 修正行（"某天赋让这个技能等级 +2"、
	// "每级额外 +1 档"）⇒ 任何查询"这个技能现在几级"的地方（UI 技能面板、按等级取档的数值来源、
	// 条件门禁）都必须读**账本算出来的等级**，而不是只看 Def 上的 `LevelBase`。
	// 改前这一支不存在 ⇒ **配了等级修正，读出来还是基础值**（静默，不报错）。
	if (Entry->NumericParamModInstances.Num() > 0)
	{
		return FTcsParamChainOps::EvaluateEffectiveLevel(Subsystem, *Entry, Def);
	}

	// **无账本条目时退回形参口径**（本函数签名的既有契约：修改器列表由调用方传入）。
	// 这条路径保留的意义 = 调用方想在**不改账本**的前提下试算等级（装置 9.8 与既有验收读数即用它）。
	double Sum = static_cast<double>(Def ? Def->LevelBase : 0);
	for (const double Modifier : LevelModifiers)
	{
		Sum += Modifier;
	}

	// clamp 下界到 0：负等级不是合法语义（同设计 D3-11 的 `clamp(0, …)` 原文）
	return FMath::Max(0, FMath::FloorToInt(Sum));
}



// 数值参数表
bool FTcsSkillOps::GetNumericParam(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	double& OutValue)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		return false;
	}

	// **实时通道（2026-10-09 用户裁定 A′，MUST 读这段再改）**：本函数 = "参数行初值 + 账本条目集
	// 按共享折叠器折叠"，是**实时**读数，**MUST NOT** 被读成"读快照拿冻结值"。
	//
	// **业务场景（谁在调它）**：UI 显示"这个技能现在这个参数是多少"、宿主脚本按技能参数做判断、
	// 伤害流程要拿 `DamageBase` 这类输入——都是**此刻**的读数，不是"某一次施法冻结的值"。
	//
	// **它相对改前的两个改进**：① 账本里的修正**被算进来了**（改前它遍历参数行自己求值，
	// 参数链算出来的加成**完全看不见**）；② 它不再自己实现一遍求值 —— 改前这里有第二份
	// "求值 + 值约定转换"，而"值约定只转一次"是本仓硬纪律，两份实现就是两次转换的风险。
	//
	// 判据（为什么不能在这里读快照）：快照住在**每次 run** 的 `FTcsCastRun.ParamSnapshot` 里，
	// 而本函数的签名是 `(Unit, EntryHandle, Key)`——**没有 run 段**；且 `CI_InstancePerExecution`
	// 下同一 Entry **可有两个 run 并存**（Task 3 已实测"两个 run 并存，读数 = 2"）⇒
	// "读哪一次施法的冻结值"**无唯一答案**。
	//
	// 快照的真实消费面 = **求值期的参数表载体**（物化 `ParamChainRows` 时，`ParamRef` 从它取值；
	// 先例 = 状态侧 `FTcsStateModifierMaterializer` 经 `FTcsStateSnapshotScope` 装 `Ctx.ParamTable`）
	// ——`FTcsCastOps::Activate` 里就这么接的。MUST NOT 为一个没有唯一答案的读数而给本函数加 run 形参
	// （今天无消费者；施法期要"某次 run 的冻结读数"时按台账另议读口）。
	//
	// **`EPM_Live` / `EPM_Snapshot` 在本路径上同解**：两者都走实时折叠（快照只影响快照面）。
	return FTcsParamChainOps::EvaluateParamKey(Subsystem, *Entry, Def, Key, OutValue);
}



// 布尔开关表
bool FTcsSkillOps::IsSwitchSet(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	bool& OutValue)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		return false;
	}

	for (const FTcsBoolSwitchRow& Row : Def->BoolSwitches)
	{
		if (Row.Key == Key)
		{
			OutValue = Row.Base;
			return true;
		}
	}

	// **两表互不兜底**：本函数只看布尔开关表，MUST NOT 回落到数值参数表（反之亦然）
	return false;
}
