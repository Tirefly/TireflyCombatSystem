// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillInstance.h"
#include "Skill/TcsSkillComponent.h"
#include "State/StateParameter/TcsStateParameter.h"
#include "TcsCombatSystemLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "State/TcsStateComponent.h"

UTcsSkillInstance::UTcsSkillInstance()
{
	CurrentLevel = 1;
}

UWorld* UTcsSkillInstance::GetWorld() const
{
	if (AActor* OwnerActor = Owner.Get())
	{
		return OwnerActor->GetWorld();
	}

	return nullptr;
}

#pragma region Initialization Implementation

void UTcsSkillInstance::Initialize(const FTcsStateDefinition& InSkillDef, FName InSkillDefId, AActor* InOwner, int32 InInitialLevel)
{
	SkillDef = InSkillDef;
	SkillDefId = InSkillDefId;
	Owner = InOwner;
	CurrentLevel = FMath::Max(1, InInitialLevel);

	// 清理现有参数
	BoolParameters.Empty();
	BoolParametersByTag.Empty();
	VectorParameters.Empty();
	VectorParametersByTag.Empty();
	NumericParameterConfigs.Empty();
	NumericParameterTagConfigs.Empty();
	CachedSnapshotParameters.Empty();
	CachedSnapshotParametersByTag.Empty();
	LastRealTimeParameterValues.Empty();
	LastRealTimeParameterValuesByTag.Empty();
	ParameterUpdateTimestamps.Empty();
	ParameterUpdateTimestampsByTag.Empty();

	// 初始化参数配置
	for (const auto& ParamPair : SkillDef.Parameters)
	{
		const FName& ParamName = ParamPair.Key;
		const FTcsStateParameter& ParamConfig = ParamPair.Value;

		switch (ParamConfig.ParameterType)
		{
		case ETcsStateParameterType::SPT_Bool:
			{
				// 从ParamValueContainer提取bool值
				// TODO: 修复FInstancedStruct对bool类型的支持问题
				// if (const bool* BoolValue = ParamConfig.ParamValueContainer.GetPtr<bool>())
				// {
				// 	BoolParameters.Add(ParamName, *BoolValue);
				// }
				// else
				{
					BoolParameters.Add(ParamName, false); // 默认值
				}
			}
			break;

		case ETcsStateParameterType::SPT_Vector:
			{
				// 从ParamValueContainer提取Vector值
				if (const FVector* VectorValue = ParamConfig.ParamValueContainer.GetPtr<FVector>())
				{
					VectorParameters.Add(ParamName, *VectorValue);
				}
				else
				{
					VectorParameters.Add(ParamName, FVector::ZeroVector);
				}
			}
			break;

		case ETcsStateParameterType::SPT_Numeric:
			{
				// 存储数值参数的计算配置
				NumericParameterConfigs.Add(ParamName, ParamConfig);
			}
			break;
		}
	}

	for (const auto& ParamPair : SkillDef.TagParameters)
	{
		const FGameplayTag& ParamTag = ParamPair.Key;
		if (!ParamTag.IsValid())
		{
			continue;
		}

		const FTcsStateParameter& ParamConfig = ParamPair.Value;

		switch (ParamConfig.ParameterType)
		{
		case ETcsStateParameterType::SPT_Bool:
			{
				BoolParametersByTag.Add(ParamTag, false);
			}
			break;

		case ETcsStateParameterType::SPT_Vector:
			{
				if (const FVector* VectorValue = ParamConfig.ParamValueContainer.GetPtr<FVector>())
				{
					VectorParametersByTag.Add(ParamTag, *VectorValue);
				}
				else
				{
					VectorParametersByTag.Add(ParamTag, FVector::ZeroVector);
				}
			}
			break;

		case ETcsStateParameterType::SPT_Numeric:
			{
				NumericParameterTagConfigs.Add(ParamTag, ParamConfig);
			}
			break;
		}
	}

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Skill instance initialized: %s (Level: %d)"), 
		*InOwner->GetName(), *SkillDefId.ToString(), CurrentLevel);
}

#pragma endregion

#pragma region DynamicProperties Implementation

