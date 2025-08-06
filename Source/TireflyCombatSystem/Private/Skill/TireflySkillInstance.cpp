// Copyright Tirefly. All Rights Reserved.

#include "Skill/TireflySkillInstance.h"
#include "State/TireflyStateParameter.h"
#include "TireflyCombatSystemLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UTireflySkillInstance::UTireflySkillInstance()
{
	CurrentLevel = 1;
	CooldownMultiplier = 1.0f;
	CostMultiplier = 1.0f;
}

UWorld* UTireflySkillInstance::GetWorld() const
{
	if (AActor* OwnerActor = Owner.Get())
	{
		return OwnerActor->GetWorld();
	}

	return nullptr;
}

#pragma region Initialization Implementation

void UTireflySkillInstance::Initialize(const FTireflyStateDefinition& InSkillDef, FName InSkillDefId, AActor* InOwner, int32 InInitialLevel)
{
	SkillDef = InSkillDef;
	SkillDefId = InSkillDefId;
	Owner = InOwner;
	CurrentLevel = FMath::Max(1, InInitialLevel);

	// 清理现有参数
	BoolParameters.Empty();
	VectorParameters.Empty();
	NumericParameterConfigs.Empty();
	CachedSnapshotParameters.Empty();
	LastRealTimeParameterValues.Empty();
	ParameterUpdateTimestamps.Empty();

	// 初始化参数配置
	for (const auto& ParamPair : SkillDef.Parameters)
	{
		const FName& ParamName = ParamPair.Key;
		const FTireflyStateParameter& ParamConfig = ParamPair.Value;

		switch (ParamConfig.ParameterType)
		{
		case ETireflyStateParameterType::Bool:
			{
				// 从ParamValueContainer提取bool值
				if (const bool* BoolValue = ParamConfig.ParamValueContainer.GetPtr<bool>())
				{
					BoolParameters.Add(ParamName, *BoolValue);
				}
				else
				{
					BoolParameters.Add(ParamName, false);
				}
			}
			break;

		case ETireflyStateParameterType::Vector:
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

		case ETireflyStateParameterType::Numeric:
			{
				// 存储数值参数的计算配置
				NumericParameterConfigs.Add(ParamName, ParamConfig);
			}
			break;
		}
	}

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Skill instance initialized: %s (Level: %d)"), 
		*InOwner->GetName(), *SkillDefId.ToString(), CurrentLevel);
}

#pragma endregion

#pragma region DynamicProperties Implementation

void UTireflySkillInstance::SetCurrentLevel(int32 InLevel)
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

bool UTireflySkillInstance::UpgradeLevel(int32 LevelIncrease)
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

bool UTireflySkillInstance::GetBoolParameter(FName ParamName, bool DefaultValue) const
{
	if (const bool* Value = BoolParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTireflySkillInstance::SetBoolParameter(FName ParamName, bool Value)
{
	bool OldValue = GetBoolParameter(ParamName, false);
	BoolParameters.FindOrAdd(ParamName) = Value;

	if (Value != OldValue)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Bool parameter changed: %s.%s = %s"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), Value ? TEXT("true") : TEXT("false"));
	}
}

FVector UTireflySkillInstance::GetVectorParameter(FName ParamName, const FVector& DefaultValue) const
{
	if (const FVector* Value = VectorParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTireflySkillInstance::SetVectorParameter(FName ParamName, const FVector& Value)
{
	FVector OldValue = GetVectorParameter(ParamName, FVector::ZeroVector);
	VectorParameters.FindOrAdd(ParamName) = Value;

	if (!Value.Equals(OldValue))
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Vector parameter changed: %s.%s = %s"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), *Value.ToString());
	}
}

float UTireflySkillInstance::CalculateNumericParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	// 使用默认目标
	if (!Instigator)
	{
		Instigator = Owner.Get();
	}
	if (!Target)
	{
		Target = Owner.Get();
	}

	// 计算基础值
	float BaseValue = CalculateBaseNumericParameter(ParamName, Instigator, Target);
	
	// 应用技能修正器
	float FinalValue = ApplySkillModifiers(ParamName, BaseValue);

	return FinalValue;
}

void UTireflySkillInstance::TakeParameterSnapshot(AActor* Instigator, AActor* Target)
{
	CachedSnapshotParameters.Empty();

	// 只对快照参数拍快照
	for (const auto& ParamConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = ParamConfigPair.Key;
		const FTireflyStateParameter& ParamConfig = ParamConfigPair.Value;

		if (ParamConfig.bIsSnapshot)
		{
			float SnapshotValue = CalculateNumericParameter(ParamName, Instigator, Target);
			CachedSnapshotParameters.Add(ParamName, SnapshotValue);

			UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter snapshot taken: %s.%s = %.2f"), 
				*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), SnapshotValue);
		}
	}
}

