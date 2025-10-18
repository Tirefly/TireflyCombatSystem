// Copyright Tirefly. All Rights Reserved.


#include "State/TcsState.h"

#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Skill/TcsSkillComponent.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "TcsEntityInterface.h"
#include "TcsGenericMacro.h"
#include "TcsLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"



UTcsStateInstance::UTcsStateInstance()
{
}

UWorld* UTcsStateInstance::GetWorld() const
{
	if (UWorldSubsystem* WorldSubsystem = Cast<UWorldSubsystem>(GetOuter()))
	{
		return WorldSubsystem->GetWorld();
	}
	
	return nullptr;
}

void UTcsStateInstance::Initialize(
	const FTcsStateDefinition& InStateDef,
	AActor* InOwner,
	AActor* InInstigator,
	int32 InInstanceId,
	int32 InLevel)
{
	StateDef = InStateDef;
	StateInstanceId = InInstanceId;
	Level = InLevel;

	// 初始化状态Owner和其状态组件、属性组件、技能组件
	Owner = InOwner;
	if (!IsValid(InOwner) || !InOwner->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Owner is invalid"), *FString(__FUNCTION__));
		return;
	}
	OwnerStateCmp = ITcsEntityInterface::Execute_GetStateComponent(InOwner);
	OwnerAttributeCmp = ITcsEntityInterface::Execute_GetAttributeComponent(InOwner);
	OwnerSkillCmp = ITcsEntityInterface::Execute_GetSkillComponent(InOwner);
	// 初始化状态Instigator和其状态组件、属性组件、技能组件
	Instigator = InInstigator;
	if (!IsValid(InInstigator) || !InInstigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Instigator is invalid"), *FString(__FUNCTION__));
		return;
	}
	InstigatorStateCmp = ITcsEntityInterface::Execute_GetStateComponent(InInstigator);
	InstigatorAttributeCmp = ITcsEntityInterface::Execute_GetAttributeComponent(InInstigator);
	InstigatorSkillCmp = ITcsEntityInterface::Execute_GetSkillComponent(InInstigator);

	// 清理参数缓存
	NumericParameters.Reset();
	NumericParametersTag.Reset();
	BoolParameters.Reset();
	BoolParametersTag.Reset();
	VectorParameters.Reset();
	VectorParametersTag.Reset();

	// 初始化基础数值参数：持续时间
	if (StateDef.DurationType == ETcsStateDurationType::SDT_Duration)
	{
		NumericParameters.Add(Tcs_Generic_Name_TotalDuration, StateDef.Duration);
	}
	// 初始化基础数值参数：堆叠层数
	if (StateDef.MaxStackCount > 0)
	{
		NumericParameters.Add(Tcs_Generic_Name_StackCount, 1);
	}
}

void UTcsStateInstance::SetCurrentStage(ETcsStateStage InStage)
{
	Stage = InStage;
}

float UTcsStateInstance::GetDurationRemaining() const
{
	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] OwnerStateCmp is invalid"), *FString(__FUNCTION__));
		return -1.f;
	}
	
	// 从StateComponent获取剩余时间
	return OwnerStateCmp->GetStateInstanceDurationRemaining(this);
}

void UTcsStateInstance::RefreshDurationRemaining()
{
	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] OwnerStateCmp is invalid"), *FString(__FUNCTION__));
	}
	
	// 通知StateComponent刷新剩余时间
	OwnerStateCmp->RefreshStateInstanceDurationRemaining(this);
}

void UTcsStateInstance::SetDurationRemaining(float InDurationRemaining)
{
	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] OwnerStateCmp is invalid"), *FString(__FUNCTION__));
	}
	
	// 通知StateComponent设置剩余时间
	OwnerStateCmp->SetStateInstanceDurationRemaining(this, InDurationRemaining);
}

float UTcsStateInstance::GetTotalDuration() const
{
	switch (StateDef.DurationType)
	{
	default:
	case SDT_None:
		return 0.0f;
	case SDT_Infinite:
		return -1.f;
	case SDT_Duration:
		break;
	}
	
	float TotalDuration = StateDef.Duration;
	if (const float* DurationParam = NumericParameters.Find(Tcs_Generic_Name_TotalDuration))
	{
		TotalDuration = *DurationParam;
	}
	
	return TotalDuration;
}