void UTcsSkillInstance::SetCurrentLevel(int32 InLevel)
{
	int32 OldLevel = CurrentLevel;
	CurrentLevel = FMath::Max(1, InLevel);

	if (CurrentLevel != OldLevel)
	{
		// 等级变化时清除快照，强制重新计算
		ClearParameterSnapshot();
		BroadcastLevelChanged(CurrentLevel);

		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Skill level changed: %s %d -> %d"), 
			*Owner->GetName(), *SkillDefId.ToString(), OldLevel, CurrentLevel);
	}
}

bool UTcsSkillInstance::UpgradeLevel(int32 LevelIncrease)
{
	if (LevelIncrease <= 0)
	{
		return false;
	}

	SetCurrentLevel(CurrentLevel + LevelIncrease);
	return true;
}

#pragma endregion

#pragma region Parameters Implementation

bool UTcsSkillInstance::GetBoolParameter(FName ParamName, bool DefaultValue) const
{
	if (const bool* Value = BoolParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTcsSkillInstance::SetBoolParameter(FName ParamName, bool Value)
{
	bool OldValue = GetBoolParameter(ParamName, false);
	BoolParameters.FindOrAdd(ParamName) = Value;

	if (Value != OldValue)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Bool parameter changed: %s.%s = %s"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), Value ? TEXT("true") : TEXT("false"));
	}
}

bool UTcsSkillInstance::GetBoolParameterByTag(FGameplayTag ParamTag, bool DefaultValue) const
{
	if (!ParamTag.IsValid())
	{
		return DefaultValue;
	}

	if (const bool* Value = BoolParametersByTag.Find(ParamTag))
	{
		return *Value;
	}

	return DefaultValue;
}

void UTcsSkillInstance::SetBoolParameterByTag(FGameplayTag ParamTag, bool Value)
{
	if (!ParamTag.IsValid())
	{
		return;
	}

	const bool OldValue = GetBoolParameterByTag(ParamTag, false);
	BoolParametersByTag.FindOrAdd(ParamTag) = Value;

	if (Value != OldValue)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Bool parameter(tag) changed: %s.%s = %s"),
			*Owner->GetName(), *SkillDefId.ToString(), *ParamTag.ToString(), Value ? TEXT("true") : TEXT("false"));
	}
}

FVector UTcsSkillInstance::GetVectorParameter(FName ParamName, const FVector& DefaultValue) const
{
	if (const FVector* Value = VectorParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTcsSkillInstance::SetVectorParameter(FName ParamName, const FVector& Value)
{
	FVector OldValue = GetVectorParameter(ParamName, FVector::ZeroVector);
	VectorParameters.FindOrAdd(ParamName) = Value;

	if (!Value.Equals(OldValue))
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Vector parameter changed: %s.%s = %s"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), *Value.ToString());
	}
}

FVector UTcsSkillInstance::GetVectorParameterByTag(FGameplayTag ParamTag, const FVector& DefaultValue) const
{
	if (!ParamTag.IsValid())
	{
		return DefaultValue;
	}

	if (const FVector* Value = VectorParametersByTag.Find(ParamTag))
	{
		return *Value;
	}

	return DefaultValue;
}

void UTcsSkillInstance::SetVectorParameterByTag(FGameplayTag ParamTag, const FVector& Value)
{
	if (!ParamTag.IsValid())
	{
		return;
	}

	const FVector OldValue = GetVectorParameterByTag(ParamTag, FVector::ZeroVector);
	VectorParametersByTag.FindOrAdd(ParamTag) = Value;

	if (!Value.Equals(OldValue))
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Vector parameter(tag) changed: %s.%s = %s"),
			*Owner->GetName(), *SkillDefId.ToString(), *ParamTag.ToString(), *Value.ToString());
	}
}

float UTcsSkillInstance::CalculateNumericParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	if (ParamName.IsNone())
	{
		return 0.0f;
	}

	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	const float BaseValue = CalculateBaseNumericParameter(ParamName, Instigator, Target);
	const float FinalValue = ApplySkillModifiers(ParamName, BaseValue);

	return FinalValue;
}

