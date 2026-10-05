// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "State/TcsStateEvents.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateRegistry.h"
#include "State/TcsStateStackFragment.h"
#include "State/TcsStateStackPolicy.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



namespace
{
	// 文件局部符号一律带文件前缀（unity 合并纪律：同模块跨 .cpp 的匿名符号会撞名，
	// 且报错位置落在**先定义**的那个文件——最具误导性的一类编译错）

	// 决策种类的日志词
	const TCHAR* TcsStackOps_DecisionToString(ETcsStateStackDecisionKind Kind)
	{
		switch (Kind)
		{
		case ETcsStateStackDecisionKind::Refresh:
			return TEXT("刷新");

		case ETcsStateStackDecisionKind::Stack:
			return TEXT("叠层");

		case ETcsStateStackDecisionKind::Replace:
			return TEXT("替换");

		case ETcsStateStackDecisionKind::Reject:
			return TEXT("拒绝");

		case ETcsStateStackDecisionKind::NewInstance:
		default:
			return TEXT("新建");
		}
	}

	/**
	 * 取自定义决策载荷（**只有 `EGB_Custom` 才查**）。
	 *
	 * 缺失 / 类型不符 ⇒ 返回 nullptr + `Warning`（**配置错误**语义：作者选了逃逸位却没挂策略）。
	 * 调用方据此退化为内置"不分组"语义——策略缺失不该让施加失败。
	 */
	const FTcsStateStackDecisionFragment* TcsStackOps_ResolveFragment(const FStateStackPolicy& Policy)
	{
		if (Policy.GroupBy != EGroupByPolicy::EGB_Custom)
		{
			return nullptr;
		}

		const FTcsStateStackDecisionFragment* Fragment =
			Policy.CustomDecision.GetPtr<FTcsStateStackDecisionFragment>();

		if (!Fragment)
		{
			UE_LOG(LogTcsState, Warning, TEXT("堆叠自定义决策载荷不可用（%s）：按内置\"不分组\"语义退化"),
				Policy.CustomDecision.IsValid() ? TEXT("类型不符") : TEXT("载荷为空"));
		}

		return Fragment;
	}

	/**
	 * "同来源"判据（**未声明来源 = 通配**）：调用方不声明来源时视为"同一续杯方"。
	 *
	 * 为什么必须有通配这一条：来源句柄在**门面**处按无效值发号（每次施加都是一枚新号），
	 * 若"未声明"被判成"换了施加方"，则任何不关心来源的调用方在默认策略（层数无限）下
	 * **每次施加都叠一层**——一个只在长跑里才显形的错误。
	 */
	bool TcsStackOps_IsSameOrigin(const FTcsStateStackRequest& Request, const FTcsStateInstance& Existing)
	{
		return !Request.Source.IsValid() || Request.Source == Existing.Source;
	}

	/**
	 * 组键判定（内置三态 + Custom 接管）。
	 *
	 * `Fragment` 由调用方**解析一次后传入**（解析失败会留 Warning ⇒ 放进按实例循环会刷屏）。
	 */
	bool TcsStackOps_IsSameGroup(
		const FTcsStateStackRequest& Request,
		const FTcsStateInstance& Existing,
		const FStateStackPolicy& Policy,
		const FTcsStateStackDecisionFragment* Fragment)
	{
		// 组键基座：同单位 + 同定义（一条实例 = 一个定义的数据 ⇒ **不同定义永不共组**）
		if (Existing.Unit != Request.Unit || Existing.DefTag != Request.DefTag)
		{
			return false;
		}

		switch (Policy.GroupBy)
		{
		case EGroupByPolicy::EGB_PerSource:
			return TcsStackOps_IsSameOrigin(Request, Existing);

		case EGroupByPolicy::EGB_PerInstigator:
			return Existing.Instigator == Request.Instigator;

		case EGroupByPolicy::EGB_Custom:
			// 载荷可用则由它判；不可用 ⇒ 退化为"不分组"（基座已过 ⇒ 同组）
			return Fragment ? Fragment->IsSameGroup(Request, Existing) : true;

		case EGroupByPolicy::EGB_None:
		default:
			return true;
		}
	}
}



