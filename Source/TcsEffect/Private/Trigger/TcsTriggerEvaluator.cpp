// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerEvaluator.h"

#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"



// 装配
void UTcsTriggerEvaluator::Initialize(UTcsEffectSubsystem* InOwner)
{
	Owner = InOwner;
}



// 求值入口
void UTcsTriggerEvaluator::HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload)
{
	UTcsEffectSubsystem* Facade = Owner.Get();
	if (!Facade)
	{
		// 门面已回收（世界拆解期）——订阅的惰性摘除由总线负责，此处静默返回
		return;
	}

	// 快照：本轮遍历期间被摘除的行按句柄代际校验跳过，期间新登记的行不入本轮
	// （与总线派发"派发中订阅/退订不影响本轮遍历"同款纪律）
	TArray<FTcsEffectTriggerHandle> Candidates;
	Facade->CollectTriggerRowsForTag(EventTag, Candidates);
	if (Candidates.Num() == 0)
	{
		return;
	}

	// 行序：Priority 降序（大者先，与覆盖带 OverridePriority 同向）；同级按**登记序**
	// （确定性——MUST NOT 依赖容器哈希序）
	Facade->SortTriggerRowsByPriority(Candidates);

	// 门① 事件 Tag 路由已由订阅完成（本回调只因该 Tag 命中而进入）；载荷读取只做一次（各行共用）
	const FTcsTriggerPayloadInfo PayloadInfo = ReadPayloadInfo(Payload);

	// 条件求值用的随机值：**每行各取一次**（同一事件的多行条件各自独立判定）
	// 门面未注入随机源时返回固定值——D0-1 确定性纪律（求值内部 MUST NOT 取随机数）
	for (const FTcsEffectTriggerHandle& Handle : Candidates)
	{
		// 代际校验 + 取值拷贝：不跨 `ExecuteChain` 持实例指针（起链可能触发新登记 → TArray 扩容搬移）
		const FTcsEffectTriggerInstance* Instance = Facade->FindTriggerRow(Handle);
		if (!Instance)
		{
			continue;
		}

		// 门② 执行闸（最廉价先判）
		if (!PassesExecutionGate(Instance->Def))
		{
			continue;
		}

		// 门③ 行级点灯
		if (!PassesGateTags(Instance->Def))
		{
			continue;
		}

		// 门④ 条件（最贵最后判——可含宿主自定义求值）
		FTcsTriggerContext Context;
		Context.EventTag = EventTag;
		Context.ClassificationTags = PayloadInfo.ClassificationTags;
		Context.Caster = PayloadInfo.Caster;

		if (!PassesConditions(Instance->Def, Context))
		{
			if (!Instance->Def.bConditionMissIsSilent)
			{
				// 条件未过是**正常业务路径**，不是故障——只记 Verbose（M8 Explain 面板的数据源）
				UE_LOG(LogTcsEffect, Verbose, TEXT("触发求值：行条件未过（事件=%s 链=%s）"),
					*EventTag.ToString(), *Instance->Def.EffectChainId.ToString());
			}
			continue;
		}

		// 起链装配：链 id 取值拷贝后**立即脱离实例引用**（ExecuteChain 可能使登记表扩容）
		const FGameplayTag ChainId = Instance->Def.EffectChainId;

		FTcsEffectContext ChainContext;
		ChainContext.Caster = Context.Caster;
		ChainContext.EventPayload = Payload;
		// Targets 本轮留空：完整"事件载荷 → 目标"通路需真实带目标的载荷类型（台账 R5-1）

		UE_LOG(LogTcsEffect, Verbose, TEXT("触发求值：行命中（事件=%s 链=%s 主体=%lld）"),
			*EventTag.ToString(), *ChainId.ToString(), Context.Caster.Id);

		// 链未登记时 ExecuteChain 已有拒绝面（Error + 不起链），此处不重复校验
		Facade->ExecuteChain(ChainId, MoveTemp(ChainContext));
	}
}



// 内核
FTcsTriggerPayloadInfo UTcsTriggerEvaluator::ReadPayloadInfo(const FInstancedStruct& Payload) const
{
	const UScriptStruct* PayloadType = Payload.GetScriptStruct();
	if (!PayloadType)
	{
		// 无类型载荷（手动发布时的常态）——不是契约违规
		return FTcsTriggerPayloadInfo();
	}

	const FTcsTriggerPayloadRead* Reader = FTcsTriggerPayloadReaderRegistry::Get().Find(PayloadType);
	if (!Reader)
	{
		// 未注册读取器：**不 ensure**——"载荷类型未知"不是契约违规（手动发布自定义事件是合法用法）
		UE_LOG(LogTcsEffect, Verbose, TEXT("触发求值：载荷类型 %s 无读取器——主体信息取默认值"), *PayloadType->GetName());
		return FTcsTriggerPayloadInfo();
	}

	return (*Reader)(Payload);
}

bool UTcsTriggerEvaluator::PassesExecutionGate(const FTcsEffectTriggerDef& Def)
{
	// R4：单机形态下本地即权威，故 TEG_AuthorityOnly 与 TEG_Always 同判。
	// 判别逻辑随网络姿态轮落地；枚举值本身不得改变含义（末位追加规约）。
	switch (Def.ExecutionGate)
	{
	case ETcsExecutionGate::TEG_Always:
	case ETcsExecutionGate::TEG_AuthorityOnly:
		return true;
	default:
		// 未识别的枚举值（内容来自更新版本的插件？）——保守拒绝 + 留痕
		UE_LOG(LogTcsEffect, Warning, TEXT("触发求值：执行闸取值 %d 未识别——按不通过处理"),
			static_cast<int32>(Def.ExecutionGate));
		return false;
	}
}

bool UTcsTriggerEvaluator::PassesGateTags(const FTcsEffectTriggerDef& Def) const
{
	const UTcsEffectSubsystem* Facade = Owner.Get();
	if (!Facade)
	{
		return false;
	}

	// 空数组 = 无开关，恒通过
	for (const FGameplayTag& GateTag : Def.GateTags)
	{
		if (!Facade->IsTriggerGateTagLit(GateTag))
		{
			return false;
		}
	}

	return true;
}

bool UTcsTriggerEvaluator::PassesConditions(const FTcsEffectTriggerDef& Def, const FTcsTriggerContext& Context)
{
	UTcsEffectSubsystem* Facade = Owner.Get();
	if (!Facade)
	{
		return false;
	}

	// 空条件数组 = 无条件通过（EvaluateTriggerConditions 的既有语义）
	return EvaluateTriggerConditions(Def.Conditions, Context, Facade->NextTriggerRandomValue());
}
