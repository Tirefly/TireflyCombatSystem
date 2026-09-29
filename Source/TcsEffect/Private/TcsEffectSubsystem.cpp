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
	// 触发行：登记表清空 + **全量退订**（不留跨世界残留订阅）；求值器随门面一起回收。
	TriggerRegistry.Reset(GetEventBus());
	TriggerEvaluator = nullptr;
	RunPool.Reset();
	ChainDefs.Empty();
	EntityQuery = TScriptInterface<ITcsEntityQuery>();

	// 宿主脚本步骤执行器：撤销**本世界**登记的动态条目（2026-09-29，DEC-04 裁定 ⑤）。
	// 注意这是**整理手段而非正确性前提**——注册表的失效判据自足（对象已被回收 / 世界不符 ⇒ 视为未命中，
	// 由 Find 负责移除并告警），此处撤销只是让进程级注册表不留本世界的残骸。
	// 键（步骤 struct 类型）与值（执行器对象）不同型，无法从对象反查键 ⇒ 直接遍历注册表的**动态**条目
	// （静态自注册项不在其中，故本循环 MUST NOT 也会移除代码登记）。
	for (const UScriptStruct* DynamicStepStruct : FTcsEffectStepExecutorRegistry::Get().GetDynamicKeys())
	{
		FTcsEffectStepExecutorRegistry::Get().Unregister(DynamicStepStruct);
	}
	RegisteredStepExecutors.Reset();

	Super::Deinitialize();
}

void UTcsEffectSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTcsEffectSubsystem* This = CastChecked<UTcsEffectSubsystem>(InThis);

	// 链定义登记表非 UPROPERTY（TUniquePtr 容器）——GC 只走 RefLink 看不见它，故在此手动补引用。
	// 链步骤可放任意宿主自定义 struct（D4-16），其中的 UPROPERTY 对象引用须靠本函数保活
	// （引擎 UDataTable::AddReferencedObjects 对 RowMap 用的同一招，见头文件说明）。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectChain>>& Pair : This->ChainDefs)
	{
		if (const FTcsEffectChain* Chain = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsEffectChain::StaticStruct(), const_cast<FTcsEffectChain*>(Chain), This);
		}
	}

	// 触发行登记表同理（非 UPROPERTY 成员；行内 FInstancedStruct 的内层可放对象引用——
	// 与链定义同款缺口，判据是"容器是否 GC 可见"而非"值语义还是指针语义"）
	This->TriggerRegistry.AddReferencedObjects(Collector, This);

	Super::AddReferencedObjects(InThis, Collector);
}



// 链定义登记表
bool UTcsEffectSubsystem::RegisterChain(const FTcsEffectChain& Chain)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(Chain.ChainId.IsValid(), TEXT("UTcsEffectSubsystem::RegisterChain: 链 id 无效——拒绝登记")))
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

bool UTcsEffectSubsystem::UnregisterChain(FGameplayTag ChainId)
{
	ensure(IsInGameThread());

	if (!ChainId.IsValid() || !ChainDefs.Contains(ChainId))
	{
		// 未登记属正常路径（重复注销/清理时序），不 ensure
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::UnregisterChain: 链 %s 未登记——忽略"), *ChainId.ToString());
		return false;
	}

	if (HasActiveRunForChain(ChainId))
	{
		// 有活动运行态 = **时序竞态**（链还在跑就要求摘定义），不是配置错误——
		// 故与"未登记"同口径用 Warning + 返回 false，不 ensure（本函数内两种拒绝面口径统一）。
		// 理由：脚本层"起一条挂起链后想注销"是完全正常的调用序列（实测先例 2026-09-24：
		// 会产出红字噪音，而红字应留给真实缺陷——见 D0-6 日志纪律）。
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::UnregisterChain: 链 %s 仍有活动运行态——拒绝注销（运行态不因定义被抽走而悬空）"),
			*ChainId.ToString());
		return false;
	}

	ChainDefs.Remove(ChainId);

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链定义注销 Chain=%s"), *ChainId.ToString());
	return true;
}

