// Copyright Tirefly. All Rights Reserved.


#include "State/TcsState.h"

#include "State/TcsStateComponent.h"
#include "StateTree/TcsStateTreeSchema_StateInstance.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "TcsEntityInterface.h"
#include "TcsGenericMacro.h"
#include "TcsLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"


UTcsStateInstance::UTcsStateInstance()
{
}

UWorld* UTcsStateInstance::GetWorld() const
{
	// 优先从 Owner Actor 获取 World
	if (Owner.IsValid())
	{
		return Owner->GetWorld();
	}

	// 回退：尝试从 Outer 获取
	if (const AActor* OuterActor = Cast<AActor>(GetOuter()))
	{
		return OuterActor->GetWorld();
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
	OwnerController = Owner->GetInstigatorController();
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
	InstigatorController = Instigator->GetInstigatorController();
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

	// 初始化参数缓存
	InitParameterValues();
	InitParameterTagValues();
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
	return OwnerStateCmp->GetStateRemainingDuration(this);
}

void UTcsStateInstance::RefreshDurationRemaining()
{
	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] OwnerStateCmp is invalid"), *FString(__FUNCTION__));
	}
	
	// 通知StateComponent刷新剩余时间
	OwnerStateCmp->RefreshStateRemainingDuration(this);
}

void UTcsStateInstance::SetDurationRemaining(float InDurationRemaining)
{
	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] OwnerStateCmp is invalid"), *FString(__FUNCTION__));
	}
	
	// 通知StateComponent设置剩余时间
	OwnerStateCmp->SetStateRemainingDuration(this, InDurationRemaining);
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

	int32 OldStackCount = GetStackCount();
	int32 NewStackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);

	if (OldStackCount == NewStackCount)
	{
		return;
	}

	*NumericParameters.Find(Tcs_Generic_Name_StackCount) = NewStackCount;

	// 通知状态组件叠层变化
	if (OwnerStateCmp.IsValid())
	{
		OwnerStateCmp->NotifyStateStackChanged(this, OldStackCount, NewStackCount);
	}
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
	if (Level == InLevel)
	{
		return;
	}

	int32 OldLevel = Level;
	Level = InLevel;

	// 通知状态组件等级变化
	if (OwnerStateCmp.IsValid())
	{
		OwnerStateCmp->NotifyStateLevelChanged(this, OldLevel, InLevel);
	}
}

void UTcsStateInstance::InitParameterValues()
{
	if (StateDef.Parameters.IsEmpty())
	{
		return;
	}

	for (const TPair<FName, FTcsStateParameter>& ParamPair : StateDef.Parameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!ParamPair.Value.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] NumericParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				float ParamValue;
				auto ParamEvaluator = ParamPair.Value.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's numeric parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}
				
				SetNumericParam(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!ParamPair.Value.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] BoolParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = ParamPair.Value.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's  bool parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetBoolParam(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!ParamPair.Value.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] VectorParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = ParamPair.Value.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's vector parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetVectorParam(ParamPair.Key, ParamValue);
				break;
			}
		}
	}
}

void UTcsStateInstance::InitParameterTagValues()
{
	if (StateDef.TagParameters.IsEmpty())
	{
		return;
	}

	for (const TPair<FGameplayTag, FTcsStateParameter>& ParamPair : StateDef.TagParameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!ParamPair.Value.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] NumericParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				float ParamValue;
				auto ParamEvaluator = ParamPair.Value.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's numeric parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}
				
				SetNumericParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!ParamPair.Value.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] BoolParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = ParamPair.Value.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's  bool parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetBoolParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!ParamPair.Value.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] VectorParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = ParamPair.Value.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's vector parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetVectorParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		}
	}
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
	float* ExistingValue = NumericParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	NumericParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知（排除初始化阶段的大量调用）
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(this, ParameterName, ETcsStateParameterType::SPT_Numeric);
	}
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
	bool* ExistingValue = BoolParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	BoolParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(this, ParameterName, ETcsStateParameterType::SPT_Bool);
	}
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
	FVector* ExistingValue = VectorParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	VectorParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(this, ParameterName, ETcsStateParameterType::SPT_Vector);
	}
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
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateTreeRef is invalid of State %s"),
			*FString(__FUNCTION__),
			*StateDefId.ToString());
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
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetContextRequirements(Context))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to set StateTree context for StateInstance: %s"),
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
		UE_LOG(LogTcsStateTree, Log, TEXT("[%s] StateTree started successfully for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
	}
	else
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to start StateTree for StateInstance: %s, Status: %d"), 
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
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetContextRequirements(Context))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("Failed to set StateTree context during tick for StateInstance: %s"), *GetStateDefId().ToString());
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
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree completed successfully for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	case EStateTreeRunStatus::Failed:
		UE_LOG(LogTcsStateTree, Warning, TEXT("StateTree failed for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	case EStateTreeRunStatus::Stopped:
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree was stopped for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	default:
		UE_LOG(LogTcsStateTree, Warning, TEXT("Unexpected StateTree status for StateInstance: %s, Status: %d"), 
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
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求（即使是停止也需要有效的上下文）
	if (SetContextRequirements(Context))
	{
		// 停止StateTree
		CurrentStateTreeStatus = Context.Stop(EStateTreeRunStatus::Stopped);
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree stopped for StateInstance: %s with status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
	}

	bStateTreeRunning = false;
}

void UTcsStateInstance::PauseStateTree()
{
	if (!StateDef.StateTreeRef.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 如果已经是暂停状态,不重复暂停
	if (GetCurrentStage() == ETcsStateStage::SS_Pause)
	{
		return;
	}

	if (bStateTreeRunning)
	{
		StopStateTree();
	}

	// 设置为完全暂停阶段
	SetCurrentStage(ETcsStateStage::SS_Pause);
}

void UTcsStateInstance::ResumeStateTree()
{
	// 如果已经是激活状态且运行中,不需要恢复
	if (GetCurrentStage() == ETcsStateStage::SS_Active && bStateTreeRunning)
	{
		return;
	}

	// 只有从暂停或挂起状态才能恢复
	ETcsStateStage CurrentStage = GetCurrentStage();
	if (CurrentStage != ETcsStateStage::SS_Pause && CurrentStage != ETcsStateStage::SS_HangUp)
	{
		UE_LOG(LogTcsStateTree, Warning, TEXT("[%s] Cannot resume StateTree for StateInstance: %s, current stage is %d"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString(),
			static_cast<int32>(CurrentStage));
		return;
	}

	if (!StateDef.StateTreeRef.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
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
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (SetContextRequirements(Context))
	{
		// 发送事件
		Context.SendEvent(EventTag, FConstStructView(EventPayload));
	}
}

bool UTcsStateInstance::SetContextRequirements(FStateTreeExecutionContext& Context)
{
	if (!Context.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("Invalid StateTree execution context"));
		return false;
	}

	// 设置外部数据收集回调
	Context.SetCollectExternalDataCallback(
		FOnCollectStateTreeExternalData::CreateUObject(
			this, 
			&UTcsStateInstance::CollectExternalData
		)
	);
	
	return UTcsStateTreeSchema_StateInstance::SetContextRequirements(*this, Context);
}

bool UTcsStateInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context, 
	const UStateTree* StateTree, 
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, 
	TArrayView<FStateTreeDataView> OutDataViews)
{
	return UTcsStateTreeSchema_StateInstance::CollectExternalData(
		Context,
		StateTree,
		this,
		ExternalDataDescs,
		OutDataViews);
}