float UTcsSkillInstance::CalculateNumericParameterByTag(FGameplayTag ParamTag, AActor* Instigator, AActor* Target) const
{
	if (!ParamTag.IsValid())
	{
		return 0.0f;
	}

	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	const float BaseValue = CalculateBaseNumericParameterByTag(ParamTag, Instigator, Target);
	const float FinalValue = ApplySkillModifiersByTag(ParamTag, BaseValue);

	return FinalValue;
}

void UTcsSkillInstance::TakeParameterSnapshot(AActor* Instigator, AActor* Target)
{
	CachedSnapshotParameters.Empty();
	CachedSnapshotParametersByTag.Empty();

	for (const auto& ParamConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = ParamConfigPair.Key;
		const FTcsStateParameter& ParamConfig = ParamConfigPair.Value;

		if (ParamConfig.bIsSnapshot)
		{
			const float SnapshotValue = CalculateNumericParameter(ParamName, Instigator, Target);
			CachedSnapshotParameters.Add(ParamName, SnapshotValue);
		}
	}

	for (const auto& ParamConfigPair : NumericParameterTagConfigs)
	{
		const FGameplayTag& ParamTag = ParamConfigPair.Key;
		const FTcsStateParameter& ParamConfig = ParamConfigPair.Value;

		if (ParamConfig.bIsSnapshot)
		{
			const float SnapshotValue = CalculateNumericParameterByTag(ParamTag, Instigator, Target);
			CachedSnapshotParametersByTag.Add(ParamTag, SnapshotValue);
		}
	}
}

void UTcsSkillInstance::TakeParameterSnapshotByTag(FGameplayTag ParamTag, AActor* Instigator, AActor* Target)
{
	if (!ParamTag.IsValid())
	{
		return;
	}

	if (const FTcsStateParameter* ParamConfig = NumericParameterTagConfigs.Find(ParamTag))
	{
		if (ParamConfig->bIsSnapshot)
		{
			const float SnapshotValue = CalculateNumericParameterByTag(ParamTag, Instigator, Target);
			CachedSnapshotParametersByTag.Add(ParamTag, SnapshotValue);
		}
	}
}