bool UTcsStateInstance::CanStack() const
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		return false;
	}
	
	return GetStackCount() < MaxStackCount;
}

int32 UTcsStateInstance::GetStackCount() const
{
	if (const float* StackCount = NumericParameters.Find(Tcs_Generic_Name_StackCount))
	{
		return *StackCount;
	}

	return -1;
}

int32 UTcsStateInstance::GetMaxStackCount() const
{
	return StateDef.MaxStackCount;
}

void UTcsStateInstance::SetStackCount(int32 InStackCount)
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		return;
	}
	
	int32 NewStackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
	*NumericParameters.Find(Tcs_Generic_Name_StackCount) = NewStackCount;
}

void UTcsStateInstance::AddStack(int32 Count)
{
	SetStackCount(GetStackCount() + Count);
}

void UTcsStateInstance::RemoveStack(int32 Count)
{
	SetStackCount(GetStackCount() - Count);
}

void UTcsStateInstance::SetLevel(int32 InLevel)
{
	Level = InLevel;
}

bool UTcsStateInstance::GetNumericParam(FName ParameterName, float& OutValue) const
{
	if (const float* Value = NumericParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetNumericParam(FName ParameterName, float Value)
{
	NumericParameters.FindOrAdd(ParameterName) = Value;
}

bool UTcsStateInstance::GetNumericParamByTag(FGameplayTag ParameterTag, float& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const float* Value = NumericParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetNumericParamByTag(FGameplayTag ParameterTag, float Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	NumericParametersTag.FindOrAdd(ParameterTag) = Value;
}

bool UTcsStateInstance::GetBoolParam(FName ParameterName, bool& OutValue) const
{
	if (const bool* Value = BoolParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetBoolParam(FName ParameterName, bool Value)
{
	BoolParameters.FindOrAdd(ParameterName) = Value;
}

bool UTcsStateInstance::GetBoolParamByTag(FGameplayTag ParameterTag, bool& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const bool* Value = BoolParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetBoolParamByTag(FGameplayTag ParameterTag, bool Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	BoolParametersTag.FindOrAdd(ParameterTag) = Value;
}

bool UTcsStateInstance::GetVectorParam(FName ParameterName, FVector& OutValue) const
{
	if (const FVector* Value = VectorParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetVectorParam(FName ParameterName, const FVector& Value)
{
	VectorParameters.FindOrAdd(ParameterName) = Value;
}

bool UTcsStateInstance::GetVectorParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const FVector* Value = VectorParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetVectorParamByTag(FGameplayTag ParameterTag, const FVector& Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	VectorParametersTag.FindOrAdd(ParameterTag) = Value;
}

TArray<FName> UTcsStateInstance::GetAllNumericParamNames() const
{
	TArray<FName> ParamNames;
	NumericParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllNumericParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	NumericParametersTag.GetKeys(ParamTags);
	return ParamTags;
}

TArray<FName> UTcsStateInstance::GetAllBoolParamNames() const
{
	TArray<FName> ParamNames;
	BoolParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllBoolParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	BoolParametersTag.GetKeys(ParamTags);
	return ParamTags;
}

TArray<FName> UTcsStateInstance::GetAllVectorParamNames() const
{
	TArray<FName> ParamNames;
	VectorParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllVectorParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	VectorParametersTag.GetKeys(ParamTags);
	return ParamTags;
}

bool UTcsStateInstance::InitializeStateTree()
{
	if (!StateDef.StateTreeRef.IsValid())
	{
		return false;
	}

	// 重置StateTree实例数据
	StateTreeInstanceData.Reset();
	bStateTreeRunning = false;
	CurrentStateTreeStatus = EStateTreeRunStatus::Unset;

	return true;
}

void UTcsStateInstance::StartStateTree()
{
	if (bStateTreeRunning)
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetupStateTreeContext(Context))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Failed to set StateTree context for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 启动StateTree
	FStateTreeExecutionContext::FStartParameters StartParams;
	CurrentStateTreeStatus = Context.Start(StartParams);

	if (CurrentStateTreeStatus == EStateTreeRunStatus::Running)
	{
		bStateTreeRunning = true;
		UE_LOG(LogTemp, Log, TEXT("[%s] StateTree started successfully for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Failed to start StateTree for StateInstance: %s, Status: %d"), 
			*FString(__FUNCTION__),
			*GetStateDefId().ToString(),
			(int32)CurrentStateTreeStatus);
	}
}

void UTcsStateInstance::TickStateTree(float DeltaTime)
{
	if (!bStateTreeRunning)
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetupStateTreeContext(Context))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set StateTree context during tick for StateInstance: %s"), *GetStateDefId().ToString());
		StopStateTree();
		return;
	}

	// 执行Tick
	CurrentStateTreeStatus = Context.Tick(DeltaTime);

	// 检查运行状态
	switch (CurrentStateTreeStatus)
	{
	case EStateTreeRunStatus::Running:
		// 继续运行
		break;

	case EStateTreeRunStatus::Succeeded:
		UE_LOG(LogTemp, Log, TEXT("StateTree completed successfully for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;

	case EStateTreeRunStatus::Failed:
		UE_LOG(LogTemp, Warning, TEXT("StateTree failed for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;

	case EStateTreeRunStatus::Stopped:
		UE_LOG(LogTemp, Log, TEXT("StateTree was stopped for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("Unexpected StateTree status for StateInstance: %s, Status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
		break;
	}
}

void UTcsStateInstance::StopStateTree()
{
	if (!bStateTreeRunning)
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求（即使是停止也需要有效的上下文）
	if (SetupStateTreeContext(Context))
	{
		// 停止StateTree
		CurrentStateTreeStatus = Context.Stop(EStateTreeRunStatus::Stopped);
		UE_LOG(LogTemp, Log, TEXT("StateTree stopped for StateInstance: %s with status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
	}

	bStateTreeRunning = false;
}

void UTcsStateInstance::PauseStateTree()
{
	if (!StateDef.StateTreeRef.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}
	
	if (GetCurrentStage() == ETcsStateStage::SS_HangUp)
	{
		return;
	}

	if (bStateTreeRunning)
	{
		StopStateTree();
	}

	SetCurrentStage(ETcsStateStage::SS_HangUp);
}

void UTcsStateInstance::ResumeStateTree()
{
	if (GetCurrentStage() == ETcsStateStage::SS_Active && bStateTreeRunning)
	{
		return;
	}

	if (!StateDef.StateTreeRef.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	if (!bStateTreeRunning)
	{
		StartStateTree();
	}

	SetCurrentStage(ETcsStateStage::SS_Active);
}

EStateTreeRunStatus UTcsStateInstance::GetStateTreeRunStatus() const
{
	return CurrentStateTreeStatus;
}

void UTcsStateInstance::SendStateTreeEvent(FGameplayTag EventTag, const FInstancedStruct& EventPayload)
{
	if (!bStateTreeRunning)
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (SetupStateTreeContext(Context))
	{
		// 发送事件
		Context.SendEvent(EventTag, FConstStructView(EventPayload));
	}
}

bool UTcsStateInstance::SetupStateTreeContext(FStateTreeExecutionContext& Context)
{
	if (!Context.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid StateTree execution context"));
		return false;
	}

	// 设置外部数据收集回调
	Context.SetCollectExternalDataCallback(
		FOnCollectStateTreeExternalData::CreateUObject(
			this, 
			&UTcsStateInstance::CollectExternalData
		)
	);

	// 设置上下文数据
	if (AActor* OwnerActor = Owner.Get())
	{
		Context.SetContextDataByName(TEXT("Owner"), FStateTreeDataView(OwnerActor));
	}

	if (AActor* InstigatorActor = Instigator.Get())
	{
		Context.SetContextDataByName(TEXT("Instigator"), FStateTreeDataView(InstigatorActor));
	}

	Context.SetContextDataByName(TEXT("StateInstance"), FStateTreeDataView(this));

	// 验证所有上下文数据是否有效
	bool bValid = Context.AreContextDataViewsValid();
	if (!bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("StateTree context data views are not valid"));
	}

	return bValid;
}

bool UTcsStateInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context, 
	const UStateTree* StateTree, 
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, 
	TArrayView<FStateTreeDataView> OutDataViews)
{
	check(ExternalDataDescs.Num() == OutDataViews.Num());

	// 获取World引用
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get World for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return false;
	}

	// 获取Owner引用
	AActor* OwnerActor = Owner.Get();
	AActor* InstigatorActor = Instigator.Get();
	if (!OwnerActor || !InstigatorActor)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get Owner or Instigator for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return false;
	}
	if (!OwnerActor->Implements<UTcsEntityInterface>() || !InstigatorActor->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Owner or Instigator does not implement TcsEntityInterface for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return false;
	}

	// 遍历所有外部数据需求
	int32 IssuesFoundCounter = 0;
	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& DataDesc = ExternalDataDescs[Index];
		if (DataDesc.Struct != nullptr)
		{
			if (DataDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(DataDesc.Struct.Get())));
				OutDataViews[Index] = FStateTreeDataView(Subsystem);
				UE_CVLOG(Subsystem == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required subsystem %s"),
					*FString(__FUNCTION__),
					*GetNameSafe(Context.GetStateTree()),
					*GetNameSafe(DataDesc.Struct));
				IssuesFoundCounter += Subsystem != nullptr ? 0 : 1;
			}
			else if (DataDesc.Struct->IsChildOf(UGameInstanceSubsystem::StaticClass()))
			{
				UGameInstance* GameInstance = World->GetGameInstance();
				OutDataViews[Index] = FStateTreeDataView(GameInstance);
				UE_CVLOG(GameInstance == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required game instance"),
					*FString(__FUNCTION__),
					*GetNameSafe(Context.GetStateTree()));
				IssuesFoundCounter += GameInstance != nullptr ? 0 : 1;
			}
			else if (DataDesc.Struct->IsChildOf(AActor::StaticClass()))
			{
				// 根据名称提供Actor数据
				if (DataDesc.Name == TEXT("Owner") && OwnerActor)
				{
					OutDataViews[Index] = FStateTreeDataView(OwnerActor);
				}
				else if (DataDesc.Name == TEXT("Instigator") && InstigatorActor)
				{
					OutDataViews[Index] = FStateTreeDataView(InstigatorActor);
				}
			}
			else if (DataDesc.Struct->IsChildOf(UTcsStateComponent::StaticClass()))
			{
				if (DataDesc.Name == TEXT("OwnerStateCmp"))
				{
					OutDataViews[Index] = OwnerStateCmp.Get();
					UE_CVLOG(OwnerStateCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required owner state component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerStateCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TEXT("InstigatorStateCmp"))
				{
					OutDataViews[Index] = InstigatorStateCmp.Get();
					UE_CVLOG(InstigatorStateCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required instigator state component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorStateCmp != nullptr ? 0 : 1;
				}
			}
			else if (DataDesc.Struct->IsChildOf(UTcsAttributeComponent::StaticClass()))
			{
				if (DataDesc.Name == TEXT("OwnerAttributeCmp"))
				{
					OutDataViews[Index] = OwnerAttributeCmp.Get();
					UE_CVLOG(OwnerAttributeCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required owner attribute component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerAttributeCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TEXT("InstigatorAttributeCmp"))
				{
					OutDataViews[Index] = InstigatorAttributeCmp.Get();
					UE_CVLOG(InstigatorAttributeCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required instigator attribute component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorAttributeCmp != nullptr ? 0 : 1;
				}
			}
			else if (DataDesc.Struct->IsChildOf(UTcsSkillComponent::StaticClass()))
			{
				if (DataDesc.Name == TEXT("OwnerSkillCmp"))
				{
					OutDataViews[Index] = OwnerSkillCmp.Get();
					UE_CVLOG(OwnerSkillCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required owner Skill component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerSkillCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TEXT("InstigatorSkillCmp"))
				{
					OutDataViews[Index] = InstigatorSkillCmp.Get();
					UE_CVLOG(InstigatorSkillCmp == nullptr, this, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required instigator skill component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorSkillCmp != nullptr ? 0 : 1;
				}
			}
			else if (DataDesc.Struct->IsChildOf(UTcsStateInstance::StaticClass()))
			{
				OutDataViews[Index] = this;
			}
		}
	}

	return IssuesFoundCounter == 0;
}


