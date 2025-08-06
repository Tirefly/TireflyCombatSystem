// Copyright Tirefly. All Rights Reserved.

#include "Skill/TireflySkillComponent.h"

#include "State/TireflyState.h"
#include "State/TireflyStateComponent.h"
#include "State/TireflyStateManagerSubsystem.h"
#include "Skill/TireflySkillManagerSubsystem.h"
#include "Skill/TireflySkillInstance.h"
#include "TireflyCombatSystemLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UTireflySkillComponent::UTireflySkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 每0.1秒更新一次冷却
}

void UTireflySkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 获取技能管理器子系统
	if (UWorld* World = GetWorld())
	{
		SkillManagerSubsystem = World->GetSubsystem<UTireflySkillManagerSubsystem>();
	}
}

void UTireflySkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 更新技能冷却
	UpdateSkillCooldowns(DeltaTime);
	
	// 更新活跃技能的实时参数
	UpdateActiveSkillRealTimeParameters();
}

#pragma region SkillLearning Implementation

UTireflySkillInstance* UTireflySkillComponent::LearnSkill(FName SkillDefId, int32 InitialLevel)
{
	if (SkillDefId.IsNone() || InitialLevel <= 0)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Invalid skill parameters: SkillDefId=%s, Level=%d"), 
			*FString(__FUNCTION__), *SkillDefId.ToString(), InitialLevel);
		return nullptr;
	}

	// 检查是否已经学会该技能
	if (UTireflySkillInstance* ExistingSkill = LearnedSkillInstances.FindRef(SkillDefId))
	{
		// 技能已存在，升级等级
		if (ExistingSkill->GetCurrentLevel() < InitialLevel)
		{
			ExistingSkill->SetCurrentLevel(InitialLevel);
			UE_LOG(LogTcsSkill, Log, TEXT("[%s] Upgraded existing skill: %s to Level %d"), 
				*GetOwner()->GetName(), *SkillDefId.ToString(), InitialLevel);
		}
		return ExistingSkill;
	}

	// 创建新的技能实例
	UTireflySkillInstance* NewSkillInstance = CreateSkillInstance(SkillDefId, InitialLevel);
	if (!NewSkillInstance)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Failed to create skill instance: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
		return nullptr;
	}

	// 添加到已学技能
	LearnedSkillInstances.Add(SkillDefId, NewSkillInstance);

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Learned new skill: %s (Level %d)"), 
		*GetOwner()->GetName(), *SkillDefId.ToString(), InitialLevel);

	return NewSkillInstance;
}

void UTireflySkillComponent::ForgetSkill(FName SkillDefId)
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		// 清除相关冷却
		ClearSkillCooldown(SkillDefId);

		// 取消正在释放的同名技能
		CancelSkill(SkillDefId);

		// 从映射中移除
		LearnedSkillInstances.Remove(SkillDefId);

		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Forgot skill: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
	}
}

bool UTireflySkillComponent::HasLearnedSkill(FName SkillDefId) const
{
	return LearnedSkillInstances.Contains(SkillDefId);
}

UTireflySkillInstance* UTireflySkillComponent::GetSkillInstance(FName SkillDefId) const
{
	return LearnedSkillInstances.FindRef(SkillDefId);
}

int32 UTireflySkillComponent::GetSkillLevel(FName SkillDefId) const
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetCurrentLevel();
	}
	return 0;
}

bool UTireflySkillComponent::UpgradeSkillInstance(FName SkillDefId, int32 LevelIncrease)
{
	if (!HasLearnedSkill(SkillDefId) || LevelIncrease <= 0)
	{
		return false;
	}

	UTireflySkillInstance* SkillInstance = LearnedSkillInstances[SkillDefId];
	if (SkillInstance)
	{
		SkillInstance->UpgradeLevel(LevelIncrease);
		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Upgraded skill: %s to Level %d"), 
			*GetOwner()->GetName(), *SkillDefId.ToString(), SkillInstance->GetCurrentLevel());
		return true;
	}

	return false;
}

