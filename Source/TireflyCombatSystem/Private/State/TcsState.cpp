// Copyright Tirefly. All Rights Reserved.


#include "State/TcsState.h"

#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "StateTree/TcsCombatStateTreeSchema.h"
#include "StateTree.h"
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
	
	Owner = InOwner;
	Instigator = InInstigator;

	// 清理参数缓存
	NumericParameters.Reset();
	NumericParametersByTag.Reset();
	BoolParameters.Reset();
	BoolParametersByTag.Reset();
	VectorParameters.Reset();
	VectorParametersByTag.Reset();

	// 初始化基础数值参数
	NumericParameters.Add("MaxStackCount", StateDef.MaxStackCount);
	NumericParameters.Add("TotalDuration", StateDef.Duration);
}

void UTcsStateInstance::SetCurrentStage(ETcsStateStage InStage)
{
	Stage = InStage;
}

bool UTcsStateInstance::CanStack() const
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		return false;
	}
	
	return StackCount < MaxStackCount;
}

float UTcsStateInstance::GetDurationRemaining() const
{
	// 从TireflyStateComponent获取剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			return StateComponent->GetStateInstanceDurationRemaining(const_cast<UTcsStateInstance*>(this));
		}
	}
	
	return 0.0f;
}

void UTcsStateInstance::RefreshDurationRemaining()
{
	// 通知TireflyStateComponent刷新剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			StateComponent->RefreshStateInstanceDurationRemaining(this);
		}
	}
}

void UTcsStateInstance::SetDurationRemaining(float InDurationRemaining)
{
	// 通知TireflyStateComponent设置剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			StateComponent->SetStateInstanceDurationRemaining(this, InDurationRemaining);
		}
	}
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
	if (const float* DurationParam = NumericParameters.Find("TotalDuration"))
	{
		TotalDuration = FMath::Max(TotalDuration, *DurationParam);
	}
	
	return TotalDuration;
}

int32 UTcsStateInstance::GetMaxStackCount() const
{
	int32 MaxStackCount = StateDef.MaxStackCount;
	if (const float* MaxStackParam = NumericParameters.Find("MaxStackCount"))
	{
		MaxStackCount = static_cast<int32>(*MaxStackParam);
	}

	return MaxStackCount;
}

void UTcsStateInstance::SetStackCount(int32 InStackCount)
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		StackCount = 0;
		return;
	}
	
	StackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
}

void UTcsStateInstance::AddStack(int32 Count)
{
	SetStackCount(StackCount + Count);
}

void UTcsStateInstance::RemoveStack(int32 Count)
{
	SetStackCount(StackCount - Count);
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

	if (const float* Value = NumericParametersByTag.Find(ParameterTag))
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

	NumericParametersByTag.FindOrAdd(ParameterTag) = Value;
}

#pragma region Parameters Extended Implementation

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

	if (const bool* Value = BoolParametersByTag.Find(ParameterTag))
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

	BoolParametersByTag.FindOrAdd(ParameterTag) = Value;
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

	if (const FVector* Value = VectorParametersByTag.Find(ParameterTag))
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

	VectorParametersByTag.FindOrAdd(ParameterTag) = Value;
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
	NumericParametersByTag.GetKeys(ParamTags);
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
	BoolParametersByTag.GetKeys(ParamTags);
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
	VectorParametersByTag.GetKeys(ParamTags);
	return ParamTags;
}

#pragma endregion

#pragma region StateTree Implementation

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
	if (!StateDef.StateTreeRef.IsValid() || bStateTreeRunning)
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetupStateTreeContext(Context))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set StateTree context for StateInstance: %s"), *GetStateDefId().ToString());
		return;
	}

	// 启动StateTree
	FStateTreeExecutionContext::FStartParameters StartParams;
	CurrentStateTreeStatus = Context.Start(StartParams);

	if (CurrentStateTreeStatus == EStateTreeRunStatus::Running)
	{
		bStateTreeRunning = true;
		UE_LOG(LogTemp, Log, TEXT("StateTree started successfully for StateInstance: %s"), *GetStateDefId().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to start StateTree for StateInstance: %s, Status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
	}
}

void UTcsStateInstance::TickStateTree(float DeltaTime)
{
	if (!bStateTreeRunning || !StateDef.StateTreeRef.IsValid())
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
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
	if (!bStateTreeRunning || !StateDef.StateTreeRef.IsValid())
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
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
	if (!bStateTreeRunning || !StateDef.StateTreeRef.IsValid())
	{
		return;
	}

	const UStateTree* StateTree = StateDef.StateTreeRef.GetStateTree();
	if (!StateTree)
	{
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
		return false;
	}

	// 获取Owner引用
	AActor* OwnerActor = Owner.Get();
	AActor* InstigatorActor = Instigator.Get();

	// 遍历所有外部数据需求
	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
		FStateTreeDataView& DataView = OutDataViews[Index];

		// 根据数据类型提供相应的数据
		if (Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			// 提供World子系统
			UWorldSubsystem* Subsystem = World->GetSubsystemBase(
				Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get()))
			);
			DataView = FStateTreeDataView(Subsystem);
		}
		else if (Desc.Struct->IsChildOf(UGameInstanceSubsystem::StaticClass()))
		{
			// 提供GameInstance子系统
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				UGameInstanceSubsystem* Subsystem = GameInstance->GetSubsystemBase(
					Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get()))
				);
				DataView = FStateTreeDataView(Subsystem);
			}
		}
		else if (Desc.Struct->IsChildOf(AActor::StaticClass()))
		{
			// 根据名称提供Actor数据
			if (Desc.Name == TEXT("Owner") && OwnerActor)
			{
				DataView = FStateTreeDataView(OwnerActor);
			}
			else if (Desc.Name == TEXT("Instigator") && InstigatorActor)
			{
				DataView = FStateTreeDataView(InstigatorActor);
			}
		}
		else if (Desc.Struct->IsChildOf(UTcsStateComponent::StaticClass()))
		{
			// 提供StateComponent
			if (OwnerActor)
			{
				if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
				{
					DataView = FStateTreeDataView(StateComponent);
				}
			}
		}
		else if (Desc.Struct->IsChildOf(UTcsAttributeComponent::StaticClass()))
		{
			// 提供AttributeComponent
			if (OwnerActor)
			{
				if (UTcsAttributeComponent* AttributeComponent = OwnerActor->FindComponentByClass<UTcsAttributeComponent>())
				{
					DataView = FStateTreeDataView(AttributeComponent);
				}
			}
		}
		else if (Desc.Struct->IsChildOf(UTcsStateInstance::StaticClass()))
		{
			// 提供StateInstance本身
			DataView = FStateTreeDataView(this);
		}

		// 如果没有找到匹配的数据，记录警告
		if (!DataView.IsValid())
		{
			UE_LOG(LogTemp, Warning, 
				TEXT("Could not provide external data for type: %s"), 
				*Desc.Struct->GetName());
		}
	}

	return true;
}

#pragma endregion
