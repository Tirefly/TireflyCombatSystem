// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsParamChain.h"

#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



namespace
{
	// 按来源摘除（**file-local，名字带 ParamChain 前缀防 unity 合并撞名**）
	int32 StripParamChainInstancesBySource(TArray<FTcsNumericParamModInstance>& Instances, const FTcsSourceHandle& Source)
	{
		const int32 Before = Instances.Num();
		Instances.RemoveAll([&Source](const FTcsNumericParamModInstance& Instance) { return Instance.Source == Source; });
		return Before - Instances.Num();
	}
}



// 选择器
bool FTcsParamChainOps::MatchesSelector(const FTcsSkillEntrySelector& Selector, const FTcsLearnedSkillEntry& Entry)
{
	switch (Selector.Mode)
	{
	case ETcsSkillEntrySelectorMode::ESS_All:
		return true;

	case ETcsSkillEntrySelectorMode::ESS_ByDefTag:
		// 层级匹配：填父 tag 即命中其全部子技能（`MatchesTag` 的既定语义）
		for (const FGameplayTag& Filter : Selector.DefTagFilter)
		{
			if (Entry.DefTag.MatchesTag(Filter))
			{
				return true;
			}
		}
		return false;

	case ETcsSkillEntrySelectorMode::ESS_ByCategoryTags:
	case ETcsSkillEntrySelectorMode::ESS_Custom:
	default:
		// 未实现档由调用方（Apply）具名报出——本函数只答"是否命中"，答 false 是保守档
		// （**MUST NOT** 静默当 `All`：那会让宿主以为特征筛选生效了）
		return false;
	}
}