float UTireflySkillInstance::GetSnapshotParameter(FName ParamName, float DefaultValue) const
{
	if (const float* Value = CachedSnapshotParameters.Find(ParamName))
	{
		return *Value;
	}
	return DefaultValue;
}

void UTireflySkillInstance::ClearParameterSnapshot()
{
	CachedSnapshotParameters.Empty();
	
	// 同时清理实时参数缓存（因为快照清除可能意味着重大变更）
	LastRealTimeParameterValues.Empty();
	ParameterUpdateTimestamps.Empty();
	
	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter snapshot and caches cleared for skill: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

void UTireflySkillInstance::SyncParametersToStateInstance(UTireflyStateInstance* StateInstance, bool bForceAll)
{
	if (!StateInstance)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Cannot sync parameters: StateInstance is null"), *Owner->GetName());
		return;
	}

	// 同步布尔参数
	for (const auto& BoolParam : BoolParameters)
	{
		StateInstance->SetBoolParam(BoolParam.Key, BoolParam.Value);
	}

	// 同步向量参数
	for (const auto& VectorParam : VectorParameters)
	{
		StateInstance->SetVectorParam(VectorParam.Key, VectorParam.Value);
	}

	// 同步数值参数
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTireflyStateParameter& ParamConfig = NumericConfigPair.Value;

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

		StateInstance->SetParamValue(ParamName, ParamValue);
	}

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameters synced to StateInstance: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

void UTireflySkillInstance::RefreshAllParameters(AActor* Instigator, AActor* Target)
{
	// 重新计算所有非快照参数（快照参数不受影响）
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTireflyStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot)
		{
			float NewValue = CalculateNumericParameter(ParamName, Instigator, Target);
			BroadcastParameterChanged(ParamName, NewValue);
		}
	}
}

void UTireflySkillInstance::SyncRealTimeParametersToStateInstance(UTireflyStateInstance* StateInstance, AActor* Instigator, AActor* Target)
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
		const FTireflyStateParameter& ParamConfig = NumericConfigPair.Value;

		// 只处理非快照参数
		if (!ParamConfig.bIsSnapshot)
		{
			// 检查参数是否需要更新（性能优化）
			if (ShouldUpdateRealTimeParameter(ParamName, Instigator, Target))
			{
				float ParamValue = CalculateNumericParameter(ParamName, Instigator, Target);
				StateInstance->SetParamValue(ParamName, ParamValue);

				// 更新缓存值
				const_cast<UTireflySkillInstance*>(this)->LastRealTimeParameterValues.FindOrAdd(ParamName) = ParamValue;
				const_cast<UTireflySkillInstance*>(this)->ParameterUpdateTimestamps.FindOrAdd(ParamName) = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				
				UpdatedParameterCount++;

				UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Real-time parameter synced: %s.%s = %.2f"), 
					*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), ParamValue);
			}
		}
	}

	if (UpdatedParameterCount > 0)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Updated %d real-time parameters for skill: %s"), 
			*Owner->GetName(), UpdatedParameterCount, *SkillDefId.ToString());
	}

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Real-time parameters synced to StateInstance: %s"), 
		*Owner->GetName(), *SkillDefId.ToString());
}

