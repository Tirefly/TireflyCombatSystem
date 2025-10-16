// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateManagerSubsystem.h"
#include "Skill/TcsSkillManagerSubsystem.h"
#include "Skill/TcsSkillInstance.h"
#include "Skill/Modifiers/TcsSkillModifierParams.h"
#include "Skill/Modifiers/TcsSkillFilter.h"
#include "Skill/Modifiers/TcsSkillModifierCondition.h"
#include "Skill/Modifiers/Executions/TcsSkillModExec_CooldownMultiplier.h"
#include "Skill/Modifiers/Executions/TcsSkillModExec_CostMultiplier.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"
#include "TcsCombatSystemLogChannels.h"
#include "TcsCombatSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "State/StateParameter/TcsStateParameter.h"

UTcsSkillComponent::UTcsSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 每0.1秒更新一次冷却
}

void UTcsSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 获取技能管理器子系统
	if (UWorld* World = GetWorld())
	{
		SkillManagerSubsystem = World->GetSubsystem<UTcsSkillManagerSubsystem>();
		StateManagerSubsystem = World->GetSubsystem<UTcsStateManagerSubsystem>();
	}

	if (UTcsStateComponent* StateComponent = GetStateComponent())
	{
		CachedStateComponent = StateComponent;
		StateComponent->OnStateStageChanged.AddDynamic(this, &UTcsSkillComponent::HandleStateStageChanged);

		// Prime active skill mapping for already-active states
		const TArray<UTcsStateInstance*> ExistingSkillStates = StateComponent->GetStatesByType(ST_Skill);
		for (UTcsStateInstance* StateInstance : ExistingSkillStates)
		{
			if (!IsValid(StateInstance))
			{
				continue;
			}

			const ETcsStateStage Stage = StateInstance->GetCurrentStage();
			if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
			{
				ActiveSkillStateInstances.FindOrAdd(StateInstance->GetStateDefId()) = StateInstance;
			}
		}
	}
}

void UTcsSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 更新技能冷却
	UpdateSkillCooldowns(DeltaTime);

	// 更新技能修改器聚合结果
	UpdateSkillModifiers();
	
	// 更新活跃技能的实时参数
	UpdateActiveSkillRealTimeParameters();
}

void UTcsSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTcsStateComponent* StateComponent = CachedStateComponent.Get())
	{
		StateComponent->OnStateStageChanged.RemoveDynamic(this, &UTcsSkillComponent::HandleStateStageChanged);
		CachedStateComponent = nullptr;
	}

	ActiveSkillStateInstances.Empty();

	Super::EndPlay(EndPlayReason);
}

#pragma region SkillLearning Implementation

UTcsSkillInstance* UTcsSkillComponent::LearnSkill(FName SkillDefId, int32 InitialLevel)
{
	if (SkillDefId.IsNone() || InitialLevel <= 0)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Invalid skill parameters: SkillDefId=%s, Level=%d"), 
			*FString(__FUNCTION__), *SkillDefId.ToString(), InitialLevel);
		return nullptr;
	}

	// 检查是否已经学会该技能
	if (UTcsSkillInstance* ExistingSkill = LearnedSkillInstances.FindRef(SkillDefId))
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
	UTcsSkillInstance* NewSkillInstance = CreateSkillInstance(SkillDefId, InitialLevel);
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

void UTcsSkillComponent::ForgetSkill(FName SkillDefId)
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		// 移除相关技能修改器
		const int32 RemovedCount = ActiveSkillModifiers.RemoveAll([SkillInstance](const FTcsSkillModifierInstance& Instance)
		{
			return Instance.SkillInstance == SkillInstance;
		});

		if (RemovedCount > 0)
		{
			RebuildAggregatedForSkill(SkillInstance);
			SkillParamEffectsByName.Remove(SkillInstance);
			SkillParamEffectsByTag.Remove(SkillInstance);
			UpdateSkillModifiers();
		}

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

bool UTcsSkillComponent::HasLearnedSkill(FName SkillDefId) const
{
	return LearnedSkillInstances.Contains(SkillDefId);
}

UTcsSkillInstance* UTcsSkillComponent::GetSkillInstance(FName SkillDefId) const
{
	return LearnedSkillInstances.FindRef(SkillDefId);
}

int32 UTcsSkillComponent::GetSkillLevel(FName SkillDefId) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetCurrentLevel();
	}
	return 0;
}

bool UTcsSkillComponent::UpgradeSkillInstance(FName SkillDefId, int32 LevelIncrease)
{
	if (!HasLearnedSkill(SkillDefId) || LevelIncrease <= 0)
	{
		return false;
	}

	UTcsSkillInstance* SkillInstance = LearnedSkillInstances[SkillDefId];
	if (SkillInstance)
	{
		SkillInstance->UpgradeLevel(LevelIncrease);
		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Upgraded skill: %s to Level %d"), 
			*GetOwner()->GetName(), *SkillDefId.ToString(), SkillInstance->GetCurrentLevel());
		return true;
	}

	return false;
}