// 施加
bool FTcsParamChainOps::ApplyParamModifiers(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	const FTcsSkillEntrySelector& Selector,
	TConstArrayView<FTcsNumericParamModifier> Modifiers,
	FTcsSourceHandle Source)
{
	// 单位未注册：忽略 + 日志，返回 false（**不 ensure**——同 M2 `ApplyModifier` 的 D2-14 口径：
	// 框架允许宿主动态增删，且不做来源追溯）
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("参数修正器施加被拒：单位 %lld 未注册（账本无桶）——忽略"), Unit.Id);
		return false;
	}

	if (Modifiers.Num() == 0)
	{
		return false;
	}

	// **来源无效 ⇒ 拒绝施加**（2026-10-09 订正后新增的守卫）：本入口的 `Source` 是"按来源摘除"的锚点，
	// 无效来源会让写入的条目**摘不掉**——而"不存在摘不掉的条目"正是本能力明文的成对性判据。
	// ⇒ 宁可拒绝这次施加，也不留下一批永久滞留的条目（那会表现为"参数改不回去"，静默且远离根因）。
	// 与 `RemoveParamModifiersBySource` 对无效来源"0 命中不 ensure"同款：正常路径，只留 Warning。
	if (!Source.IsValid())
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("参数修正器施加被拒：来源句柄无效（单位 %lld）——无来源锚点则条目摘不掉，本次零修正被施加"),
			Unit.Id);
		return false;
	}

	// **未实现档 MUST 具名报出且零施加**（两档同款）：静默当 `All` 或静默无操作都会让宿主以为已施加。
	// 此处先于任何写入返回。
	// - `ESS_ByCategoryTags`：类别标识筛选的**载体与规则都未拍板**（台账 `STAT-10`）——载体不在
	//   （StateDef 侧类别标识容器未新增）、档名仍是开放项、且可能另立 `TcsGameplayTag` 模块；
	// - `ESS_Custom`：片段契约归 `R6.5-g`。
	if (Selector.Mode == ETcsSkillEntrySelectorMode::ESS_ByCategoryTags
		|| Selector.Mode == ETcsSkillEntrySelectorMode::ESS_Custom)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("参数修正器施加未实现：选择器档 `%s` 的契约未落地（见台账 STAT-10 / R6.5-g）——本次零修正被施加（单位 %lld）"),
			Selector.Mode == ETcsSkillEntrySelectorMode::ESS_ByCategoryTags ? TEXT("ByCategoryTags") : TEXT("Custom"),
			Unit.Id);
		return false;
	}

	// 物化求值上下文：主体 = 该单位（外部施加入口不收施法上下文，与 `TryActivate` 同款边界）；
	// `EffectiveLevel` 逐条目取该条目自身等级（等级类数值源按各自条目定档）。
	// **`ParamTable` 恒空**：外部施加是"账本外的一次写入"，没有"本次求值的参数表"可言
	// （技能侧唯一的参数表载体是施法运行态的快照，那是激活期物化专用的）。
	int32 AppliedEntryCount = 0;
	int32 AppliedSlotCount = 0;

	// **两段式**：桶的 `ForEach` 期间 MUST NOT 增删条目（会改动槽位/空闲栈）。
	// 本函数只**就地改字段**（追加 `NumericParamModInstances` 元素）——那是被允许的（同 `ForEach` 可写重载的说明）。
	Bucket->ForEach([&](FTcsLearnedSkillEntry& Entry)
	{
		if (!MatchesSelector(Selector, Entry))
		{
			return true;
		}

		++AppliedEntryCount;

		// 求值上下文：主体与发起者同为该单位（外部施加入口不收施法上下文——同 `TryActivate`
		// 的"零消费者不预建"边界）；`EffectiveLevel` 取**该条目自身等级**（等级类数值源按各自条目定档）。
		FTcsParamEvaluateContext Ctx;
		Ctx.Subject = Unit;
		Ctx.Instigator = Unit;
		Ctx.EffectiveLevel = Entry.Level;
		Ctx.ParamTable = TScriptInterface<ITcsParamTableReader>();

		for (const FTcsNumericParamModifier& Row : Modifiers)
		{
			// 物化边界单点：求值一次 + 按行值约定转一次规范值（判据由源自身声明，见该函数注释）。
			// 来源取**本入口的 `Source` 形参**（定义侧行已无该字段——见其订正说明）：来源因实例而异，
			// 属"这一次施加"。它保证**写入的每一条都带同一个可摘除的来源** = 成对性判据。
			FTcsNumericParamModInstance Instance = FTcsNumericParamModInstance::MakeFromModifier(
				Row, ResolveModifierValue(Row, Ctx), Source);

			Entry.NumericParamModInstances.Add(MoveTemp(Instance));
			++AppliedSlotCount;
		}

		return true;
	});

	UE_LOG(LogTcsSkill, Log,
		TEXT("参数修正器施加：单位=%lld 档=%d 命中条目=%d 写入条目=%d"),
		Unit.Id, static_cast<int32>(Selector.Mode), AppliedEntryCount, AppliedSlotCount);

	// 无匹配条目 = 正常路径（该单位没有符合选择器的在册条目），返回 false 且不 ensure
	return AppliedEntryCount > 0;
}



// 级联摘除
int32 FTcsParamChainOps::RemoveParamModifiersBySource(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSourceHandle Source)
{
	if (!Source.IsValid())
	{
		// 无效来源不构成"要摘谁"——0 命中，不 ensure（同 `RevokeBySource` 的无效来源口径）
		return 0;
	}

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		// 单位未注册：0 命中是正常路径（来源可能只挂过已撤销的修正器）
		return 0;
	}

	int32 RemovedCount = 0;

	// **只扫在册条目**（技能账本今天没有"冻结暂存区"这一面——见头文件的边界登记；
	// 若将来出现"技能条目冻结"，该扫描面 MUST 同批补上，否则来源在冻结期结束会让修正器永久滞留、
	// 解冻时凭空多出数值）。
	Bucket->ForEach([&Source, &RemovedCount](FTcsLearnedSkillEntry& Entry)
	{
		RemovedCount += StripParamChainInstancesBySource(Entry.NumericParamModInstances, Source);
		return true;
	});

	// 0 条命中 = 正常路径，MUST NOT ensure
	if (RemovedCount > 0)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("参数修正器按来源摘除：单位=%lld 来源=%llu 摘除 %d 条"),
			Unit.Id, Source.Id, RemovedCount);
	}

	return RemovedCount;
}