bool UTireflySkillInstance::ShouldUpdateRealTimeParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	// 检查参数配置
	const FTireflyStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
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

bool UTireflySkillInstance::HasPendingRealTimeParameterUpdates(AActor* Instigator, AActor* Target) const
{
	// 检查是否有任何非快照参数需要更新
	for (const auto& NumericConfigPair : NumericParameterConfigs)
	{
		const FName& ParamName = NumericConfigPair.Key;
		const FTireflyStateParameter& ParamConfig = NumericConfigPair.Value;

		if (!ParamConfig.bIsSnapshot) // 只检查实时参数
		{
			if (ShouldUpdateRealTimeParameter(ParamName, Instigator, Target))
			{
				return true;
			}
		}
	}

	return false;
}

float UTireflySkillInstance::CalculateBaseNumericParameter(FName ParamName, AActor* Instigator, AActor* Target) const
{
	const FTireflyStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
	if (!ParamConfig || !ParamConfig->ParamResolverClass)
	{
		return 0.0f;
	}

	const UTireflyStateParamParser* ParamParser = ParamConfig->ParamResolverClass.GetDefaultObject();
	if (!ParamParser)
	{
		return 0.0f;
	}

	float BaseValue = 0.0f;
	
	// 创建一个临时的StateInstance来支持参数解析
	// 这样可以让ParamParser访问到技能等级等信息
	UTireflyStateInstance* TempStateInstance = nullptr;
	
	// 如果Owner有StateComponent，尝试创建临时StateInstance
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			// 创建临时StateInstance用于参数计算
			TempStateInstance = NewObject<UTireflyStateInstance>();
			if (TempStateInstance)
			{
				TempStateInstance->Initialize(SkillDef, OwnerActor, Instigator, 0, CurrentLevel);
				
				// 设置基础参数以便ParamParser可以使用
				TempStateInstance->SetParamValue(TEXT("Level"), static_cast<float>(CurrentLevel));
				TempStateInstance->SetParamValue(TEXT("CooldownMultiplier"), CooldownMultiplier);
				TempStateInstance->SetParamValue(TEXT("CostMultiplier"), CostMultiplier);
			}
		}
	}
	
	ParamParser->ParseStateParameter(
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

float UTireflySkillInstance::ApplySkillModifiers(FName ParamName, float BaseValue) const
{
	float FinalValue = BaseValue;

	// 应用加法修正
	if (const TArray<float>* AdditiveArray = AdditiveModifiers.Find(ParamName))
	{
		for (float Modifier : *AdditiveArray)
		{
			FinalValue += Modifier;
		}
	}

	// 应用乘法修正
	if (const TArray<float>* MultiplicativeArray = MultiplicativeModifiers.Find(ParamName))
	{
		for (float Modifier : *MultiplicativeArray)
		{
			FinalValue *= Modifier;
		}
	}

	// 应用等级修正（示例：某些参数可能随等级缩放）
	if (ParamName == TEXT("Damage") || ParamName == TEXT("Healing"))
	{
		// 示例：伤害和治疗随等级线性增长
		float LevelBonus = (CurrentLevel - 1) * 0.1f; // 每级增加10%
		FinalValue *= (1.0f + LevelBonus);
	}
	else if (ParamName == TEXT("Cooldown"))
	{
		// 冷却时间受冷却修正器影响
		FinalValue *= CooldownMultiplier;
	}
	else if (ParamName.ToString().Contains(TEXT("Cost")))
	{
		// 消耗类参数受消耗修正器影响
		FinalValue *= CostMultiplier;
	}

	return FinalValue;
}

#pragma endregion

#pragma region ParameterModifiers Implementation

void UTireflySkillInstance::AddParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier)
{
	if (bIsMultiplier)
	{
		MultiplicativeModifiers.FindOrAdd(ParamName).Add(ModifierValue);
	}
	else
	{
		AdditiveModifiers.FindOrAdd(ParamName).Add(ModifierValue);
	}

	// 触发参数变化事件（对于非快照参数）
	const FTireflyStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
	if (ParamConfig && !ParamConfig->bIsSnapshot)
	{
		float NewValue = CalculateNumericParameter(ParamName, Owner.Get(), Owner.Get());
		BroadcastParameterChanged(ParamName, NewValue);
	}

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter modifier added: %s.%s %s %.2f"), 
		*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), 
		bIsMultiplier ? TEXT("*") : TEXT("+"), ModifierValue);
}