void UTireflySkillComponent::ModifySkillParameter(FName SkillDefId, FName ParamName, float Modifier, bool bIsMultiplier)
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		SkillInstance->AddParameterModifier(ParamName, Modifier, bIsMultiplier);
	}
}

void UTireflySkillComponent::SetSkillCooldownMultiplier(FName SkillDefId, float Multiplier)
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		SkillInstance->SetCooldownMultiplier(Multiplier);
	}
}

void UTireflySkillComponent::SetSkillCostMultiplier(FName SkillDefId, float Multiplier)
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		SkillInstance->SetCostMultiplier(Multiplier);
	}
}

UTireflySkillInstance* UTireflySkillComponent::CreateSkillInstance(FName SkillDefId, int32 InitialLevel)
{
	if (!SkillManagerSubsystem)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] SkillManagerSubsystem is not available"), *FString(__FUNCTION__));
		return nullptr;
	}

	// 获取技能定义
	FTireflyStateDefinition SkillDef = SkillManagerSubsystem->GetSkillDefinition(SkillDefId);
	if (SkillDef.StateType != ST_Skill)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Invalid skill definition: %s is not a skill type"), 
			*FString(__FUNCTION__), *SkillDefId.ToString());
		return nullptr;
	}

	// 创建技能实例
	UTireflySkillInstance* NewSkillInstance = NewObject<UTireflySkillInstance>(this);
	if (!NewSkillInstance)
	{
		return nullptr;
	}

	// 初始化技能实例
	NewSkillInstance->Initialize(SkillDef, SkillDefId, GetOwner(), InitialLevel);

	return NewSkillInstance;
}

#pragma endregion

#pragma region SkillCasting Implementation

bool UTireflySkillComponent::TryCastSkill(FName SkillDefId, AActor* TargetActor, const FInstancedStruct& CastParameters)
{
	// 验证技能是否可以释放
	FText FailureReason;
	if (!ValidateSkillCast(SkillDefId, TargetActor, FailureReason))
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Cannot cast skill %s: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString(), *FailureReason.ToString());
		return false;
	}

	// 创建技能状态实例
	UTireflyStateInstance* SkillInstance = CreateSkillStateInstance(SkillDefId, TargetActor, CastParameters);
	if (!SkillInstance)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Failed to create skill state instance for %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
		return false;
	}

	// 应用技能状态
	UTireflyStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] No StateComponent found"), *GetOwner()->GetName());
		return false;
	}

	// 添加到状态管理
	StateComponent->AddStateInstance(SkillInstance);

	// 跟踪活跃技能状态
	ActiveSkillStateInstances.Add(SkillDefId, SkillInstance);

	// 获取技能实例并计算冷却时间
	UTireflySkillInstance* LearnedSkillInstance = GetSkillInstance(SkillDefId);
	if (LearnedSkillInstance)
	{
		// 使用SkillInstance计算冷却时间
		float CooldownDuration = LearnedSkillInstance->CalculateNumericParameter(TEXT("Cooldown"), GetOwner(), TargetActor);
		if (CooldownDuration <= 0.0f)
		{
			// 如果没有定义冷却参数，使用传统方法计算
			const FTireflyStateDefinition& SkillDef = SkillInstance->GetStateDef();
			CooldownDuration = CalculateSkillCooldown(SkillDef, LearnedSkillInstance->GetCurrentLevel());
		}
		SetSkillCooldown(SkillDefId, CooldownDuration);
	}
	else
	{
		// 回退到传统计算方法
		const FTireflyStateDefinition& SkillDef = SkillInstance->GetStateDef();
		float CooldownDuration = CalculateSkillCooldown(SkillDef, GetSkillLevel(SkillDefId));
		SetSkillCooldown(SkillDefId, CooldownDuration);
	}

	// 启动技能状态的StateTree
	SkillInstance->InitializeStateTree();
	SkillInstance->StartStateTree();

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Successfully cast skill: %s (Cooldown: %.1fs)"), 
		*GetOwner()->GetName(), *SkillDefId.ToString(), CooldownDuration);

	return true;
}

