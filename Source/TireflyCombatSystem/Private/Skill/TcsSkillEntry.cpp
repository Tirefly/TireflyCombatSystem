// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillEntry.h"

#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillInstance.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "TcsLogChannels.h"



UTcsSkillEntry::UTcsSkillEntry()
{
}

UWorld* UTcsSkillEntry::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}


int32 UTcsSkillEntry::GetLevel() const
{
	if (!SkillDefinition)
	{
		return 1;
	}

	const FTcsNumericStateParamInstance* Inst = NumericParamInstances.Find(SkillDefinition->LevelParamTag);
	if (!Inst)
	{
		return 1;
	}

	return FMath::RoundToInt(Inst->GetModifiedValue());
}


void UTcsSkillEntry::SetLevel(int32 InLevel)
{
	if (!SkillDefinition)
	{
		return;
	}

	FTcsNumericStateParamInstance* Inst = NumericParamInstances.Find(SkillDefinition->LevelParamTag);
	if (Inst)
	{
		Inst->NumericValue = static_cast<float>(InLevel);
	}
}


bool UTcsSkillEntry::InitializeFromDef(FName InSkillDefId, const UTcsSkillDefinition* Def)
{
	if (InSkillDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] SkillDefId is none"));
		return false;
	}

	if (!Def)
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] SkillDefinition is null"));
		return false;
	}

	if (Def->StateDefId != InSkillDefId)
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[SkillEntry::InitializeFromDef] SkillDefId mismatch. Input=%s Definition=%s"),
			*InSkillDefId.ToString(),
			*Def->StateDefId.ToString());
		return false;
	}

	SkillDefId = InSkillDefId;
	// DefinitionManager 对外返回只读定义；Entry 只缓存 UObject 引用，不修改定义资产。
	SkillDefinition = const_cast<UTcsSkillDefinition*>(Def);
	RemainingCooldown = 0.0f;
	NumericParamInstances.Reset();
	BoolParamInstances.Reset();
	VectorParamInstances.Reset();

	// Learned SkillEntry 由 SkillComponent 持有，参数上下文固定为该组件所属实体。
	const UTcsSkillComponent* OwningSkillComponent = Cast<UTcsSkillComponent>(GetOuter());
	AActor* const OwningActor = OwningSkillComponent ? OwningSkillComponent->GetOwner() : nullptr;

	bool bSucceeded = true;
	for (const auto& ParamPair : Def->Parameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				FTcsNumericStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					Instance.BindEvaluationContext(this, OwningActor);
					NumericParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
					bSucceeded = false;
				}
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				FTcsBoolStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					Instance.BindEvaluationContext(this, OwningActor);
					BoolParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
					bSucceeded = false;
				}
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				FTcsVectorStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					Instance.BindEvaluationContext(this, OwningActor);
					VectorParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
					bSucceeded = false;
				}
				break;
			}
		default:
			break;
		}
	}

	if (Def->LevelParamTag.IsValid())
	{
		FTcsNumericStateParamInstance LevelInst;
		LevelInst.ParamTag = Def->LevelParamTag;
		LevelInst.bIsSnapshot = false;
		LevelInst.NumericValue = 1.0f;
		LevelInst.BindEvaluationContext(this, OwningActor);
		NumericParamInstances.Add(Def->LevelParamTag, LevelInst);
	}

	return bSucceeded;
}


FTcsNumericStateParamInstance* UTcsSkillEntry::FindNumericParamInstance(FGameplayTag ParamTag)
{
	return NumericParamInstances.Find(ParamTag);
}


FTcsBoolStateParamInstance* UTcsSkillEntry::FindBoolParamInstance(FGameplayTag ParamTag)
{
	return BoolParamInstances.Find(ParamTag);
}


FTcsVectorStateParamInstance* UTcsSkillEntry::FindVectorParamInstance(FGameplayTag ParamTag)
{
	return VectorParamInstances.Find(ParamTag);
}


bool UTcsSkillEntry::StartCooldown(UTcsSkillInstance* SkillInstance)
{
	if (!SkillInstance || !SkillDefinition || !SkillDefinition->CooldownParamTag.IsValid())
	{
		return false;
	}

	FTcsNumericStateParamInstance* CI = NumericParamInstances.Find(SkillDefinition->CooldownParamTag);
	if (!CI || !CI->CachedEvaluator)
	{
		return true;
	}

	float Duration = 0.0f;
	if (!CI->CachedEvaluator.Get()->Evaluate(
		SkillInstance->GetInstigator(),
		SkillInstance->GetOwner(),
		SkillInstance,
		CI->ParamData,
		Duration))
	{
		return false;
	}

	CI->NumericValue = Duration;
	CI->bHasEvaluated = CI->bIsSnapshot;

	if (Duration > 0.0f)
	{
		RemainingCooldown = CI->GetModifiedValue();
		return true;
	}

	return false;
}


void UTcsSkillEntry::TickCooldown(float DeltaTime)
{
	if (RemainingCooldown > 0.0f)
	{
		RemainingCooldown = FMath::Max(0.0f, RemainingCooldown - DeltaTime);
	}
}


bool UTcsSkillEntry::IsOnCooldown() const
{
	return RemainingCooldown > 0.0f;
}


float UTcsSkillEntry::GetRemainingCooldownRatio() const
{
	if (!SkillDefinition)
	{
		return 0.0f;
	}

	const FTcsNumericStateParamInstance* CI = NumericParamInstances.Find(SkillDefinition->CooldownParamTag);
	if (!CI)
	{
		return 0.0f;
	}

	// 进度分母使用 effective 冷却时长，与 StartCooldown 写入 RemainingCooldown 的口径一致
	const float EffectiveCooldown = CI->GetModifiedValue();
	if (EffectiveCooldown > 0.0f)
	{
		return RemainingCooldown / EffectiveCooldown;
	}
	return 0.0f;
}