FTcsStateStackDecision FTcsStateOps::ResolveStackDecision(
	const FTcsStateStackRequest& Request,
	const FStateBucket& Bucket,
	const FStateStackPolicy& Policy)
{
	FTcsStateStackDecision Decision;

	// 决策载荷**只解析一次**（解析失败会留一条 Warning ⇒ 一次施加只该出现一条：
	// 若"找成员"与"出决策"各解析一次，同一处配置错误会刷两条重复 Warning）
	const FTcsStateStackDecisionFragment* Fragment = TcsStackOps_ResolveFragment(Policy);

	// 在桶内找同组成员（**至多命中一条**——本模型下组内至多一条实例）。
	// 两段式读：`ForEach` 期间不得增删实例（本处只读，命中即提前终止）。
	FTcsStateHandle MemberHandle;
	Bucket.ForEach([&Request, &Policy, Fragment, &MemberHandle](const FTcsStateInstance& Instance)
	{
		if (TcsStackOps_IsSameGroup(Request, Instance, Policy, Fragment))
		{
			MemberHandle = Instance.Handle;
			return false;
		}

		return true;
	});

	const FTcsStateInstance* Existing = Bucket.Find(MemberHandle);

	// ① 组内无实例 ⇒ 建新条（唯一不读策略的档）
	if (!Existing)
	{
		Decision.Kind = ETcsStateStackDecisionKind::NewInstance;
		Decision.NewStacks = 1;
		return Decision;
	}

	Decision.ExistingHandle = Existing->Handle;
	Decision.NewStacks = Existing->Stacks;

	// ② 接受与否（Custom 特有的第一道门；内置路径恒接受）——排在容量之前：
	// 逃逸位的意思就是"它能接受容量政策会拒的那一次"
	if (Fragment && !Fragment->ShouldAccept(Request, *Existing))
	{
		Decision.Kind = ETcsStateStackDecisionKind::Reject;
		return Decision;
	}

	// ③ 容量：`MaxStacks <= 0` = 无限 ⇒ 本档不可达（无限层永不判满）
	const bool bFull = Policy.MaxStacks > 0 && Existing->Stacks >= Policy.MaxStacks;
	if (bFull)
	{
		Decision.Kind = Policy.Overflow == EOverflowPolicy::EOP_ReplaceExisting
			? ETcsStateStackDecisionKind::Replace
			: ETcsStateStackDecisionKind::Reject;
		Decision.NewStacks = 1;
		return Decision;
	}

	// ④ 未满仓：同来源续杯（层数不变）、异来源叠一层。层数由 Custom 接管（若挂了策略）或内置规则给出；
	// **档位由"层数变没变"反推**——这样 Custom 的 `ResolveStacks` 可以直接决定"这一次算不算叠层"。
	Decision.NewStacks = Fragment
		? Fragment->ResolveStacks(Request, *Existing)
		: (TcsStackOps_IsSameOrigin(Request, *Existing) ? Existing->Stacks : Existing->Stacks + 1);

	Decision.Kind = Decision.NewStacks != Existing->Stacks
		? ETcsStateStackDecisionKind::Stack
		: ETcsStateStackDecisionKind::Refresh;

	return Decision;
}