void UTireflySkillComponent::CancelSkill(FName SkillDefId)
{
	UTireflyStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return;
	}

	UTireflyStateInstance* SkillInstance = StateComponent->GetStateInstance(SkillDefId);
	if (SkillInstance && SkillInstance->GetStateDef().StateType == ST_Skill)
	{
		// 停止StateTree执行
		SkillInstance->StopStateTree();

		// 设置状态为到期
		SkillInstance->SetCurrentStage(ETireflyStateStage::SS_Expired);

		// 从状态管理中移除
		StateComponent->RemoveStateInstance(SkillInstance);

		// 从活跃技能跟踪中移除
		ActiveSkillStateInstances.Remove(SkillDefId);

		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Cancelled skill: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
	}
}

void UTireflySkillComponent::CancelAllSkills()
{
	UTireflyStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return;
	}

	TArray<UTireflyStateInstance*> SkillInstances = StateComponent->GetStatesByType(ST_Skill);
	
	for (UTireflyStateInstance* SkillInstance : SkillInstances)
	{
		if (IsValid(SkillInstance))
		{
			CancelSkill(SkillInstance->GetStateDefId());
		}
	}

	// 清空所有活跃技能跟踪
	ActiveSkillStateInstances.Empty();

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Cancelled all skills (%d skills)"), 
		*GetOwner()->GetName(), SkillInstances.Num());
}

bool UTireflySkillComponent::CanCastSkill(FName SkillDefId, AActor* TargetActor) const
{
	FText FailureReason;
	return ValidateSkillCast(SkillDefId, TargetActor, FailureReason);
}

bool UTireflySkillComponent::ValidateSkillCast(FName SkillDefId, AActor* TargetActor, FText& FailureReason) const
{
	// 检查技能是否已学习
	if (!HasLearnedSkill(SkillDefId))
	{
		FailureReason = FText::FromString(TEXT("Skill not learned"));
		return false;
	}

	// 检查技能是否在冷却中
	if (IsSkillOnCooldown(SkillDefId))
	{
		FailureReason = FText::FromString(FString::Printf(TEXT("Skill on cooldown (%.1fs remaining)"), 
			GetSkillCooldownRemaining(SkillDefId)));
		return false;
	}

	// 检查是否已经在释放相同技能
	if (IsSkillActive(SkillDefId))
	{
		FailureReason = FText::FromString(TEXT("Skill already active"));
		return false;
	}

	return true;
}

UTireflyStateInstance* UTireflySkillComponent::CreateSkillStateInstance(FName SkillDefId, AActor* TargetActor, const FInstancedStruct& CastParameters)
{
	if (!SkillManagerSubsystem)
	{
		return nullptr;
	}

	// 获取技能定义
	FTireflyStateDefinition SkillDef = SkillManagerSubsystem->GetSkillDefinition(SkillDefId);
	if (SkillDef.StateType != ST_Skill)
	{
		return nullptr;
	}

	// 创建状态实例
	UTireflyStateInstance* StateInstance = SkillManagerSubsystem->CreateSkillStateInstance(
		GetOwner(), SkillDefId, GetOwner());
	
	if (!StateInstance)
	{
		return nullptr;
	}

	// 获取学习的技能实例
	UTireflySkillInstance* LearnedSkillInstance = GetSkillInstance(SkillDefId);
	if (LearnedSkillInstance)
	{
		// 设置技能等级
		StateInstance->SetLevel(LearnedSkillInstance->GetCurrentLevel());

		// 拍摄参数快照（对于需要快照的参数）
		LearnedSkillInstance->TakeParameterSnapshot(GetOwner(), TargetActor);

		// 同步所有参数到StateInstance
		LearnedSkillInstance->SyncParametersToStateInstance(StateInstance, true);

		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Synced SkillInstance parameters to StateInstance: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
	}
	else
	{
		// 回退到传统参数计算（如果没有SkillInstance）
		int32 SkillLevel = GetSkillLevel(SkillDefId);
		StateInstance->SetLevel(SkillLevel);
		CalculateSkillParameters(SkillDef, StateInstance, CastParameters);

		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Using legacy parameter calculation for skill: %s (no SkillInstance found)"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
	}

	return StateInstance;
}

