// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectLogChannel.h"



// 世界类型过滤
bool UTcsEffectSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐时钟/总线/属性门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsEffectSubsystem::Deinitialize()
{
	// 确定性清理：运行态池整体重置（占用统计回落、旧句柄凭代际失配失效）+ 链定义表与注入引用清空。
	// 已在到期堆里的挂起条目不再撤销——到期时按句柄代际校验静默丢弃（条目由堆在回调后回收，无泄漏）。
	RunPool.Reset();
	ChainDefs.Empty();
	EntityQuery = TScriptInterface<ITcsEntityQuery>();

	Super::Deinitialize();
}



// 链定义登记表
bool UTcsEffectSubsystem::RegisterChain(const FTcsEffectChain& Chain)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(!Chain.ChainId.IsNone(), TEXT("UTcsEffectSubsystem::RegisterChain: 链 id 为空——拒绝登记")))
	{
		return false;
	}

	if (ChainDefs.Contains(Chain.ChainId))
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterChain: 链 %s 已登记——拒绝重复登记（不得静默覆写）"),
			*Chain.ChainId.ToString());
		return false;
	}

	ChainDefs.Add(Chain.ChainId, MakeUnique<FTcsEffectChain>(Chain));

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链定义登记 Chain=%s 步数=%d 单次上限=%d"),
		*Chain.ChainId.ToString(), Chain.Steps.Num(), Chain.MaxStepsPerFrame);
	return true;
}

bool UTcsEffectSubsystem::UnregisterChain(FName ChainId)
{
	ensure(IsInGameThread());

	if (ChainId.IsNone() || !ChainDefs.Contains(ChainId))
	{
		// 未登记属正常路径（重复注销/清理时序），不 ensure
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::UnregisterChain: 链 %s 未登记——忽略"), *ChainId.ToString());
		return false;
	}

	if (HasActiveRunForChain(ChainId))
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::UnregisterChain: 链 %s 仍有活动运行态——拒绝注销（运行态不因定义被抽走而悬空）"),
			*ChainId.ToString());
		return false;
	}

	ChainDefs.Remove(ChainId);

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链定义注销 Chain=%s"), *ChainId.ToString());
	return true;
}

const FTcsEffectChain* UTcsEffectSubsystem::FindChain(FName ChainId) const
{
	const TUniquePtr<FTcsEffectChain>* Found = ChainDefs.Find(ChainId);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}



// 执行
FTcsChainRunHandle UTcsEffectSubsystem::ExecuteChain(FName ChainId, FTcsEffectContext Context)
{
	ensure(IsInGameThread());

	const FTcsEffectChain* Chain = FindChain(ChainId);
	if (!Chain)
	{
		UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::ExecuteChain: 链 %s 未登记——拒绝起链"), *ChainId.ToString());
		return FTcsChainRunHandle();
	}

	FTcsChainRunHandle Handle;
	Handle.Inner = RunPool.Allocate();

	FTcsChainRun* Run = RunPool.Resolve(Handle.Inner);
	Run->ChainId = ChainId;
	Run->PC = 0;
	Run->Context = MoveTemp(Context);
	Run->Owner = this;
	Run->Self = Handle;
	Run->PendingExpiry = FTcsTimeEntryHandle();
	Run->bHasPendingExpiry = false;

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 起（步数=%d 单次上限=%d）"),
		*ChainId.ToString(), Chain->Steps.Num(), Chain->MaxStepsPerFrame);

	RunFrom(Handle);

	// 全即时链在返回前即走完（运行态已释放）——句柄活性由 IsRunActive 判定
	return Handle;
}

bool UTcsEffectSubsystem::ResumeRun(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	// 代际校验：句柄悬空 / 运行态已释放 / 世界将拆 = 正常竞态，静默返回（不 ensure）
	if (!RunPool.IsValid(Handle.Inner))
	{
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.Inner))
	{
		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 唤醒（PC=%d）"), *Run->ChainId.ToString(), Run->PC);
	}

	RunFrom(Handle);
	return true;
}

bool UTcsEffectSubsystem::IsRunActive(FTcsChainRunHandle Handle) const
{
	return RunPool.IsValid(Handle.Inner);
}