EApplyResult FTcsStateOps::RefreshStacked(
	UTcsStateSubsystem& Subsystem,
	const FTcsBuffDef& Def,
	FTcsStateInstance& Instance,
	const FTcsStateStackRequest& Request,
	const FTcsStateStackDecision& Decision,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	const TMap<FGameplayTag, double>& Overrides)
{
	const FTcsStateHandle Handle = Instance.Handle;
	const bool bStackChanged = Decision.NewStacks != Instance.Stacks;
	const EApplyResult Result = bStackChanged ? EApplyResult::EAR_Stacked : EApplyResult::EAR_Refreshed;

	// ① 层数与归属字段。**两个句柄都刻意不动**：`Source` 是"谁最先施加的"（续杯 / 叠层的判据基座），
	// `CascadeAnchor` 是级联撤销锚点——两者都已被触发行/修正器按原值登记过；
	// `Instigator` 反过来：它答"最近一次施加是谁发起的"，故随本次施加更新。
	Instance.Stacks = Decision.NewStacks;
	Instance.Level = Def.LevelBase;
	Instance.Instigator = Request.Instigator;

	// ② 快照重建（D3-12："修改 = 重新施加，新 payload 覆盖旧的"）
	FTcsStateEvaluateContext Ctx;
	MakeContext(Ctx, Subsystem, Request.Unit, Instance.Instigator, Def.LevelBase, ParamTable);
	BuildSnapshot(Instance.ParamSnapshot, Def, Ctx, Overrides);

	// ③ 修正器重挂：先按级联锚点摘旧、再按新快照挂新（同一批内完成，条数不累加）。
	// 数值随层数的部分在**物化边界**按 `ValueStack` 乘上当前层数——`Instance.Stacks` 已在①写入，
	// 故物化器读到的是本次施加之后的层数（顺序有意：层数先落，数值后算）。
	MountModifiers(Subsystem, Def, Instance, /*bStripFirst=*/true);

	// 挂载提交会重算 + 广播 ⇒ 订阅者可重入状态操作（移除本实例 / 注销单位）⇒ MUST 重新定位
	FStateBucket* LiveBucket = Subsystem.Registry.FindBucket(Request.Unit);
	FTcsStateInstance* Live = LiveBucket ? LiveBucket->Find(Handle) : nullptr;
	if (!Live)
	{
		// 更新已完成，回调合法移除实例时不再补播事件，回执仍按决策档给。
		UE_LOG(LogTcsState, Log,
			TEXT("状态%s后实例已不在册（挂载提交期间被重入移除）：单位=%lld 定义=%s 句柄=%d/%d"),
			TcsStackOps_DecisionToString(Decision.Kind), Request.Unit.Id, *Request.DefTag.ToString(),
			Handle.Index, Handle.Generation);
		return Result;
	}

	// 属性提交可以注销定义；只使用提交后重新解析的定义，不跨回调持旧 Def 引用。
	const FTcsBuffDef* LiveDef = Subsystem.GetRegisteredStateDef(Request.DefTag);
	if (!LiveDef)
	{
		return Result;
	}

	if (Live->Phase != EStatePhase::ESP_Expiring)
	{
		Transit(Subsystem, Live, EStatePhase::ESP_Expiring);
	}

	// 时间重挂的 Immediate 周期事件也可能移除本实例，返回后必须重新定位。
	ScheduleTime(Subsystem, *Live, *LiveDef, Live->ParamSnapshot, ParamTable, /*bRefresh=*/true);
	Live = Subsystem.Registry.Find(Request.Unit, Handle);
	if (!Live)
	{
		return Result;
	}

	// 层数变化先播、刷新后播；每一笔都是值快照，不跨广播持实例或桶指针。
	const FTcsStateInstance EventSnapshot = *Live;
	if (bStackChanged)
	{
		Broadcast(Subsystem, Tag_TcsEvent_State_StackChanged, EventSnapshot, EStateRemoveCause::ESRC_Removed);
	}

	Live = Subsystem.Registry.Find(Request.Unit, Handle);
	if (Live)
	{
		// 同次更新的两笔载荷保持相同层数；重入后的 Live 只用作活性检查，不覆写事件快照。
		Broadcast(Subsystem, Tag_TcsEvent_State_Refreshed, EventSnapshot, EStateRemoveCause::ESRC_Removed);
	}

	// Refreshed 回调可以自移除或嵌套刷新。只恢复仍在册且仍处过渡态的实例。
	Live = Subsystem.Registry.Find(Request.Unit, Handle);
	if (Live && Live->Phase == EStatePhase::ESP_Expiring)
	{
		Transit(Subsystem, Live, EStatePhase::ESP_Active);
	}

	UE_LOG(LogTcsState, Log, TEXT("状态%s：单位=%lld 定义=%s 句柄=%d/%d 层数=%d 快照=%d 条 剩余=%.3f"),
		TcsStackOps_DecisionToString(Decision.Kind), Request.Unit.Id, *Request.DefTag.ToString(),
		EventSnapshot.Handle.Index, EventSnapshot.Handle.Generation, EventSnapshot.Stacks, EventSnapshot.ParamSnapshot.Num(),
		EventSnapshot.DurationRemaining);

	return Result;
}