const FTcsEffectChain* UTcsEffectSubsystem::FindChain(FGameplayTag ChainId) const
{
	const TUniquePtr<FTcsEffectChain>* Found = ChainDefs.Find(ChainId);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

bool UTcsEffectSubsystem::IsChainRegistered(FGameplayTag ChainId) const
{
	return FindChain(ChainId) != nullptr;
}

bool UTcsEffectSubsystem::RegisterStepExecutor(const UScriptStruct* StepStruct, UTcsStepExecutor* Executor)
{
	ensure(IsInGameThread());

	// 拒绝面（配置错误 → ensure + false）
	if (!StepStruct)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterStepExecutor: StepStruct 为空"));
		return false;
	}
	if (!Executor)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterStepExecutor: Executor 为空（步骤类型 %s）"),
			*StepStruct->GetName());
		return false;
	}

	// 包成 TFunction 转发进既有注册表（键与查表逻辑零改动——C++ 快路径原样保留）
	// 捕获裸指针：本对象同时被下面的 UPROPERTY 数组强持有（防静默回收）**与**注册表的弱引用寿命信息
	// （防跨世界残留）——两者互补，缺一都有洞（GC 可见持有见 RegisteredStepExecutors）
	FTcsStepExecute Forwarder = [Executor](const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		// 转发到脚本/UObject 执行器（**反射分派**——脚本层可达）
		// 传运行态句柄而非上下文：上下文是非反射纯 C++ struct，不能作 UFUNCTION 形参；
		// 脚本侧需要上下文时经门面按句柄访问器取（GetRunTargets / SetRunVariable 一组）
		//
		// 直接调命名函数（**不是** `Execute_Execute`——那是 UINTERFACE 才生成的静态助手）：
		// UHT 为 UCLASS 的 BlueprintNativeEvent 生成的命名函数自带分派——非原生类走 ProcessEvent
		// （脚本覆写经此抵达）、原生类直调 `_Implementation`（虚分派抵达 C++ 覆写）。
		// 生成代码形态见 `TcsEventHandler.gen.cpp` 的 `HandleEvent`（同款 BlueprintNativeEvent）。
		return Executor->Execute(Run.ChainId, Run.Self, StepData);
	};

	// 记录寿命（对象 + 本世界）：注册表的拒绝门据此判"同世界活对象重复"——失效条目会被替换而非拒绝
	FTcsEffectStepExecutorRegistry::Get().Register(StepStruct, MoveTemp(Forwarder), Executor, GetWorld());

	// GC 可见持有（WAIT-8 教训：裸注册表持不住对象引用 ⇒ 静默回收 ⇒ 表现为"步骤不生效"而非崩溃）
	RegisteredStepExecutors.AddUnique(Executor);

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 步骤类型 %s 已登记宿主执行器（%s）"),
		*StepStruct->GetName(), *Executor->GetClass()->GetName());
	return true;
}



// 执行
FTcsChainRunHandle UTcsEffectSubsystem::ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context)
{
	ensure(IsInGameThread());

	const FTcsEffectChain* Chain = FindChain(ChainId);
	if (!Chain)
	{
		UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::ExecuteChain: 链 %s 未登记——拒绝起链"), *ChainId.ToString());
		return FTcsChainRunHandle();
	}

	FTcsChainRunHandle Handle;
	Handle.SetInner(RunPool.Allocate());

	FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
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

FTcsChainRunHandle UTcsEffectSubsystem::ExecuteChainForCaster(FGameplayTag ChainId, FTcsCombatEntityHandle Caster)
{
	// 上下文装配：Caster = Instigator = 传入句柄，Targets = 仅该句柄
	// （D4-4 v2"默认目标 = 事件目标"；脚本层入口无事件载荷，故以 Caster 为默认目标）
	FTcsEffectContext Context;
	Context.Caster = Caster;
	Context.Instigator = Caster;
	Context.Targets.Add(Caster);

	return ExecuteChain(ChainId, MoveTemp(Context));
}

bool UTcsEffectSubsystem::ResumeRun(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	// 代际校验：句柄悬空 / 运行态已释放 / 世界将拆 = 正常竞态，静默返回（不 ensure）
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 唤醒（PC=%d）"), *Run->ChainId.ToString(), Run->PC);
	}

	RunFrom(Handle);
	return true;
}