// 宿主能力注入
void UTcsEffectSubsystem::SetEntityQuery(const TScriptInterface<ITcsEntityQuery>& InEntityQuery)
{
	ensure(IsInGameThread());

	EntityQuery = InEntityQuery;
}

ITcsEntityQuery* UTcsEffectSubsystem::GetEntityQuery() const
{
	return EntityQuery.GetInterface();
}



// 内核
void UTcsEffectSubsystem::RunFrom(FTcsChainRunHandle Handle)
{
	// 单次进入的步数预算：按链定义取（熔断护栏——自激/循环链不得无限占用本帧）
	int32 StepsThisEntry = 0;

	while (true)
	{
		// 池元素住连续缓冲、扩容即搬移（引擎事实 2026-09-17）——每步入器前按句柄重解析
		FTcsChainRun* Run = RunPool.Resolve(Handle.Inner);
		if (!Run)
		{
			return;
		}

		const FTcsEffectChain* Chain = FindChain(Run->ChainId);
		if (!Chain)
		{
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 定义不可解析——运行态释放"),
				*Run->ChainId.ToString());
			ReleaseRun(Handle);
			return;
		}

		// 走完：正常路径（零诊断噪音）
		if (Run->PC >= Chain->Steps.Num())
		{
			UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 完成（共 %d 步）"),
				*Chain->ChainId.ToString(), Chain->Steps.Num());
			ReleaseRun(Handle);
			return;
		}

		if (StepsThisEntry >= FMath::Max(1, Chain->MaxStepsPerFrame))
		{
			ensureMsgf(false, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 单次进入步数超上限（%d）——熔断断链"),
				*Chain->ChainId.ToString(), Chain->MaxStepsPerFrame);
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 单次进入步数超上限（%d）——断链"),
				*Chain->ChainId.ToString(), Chain->MaxStepsPerFrame);
			ReleaseRun(Handle);
			return;
		}

		const FInstancedStruct& Step = Chain->Steps[Run->PC];
		const UScriptStruct* StepStruct = Step.GetScriptStruct();
		const FTcsStepExecute* Executor = FTcsEffectStepExecutorRegistry::Get().Find(StepStruct);
		if (!Executor)
		{
			// 未知步骤类型：断链（不静默跳过——静默会让"配置写错"表现成"效果没生效"）
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 第 %d 步类型 %s 无执行器——断链"),
				*Chain->ChainId.ToString(), Run->PC, StepStruct ? *StepStruct->GetName() : TEXT("<空>"));
			ReleaseRun(Handle);
			return;
		}

		const int32 StepIndex = Run->PC;
		const ETcsStepResult Result = (*Executor)(Step, Run->Context, *Run);
		++StepsThisEntry;

		if (Result == ETcsStepResult::TSR_Running)
		{
			// 步内挂起：PC 停驻原步、运行态保持活动——等唤醒源按句柄重入（零每帧成本）
			UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 步 %d 挂起（PC 停驻）"),
				*Chain->ChainId.ToString(), StepIndex);
			return;
		}

		// 步骤执行期间可能新增运行态（池扩容即搬移）——推进 PC 前重新解析
		FTcsChainRun* AdvancedRun = RunPool.Resolve(Handle.Inner);
		if (!AdvancedRun)
		{
			return;
		}
		AdvancedRun->PC = StepIndex + 1;
	}
}

void UTcsEffectSubsystem::ReleaseRun(FTcsChainRunHandle Handle)
{
	if (!RunPool.IsValid(Handle.Inner))
	{
		return;
	}

	FTcsChainRun* Run = RunPool.Resolve(Handle.Inner);

	// 清黑板与唤醒锚：槽位内容不跨生命周期残留（池 Free 不清零——池零策略纪律）
	Run->ChainId = NAME_None;
	Run->PC = 0;
	Run->Context = FTcsEffectContext();
	Run->Owner = nullptr;
	Run->Self = FTcsChainRunHandle();
	Run->PendingExpiry = FTcsTimeEntryHandle();
	Run->bHasPendingExpiry = false;

	RunPool.Free(Handle.Inner);
}

bool UTcsEffectSubsystem::HasActiveRunForChain(FName ChainId)
{
	bool bFound = false;
	RunPool.ForEach([ChainId, &bFound](FTcsChainRun& Run)
	{
		if (Run.ChainId == ChainId)
		{
			bFound = true;
		}
	});
	return bFound;
}