UTcsSkillInstance* UTcsSkillComponent::CreateSkillInstance(FName SkillDefId, int32 InitialLevel)
{
	if (!SkillManagerSubsystem)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] SkillManagerSubsystem is not available"), *FString(__FUNCTION__));
		return nullptr;
	}

	// 获取技能定义
	FTcsStateDefinition SkillDef = SkillManagerSubsystem->GetSkillDefinition(SkillDefId);
	if (SkillDef.StateType != ST_Skill)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Invalid skill definition: %s is not a skill type"), 
			*FString(__FUNCTION__), *SkillDefId.ToString());
		return nullptr;
	}

	// 创建技能实例
	UTcsSkillInstance* NewSkillInstance = NewObject<UTcsSkillInstance>(this);
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

bool UTcsSkillComponent::TryCastSkill(FName SkillDefId, AActor* TargetActor, const FInstancedStruct& CastParameters)
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
	UTcsStateInstance* SkillInstance = CreateSkillStateInstance(SkillDefId, TargetActor, CastParameters);
	if (!SkillInstance)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Failed to create skill state instance for %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
		return false;
	}

	// 获取状态组件（阶段3要求：技能状态统一走槽位管线）
	UTcsStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] No StateComponent found"), *GetOwner()->GetName());
		return false;
	}

	const FTcsStateDefinition& SkillDef = SkillInstance->GetStateDef();
	const bool bHasSlotTag = SkillDef.StateSlotType.IsValid();

	if (!bHasSlotTag && !WarnedSkillsMissingSlot.Contains(SkillDefId))
	{
		WarnedSkillsMissingSlot.Add(SkillDefId);
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Skill definition %s is missing StateSlotType; Stage3 slot pipeline requires a valid slot tag. Falling back to direct activation."),
			*GetOwner()->GetName(),
			*SkillDefId.ToString());
	}

	bool bApplied = false;

	if (StateManagerSubsystem)
	{
		bApplied = StateManagerSubsystem->ApplyStateInstanceToSlot(GetOwner(), SkillInstance, SkillDef.StateSlotType, /*bAllowFallback=*/true);
		if (!bApplied)
		{
			UE_LOG(LogTcsSkill, Warning, TEXT("[%s] StateManagerSubsystem failed to apply skill %s, falling back to local handler."),
				*GetOwner()->GetName(), *SkillDefId.ToString());
		}
	}

	if (!bApplied)
	{
		// 手工挂接到状态组件（Stage3：确保走槽位 or 回退直接激活）
		StateComponent->AddStateInstance(SkillInstance);

		SkillInstance->InitializeStateTree();
		SkillInstance->SetCurrentStage(ETcsStateStage::SS_Inactive);

		bool bAssigned = false;
		if (bHasSlotTag)
		{
			bAssigned = StateComponent->AssignStateToStateSlot(SkillInstance, SkillDef.StateSlotType);
			if (!bAssigned)
			{
				UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Failed to assign skill %s to slot %s, activating directly."),
					*GetOwner()->GetName(), *SkillDefId.ToString(), *SkillDef.StateSlotType.ToString());
			}
		}

		if (!bAssigned)
		{
			SkillInstance->StartStateTree();
			const ETcsStateStage PreviousStage = SkillInstance->GetCurrentStage();
			SkillInstance->SetCurrentStage(ETcsStateStage::SS_Active);
			StateComponent->NotifyStateStageChanged(SkillInstance, PreviousStage, ETcsStateStage::SS_Active);
		}

		bApplied = true;
	}

	if (!bApplied)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Unable to apply skill state for %s"), *GetOwner()->GetName(), *SkillDefId.ToString());
		return false;
	}

	bool bAssignedViaSlot = false;
	if (bHasSlotTag)
	{
		const TArray<UTcsStateInstance*> SlotStates = StateComponent->GetAllStatesInStateSlot(SkillDef.StateSlotType);
		bAssignedViaSlot = SlotStates.Contains(SkillInstance);
	}

	// 获取技能实例并计算冷却时间
	float CooldownDuration = 0.0f;
	UTcsSkillInstance* LearnedSkillInstance = GetSkillInstance(SkillDefId);
	if (LearnedSkillInstance)
	{
		// 使用SkillInstance计算冷却时间
		CooldownDuration = LearnedSkillInstance->CalculateNumericParameter(TEXT("Cooldown"), GetOwner(), TargetActor);
		if (CooldownDuration <= 0.0f)
		{
			// 如果没有定义冷却参数，使用传统方法计算
			CooldownDuration = CalculateSkillCooldown(SkillDef, LearnedSkillInstance->GetCurrentLevel());
		}
	}
	else
	{
		// 回退到传统计算方法
		CooldownDuration = CalculateSkillCooldown(SkillDef, GetSkillLevel(SkillDefId));
	}
	
	SetSkillCooldown(SkillDefId, CooldownDuration);

	UE_LOG(LogTcsSkill, Log, TEXT("[%s] Successfully cast skill: %s (Cooldown: %.1fs)%s"), 
		*GetOwner()->GetName(), 
		*SkillDefId.ToString(), 
		CooldownDuration,
		bAssignedViaSlot ? TEXT(" via slot pipeline") : TEXT(""));

	return true;
}

