// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluator.h"
#include "Attribute/TcsAttributeModifierApplication.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"



namespace TcsAttributeModPrivate
{
	inline bool SetApplicationFailure(
		FTcsAttributeModifierApplicationResult* Result,
		ETcsAttributeModifierApplicationFailure Failure)
	{
		if (Result && Result->Failure == ETcsAttributeModifierApplicationFailure::AMAF_None)
		{
			Result->Failure = Failure;
		}

		return false;
	}

	inline bool IsValidOperandPayload(const FInstancedStruct& Payload)
	{
		const UScriptStruct* const PayloadStruct = Payload.GetScriptStruct();
		return PayloadStruct && PayloadStruct->IsChildOf(FTcsAttributeOperandPayload::StaticStruct());
	}

	inline void GetSortedOperationIds(
		const UTcsAttributeModifierDefinition& ModifierDefinition,
		TArray<FName>& OutOperationIds)
	{
		ModifierDefinition.Operations.GetKeys(OutOperationIds);
		OutOperationIds.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
	}

	inline UTcsSkillEntry* GetSourceSkillEntry(
		UTcsStateInstance* SourceStateInstance,
		UTcsSkillEntry* ExplicitSourceSkillEntry)
	{
		if (ExplicitSourceSkillEntry)
		{
			return ExplicitSourceSkillEntry;
		}

		if (const UTcsSkillInstance* const SkillInstance = Cast<UTcsSkillInstance>(SourceStateInstance))
		{
			return SkillInstance->GetSkillEntry();
		}

		return nullptr;
	}

	inline bool IsStateInstanceRegisteredWithOwner(UTcsStateInstance* StateInstance)
	{
		if (!StateInstance)
		{
			return false;
		}

		UTcsStateComponent* const OwnerStateComponent = StateInstance->GetOwnerStateComponent();
		if (!OwnerStateComponent)
		{
			return false;
		}

		TArray<UTcsStateInstance*> RegisteredStates;
		if (!OwnerStateComponent->GetStatesByDefId(StateInstance->GetStateDefId(), RegisteredStates))
		{
			return false;
		}

		return RegisteredStates.Contains(StateInstance);
	}
}