float UTcsSkillInstance::GetSnapshotParameter(FName ParamName, float DefaultValue) const
{
	if (const float* Value = CachedSnapshotParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

float UTcsSkillInstance::GetSnapshotParameterByTag(FGameplayTag ParamTag, float DefaultValue) const
{
	if (!ParamTag.IsValid())
	{
		return DefaultValue;
	}

	if (const float* Value = CachedSnapshotParametersByTag.Find(ParamTag))
	{
		return *Value;
	}

	return DefaultValue;
}

void UTcsSkillInstance::ClearParameterSnapshot()
{
	CachedSnapshotParameters.Empty();
	CachedSnapshotParametersByTag.Empty();
	
	// 同时清理实时参数缓存（因为快照清除可能意味着重大变更）
	LastRealTimeParameterValues.Empty();
	LastRealTimeParameterValuesByTag.Empty();
	ParameterUpdateTimestamps.Empty();
	ParameterUpdateTimestampsByTag.Empty();
	
	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter snapshot and caches cleared for skill: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

void UTcsSkillInstance::SyncParametersToStateInstance(UTcsStateInstance* StateInstance, bool bForceAll)
{
	if (!StateInstance)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Cannot sync parameters: StateInstance is null"), *Owner->GetName());
		return;
	}

	// 同步布尔参数（Name）
	for (const auto& BoolParam : BoolParameters)
	{
		StateInstance->SetBoolParam(BoolParam.Key, BoolParam.Value);
	}

	// 同步布尔参数（Tag）
	for (const auto& BoolParam : BoolParametersByTag)
	{
		StateInstance->SetBoolParamByTag(BoolParam.Key, BoolParam.Value);
	}

	// 同步向量参数（Name）
	for (const auto& VectorParam : VectorParameters)
	{
		StateInstance->SetVectorParam(VectorParam.Key, VectorParam.Value);
	}

	// 同步向量参数（Tag）
	for (const auto& VectorParam : VectorParametersByTag)
	{
		StateInstance->SetVectorParamByTag(VectorParam.Key, VectorParam.Value);
	}

	// 同步数值参数
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		float ParamValue = 0.0f;

		if (ParamConfig.bIsSnapshot)
		{
			// 快照参数：使用缓存值
			ParamValue = GetSnapshotParameter(ParamName, 0.0f);
		}
		else if (bForceAll)
		{
			// 非快照参数且强制同步：重新计算
			ParamValue = CalculateNumericParameter(ParamName, Owner.Get(), Owner.Get());
		}
		else
		{
			// 非快照参数且非强制同步：跳过（由实时同步机制处理）
			continue;
		}

		StateInstance->SetNumericParam(ParamName, ParamValue);
	}

	for (const auto& NumericConfigPair : NumericParameterTagConfigs)
	{
		const FGameplayTag& ParamTag = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		float ParamValue = 0.0f;

		if (ParamConfig.bIsSnapshot)
		{
			ParamValue = GetSnapshotParameterByTag(ParamTag, 0.0f);
		}
		else if (bForceAll)
		{
			ParamValue = CalculateNumericParameterByTag(ParamTag, Owner.Get(), Owner.Get());
		}
		else
		{
			continue;
		}

		StateInstance->SetNumericParamByTag(ParamTag, ParamValue);
	}

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameters synced to StateInstance: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

void UTcsSkillInstance::RefreshAllParameters(AActor* Instigator, AActor* Target)
{
	// 重新计算所有非快照参数（快照参数不受影响）
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot)
		{
			float NewValue = CalculateNumericParameter(ParamName, Instigator, Target);
			BroadcastParameterChanged(ParamName, NewValue);
		}
	}

	for (const auto& NumericConfigPair : NumericParameterTagConfigs)
	{
		const FGameplayTag& ParamTag = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot)
		{
			const float NewValue = CalculateNumericParameterByTag(ParamTag, Instigator, Target);
			BroadcastParameterChanged(ParamTag.GetTagName(), NewValue);
		}
	}
}

void UTcsSkillInstance::SyncRealtimeParametersToStateInstance(UTcsStateInstance* StateInstance, AActor* Instigator, AActor* Target)
{
	if (!StateInstance)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Cannot sync real-time parameters: StateInstance is null"), *Owner->GetName());
		return;
	}

	// 使用默认目标
	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	// 只同步非快照的数值参数（实时计算），使用智能更新检测
	int32 UpdatedParameterCount = 0;
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		// 只处理非快照参数
		if (!ParamConfig.bIsSnapshot)
		{
			// 检查参数是否需要更新（性能优化）
			if (ShouldUpdateParameter(ParamName, Instigator, Target))
			{
				float ParamValue = CalculateNumericParameter(ParamName, Instigator, Target);
				StateInstance->SetNumericParam(ParamName, ParamValue);

				// 更新缓存值
				const_cast<UTcsSkillInstance*>(this)->LastRealTimeParameterValues.FindOrAdd(ParamName) = ParamValue;
				const_cast<UTcsSkillInstance*>(this)->ParameterUpdateTimestamps.FindOrAdd(ParamName) = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				
				UpdatedParameterCount++;

				UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Real-time parameter synced: %s.%s = %.2f"), 
					*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), ParamValue);
			}
		}
	}

	for (const auto& NumericConfigPair : NumericParameterTagConfigs)
	{
		const FGameplayTag& ParamTag = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot && ShouldUpdateParameterByTag(ParamTag, Instigator, Target))
		{
			const float ParamValue = CalculateNumericParameterByTag(ParamTag, Instigator, Target);
			StateInstance->SetNumericParamByTag(ParamTag, ParamValue);

			const_cast<UTcsSkillInstance*>(this)->LastRealTimeParameterValuesByTag.FindOrAdd(ParamTag) = ParamValue;
			const_cast<UTcsSkillInstance*>(this)->ParameterUpdateTimestampsByTag.FindOrAdd(ParamTag) = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

			UpdatedParameterCount++;

			UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Real-time parameter(tag) synced: %s.%s = %.2f"),
				*Owner->GetName(), *SkillDefId.ToString(), *ParamTag.ToString(), ParamValue);
		}
	}

	if (UpdatedParameterCount > 0)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Updated %d total real-time parameters for skill: %s"),
			*Owner->GetName(), UpdatedParameterCount, *SkillDefId.ToString());
	}

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Real-time parameters synced to StateInstance: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