void UTcsSkillComponent::CancelSkill(FName SkillDefId)
{
	UTcsStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return;
	}

	UTcsStateInstance* SkillInstance = StateComponent->GetStateInstance(SkillDefId);
	if (SkillInstance && SkillInstance->GetStateDef().StateType == ST_Skill)
	{
		// 停止StateTree执行
		SkillInstance->StopStateTree();

		// 设置状态为到期
		const ETcsStateStage PreviousStage = SkillInstance->GetCurrentStage();
		SkillInstance->SetCurrentStage(ETcsStateStage::SS_Expired);
		StateComponent->NotifyStateStageChanged(SkillInstance, PreviousStage, ETcsStateStage::SS_Expired);

		// 从槽位中移除
		StateComponent->RemoveStateInstanceFromStateSlot(SkillInstance);

		// 从状态管理中移除
		StateComponent->RemoveStateInstance(SkillInstance);

		UE_LOG(LogTcsSkill, Log, TEXT("[%s] Cancelled skill: %s"), 
			*GetOwner()->GetName(), *SkillDefId.ToString());
	}
}

void UTcsSkillComponent::CancelAllSkills()
{
	UTcsStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return;
	}

	TArray<UTcsStateInstance*> SkillInstances = StateComponent->GetStatesByType(ST_Skill);
	
	for (UTcsStateInstance* SkillInstance : SkillInstances)
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

bool UTcsSkillComponent::CanCastSkill(FName SkillDefId, AActor* TargetActor) const
{
	FText FailureReason;
	return ValidateSkillCast(SkillDefId, TargetActor, FailureReason);
}

bool UTcsSkillComponent::ValidateSkillCast(FName SkillDefId, AActor* TargetActor, FText& FailureReason) const
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

UTcsStateInstance* UTcsSkillComponent::CreateSkillStateInstance(FName SkillDefId, AActor* TargetActor, const FInstancedStruct& CastParameters)
{
	if (!SkillManagerSubsystem)
	{
		return nullptr;
	}

	// 获取技能定义
	FTcsStateDefinition SkillDef = SkillManagerSubsystem->GetSkillDefinition(SkillDefId);
	if (SkillDef.StateType != ST_Skill)
	{
		return nullptr;
	}

	// 创建状态实例
	UTcsStateInstance* StateInstance = SkillManagerSubsystem->CreateSkillStateInstance(
		GetOwner(), SkillDefId, GetOwner());
	
	if (!StateInstance)
	{
		return nullptr;
	}

	// 获取学习的技能实例
	UTcsSkillInstance* LearnedSkillInstance = GetSkillInstance(SkillDefId);
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

TArray<FName> UTcsSkillComponent::GetLearnedSkills() const
{
	TArray<FName> SkillNames;
	LearnedSkillInstances.GetKeys(SkillNames);
	return SkillNames;
}

TArray<UTcsSkillInstance*> UTcsSkillComponent::GetAllSkillInstances() const
{
	TArray<UTcsSkillInstance*> SkillInstances;
	LearnedSkillInstances.GenerateValueArray(SkillInstances);
	return SkillInstances;
}

TArray<UTcsStateInstance*> UTcsSkillComponent::GetActiveSkillStateInstances() const
{
	UTcsStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return TArray<UTcsStateInstance*>();
	}

	return StateComponent->GetStatesByType(ST_Skill);
}

UTcsStateInstance* UTcsSkillComponent::GetActiveSkillStateInstance(FName SkillDefId) const
{
	if (UTcsStateInstance* const* FoundInstance = ActiveSkillStateInstances.Find(SkillDefId))
	{
		UTcsStateInstance* StateInstance = *FoundInstance;
		if (IsValid(StateInstance))
		{
			const ETcsStateStage Stage = StateInstance->GetCurrentStage();
			if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
			{
				return StateInstance;
			}
		}
	}

	UTcsStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return nullptr;
	}

	UTcsStateInstance* StateInstance = StateComponent->GetStateInstance(SkillDefId);
	if (StateInstance && StateInstance->GetStateDef().StateType == ST_Skill)
	{
		const ETcsStateStage Stage = StateInstance->GetCurrentStage();
		if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
		{
			return StateInstance;
		}
	}

	return nullptr;
}

bool UTcsSkillComponent::IsSkillActive(FName SkillDefId) const
{
	return GetActiveSkillStateInstance(SkillDefId) != nullptr;
}

