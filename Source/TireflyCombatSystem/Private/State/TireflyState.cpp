// Copyright Tirefly. All Rights Reserved.


#include "State/TireflyState.h"

#include "State/TireflyStateComponent.h"
#include "Attribute/TireflyAttributeComponent.h"
#include "StateTree/TireflyCombatStateTreeSchema.h"
#include "StateTree.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"


UTireflyStateInstance::UTireflyStateInstance()
{
}

UWorld* UTireflyStateInstance::GetWorld() const
{
	if (UWorldSubsystem* WorldSubsystem = Cast<UWorldSubsystem>(GetOuter()))
	{
		return WorldSubsystem->GetWorld();
	}
	
	return nullptr;
}

void UTireflyStateInstance::Initialize(
	const FTireflyStateDefinition& InStateDef,
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

	// 初始化基础数值参数
	NumericParameters.Add("MaxStackCount", StateDef.MaxStackCount);
	NumericParameters.Add("TotalDuration", StateDef.Duration);
}

void UTireflyStateInstance::SetCurrentStage(ETireflyStateStage InStage)
{
	Stage = InStage;
}

bool UTireflyStateInstance::CanStack() const
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		return false;
	}
	
	return StackCount < MaxStackCount;
}

float UTireflyStateInstance::GetDurationRemaining() const
{
	// 从TireflyStateComponent获取剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			return StateComponent->GetStateInstanceDurationRemaining(const_cast<UTireflyStateInstance*>(this));
		}
	}
	
	return 0.0f;
}

void UTireflyStateInstance::RefreshDurationRemaining()
{
	// 通知TireflyStateComponent刷新剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			StateComponent->RefreshStateInstanceDurationRemaining(this);
		}
	}
}

void UTireflyStateInstance::SetDurationRemaining(float InDurationRemaining)
{
	// 通知TireflyStateComponent设置剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			StateComponent->SetStateInstanceDurationRemaining(this, InDurationRemaining);
		}
	}
}

float UTireflyStateInstance::GetTotalDuration() const
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

int32 UTireflyStateInstance::GetMaxStackCount() const
{
	int32 MaxStackCount = StateDef.MaxStackCount;
	if (const float* MaxStackParam = NumericParameters.Find("MaxStackCount"))
	{
		MaxStackCount = static_cast<int32>(*MaxStackParam);
	}

	return MaxStackCount;
}

void UTireflyStateInstance::SetStackCount(int32 InStackCount)
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		StackCount = 0;
		return;
	}
	
	StackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
}

void UTireflyStateInstance::AddStack(int32 Count)
{
	SetStackCount(StackCount + Count);
}

void UTireflyStateInstance::RemoveStack(int32 Count)
{
	SetStackCount(StackCount - Count);
}

void UTireflyStateInstance::SetLevel(int32 InLevel)
{
	Level = InLevel;
}

float UTireflyStateInstance::GetParamValue(FName ParameterName) const
{
	if (const float* Value = NumericParameters.Find(ParameterName))
	{
		return *Value;
	}
	return 0.0f;
}

void UTireflyStateInstance::SetParamValue(FName ParameterName, float Value)
{
	NumericParameters.FindOrAdd(ParameterName) = Value;
}

#pragma region Parameters Extended Implementation

bool UTireflyStateInstance::GetBoolParam(FName ParameterName, bool DefaultValue) const
{
	if (const bool* Value = BoolParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTireflyStateInstance::SetBoolParam(FName ParameterName, bool Value)
{
	BoolParameters.FindOrAdd(ParameterName) = Value;
}

FVector UTireflyStateInstance::GetVectorParam(FName ParameterName, const FVector& DefaultValue) const
{
	if (const FVector* Value = VectorParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTireflyStateInstance::SetVectorParam(FName ParameterName, const FVector& Value)
{
	VectorParameters.FindOrAdd(ParameterName) = Value;
}

bool UTireflyStateInstance::HasNumericParam(FName ParameterName) const
{
	return NumericParameters.Contains(ParameterName);
}

bool UTireflyStateInstance::HasBoolParam(FName ParameterName) const
{
	return BoolParameters.Contains(ParameterName);
}

bool UTireflyStateInstance::HasVectorParam(FName ParameterName) const
{
	return VectorParameters.Contains(ParameterName);
}

TArray<FName> UTireflyStateInstance::GetAllNumericParamNames() const
{
	TArray<FName> ParamNames;
	NumericParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FName> UTireflyStateInstance::GetAllBoolParamNames() const
{
	TArray<FName> ParamNames;
	BoolParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FName> UTireflyStateInstance::GetAllVectorParamNames() const
{
	TArray<FName> ParamNames;
	VectorParameters.GetKeys(ParamNames);
	return ParamNames;
}

#pragma endregion

#pragma region StateTree Implementation

bool UTireflyStateInstance::InitializeStateTree()
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

void UTireflyStateInstance::StartStateTree()
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

void UTireflyStateInstance::TickStateTree(float DeltaTime)
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

void UTireflyStateInstance::StopStateTree()
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

EStateTreeRunStatus UTireflyStateInstance::GetStateTreeRunStatus() const
{
	return CurrentStateTreeStatus;
}

void UTireflyStateInstance::SendStateTreeEvent(FGameplayTag EventTag, const FInstancedStruct& EventPayload)
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
		Context.SendEvent(EventTag, FConstStructView::Make(EventPayload));
	}
}

bool UTireflyStateInstance::SetupStateTreeContext(FStateTreeExecutionContext& Context)
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
			&UTireflyStateInstance::CollectExternalData
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

bool UTireflyStateInstance::CollectExternalData(
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
		else if (Desc.Struct->IsChildOf(UTireflyStateComponent::StaticClass()))
		{
			// 提供StateComponent
			if (OwnerActor)
			{
				if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
				{
					DataView = FStateTreeDataView(StateComponent);
				}
			}
		}
		else if (Desc.Struct->IsChildOf(UTireflyAttributeComponent::StaticClass()))
		{
			// 提供AttributeComponent
			if (OwnerActor)
			{
				if (UTireflyAttributeComponent* AttributeComponent = OwnerActor->FindComponentByClass<UTireflyAttributeComponent>())
				{
					DataView = FStateTreeDataView(AttributeComponent);
				}
			}
		}
		else if (Desc.Struct->IsChildOf(UTireflyStateInstance::StaticClass()))
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