#pragma endregion

#pragma region SkillQuery Implementation

TArray<FName> UTireflySkillComponent::GetLearnedSkills() const
{
	TArray<FName> SkillNames;
	LearnedSkillInstances.GetKeys(SkillNames);
	return SkillNames;
}

TArray<UTireflySkillInstance*> UTireflySkillComponent::GetAllSkillInstances() const
{
	TArray<UTireflySkillInstance*> SkillInstances;
	LearnedSkillInstances.GenerateValueArray(SkillInstances);
	return SkillInstances;
}

TArray<UTireflyStateInstance*> UTireflySkillComponent::GetActiveSkillStateInstances() const
{
	UTireflyStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return TArray<UTireflyStateInstance*>();
	}

	return StateComponent->GetStatesByType(ST_Skill);
}

UTireflyStateInstance* UTireflySkillComponent::GetActiveSkillStateInstance(FName SkillDefId) const
{
	UTireflyStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return nullptr;
	}

	UTireflyStateInstance* StateInstance = StateComponent->GetStateInstance(SkillDefId);
	if (StateInstance && StateInstance->GetStateDef().StateType == ST_Skill)
	{
		return StateInstance;
	}

	return nullptr;
}

bool UTireflySkillComponent::IsSkillActive(FName SkillDefId) const
{
	return GetActiveSkillStateInstance(SkillDefId) != nullptr;
}

float UTireflySkillComponent::GetSkillNumericParameter(FName SkillDefId, FName ParamName) const
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->CalculateNumericParameter(ParamName, GetOwner(), GetOwner());
	}
	return 0.0f;
}

bool UTireflySkillComponent::GetSkillBoolParameter(FName SkillDefId, FName ParamName, bool DefaultValue) const
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetBoolParameter(ParamName, DefaultValue);
	}
	return DefaultValue;
}

FVector UTireflySkillComponent::GetSkillVectorParameter(FName SkillDefId, FName ParamName, const FVector& DefaultValue) const
{
	if (UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetVectorParameter(ParamName, DefaultValue);
	}
	return DefaultValue;
}

#pragma endregion

#pragma region SkillCooldown Implementation

bool UTireflySkillComponent::IsSkillOnCooldown(FName SkillDefId) const
{
	return GetSkillCooldownRemaining(SkillDefId) > 0.0f;
}

float UTireflySkillComponent::GetSkillCooldownRemaining(FName SkillDefId) const
{
	if (const float* CooldownEndTime = SkillCooldowns.Find(SkillDefId))
	{
		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		return FMath::Max(0.0f, *CooldownEndTime - CurrentTime);
	}
	return 0.0f;
}

float UTireflySkillComponent::GetSkillCooldownTotal(FName SkillDefId) const
{
	if (const float* TotalDuration = SkillCooldownDurations.Find(SkillDefId))
	{
		return *TotalDuration;
	}
	return 0.0f;
}

void UTireflySkillComponent::SetSkillCooldown(FName SkillDefId, float CooldownDuration)
{
	if (CooldownDuration <= 0.0f)
	{
		ClearSkillCooldown(SkillDefId);
		return;
	}

	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SkillCooldowns.FindOrAdd(SkillDefId) = CurrentTime + CooldownDuration;
	SkillCooldownDurations.FindOrAdd(SkillDefId) = CooldownDuration;

	UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Set cooldown for skill %s: %.1fs"), 
		*GetOwner()->GetName(), *SkillDefId.ToString(), CooldownDuration);
}