float UTcsSkillComponent::GetSkillNumericParameter(FName SkillDefId, FName ParamName) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->CalculateNumericParameter(ParamName, GetOwner(), GetOwner());
	}
	return 0.0f;
}

bool UTcsSkillComponent::GetSkillBoolParameter(FName SkillDefId, FName ParamName, bool DefaultValue) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetBoolParameter(ParamName, DefaultValue);
	}
	return DefaultValue;
}

FVector UTcsSkillComponent::GetSkillVectorParameter(FName SkillDefId, FName ParamName, const FVector& DefaultValue) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetVectorParameter(ParamName, DefaultValue);
	}
	return DefaultValue;
}

float UTcsSkillComponent::GetSkillNumericParameterByTag(FName SkillDefId, FGameplayTag ParamTag) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->CalculateNumericParameterByTag(ParamTag, GetOwner(), GetOwner());
	}
	return 0.0f;
}

bool UTcsSkillComponent::GetSkillBoolParameterByTag(FName SkillDefId, FGameplayTag ParamTag, bool DefaultValue) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetBoolParameterByTag(ParamTag, DefaultValue);
	}
	return DefaultValue;
}

FVector UTcsSkillComponent::GetSkillVectorParameterByTag(FName SkillDefId, FGameplayTag ParamTag, const FVector& DefaultValue) const
{
	if (UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId))
	{
		return SkillInstance->GetVectorParameterByTag(ParamTag, DefaultValue);
	}
	return DefaultValue;
}

#pragma endregion

#pragma region SkillCooldown Implementation

bool UTcsSkillComponent::IsSkillOnCooldown(FName SkillDefId) const
{
	return GetSkillCooldownRemaining(SkillDefId) > 0.0f;
}

float UTcsSkillComponent::GetSkillCooldownRemaining(FName SkillDefId) const
{
	if (const float* CooldownEndTime = SkillCooldowns.Find(SkillDefId))
	{
		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		return FMath::Max(0.0f, *CooldownEndTime - CurrentTime);
	}
	return 0.0f;
}

float UTcsSkillComponent::GetSkillCooldownTotal(FName SkillDefId) const
{
	if (const float* TotalDuration = SkillCooldownDurations.Find(SkillDefId))
	{
		return *TotalDuration;
	}
	return 0.0f;
}

void UTcsSkillComponent::SetSkillCooldown(FName SkillDefId, float CooldownDuration)
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

void UTcsSkillComponent::ReduceSkillCooldown(FName SkillDefId, float CooldownReduction)
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

void UTcsSkillComponent::ClearSkillCooldown(FName SkillDefId)
{
	SkillCooldowns.Remove(SkillDefId);
	SkillCooldownDurations.Remove(SkillDefId);
}

void UTcsSkillComponent::UpdateSkillCooldowns(float DeltaTime)
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

void UTcsSkillComponent::UpdateActiveSkillRealTimeParameters()
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
		UTcsStateInstance* ActiveStateInstance = ActiveSkillPair.Value;
		
		if (!IsValid(ActiveStateInstance))
		{
			// 如果StateInstance已经无效，标记为待移除
			InvalidSkills.Add(SkillDefId);
			continue;
		}
		
		// 获取对应的技能实例
		UTcsSkillInstance* SkillInstance = LearnedSkillInstances.FindRef(SkillDefId);
		if (!SkillInstance)
		{
			continue;
		}

		if (ActiveStateInstance->GetCurrentStage() != ETcsStateStage::SS_Active)
		{
			continue;
		}
		
		// 先检查是否有需要更新的参数（性能优化）
		if (SkillInstance->HasPendingParameterUpdates(GetOwner(), GetOwner()))
		{
			// 同步实时参数到活跃的StateInstance
			SkillInstance->SyncRealtimeParametersToStateInstance(ActiveStateInstance, GetOwner(), GetOwner());
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

void UTcsSkillComponent::HandleStateStageChanged(UTcsStateComponent* StateComponent, UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (StateComponent != CachedStateComponent.Get())
	{
		return;
	}

	if (!IsValid(StateInstance) || StateInstance->GetStateDef().StateType != ST_Skill)
	{
		return;
	}

	const FName SkillDefId = StateInstance->GetStateDefId();
	const bool bShouldTrack = (NewStage == ETcsStateStage::SS_Active || NewStage == ETcsStateStage::SS_HangUp);

	if (bShouldTrack)
	{
		ActiveSkillStateInstances.FindOrAdd(SkillDefId) = StateInstance;
	}
	else
	{
		ActiveSkillStateInstances.Remove(SkillDefId);
	}
}

UTcsStateComponent* UTcsSkillComponent::GetStateComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UTcsStateComponent>() : nullptr;
}

#pragma endregion

#pragma region SkillParameters Implementation