bool UTcsSkillInstance::ShouldUpdateParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	// 检查参数配置
	const FTcsStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
	if (!ParamConfig || ParamConfig->bIsSnapshot)
	{
		return false; // 快照参数不需要实时更新
	}

	// 使用默认目标
	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	// 计算当前值
	float CurrentValue = CalculateNumericParameter(ParamName, Instigator, Target);
	
	// 检查是否有缓存的上次值
	const float* LastValue = LastRealTimeParameterValues.Find(ParamName);
	if (!LastValue)
	{
		return true; // 没有缓存值，需要更新
	}

	// 比较值是否发生变化（使用小的容差）
	const float Tolerance = KINDA_SMALL_NUMBER;
	bool bValueChanged = !FMath::IsNearlyEqual(CurrentValue, *LastValue, Tolerance);

	return bValueChanged;
}

bool UTcsSkillInstance::ShouldUpdateParameterByTag(FGameplayTag ParamTag, AActor* Instigator, AActor* Target) const
{
	const FTcsStateParameter* ParamConfig = NumericParameterTagConfigs.Find(ParamTag);
	if (!ParamConfig || ParamConfig->bIsSnapshot)
	{
		return false;
	}

	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	const float CurrentValue = CalculateNumericParameterByTag(ParamTag, Instigator, Target);
	const float* LastValue = LastRealTimeParameterValuesByTag.Find(ParamTag);
	if (!LastValue)
	{
		return true;
	}

	const float Tolerance = KINDA_SMALL_NUMBER;
	return !FMath::IsNearlyEqual(CurrentValue, *LastValue, Tolerance);
}

bool UTcsSkillInstance::HasPendingParameterUpdates(AActor* Instigator, AActor* Target) const
{
	// 检查是否有任何非快照参数需要更新
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot) // 只检查实时参数
		{
			if (ShouldUpdateParameter(ParamName, Instigator, Target))
			{
				return true;
			}
		}
	}

	for (const auto& NumericConfigPair : NumericParameterTagConfigs)
	{
		const FGameplayTag& ParamTag = NumericConfigPair.Key;
		const FTcsStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot)
		{
			if (ShouldUpdateParameterByTag(ParamTag, Instigator, Target))
			{
				return true;
			}
		}
	}

	return false;
}

float UTcsSkillInstance::CalculateBaseNumericParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	const FTcsStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
	if (!ParamConfig || !ParamConfig->ParamEvaluatorType)
	{
		return 0.0f;
	}

	const UTcsStateParamEvaluator* ParamParser = ParamConfig->ParamEvaluatorType.GetDefaultObject();
	if (!ParamParser)
	{
		return 0.0f;
	}

	float BaseValue = 0.0f;
	
	// 创建一个临时的StateInstance来支持参数解析
	// 这样可以让ParamParser访问到技能等级等信息
	UTcsStateInstance* TempStateInstance = nullptr;
	
	// 如果Owner有StateComponent，尝试创建临时StateInstance
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			// 创建临时StateInstance用于参数计算
			TempStateInstance = NewObject<UTcsStateInstance>();
			if (TempStateInstance)
			{
				TempStateInstance->Initialize(SkillDef, OwnerActor, Instigator, 0, CurrentLevel);
				
				// 设置基础参数以便ParamParser可以使用
				TempStateInstance->SetNumericParam(TEXT("Level"), static_cast<float>(CurrentLevel));
				TempStateInstance->SetNumericParam(TEXT("CooldownMultiplier"), 1.0f);
				TempStateInstance->SetNumericParam(TEXT("CostMultiplier"), 1.0f);
			}
		}
	}
	
	ParamParser->Evaluate(
		Instigator,
		Target,
		TempStateInstance, // 使用临时StateInstance而不是nullptr
		ParamConfig->ParamValueContainer,
		BaseValue
	);
	
	// 清理临时对象
	if (TempStateInstance)
	{
		TempStateInstance->ConditionalBeginDestroy();
	}

	return BaseValue;
}

