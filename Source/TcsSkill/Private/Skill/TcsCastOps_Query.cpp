// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsSkillRegistry.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



namespace
{
	/**
	 * 三档查询分派的**唯一实现**（`IsInterruptibleNow` / `CanMoveNow` 共用）。
	 *
	 * **为什么共用一份而不是各写一遍**：两列的差别**只是读哪个字段**（`bInterruptible` 与
	 * `bCanMove`）——分派规则、两处具名降级、回退处置**逐字相同**。若各写一遍，改动一处必然漏另一处
	 * （例如将来给 `Custom` 档补上求值时只改了一条）。⇒ 用**成员指针**把"读哪一列"参数化，
	 * 分派只有一份。
	 *
	 * **两处具名降级的判据（MUST，不是可选）**：
	 * - `CQM_PhaseTable` 档**无当前时段**（`Phases` 空 或 下标越界）⇒ 具名 `Log` + 回退 `DefSwitches`。
	 *   **MUST NOT** 越界读；**MUST NOT** 返回无条件"永不可打断"——后者是一处**无声的行为变更**
	 *   （它会让瞬发技能变得不可打断，而瞬发是本能力明文的主路径）。
	 * - `CQM_Custom` 档 ⇒ 具名 `Warning`（含档名）+ 回退 `DefSwitches`。
	 *   **回退是本轮明确选定的行为，不是漏做**（契约求值归 `R6.5-g`）。
	 *
	 * **纯查询语义（MUST）**：本函数 MUST NOT 改运行态、MUST NOT 发事件、MUST NOT 动账本。
	 * 它的全部输出只有"一个 bool + 若干日志"。
	 *
	 * **名字带 `CastQuery` 前缀**：同模块多个 `.cpp` 会被 UBT 合并进同一 unity TU，
	 * 匿名命名空间的通用名会跨文件相撞（本仓 unity 合并撞名的既有实证）。
	 */
	bool DispatchCastQueryColumn(
		const FTcsSkillDefData& Def,
		const FTcsCastRun& Run,
		const TCHAR* ColumnName,
		bool FTcsSkillDefData::* DefSwitch,
		bool FTcsPhaseSpan::* PhaseSwitch)
	{
		// **回退实现只有一处**：任何降级路径都汇聚到它（保证"回退到默认档"与"默认档"逐字同义）
		const bool FallbackToDefSwitches = Def.*DefSwitch;

		switch (Def.CastQueryMode)
		{
		case ECastQueryMode::CQM_PhaseTable:
		{
			if (!Def.Phases.IsValidIndex(Run.PhaseIndex))
			{
				// 无当前时段（瞬发 / 越界）⇒ 具名 Log + 回退默认档（**不是**越界读、**不是**无条件 false）
				UE_LOG(LogTcsSkill, Log,
					TEXT("施法查询[%s]：时段表档无当前时段（下标=%d 段数=%d）——回退定义开关档（值=%s）"),
					ColumnName, Run.PhaseIndex, Def.Phases.Num(),
					FallbackToDefSwitches ? TEXT("真") : TEXT("假"));
				return FallbackToDefSwitches;
			}

			return Def.Phases[Run.PhaseIndex].*PhaseSwitch;
		}

		case ECastQueryMode::CQM_Custom:
		{
			// **本轮具名未实现**（契约求值归 `R6.5-g`）：Warning 级（这是**宿主配了但框架没做**，
			// 与"瞬发无时段"那种正常路径不同级），含档名，**回退默认档**。
			// **MUST NOT** 静默当默认档（那会让宿主以为片段生效了）；**MUST NOT** 让该档变成永不可打断。
			UE_LOG(LogTcsSkill, Warning,
				TEXT("施法查询[%s]：自定义档的契约求值未实现（归 R6.5-g）——回退定义开关档（值=%s）"),
				ColumnName, FallbackToDefSwitches ? TEXT("真") : TEXT("假"));
			return FallbackToDefSwitches;
		}

		case ECastQueryMode::CQM_DefSwitches:
		default:
			// 默认档：**它就是回退目标本身**，故无日志（这条路径是正常且高频的）
			return FallbackToDefSwitches;
		}
	}
}



// 查询契约
bool FTcsCastOps::IsInterruptibleNow(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	const FTcsCastRun* Run = ResolveRun(Subsystem, RunHandle);
	if (!Run)
	{
		// 悬空句柄 = 时序竞态 ⇒ 返回"不可打断"（不动作的保守档）。**不 ensure**：
		// 终结一个已不存在的 run 没有意义，故此处不必把它当契约违例。
		// **它也不是"无声的行为变更"**：本函数对悬空句柄的读法只影响"该不该终结一个不存在的 run"。
		return false;
	}

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Run->Entity);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(Run->EntryHandle) : nullptr;
	const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(Entry->DefTag) : nullptr;
	if (!Def)
	{
		// 定义取不到 ⇒ 无三档可言。**返回不可打断**（不动作的保守档），Log 级报出原因。
		UE_LOG(LogTcsSkill, Log, TEXT("施法查询[可打断]：定义取不到（实体=%lld）——按不可打断处理"), Run->Entity.Id);
		return false;
	}

	return DispatchCastQueryColumn(
		*Def, *Run, TEXT("可打断"),
		&FTcsSkillDefData::bInterruptibleDefault,
		&FTcsPhaseSpan::bInterruptible);
}

bool FTcsCastOps::CanMoveNow(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	const FTcsCastRun* Run = ResolveRun(Subsystem, RunHandle);
	if (!Run)
	{
		// 同 `IsInterruptibleNow`：悬空句柄返回"不允许"（保守档），不 ensure
		return false;
	}

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Run->Entity);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(Run->EntryHandle) : nullptr;
	const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(Entry->DefTag) : nullptr;
	if (!Def)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("施法查询[可移动]：定义取不到（实体=%lld）——按不可移动处理"), Run->Entity.Id);
		return false;
	}

	return DispatchCastQueryColumn(
		*Def, *Run, TEXT("可移动"),
		&FTcsSkillDefData::bCanMoveDefault,
		&FTcsPhaseSpan::bCanMove);
}



// 打断
bool FTcsCastOps::CancelCast(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	const FTcsCastRun* Run = ResolveRun(Subsystem, RunHandle);
	if (!Run)
	{
		// 悬空句柄 = 时序竞态（调用方手上的句柄过期）⇒ 静默 false，不 ensure
		return false;
	}

	const FTcsCombatEntityHandle Entity = Run->Entity;

	// ① 先过查询契约（**MUST NOT 无条件打断**）——② 不可打断 ⇒ 拒绝
	// **拒绝的判据 MUST 是"什么都没变"**：运行态原样继续、零事件、零账本改动。
	// 故本分支 MUST NOT 有任何写入动作（它下面直接 return）。
	if (!IsInterruptibleNow(Subsystem, RunHandle))
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("打断被拒：当前时段不可打断（实体=%lld run=%d/%d）——运行态原样继续、零事件、零账本改动"),
			Entity.Id, RunHandle.Index, RunHandle.Generation);
		return false;
	}

	// ③ 可打断 ⇒ 走**四路共用的终结例程**，记原因为"打断"
	// （回收动作与事件分岔全在 `FinishCastRun` 里——本函数 MUST NOT 自己写一份清理）
	return FinishCastRun(Subsystem, RunHandle, ECastFinishReason::ECFR_Interrupted);
}