void UTcsSkillComponent::CalculateSkillParameters(const FTcsStateDefinition& SkillDef, UTcsStateInstance* StateInstance, const FInstancedStruct& CastParameters)
{
	if (!StateInstance)
	{
		return;
	}

	// 遍历技能定义中的参数，根据参数类型分别处理
	for (const auto& ParamPair : SkillDef.Parameters)
	{
		const FName& ParamName = ParamPair.Key;
		const FTcsStateParameter& ParamConfig = ParamPair.Value;

		// 根据参数类型进行不同的处理
		switch (ParamConfig.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				// 数值参数：使用StateParamParser计算
				if (ParamConfig.ParamEvaluatorType)
				{
					const UTcsStateParamEvaluator* ParamParser = ParamConfig.ParamEvaluatorType.GetDefaultObject();
					if (ParamParser)
					{
						float ParamValue = 0.0f;
						ParamParser->Evaluate(
							GetOwner(),     // Instigator
							GetOwner(),     // Target
							StateInstance,  // StateInstance
							ParamConfig.ParamValueContainer, // InstancedStruct
							ParamValue      // OutValue
						);

						StateInstance->SetNumericParam(ParamName, ParamValue);
					}
				}
			}
			break;

		case ETcsStateParameterType::SPT_Bool:
			{
				// 布尔参数：从ParamValueContainer直接提取
				// TODO: 修复FInstancedStruct对bool类型的支持问题
				// if (const bool* BoolValue = ParamConfig.ParamValueContainer.GetPtr<bool>())
				// {
				// 	StateInstance->SetBoolParam(ParamName, *BoolValue);
				// }
				// else
				{
					StateInstance->SetBoolParam(ParamName, false); // 默认值
				}
			}
			break;

		case ETcsStateParameterType::SPT_Vector:
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

	// 处理 Tag 通道参数
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
				StateInstance->SetBoolParamByTag(ParamTag, false);
			}
			break;

		case ETcsStateParameterType::SPT_Vector:
			{
				if (const FVector* VectorValue = ParamConfig.ParamValueContainer.GetPtr<FVector>())
				{
					StateInstance->SetVectorParamByTag(ParamTag, *VectorValue);
				}
				else
				{
					StateInstance->SetVectorParamByTag(ParamTag, FVector::ZeroVector);
				}
			}
			break;

		default:
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

float UTcsSkillComponent::CalculateSkillCooldown(const FTcsStateDefinition& SkillDef, int32 SkillLevel) const
{
	float BaseCooldown = 0.0f;

	// 尝试从技能参数中获取冷却时间
	if (const FTcsStateParameter* CooldownParam = SkillDef.Parameters.Find(TEXT("Cooldown")))
	{
		if (CooldownParam->ParamEvaluatorType)
		{
			const UTcsStateParamEvaluator* ParamParser = CooldownParam->ParamEvaluatorType.GetDefaultObject();
			if (ParamParser)
			{
				// 这里简化处理，实际应该根据技能等级和属性计算
				ParamParser->Evaluate(
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
#pragma region SkillModifiers Implementation

namespace
{
static bool AccumulateEffectFromDefinitionByName(const FTcsSkillModifierInstance& Instance, FName ParamName, FTcsAggregatedParamEffect& InOutEffect)
{
	if (ParamName.IsNone())
	{
		return false;
	}

	const FInstancedStruct& Payload = Instance.ModifierDef.ModifierParameter.ParamValueContainer;
	if (!Payload.IsValid())
	{
		return false;
	}

	bool bModified = false;

	if (Payload.GetScriptStruct() == FTcsModParam_Additive::StaticStruct())
	{
		if (const FTcsModParam_Additive* Params = Payload.GetPtr<FTcsModParam_Additive>())
		{
			if (Params->ParamName == ParamName)
			{
				InOutEffect.AddSum += Params->Magnitude;
				bModified = true;
			}
		}
	}
	else if (Payload.GetScriptStruct() == FTcsModParam_Multiplicative::StaticStruct())
	{
		if (const FTcsModParam_Multiplicative* Params = Payload.GetPtr<FTcsModParam_Multiplicative>())
		{
			if (Params->ParamName == ParamName)
			{
				InOutEffect.MulProd *= Params->Multiplier;
				bModified = true;
			}
		}
	}

	else if (Payload.GetScriptStruct() == FTcsModParam_Scalar::StaticStruct())
	{
		if (const FTcsModParam_Scalar* Params = Payload.GetPtr<FTcsModParam_Scalar>())
		{
			if (Instance.ModifierDef.ExecutionType && Instance.ModifierDef.ExecutionType->IsChildOf(UTcsSkillModExec_CooldownMultiplier::StaticClass()) && ParamName == TEXT("Cooldown"))
			{
				InOutEffect.CooldownMultiplier *= Params->Value;
				bModified = true;
			}
			else if (Instance.ModifierDef.ExecutionType && Instance.ModifierDef.ExecutionType->IsChildOf(UTcsSkillModExec_CostMultiplier::StaticClass()) && ParamName.ToString().Contains(TEXT("Cost")))
			{
				InOutEffect.CostMultiplier *= Params->Value;
				bModified = true;
			}
		}
	}

	return bModified;
}

static bool AccumulateEffectFromDefinitionByTag(const FTcsSkillModifierInstance& Instance, const FGameplayTag& ParamTag, FTcsAggregatedParamEffect& InOutEffect)
{
	if (!ParamTag.IsValid())
	{
		return false;
	}

	const FInstancedStruct& Payload = Instance.ModifierDef.ModifierParameter.ParamValueContainer;
	if (!Payload.IsValid())
	{
		return false;
	}

	bool bModified = false;

	if (Payload.GetScriptStruct() == FTcsModParam_Additive::StaticStruct())
	{
		if (const FTcsModParam_Additive* Params = Payload.GetPtr<FTcsModParam_Additive>())
		{
			if (Params->ParamTag == ParamTag)
			{
				InOutEffect.AddSum += Params->Magnitude;
				bModified = true;
			}
		}
	}
	else if (Payload.GetScriptStruct() == FTcsModParam_Multiplicative::StaticStruct())
	{
		if (const FTcsModParam_Multiplicative* Params = Payload.GetPtr<FTcsModParam_Multiplicative>())
		{
			if (Params->ParamTag == ParamTag)
			{
				InOutEffect.MulProd *= Params->Multiplier;
				bModified = true;
			}
		}
	}
	else if (Payload.GetScriptStruct() == FTcsModParam_Scalar::StaticStruct())
	{
		if (const FTcsModParam_Scalar* Params = Payload.GetPtr<FTcsModParam_Scalar>())
		{
			if (Instance.ModifierDef.ExecutionType && Instance.ModifierDef.ExecutionType->IsChildOf(UTcsSkillModExec_CooldownMultiplier::StaticClass()))
			{
				InOutEffect.CooldownMultiplier *= Params->Value;
				bModified = true;
			}
			else if (Instance.ModifierDef.ExecutionType && Instance.ModifierDef.ExecutionType->IsChildOf(UTcsSkillModExec_CostMultiplier::StaticClass()))
			{
				InOutEffect.CostMultiplier *= Params->Value;
				bModified = true;
			}
		}
	}

	return bModified;
}

} // namespace

bool UTcsSkillComponent::ApplySkillModifiers(const TArray<FTcsSkillModifierDefinition>& Modifiers, TArray<int32>& OutInstanceIds)
{
	bool bApplied = false;
	OutInstanceIds.Reset();

	if (Modifiers.IsEmpty())
	{
		return false;
	}

	const int64 Timestamp = FDateTime::UtcNow().ToUnixTimestamp();

	for (const FTcsSkillModifierDefinition& Definition : Modifiers)
	{
		TArray<UTcsSkillInstance*> TargetSkills;
		RunFilter(Definition, TargetSkills);

		if (TargetSkills.IsEmpty())
		{
			continue;
		}

		for (UTcsSkillInstance* SkillInstance : TargetSkills)
		{
			if (!IsValid(SkillInstance))
			{
				continue;
			}

			FTcsSkillModifierInstance PreviewInstance;
			PreviewInstance.ModifierDef = Definition;
			PreviewInstance.SkillInstance = SkillInstance;
			PreviewInstance.SkillModInstanceId = SkillModifierInstanceIdMgr + 1;
			PreviewInstance.ApplyTime = Timestamp;
			PreviewInstance.UpdateTime = Timestamp;

			UTcsStateInstance* ActiveState = ActiveSkillStateInstances.FindRef(SkillInstance->GetSkillDefId());
			if (!EvaluateConditions(GetOwner(), SkillInstance, ActiveState, PreviewInstance))
			{
				continue;
			}

			FTcsSkillModifierInstance ModifierInstance;
			ModifierInstance.ModifierDef = Definition;
			ModifierInstance.SkillInstance = SkillInstance;
			ModifierInstance.SkillModInstanceId = ++SkillModifierInstanceIdMgr;
			ModifierInstance.ApplyTime = Timestamp;
			ModifierInstance.UpdateTime = Timestamp;

			ActiveSkillModifiers.Add(ModifierInstance);
			OutInstanceIds.Add(ModifierInstance.SkillModInstanceId);
			MarkSkillParamDirty(SkillInstance, NAME_None);
			MarkSkillParamDirtyByTag(SkillInstance, FGameplayTag());
			bApplied = true;
		}
	}

	if (bApplied)
	{
		MergeAndSortModifiers(ActiveSkillModifiers);
		UpdateSkillModifiers();
	}

	return bApplied;
}

bool UTcsSkillComponent::RemoveSkillModifierById(int32 InstanceId)
{
	for (int32 Index = 0; Index < ActiveSkillModifiers.Num(); ++Index)
	{
		if (ActiveSkillModifiers[Index].SkillModInstanceId == InstanceId)
		{
			UTcsSkillInstance* SkillInstance = ActiveSkillModifiers[Index].SkillInstance;
			ActiveSkillModifiers.RemoveAt(Index);
			MergeAndSortModifiers(ActiveSkillModifiers);
			MarkSkillParamDirty(SkillInstance, NAME_None);
			MarkSkillParamDirtyByTag(SkillInstance, FGameplayTag());
			UpdateSkillModifiers();
			return true;
		}
	}

	return false;
}

void UTcsSkillComponent::UpdateSkillModifiers()
{
	for (auto It = SkillParamEffectsByName.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = SkillParamEffectsByTag.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	// 当前实现按需清理缓存；聚合结果在查询时实时重建
	for (TPair<const UTcsSkillInstance*, FTcsSkillParamEffectByName>& Pair : SkillParamEffectsByName)
	{
		if (Pair.Value.DirtyParams.Contains(NAME_None))
		{
			Pair.Value.AggregatedEffects.Empty();
		}
		else
		{
			for (const FName& ParamName : Pair.Value.DirtyParams)
			{
				Pair.Value.AggregatedEffects.Remove(ParamName);
			}
		}
		Pair.Value.DirtyParams.Empty();
	}

	for (TPair<const UTcsSkillInstance*, FTcsSkillParamEffectByTag>& Pair : SkillParamEffectsByTag)
	{
		if (Pair.Value.DirtyParams.Contains(FGameplayTag()))
		{
			Pair.Value.AggregatedEffects.Empty();
		}
		else
		{
			for (const FGameplayTag& ParamTag : Pair.Value.DirtyParams)
			{
				Pair.Value.AggregatedEffects.Remove(ParamTag);
			}
		}
		Pair.Value.DirtyParams.Empty();
	}
}

bool UTcsSkillComponent::GetAggregatedParamEffect(const UTcsSkillInstance* Skill, FName ParamName, FTcsAggregatedParamEffect& OutEffect) const
{
	OutEffect = FTcsAggregatedParamEffect();
	if (!IsValid(Skill) || ParamName.IsNone())
	{
		return false;
	}

	FTcsSkillParamEffectByName& EffectContainer = SkillParamEffectsByName.FindOrAdd(Skill);
	if (const FTcsAggregatedParamEffect* CachedEffect = EffectContainer.FindEffect(ParamName))
	{
		OutEffect = *CachedEffect;
		return true;
	}

	FTcsAggregatedParamEffect Effect = BuildAggregatedEffect(Skill, ParamName);
	EffectContainer.SetEffect(ParamName, Effect);
	OutEffect = Effect;

	return !FMath::IsNearlyZero(Effect.AddSum) ||
		!FMath::IsNearlyEqual(Effect.MulProd, 1.f) ||
		Effect.bHasOverride ||
		!FMath::IsNearlyEqual(Effect.CooldownMultiplier, 1.f) ||
		!FMath::IsNearlyEqual(Effect.CostMultiplier, 1.f);
}

bool UTcsSkillComponent::GetAggregatedParamEffectByTag(const UTcsSkillInstance* Skill, FGameplayTag ParamTag, FTcsAggregatedParamEffect& OutEffect) const
{
	OutEffect = FTcsAggregatedParamEffect();
	if (!IsValid(Skill) || !ParamTag.IsValid())
	{
		return false;
	}

	FTcsSkillParamEffectByTag& EffectContainer = SkillParamEffectsByTag.FindOrAdd(Skill);
	if (const FTcsAggregatedParamEffect* CachedEffect = EffectContainer.FindEffect(ParamTag))
	{
		OutEffect = *CachedEffect;
		return true;
	}

	FTcsAggregatedParamEffect Effect = BuildAggregatedEffectByTag(Skill, ParamTag);
	EffectContainer.SetEffect(ParamTag, Effect);
	OutEffect = Effect;

	return !FMath::IsNearlyZero(Effect.AddSum) ||
		!FMath::IsNearlyEqual(Effect.MulProd, 1.f) ||
		Effect.bHasOverride ||
		!FMath::IsNearlyEqual(Effect.CooldownMultiplier, 1.f) ||
		!FMath::IsNearlyEqual(Effect.CostMultiplier, 1.f);
}

void UTcsSkillComponent::MarkSkillParamDirty(UTcsSkillInstance* Skill, FName ParamName)
{
	if (!IsValid(Skill))
	{
		return;
	}

	FTcsSkillParamEffectByName& EffectContainer = SkillParamEffectsByName.FindOrAdd(Skill);

	if (ParamName.IsNone())
	{
		EffectContainer.AggregatedEffects.Empty();
		EffectContainer.MarkDirty(NAME_None);
	}
	else
	{
		EffectContainer.AggregatedEffects.Remove(ParamName);
		EffectContainer.MarkDirty(ParamName);
	}
}

void UTcsSkillComponent::MarkSkillParamDirtyByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag)
{
	if (!IsValid(Skill))
	{
		return;
	}

	FTcsSkillParamEffectByTag& EffectContainer = SkillParamEffectsByTag.FindOrAdd(Skill);

	if (!ParamTag.IsValid())
	{
		EffectContainer.AggregatedEffects.Empty();
		EffectContainer.MarkDirty(FGameplayTag());
	}
	else
	{
		EffectContainer.AggregatedEffects.Remove(ParamTag);
		EffectContainer.MarkDirty(ParamTag);
	}
}

void UTcsSkillComponent::RebuildAggregatedForSkill(UTcsSkillInstance* Skill)
{
	if (!IsValid(Skill))
	{
		return;
	}

	SkillParamEffectsByName.Remove(Skill);
	SkillParamEffectsByTag.Remove(Skill);
}

void UTcsSkillComponent::RebuildAggregatedForSkillParam(UTcsSkillInstance* Skill, FName ParamName)
{
	if (!IsValid(Skill))
	{
		return;
	}

	if (FTcsSkillParamEffectByName* EffectContainer = SkillParamEffectsByName.Find(Skill))
	{
		EffectContainer->RemoveParam(ParamName);
	}
}

void UTcsSkillComponent::RebuildAggregatedForSkillParamByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag)
{
	if (!IsValid(Skill))
	{
		return;
	}

	if (FTcsSkillParamEffectByTag* EffectContainer = SkillParamEffectsByTag.Find(Skill))
	{
		EffectContainer->RemoveParam(ParamTag);
	}
}

void UTcsSkillComponent::RunFilter(const FTcsSkillModifierDefinition& Definition, TArray<UTcsSkillInstance*>& OutSkills) const
{
	OutSkills.Reset();

	if (Definition.SkillFilter)
	{
		if (UTcsSkillFilter* Filter = Definition.SkillFilter->GetDefaultObject<UTcsSkillFilter>())
		{
			Filter->Filter(GetOwner(), OutSkills);
		}
	}

	if (OutSkills.IsEmpty())
	{
		for (const TPair<FName, UTcsSkillInstance*>& Pair : LearnedSkillInstances)
		{
			if (IsValid(Pair.Value))
			{
				OutSkills.AddUnique(Pair.Value);
			}
		}
	}
}

bool UTcsSkillComponent::EvaluateConditions(AActor* Owner, UTcsSkillInstance* Skill, UTcsStateInstance* ActiveState, const FTcsSkillModifierInstance& Instance) const
{
	for (TSubclassOf<UTcsSkillModifierCondition> ConditionClass : Instance.ModifierDef.ActiveConditions)
	{
		if (!ConditionClass)
		{
			continue;
		}

		const UTcsSkillModifierCondition* Condition = ConditionClass->GetDefaultObject<UTcsSkillModifierCondition>();
		if (!Condition)
		{
			continue;
		}

		if (!Condition->Evaluate(Owner, Skill, ActiveState, Instance))
		{
			return false;
		}
	}

	return true;
}

void UTcsSkillComponent::MergeAndSortModifiers(TArray<FTcsSkillModifierInstance>& InOutModifiers) const
{
	InOutModifiers.Sort([](const FTcsSkillModifierInstance& A, const FTcsSkillModifierInstance& B)
	{
		return A.ModifierDef.Priority < B.ModifierDef.Priority;
	});
}

FTcsAggregatedParamEffect UTcsSkillComponent::BuildAggregatedEffect(const UTcsSkillInstance* Skill, const FName& ParamName) const
{
	FTcsAggregatedParamEffect Result;
	Result.MulProd = 1.f;
	Result.CooldownMultiplier = 1.f;
	Result.CostMultiplier = 1.f;

	if (!IsValid(Skill))
	{
		return Result;
	}

	for (const FTcsSkillModifierInstance& Instance : ActiveSkillModifiers)
	{
		if (Instance.SkillInstance != Skill)
		{
			continue;
		}

		AccumulateEffectFromDefinitionByName(Instance, ParamName, Result);
	}

	return Result;
}

FTcsAggregatedParamEffect UTcsSkillComponent::BuildAggregatedEffectByTag(const UTcsSkillInstance* Skill, const FGameplayTag& ParamTag) const
{
	FTcsAggregatedParamEffect Result;
	Result.MulProd = 1.f;
	Result.CooldownMultiplier = 1.f;
	Result.CostMultiplier = 1.f;

	if (!IsValid(Skill))
	{
		return Result;
	}

	for (const FTcsSkillModifierInstance& Instance : ActiveSkillModifiers)
	{
		if (Instance.SkillInstance != Skill)
		{
			continue;
		}

		AccumulateEffectFromDefinitionByTag(Instance, ParamTag, Result);
	}

	return Result;
}

#pragma endregion