float UTcsSkillInstance::CalculateBaseNumericParameterByTag(FGameplayTag ParamTag, AActor* Instigator, AActor* Target) const
{
	if (!ParamTag.IsValid())
	{
		return 0.0f;
	}

	const FTcsStateParameter* ParamConfig = NumericParameterTagConfigs.Find(ParamTag);
	if (!ParamConfig || !ParamConfig->ParamEvaluatorType)
	{
		return 0.0f;
	}

	const UTcsStateParamEvaluator* ParamParser = ParamConfig->ParamEvaluatorType.GetDefaultObject();
	if (!ParamParser)
	{
		return 0.0f;
	}

	float BaseValue = 0.0f;

	UTcsStateInstance* TempStateInstance = nullptr;

	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			TempStateInstance = NewObject<UTcsStateInstance>();
			if (TempStateInstance)
			{
				TempStateInstance->Initialize(SkillDef, OwnerActor, Instigator, 0, CurrentLevel);
				TempStateInstance->SetNumericParam(TEXT("Level"), static_cast<float>(CurrentLevel));
				TempStateInstance->SetNumericParam(TEXT("CooldownMultiplier"), 1.0f);
				TempStateInstance->SetNumericParam(TEXT("CostMultiplier"), 1.0f);
			}
		}
	}

	ParamParser->Evaluate(
		Instigator,
		Target,
		TempStateInstance,
		ParamConfig->ParamValueContainer,
		BaseValue
	);

	if (TempStateInstance)
	{
		TempStateInstance->ConditionalBeginDestroy();
	}

	return BaseValue;
}

float UTcsSkillInstance::ApplySkillModifiers(FName ParamName, float BaseValue) const
{
	AActor* OwnerActor = Owner.Get();
	if (!OwnerActor)
	{
		return BaseValue;
	}

	if (UTcsSkillComponent* SkillComponent = OwnerActor->FindComponentByClass<UTcsSkillComponent>())
	{
		FTcsAggregatedParamEffect Effect;
		if (SkillComponent->GetAggregatedParamEffect(this, ParamName, Effect))
		{
			return ApplyAggregatedEffect(Effect, BaseValue);
		}
	}

	return BaseValue;
}

float UTcsSkillInstance::ApplySkillModifiersByTag(FGameplayTag ParamTag, float BaseValue) const
{
	if (!ParamTag.IsValid())
	{
		return BaseValue;
	}

	AActor* OwnerActor = Owner.Get();
	if (!OwnerActor)
	{
		return BaseValue;
	}

	if (UTcsSkillComponent* SkillComponent = OwnerActor->FindComponentByClass<UTcsSkillComponent>())
	{
		FTcsAggregatedParamEffect Effect;
		if (SkillComponent->GetAggregatedParamEffectByTag(this, ParamTag, Effect))
		{
			return ApplyAggregatedEffect(Effect, BaseValue);
		}
	}

	return BaseValue;
}

float UTcsSkillInstance::ApplyAggregatedEffect(const FTcsAggregatedParamEffect& Effect, float BaseValue) const
{
	if (Effect.bHasOverride)
	{
		return Effect.OverrideValue;
	}

	float Result = (BaseValue + Effect.AddSum) * Effect.MulProd;
	Result *= Effect.CooldownMultiplier;
	Result *= Effect.CostMultiplier;
	return Result;
}

#pragma endregion


#pragma region Events Implementation

void UTcsSkillInstance::BroadcastParameterChanged(FName ParamName, float NewValue)
{
	OnSkillParameterChanged.Broadcast(ParamName, NewValue);
}

void UTcsSkillInstance::BroadcastLevelChanged(int32 NewLevel)
{
	OnSkillLevelChanged.Broadcast(NewLevel);
}

#pragma endregion