void UTireflySkillInstance::RemoveParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier)
{
	bool bRemoved = false;

	if (bIsMultiplier)
	{
		if (TArray<float>* ModifierArray = MultiplicativeModifiers.Find(ParamName))
		{
			bRemoved = ModifierArray->Remove(ModifierValue) > 0;
			if (ModifierArray->Num() == 0)
			{
				MultiplicativeModifiers.Remove(ParamName);
			}
		}
	}
	else
	{
		if (TArray<float>* ModifierArray = AdditiveModifiers.Find(ParamName))
		{
			bRemoved = ModifierArray->Remove(ModifierValue) > 0;
			if (ModifierArray->Num() == 0)
			{
				AdditiveModifiers.Remove(ParamName);
			}
		}
	}

	if (bRemoved)
	{
		// 触发参数变化事件（对于非快照参数）
		const FTireflyStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
		if (ParamConfig && !ParamConfig->bIsSnapshot)
		{
			float NewValue = CalculateNumericParameter(ParamName, Owner.Get(), Owner.Get());
			BroadcastParameterChanged(ParamName, NewValue);
		}

		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter modifier removed: %s.%s %s %.2f"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString(), 
			bIsMultiplier ? TEXT("*") : TEXT("+"), ModifierValue);
	}
}

void UTireflySkillInstance::ClearParameterModifiers(FName ParamName)
{
	bool bChanged = false;

	if (AdditiveModifiers.Remove(ParamName) > 0)
	{
		bChanged = true;
	}

	if (MultiplicativeModifiers.Remove(ParamName) > 0)
	{
		bChanged = true;
	}

	if (bChanged)
	{
		// 触发参数变化事件（对于非快照参数）
		const FTireflyStateParameter* ParamConfig = NumericParameterConfigs.Find(ParamName);
		if (ParamConfig && !ParamConfig->bIsSnapshot)
		{
			float NewValue = CalculateNumericParameter(ParamName, Owner.Get(), Owner.Get());
			BroadcastParameterChanged(ParamName, NewValue);
		}

		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Parameter modifiers cleared: %s.%s"), 
			*Owner->GetName(), *SkillDefId.ToString(), *ParamName.ToString());
	}
}

float UTireflySkillInstance::GetTotalParameterModifier(FName ParamName) const
{
	float TotalModifier = 0.0f;

	// 加法修正求和
	if (const TArray<float>* AdditiveArray = AdditiveModifiers.Find(ParamName))
	{
		for (float Modifier : *AdditiveArray)
		{
			TotalModifier += Modifier;
		}
	}

	// 乘法修正连乘（转换为加法形式返回总影响）
	float MultiplicativeEffect = 1.0f;
	if (const TArray<float>* MultiplicativeArray = MultiplicativeModifiers.Find(ParamName))
	{
		for (float Modifier : *MultiplicativeArray)
		{
			MultiplicativeEffect *= Modifier;
		}
	}

	// 返回综合修正效果（简化计算）
	return TotalModifier + (MultiplicativeEffect - 1.0f);
}

#pragma endregion

#pragma region Events Implementation

void UTireflySkillInstance::BroadcastParameterChanged(FName ParamName, float NewValue)
{
	OnSkillParameterChanged.Broadcast(ParamName, NewValue);
}

void UTireflySkillInstance::BroadcastLevelChanged(int32 NewLevel)
{
	OnSkillLevelChanged.Broadcast(NewLevel);
}

#pragma endregion