bool UTcsEffectSubsystem::IsRunActive(FTcsChainRunHandle Handle) const
{
	return RunPool.IsValid(Handle.GetInner());
}



// 运行态按句柄访问（脚本层插槽的"传句柄、不传上下文"手法，2026-09-24 台账 SCRIPT-8）
//
// **为什么需要这一组**：插槽接口的形参只能是反射类型，而 `FTcsEffectContext` 是非反射纯 C++
// struct（含 `FInstancedStruct` 事件载荷与 `TMap` 变量表）——脚本层拿不到它。故改为"传句柄 +
// 按句柄访问器读写"，**绕开上下文反射化（台账 SCRIPT-3）**。
//
// **悬空句柄语义统一**（口径同 `ResumeRun`，SCRIPT-7 先例）：代际失配/已释放是**正常时序竞态**，
// 不是配置错误 ⇒ 读口返回空值/无效句柄、写口返回 false + Warning，**一律不 ensure**。
TArray<FTcsCombatEntityHandle> UTcsEffectSubsystem::GetRunTargets(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	// 代际校验先于解析（Resolve 对悬空句柄会 ensure——本组方法按"竞态不 ensure"口径自行前置校验）
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return TArray<FTcsCombatEntityHandle>();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Targets : TArray<FTcsCombatEntityHandle>();
}

bool UTcsEffectSubsystem::SetRunTargets(FTcsChainRunHandle Handle, const TArray<FTcsCombatEntityHandle>& InTargets)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::SetRunTargets: 句柄悬空或已释放（Index=%d, Generation=%d）——拒绝写入"),
			Handle.Index, Handle.Generation);
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		Run->Context.Targets = InTargets;
		UE_LOG(LogTcsEffect, Verbose, TEXT("UTcsEffectSubsystem: 链 %s 目标集被改写为 %d 个（脚本层访问器）"),
			*Run->ChainId.ToString(), InTargets.Num());
		return true;
	}

	return false;
}

bool UTcsEffectSubsystem::TryGetRunVariable(FTcsChainRunHandle Handle, FGameplayTag Key, double& OutValue)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return false;
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	if (!Run)
	{
		return false;
	}

	// miss 时出参内容未定义（调用方不得使用——契约同 ITcsParamTableReader::TryGetNumericParam）
	if (const double* Found = Run->Context.Variables.Find(Key))
	{
		OutValue = *Found;
		return true;
	}

	return false;
}

bool UTcsEffectSubsystem::SetRunVariable(FTcsChainRunHandle Handle, FGameplayTag Key, double Value)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::SetRunVariable: 句柄悬空或已释放（Index=%d, Generation=%d）——拒绝写入"),
			Handle.Index, Handle.Generation);
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		Run->Context.Variables.Add(Key, Value);
		return true;
	}

	return false;
}

FTcsCombatEntityHandle UTcsEffectSubsystem::GetRunCaster(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return FTcsCombatEntityHandle();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Caster : FTcsCombatEntityHandle();
}

FTcsCombatEntityHandle UTcsEffectSubsystem::GetRunInstigator(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return FTcsCombatEntityHandle();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Instigator : FTcsCombatEntityHandle();
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
		FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
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
		const FTcsStepExecute* Executor = FTcsEffectStepExecutorRegistry::Get().Find(StepStruct, GetWorld());
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
		FTcsChainRun* AdvancedRun = RunPool.Resolve(Handle.GetInner());
		if (!AdvancedRun)
		{
			return;
		}
		AdvancedRun->PC = StepIndex + 1;
	}
}

void UTcsEffectSubsystem::ReleaseRun(FTcsChainRunHandle Handle)
{
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return;
	}

	FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());

	// 清黑板与唤醒锚：槽位内容不跨生命周期残留（池 Free 不清零——池零策略纪律）
	Run->ChainId = FGameplayTag();
	Run->PC = 0;
	Run->Context = FTcsEffectContext();
	Run->Owner = nullptr;
	Run->Self = FTcsChainRunHandle();
	Run->PendingExpiry = FTcsTimeEntryHandle();
	Run->bHasPendingExpiry = false;

	RunPool.Free(Handle.GetInner());
}

bool UTcsEffectSubsystem::HasActiveRunForChain(FGameplayTag ChainId)
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