void UTireflySkillComponent::ReduceSkillCooldown(FName SkillDefId, float CooldownReduction)
{
	if (float* CooldownEndTime = SkillCooldowns.Find(SkillDefId))
	{
		*CooldownEndTime -= CooldownReduction;
		
		// 如果冷却时间变为负数或零，清除冷却
		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (*CooldownEndTime <= CurrentTime)
		{
			ClearSkillCooldown(SkillDefId);
		}
	}
}

void UTireflySkillComponent::ClearSkillCooldown(FName SkillDefId)
{
	SkillCooldowns.Remove(SkillDefId);
	SkillCooldownDurations.Remove(SkillDefId);
}

void UTireflySkillComponent::UpdateSkillCooldowns(float DeltaTime)
{
	if (SkillCooldowns.Num() == 0)
	{
		return;
	}

	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TArray<FName> ExpiredCooldowns;

	// 检查过期的冷却
	for (const auto& CooldownPair : SkillCooldowns)
	{
		if (CooldownPair.Value <= CurrentTime)
		{
			ExpiredCooldowns.Add(CooldownPair.Key);
		}
	}

	// 移除过期的冷却
	for (FName ExpiredSkill : ExpiredCooldowns)
	{
		ClearSkillCooldown(ExpiredSkill);
		
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Skill cooldown expired: %s"), 
			*GetOwner()->GetName(), *ExpiredSkill.ToString());
	}
}

void UTireflySkillComponent::UpdateActiveSkillRealTimeParameters()
{
	if (ActiveSkillStateInstances.Num() == 0)
	{
		return; // 没有活跃技能，直接返回
	}

	// 遍历所有活跃的技能状态实例
	TArray<FName> InvalidSkills;
	int32 TotalUpdatedSkills = 0;
	
	for (auto& ActiveSkillPair : ActiveSkillStateInstances)
	{
		const FName& SkillDefId = ActiveSkillPair.Key;
		UTireflyStateInstance* ActiveStateInstance = ActiveSkillPair.Value;
		
		if (!IsValid(ActiveStateInstance))
		{
			// 如果StateInstance已经无效，标记为待移除
			InvalidSkills.Add(SkillDefId);
			continue;
		}
		
		// 获取对应的技能实例
		UTireflySkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId);
		if (!SkillInstance)
		{
			continue;
		}
		
		// 先检查是否有需要更新的参数（性能优化）
		if (SkillInstance->HasPendingRealTimeParameterUpdates(GetOwner(), GetOwner()))
		{
			// 同步实时参数到活跃的StateInstance
			SkillInstance->SyncRealTimeParametersToStateInstance(ActiveStateInstance, GetOwner(), GetOwner());
			TotalUpdatedSkills++;
		}
	}

	// 清理无效的技能跟踪
	for (const FName& InvalidSkill : InvalidSkills)
	{
		ActiveSkillStateInstances.Remove(InvalidSkill);
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Removed invalid active skill: %s"), 
			*GetOwner()->GetName(), *InvalidSkill.ToString());
	}

	if (TotalUpdatedSkills > 0)
	{
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Updated real-time parameters for %d active skills"), 
			*GetOwner()->GetName(), TotalUpdatedSkills);
	}
}

#pragma endregion

#pragma region Components Implementation

UTireflyStateComponent* UTireflySkillComponent::GetStateComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UTireflyStateComponent>() : nullptr;
}

#pragma endregion

#pragma region SkillParameters Implementation

