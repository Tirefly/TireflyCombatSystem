// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"



UTcsSkillComponent::UTcsSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UTcsSkillComponent::InitializeComponent()
{
	Super::InitializeComponent();

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = ResolveRuntimeBootstrapSubsystem();
	if (RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRegistered(this);
	}

	if (PrepareSkillRuntime() && RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
	}
}

void UTcsSkillComponent::UninitializeComponent()
{
	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyComponentUnregistered(this);
	}

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = nullptr;

	Super::UninitializeComponent();
}

void UTcsSkillComponent::OnUnregister()
{
	if (UTcsStateComponent* OwnerStateComponent = GetOwnerStateComponent())
	{
		UnbindOwnerStateLifecycleEvents(OwnerStateComponent);
	}

	Super::OnUnregister();
}

void UTcsSkillComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsRuntimeReady())
	{
		return;
	}

	CooldownTracker.Tick(DeltaTime);
}

bool UTcsSkillComponent::IsRuntimeReady() const
{
	const UTcsStateComponent* StateComponent = GetOwnerStateComponent();
	return bRuntimePrepared && IsValid(StateComponent) && StateComponent->IsRuntimeReady();
}

bool UTcsSkillComponent::PrepareSkillRuntime()
{
	UTcsStateComponent* StateComponent = GetOwnerStateComponent();
	bRuntimePrepared = IsValid(StateComponent) && StateComponent->IsRuntimeReady();
	if (bRuntimePrepared)
	{
		BindOwnerStateLifecycleEvents(StateComponent);
	}
	else if (IsValid(StateComponent))
	{
		UnbindOwnerStateLifecycleEvents(StateComponent);
	}

	return bRuntimePrepared;
}

UTcsRuntimeBootstrapSubsystem* UTcsSkillComponent::ResolveRuntimeBootstrapSubsystem()
{
	if (!RuntimeBootstrapSubsystem)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				RuntimeBootstrapSubsystem = GameInstance->GetSubsystem<UTcsRuntimeBootstrapSubsystem>();
			}
		}
	}

	return RuntimeBootstrapSubsystem;
}


int32 UTcsSkillComponent::AllocateSkillModifierRuntimeId()
{
	++NextSkillModifierRuntimeId;
	return NextSkillModifierRuntimeId;
}


void UTcsSkillComponent::LearnSkill(UTcsSkillDefinition* Def)
{
	if (!Def)
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillComp::LearnSkill] SkillDefinition is null"));
		return;
	}

	const FName SkillDefId = Def->GetFName();
	if (LearnedSkills.Contains(SkillDefId))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[SkillComp::LearnSkill] Skill '%s' already learned"), *SkillDefId.ToString());
		return;
	}

	const TSubclassOf<UTcsSkillEntry> EntryClass = Def->SkillEntryClass.Get() ? Def->SkillEntryClass : TSubclassOf<UTcsSkillEntry>(UTcsSkillEntry::StaticClass());
	UTcsSkillEntry* Entry = NewObject<UTcsSkillEntry>(this, EntryClass);
	Entry->SetSkillDefinition(Def);
	Entry->InitializeFromDef(Def);

	// 注入 Level 参数（运行时常量，默认等级 1）
	if (Def->LevelParamTag.IsValid())
	{
		FTcsNumericStateParamInstance LevelInst;
		LevelInst.ParamTag = Def->LevelParamTag;
		LevelInst.bIsSnapshot = false;
		LevelInst.NumericValue = 1.0f;
		Entry->NumericParamInstances.Add(Def->LevelParamTag, LevelInst);
	}

	LearnedSkills.Add(SkillDefId, Entry);

	UE_LOG(LogTcsState, Log, TEXT("[SkillComp::LearnSkill] Learned skill '%s'"), *SkillDefId.ToString());
}


void UTcsSkillComponent::ForgetSkill(FName SkillDefId)
{
	TObjectPtr<UTcsSkillEntry>* Found = LearnedSkills.Find(SkillDefId);
	if (!Found || !(*Found))
	{
		return;
	}

	UTcsSkillEntry* Entry = Found->Get();

	// 取消活跃实例
	if (Entry->ActiveInstance.IsValid())
	{
		if (UTcsStateComponent* StateCmp = GetOwnerStateComponent())
		{
			StateCmp->RequestStateRemoval(Entry->ActiveInstance.Get(), TcsStateRemovalReasons::Cancelled);
		}
		Entry->ActiveInstance.Reset();
	}

	RemoveSkillModifiersForSkillEntry(Entry);

	CooldownTracker.Remove(Entry);
	LearnedSkills.Remove(SkillDefId);
	Entry->MarkAsGarbage();

	UE_LOG(LogTcsState, Log, TEXT("[SkillComp::ForgetSkill] Forgot skill '%s'"), *SkillDefId.ToString());
}


bool UTcsSkillComponent::HasSkill(FName SkillDefId) const
{
	return LearnedSkills.Contains(SkillDefId);
}


UTcsSkillEntry* UTcsSkillComponent::GetSkillEntry(FName SkillDefId) const
{
	if (const TObjectPtr<UTcsSkillEntry>* Found = LearnedSkills.Find(SkillDefId))
	{
		return *Found;
	}
	return nullptr;
}


TArray<UTcsSkillEntry*> UTcsSkillComponent::GetAllSkillEntries() const
{
	TArray<UTcsSkillEntry*> Result;
	Result.Reserve(LearnedSkills.Num());
	for (const auto& Pair : LearnedSkills)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}


ETcsSkillActivateResult UTcsSkillComponent::ActivateSkill(FName SkillDefId, AActor* Instigator)
{
	if (!IsRuntimeReady())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[SkillComp::ActivateSkill] Skill runtime is not ready for %s"), *GetPathName());
		return ETcsSkillActivateResult::NotReady;
	}

	UTcsSkillEntry* Entry = GetSkillEntry(SkillDefId);
	if (!Entry)
	{
		return ETcsSkillActivateResult::NotLearned;
	}

	if (Entry->IsOnCooldown())
	{
		return ETcsSkillActivateResult::OnCooldown;
	}

	// 单实例：取消上一个
	if (Entry->ActiveInstance.IsValid())
	{
		if (UTcsStateComponent* StateCmp = GetOwnerStateComponent())
		{
			StateCmp->RequestStateRemoval(Entry->ActiveInstance.Get(), TcsStateRemovalReasons::Cancelled);
		}
		Entry->ActiveInstance.Reset();
	}

	UTcsSkillDefinition* Def = Entry->GetSkillDefinition();
	if (!Def || !Def->SkillInstanceClass)
	{
		return ETcsSkillActivateResult::InvalidDefinition;
	}

	UTcsSkillInstance* SkillInst = NewObject<UTcsSkillInstance>(this, Def->SkillInstanceClass);
	SkillInst->SetSkillEntry(Entry);

	UTcsStateComponent* StateCmp = GetOwnerStateComponent();
	if (!StateCmp)
	{
		return ETcsSkillActivateResult::ApplyFailed;
	}

	const bool bApplied = StateCmp->TryApplyStateInstance(SkillInst);
	if (!bApplied)
	{
		return ETcsSkillActivateResult::ApplyFailed;
	}

	if (Entry->StartCooldown(SkillInst))
	{
		CooldownTracker.Add(Entry);
	}

	Entry->ActiveInstance = SkillInst;

	return ETcsSkillActivateResult::Success;
}


UTcsStateComponent* UTcsSkillComponent::GetOwnerStateComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	return Owner->FindComponentByClass<UTcsStateComponent>();
}