void UTireflySkillComponent::CalculateSkillParameters(const FTireflyStateDefinition& SkillDef, UTireflyStateInstance* StateInstance, const FInstancedStruct& CastParameters)
{
	if (!StateInstance)
	{
		return;
	}

	// 遍历技能定义中的参数，根据参数类型分别处理
	for (const auto& ParamPair : SkillDef.Parameters)
	{
		const FName& ParamName = ParamPair.Key;
		const FTireflyStateParameter& ParamConfig = ParamPair.Value;

		// 根据参数类型进行不同的处理
		switch (ParamConfig.ParameterType)
		{
		case ETireflyStateParameterType::Numeric:
			{
				// 数值参数：使用StateParamParser计算
				if (ParamConfig.ParamResolverClass)
				{
					const UTireflyStateParamParser* ParamParser = ParamConfig.ParamResolverClass.GetDefaultObject();
					if (ParamParser)
					{
						float ParamValue = 0.0f;
						ParamParser->ParseStateParameter(
							GetOwner(),     // Instigator
							GetOwner(),     // Target
							StateInstance,  // StateInstance
							ParamConfig.ParamValueContainer, // InstancedStruct
							ParamValue      // OutValue
						);

						StateInstance->SetParamValue(ParamName, ParamValue);
					}
				}
			}
			break;

		case ETireflyStateParameterType::Bool:
			{
				// 布尔参数：从ParamValueContainer直接提取
				if (const bool* BoolValue = ParamConfig.ParamValueContainer.GetPtr<bool>())
				{
					StateInstance->SetBoolParam(ParamName, *BoolValue);
				}
				else
				{
					StateInstance->SetBoolParam(ParamName, false);
				}
			}
			break;

		case ETireflyStateParameterType::Vector:
			{
				// 向量参数：从ParamValueContainer直接提取
				if (const FVector* VectorValue = ParamConfig.ParamValueContainer.GetPtr<FVector>())
				{
					StateInstance->SetVectorParam(ParamName, *VectorValue);
				}
				else
				{
					StateInstance->SetVectorParam(ParamName, FVector::ZeroVector);
				}
			}
			break;

		default:
			UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Unknown parameter type for %s: %d"), 
				*GetOwner()->GetName(), *ParamName.ToString(), (int32)ParamConfig.ParameterType);
			break;
		}
	}

	// 处理额外的释放参数
	if (CastParameters.IsValid())
	{
		// 这里可以根据CastParameters的具体类型进行处理
		// 例如：如果是FSkillCastParameters结构，可以提取其中的数据
		UE_LOG(LogTcsSkill, VeryVerbose, TEXT("[%s] Processed cast parameters for skill %s"), 
			*GetOwner()->GetName(), *StateInstance->GetStateDefId().ToString());
	}
}

float UTireflySkillComponent::CalculateSkillCooldown(const FTireflyStateDefinition& SkillDef, int32 SkillLevel) const
{
	float BaseCooldown = 0.0f;

	// 尝试从技能参数中获取冷却时间
	if (const FTireflyStateParameter* CooldownParam = SkillDef.Parameters.Find(TEXT("Cooldown")))
	{
		if (CooldownParam->ParamResolverClass)
		{
			const UTireflyStateParamParser* ParamParser = CooldownParam->ParamResolverClass.GetDefaultObject();
			if (ParamParser)
			{
				// 这里简化处理，实际应该根据技能等级和属性计算
				ParamParser->ParseStateParameter(
					GetOwner(),
					GetOwner(), 
					nullptr,    // 没有StateInstance可以传null
					CooldownParam->ParamValueContainer,
					BaseCooldown
				);
			}
		}
	}

	// 如果没有找到冷却参数，使用默认值
	if (BaseCooldown <= 0.0f)
	{
		BaseCooldown = 1.0f; // 默认1秒冷却
	}

	// 根据技能等级调整冷却时间（可选）
	// 例如：技能等级越高，冷却时间可能会减少
	float LevelModifier = 1.0f - (SkillLevel - 1) * 0.05f; // 每级减少5%冷却
	LevelModifier = FMath::Clamp(LevelModifier, 0.1f, 1.0f); // 最多减少90%

	return BaseCooldown * LevelModifier;
}

#pragma endregion
