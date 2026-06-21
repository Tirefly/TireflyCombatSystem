// Copyright Tirefly. All Rights Reserved.

#include "DataTableSync/TcsDefDataTableRows.h"

#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/BuffMerger/TcsBuffMerger_NoMerge.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/SkillModExecution/TcsSkillModExec_Addition.h"
#include "Skill/SkillModExecution/TcsSkillModExec_SetBool.h"
#include "Skill/SkillModExecution/TcsSkillModExec_SetVector.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseNewest.h"
#include "State/TcsStateSlotDefinition.h"

namespace
{
	/**
	 * DefAsset ↔ RowStruct 的静态描述符。
	 */
	struct FTcsDefAssetSyncTypeDescriptor
	{
		/** 对应的 DefAsset 运行时类型。 */
		const UClass* DefAssetClass = nullptr;

		/** 对应的 DataTable RowStruct。 */
		UScriptStruct* RowStruct = nullptr;

		/** DefAsset 权威标识字段名。 */
		FName DefIdPropertyName;

		/** 用于日志输出的人类可读类型别名。 */
		FName TypeAlias;
	};

	void NormalizeAttributeDefRowDefaults(FTcsAttributeDefRow& Row)
	{
		if (!Row.ClampStrategyClass)
		{
			Row.ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
		}
	}

	void NormalizeAttributeModifierDefRowDefaults(FTcsAttributeModifierDefRow& Row)
	{
		if (!Row.MergerType)
		{
			Row.MergerType = UTcsAttrModMerger_NoMerge::StaticClass();
		}
	}

	template <typename TRow>
	void NormalizeStateDefinitionRowDefaults(TRow& Row)
	{
		for (TPair<FGameplayTag, FTcsStateParameter>& Pair : Row.Parameters)
		{
			NormalizeStateParameterStrategyDefaults(Pair.Value);
		}
	}

	void NormalizeStateDefinitionAssetDefaults(UTcsStateDefinition& StateDefinition)
	{
		for (TPair<FGameplayTag, FTcsStateParameter>& Pair : StateDefinition.Parameters)
		{
			NormalizeStateParameterStrategyDefaults(Pair.Value);
		}
	}

	void NormalizeBuffDefRowDefaults(FTcsBuffDefRow& Row)
	{
		NormalizeStateDefinitionRowDefaults(Row);

		if (!Row.MergerType)
		{
			Row.MergerType = UTcsBuffMerger_NoMerge::StaticClass();
		}
	}

	void NormalizeSkillDefRowDefaults(FTcsSkillDefRow& Row)
	{
		NormalizeStateDefinitionRowDefaults(Row);
		Row.CooldownParam.ParameterType = ETcsStateParameterType::SPT_Numeric;
		NormalizeStateParameterStrategyDefaults(Row.CooldownParam);
	}

	void NormalizeSkillDefinitionAssetDefaults(UTcsSkillDefinition& SkillDefinition)
	{
		NormalizeStateDefinitionAssetDefaults(SkillDefinition);
		SkillDefinition.CooldownParam.ParameterType = ETcsStateParameterType::SPT_Numeric;
		NormalizeStateParameterStrategyDefaults(SkillDefinition.CooldownParam);
	}

	void NormalizeSkillModifierDefRowDefaults(FTcsSkillModifierDefRow& Row)
	{
		if (!Row.NumericEvaluatorClass)
		{
			Row.NumericEvaluatorClass = UTcsSkillModExec_Addition::StaticClass();
		}

		if (!Row.BoolEvaluatorClass)
		{
			Row.BoolEvaluatorClass = UTcsSkillModExec_SetBool::StaticClass();
		}

		if (!Row.VectorEvaluatorClass)
		{
			Row.VectorEvaluatorClass = UTcsSkillModExec_SetVector::StaticClass();
		}
	}

	void NormalizeStateSlotDefRowDefaults(FTcsStateSlotDefRow& Row)
	{
		if (!Row.SamePriorityPolicy)
		{
			Row.SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
		}
	}

	template <typename TRow>
	void CopyStateDefinitionToRow(const UTcsStateDefinition& StateDefinition, TRow& Row)
	{
		Row.StateTag = StateDefinition.StateTag;
		Row.StateSlotType = StateDefinition.StateSlotType;
		Row.Priority = StateDefinition.Priority;
		Row.CategoryTags = StateDefinition.CategoryTags;
		Row.FunctionTags = StateDefinition.FunctionTags;
		Row.StateTreeRef = StateDefinition.StateTreeRef;
		Row.TickPolicy = StateDefinition.TickPolicy;
		Row.ActiveConditions = StateDefinition.ActiveConditions;
		Row.Parameters = StateDefinition.Parameters;
		Row.LevelParamTag = StateDefinition.LevelParamTag;
		NormalizeStateDefinitionRowDefaults(Row);
	}

	template <typename TRow>
	void CopyRowToStateDefinition(const TRow& Row, const FName RowName, UTcsStateDefinition& StateDefinition)
	{
		StateDefinition.StateDefId = RowName;
		StateDefinition.StateTag = Row.StateTag;
		StateDefinition.StateSlotType = Row.StateSlotType;
		StateDefinition.Priority = Row.Priority;
		StateDefinition.CategoryTags = Row.CategoryTags;
		StateDefinition.FunctionTags = Row.FunctionTags;
		StateDefinition.StateTreeRef = Row.StateTreeRef;
		StateDefinition.TickPolicy = Row.TickPolicy;
		StateDefinition.ActiveConditions = Row.ActiveConditions;
		StateDefinition.Parameters = Row.Parameters;
		StateDefinition.LevelParamTag = Row.LevelParamTag;
		NormalizeStateDefinitionAssetDefaults(StateDefinition);
	}

	/**
	 * 返回全部静态同步描述符。
	 *
	 * @return 静态描述符数组。
	 */
	const TArray<FTcsDefAssetSyncTypeDescriptor>& GetDefAssetSyncTypeDescriptors()
	{
		static TArray<FTcsDefAssetSyncTypeDescriptor> Descriptors = {
			{ UTcsAttributeDefinition::StaticClass(), FTcsAttributeDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsAttributeDefinition, AttributeDefId), TEXT("Attribute") },
			{ UTcsAttributeModifierDefinition::StaticClass(), FTcsAttributeModifierDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsAttributeModifierDefinition, AttributeModifierDefId), TEXT("AttributeModifier") },
			{ UTcsBuffDefinition::StaticClass(), FTcsBuffDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsStateDefinition, StateDefId), TEXT("Buff") },
			{ UTcsSkillDefinition::StaticClass(), FTcsSkillDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsStateDefinition, StateDefId), TEXT("Skill") },
			{ UTcsSkillModifierDefinition::StaticClass(), FTcsSkillModifierDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, ModifierId), TEXT("SkillModifier") },
			{ UTcsStateSlotDefinition::StaticClass(), FTcsStateSlotDefRow::StaticStruct(), GET_MEMBER_NAME_CHECKED(UTcsStateSlotDefinition, StateSlotDefId), TEXT("StateSlot") },
		};

		return Descriptors;
	}

	const FTcsDefAssetSyncTypeDescriptor* FindDefAssetSyncTypeDescriptorByAssetClass(const UClass* DefAssetClass)
	{
		if (!DefAssetClass)
		{
			return nullptr;
		}

		for (const FTcsDefAssetSyncTypeDescriptor& Descriptor : GetDefAssetSyncTypeDescriptors())
		{
			if (Descriptor.DefAssetClass && DefAssetClass->IsChildOf(Descriptor.DefAssetClass))
			{
				return &Descriptor;
			}
		}

		return nullptr;
	}
}


FTcsAttributeDefRow::FTcsAttributeDefRow()
{
	NormalizeAttributeDefRowDefaults(*this);
}


FTcsAttributeModifierDefRow::FTcsAttributeModifierDefRow()
{
	NormalizeAttributeModifierDefRowDefaults(*this);
}


FTcsBuffDefRow::FTcsBuffDefRow()
{
	NormalizeBuffDefRowDefaults(*this);
}


FTcsSkillModifierDefRow::FTcsSkillModifierDefRow()
{
	NormalizeSkillModifierDefRowDefaults(*this);
}


FTcsStateSlotDefRow::FTcsStateSlotDefRow()
{
	NormalizeStateSlotDefRowDefaults(*this);
}

UScriptStruct* ResolveExpectedDefDataTableRowStruct(const UClass* DefAssetClass)
{
	if (const FTcsDefAssetSyncTypeDescriptor* Descriptor = FindDefAssetSyncTypeDescriptorByAssetClass(DefAssetClass))
	{
		return Descriptor->RowStruct;
	}

	return nullptr;
}

bool TryGetDefAssetSyncId(const UPrimaryDataAsset* DefAsset, FName& OutDefId)
{
	OutDefId = NAME_None;

	if (const UTcsAttributeDefinition* const AttributeDefinition = Cast<UTcsAttributeDefinition>(DefAsset))
	{
		OutDefId = AttributeDefinition->AttributeDefId;
		return !OutDefId.IsNone();
	}

	if (const UTcsAttributeModifierDefinition* const AttributeModifierDefinition = Cast<UTcsAttributeModifierDefinition>(DefAsset))
	{
		OutDefId = AttributeModifierDefinition->AttributeModifierDefId;
		return !OutDefId.IsNone();
	}

	if (const UTcsBuffDefinition* const BuffDefinition = Cast<UTcsBuffDefinition>(DefAsset))
	{
		OutDefId = BuffDefinition->StateDefId;
		return !OutDefId.IsNone();
	}

	if (const UTcsSkillDefinition* const SkillDefinition = Cast<UTcsSkillDefinition>(DefAsset))
	{
		OutDefId = SkillDefinition->StateDefId;
		return !OutDefId.IsNone();
	}

	if (const UTcsSkillModifierDefinition* const SkillModifierDefinition = Cast<UTcsSkillModifierDefinition>(DefAsset))
	{
		OutDefId = SkillModifierDefinition->ModifierId;
		return !OutDefId.IsNone();
	}

	if (const UTcsStateSlotDefinition* const StateSlotDefinition = Cast<UTcsStateSlotDefinition>(DefAsset))
	{
		OutDefId = StateSlotDefinition->StateSlotDefId;
		return !OutDefId.IsNone();
	}

	return false;
}

bool TrySetDefAssetSyncId(UPrimaryDataAsset* DefAsset, const FName DefId)
{
	if (!DefAsset || DefId.IsNone())
	{
		return false;
	}

	if (UTcsAttributeDefinition* const AttributeDefinition = Cast<UTcsAttributeDefinition>(DefAsset))
	{
		AttributeDefinition->AttributeDefId = DefId;
		return true;
	}

	if (UTcsAttributeModifierDefinition* const AttributeModifierDefinition = Cast<UTcsAttributeModifierDefinition>(DefAsset))
	{
		AttributeModifierDefinition->AttributeModifierDefId = DefId;
		return true;
	}

	if (UTcsBuffDefinition* const BuffDefinition = Cast<UTcsBuffDefinition>(DefAsset))
	{
		BuffDefinition->StateDefId = DefId;
		return true;
	}

	if (UTcsSkillDefinition* const SkillDefinition = Cast<UTcsSkillDefinition>(DefAsset))
	{
		SkillDefinition->StateDefId = DefId;
		return true;
	}

	if (UTcsSkillModifierDefinition* const SkillModifierDefinition = Cast<UTcsSkillModifierDefinition>(DefAsset))
	{
		SkillModifierDefinition->ModifierId = DefId;
		return true;
	}

	if (UTcsStateSlotDefinition* const StateSlotDefinition = Cast<UTcsStateSlotDefinition>(DefAsset))
	{
		StateSlotDefinition->StateSlotDefId = DefId;
		return true;
	}

	return false;
}

bool TryBuildDefAssetDataTableRow(const UPrimaryDataAsset* DefAsset, FName& OutRowName, FInstancedStruct& OutRowData)
{
	OutRowName = NAME_None;
	OutRowData.Reset();

	if (!DefAsset)
	{
		return false;
	}

	if (!TryGetDefAssetSyncId(DefAsset, OutRowName))
	{
		return false;
	}

	if (const UTcsAttributeDefinition* const AttributeDefinition = Cast<UTcsAttributeDefinition>(DefAsset))
	{
		FTcsAttributeDefRow Row;
		Row.AttributeCategory = AttributeDefinition->AttributeCategory;
		Row.AttributeTag = AttributeDefinition->AttributeTag;
		Row.AttributeRange = AttributeDefinition->AttributeRange;
		Row.ClampStrategyClass = AttributeDefinition->ClampStrategyClass;
		Row.ClampStrategyConfig = AttributeDefinition->ClampStrategyConfig;
		Row.AttributeName = AttributeDefinition->AttributeName;
		Row.AttributeDescription = AttributeDefinition->AttributeDescription;
		Row.bShowInUI = AttributeDefinition->bShowInUI;
		Row.Icon = AttributeDefinition->Icon;
		Row.bAsDecimal = AttributeDefinition->bAsDecimal;
		Row.bAsPercentage = AttributeDefinition->bAsPercentage;
		NormalizeAttributeDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	if (const UTcsAttributeModifierDefinition* const AttributeModifierDefinition = Cast<UTcsAttributeModifierDefinition>(DefAsset))
	{
		FTcsAttributeModifierDefRow Row;
		Row.ModifierName = AttributeModifierDefinition->ModifierName;
		Row.Tags = AttributeModifierDefinition->Tags;
		Row.Priority = AttributeModifierDefinition->Priority;
		Row.AttributeId = AttributeModifierDefinition->AttributeId;
		Row.ModifierMode = AttributeModifierDefinition->ModifierMode;
		Row.Operands = AttributeModifierDefinition->Operands;
		Row.ModifierType = AttributeModifierDefinition->ModifierType;
		Row.MergerType = AttributeModifierDefinition->MergerType;
		NormalizeAttributeModifierDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	if (const UTcsBuffDefinition* const BuffDefinition = Cast<UTcsBuffDefinition>(DefAsset))
	{
		FTcsBuffDefRow Row;
		CopyStateDefinitionToRow(*BuffDefinition, Row);
		Row.DurationType = BuffDefinition->DurationType;
		Row.Duration = BuffDefinition->Duration;
		Row.Period = BuffDefinition->Period;
		Row.MaxStackCount = BuffDefinition->MaxStackCount;
		Row.MergerType = BuffDefinition->MergerType;
		Row.OnStackIncrease = BuffDefinition->OnStackIncrease;
		Row.OnDurationExpired = BuffDefinition->OnDurationExpired;
		Row.BuffInstanceClass = BuffDefinition->BuffInstanceClass;
		NormalizeBuffDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	if (const UTcsSkillDefinition* const SkillDefinition = Cast<UTcsSkillDefinition>(DefAsset))
	{
		FTcsSkillDefRow Row;
		CopyStateDefinitionToRow(*SkillDefinition, Row);
		Row.SkillInstanceClass = SkillDefinition->SkillInstanceClass;
		Row.SkillEntryClass = SkillDefinition->SkillEntryClass;
		Row.CooldownParamTag = SkillDefinition->CooldownParamTag;
		Row.CooldownParam = SkillDefinition->CooldownParam;
		NormalizeSkillDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	if (const UTcsSkillModifierDefinition* const SkillModifierDefinition = Cast<UTcsSkillModifierDefinition>(DefAsset))
	{
		FTcsSkillModifierDefRow Row;
		Row.EntrySelectorClass = SkillModifierDefinition->EntrySelectorClass;
		Row.EntrySelectorConfig = SkillModifierDefinition->EntrySelectorConfig;
		Row.TargetParamTag = SkillModifierDefinition->TargetParamTag;
		Row.TargetParamType = SkillModifierDefinition->TargetParamType;
		Row.NumericEvaluatorClass = SkillModifierDefinition->NumericEvaluatorClass;
		Row.BoolEvaluatorClass = SkillModifierDefinition->BoolEvaluatorClass;
		Row.VectorEvaluatorClass = SkillModifierDefinition->VectorEvaluatorClass;
		Row.EvaluatorConfig = SkillModifierDefinition->EvaluatorConfig;
		Row.Priority = SkillModifierDefinition->Priority;
		Row.MergePolicy = SkillModifierDefinition->MergePolicy;
		NormalizeSkillModifierDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	if (const UTcsStateSlotDefinition* const StateSlotDefinition = Cast<UTcsStateSlotDefinition>(DefAsset))
	{
		FTcsStateSlotDefRow Row;
		Row.SlotTag = StateSlotDefinition->SlotTag;
		Row.StateTreeStateName = StateSlotDefinition->StateTreeStateName;
		Row.ActivationMode = StateSlotDefinition->ActivationMode;
		Row.GateCloseBehavior = StateSlotDefinition->GateCloseBehavior;
		Row.PreemptionPolicy = StateSlotDefinition->PreemptionPolicy;
		Row.SamePriorityPolicy = StateSlotDefinition->SamePriorityPolicy;
		NormalizeStateSlotDefRowDefaults(Row);
		OutRowData = FInstancedStruct::Make(Row);
		return true;
	}

	OutRowName = NAME_None;
	return false;
}

bool TryApplyDefAssetDataTableRow(const FName RowName, const FInstancedStruct& RowData, UPrimaryDataAsset* DefAsset)
{
	if (RowName.IsNone() || !RowData.IsValid() || !DefAsset)
	{
		return false;
	}

	if (UTcsAttributeDefinition* const AttributeDefinition = Cast<UTcsAttributeDefinition>(DefAsset))
	{
		const FTcsAttributeDefRow* const Row = RowData.GetPtr<FTcsAttributeDefRow>();
		if (!Row)
		{
			return false;
		}

		AttributeDefinition->AttributeDefId = RowName;
		AttributeDefinition->AttributeCategory = Row->AttributeCategory;
		AttributeDefinition->AttributeTag = Row->AttributeTag;
		AttributeDefinition->AttributeRange = Row->AttributeRange;
		AttributeDefinition->ClampStrategyClass = Row->ClampStrategyClass;
		AttributeDefinition->ClampStrategyConfig = Row->ClampStrategyConfig;
		AttributeDefinition->AttributeName = Row->AttributeName;
		AttributeDefinition->AttributeDescription = Row->AttributeDescription;
		AttributeDefinition->bShowInUI = Row->bShowInUI;
		AttributeDefinition->Icon = Row->Icon;
		AttributeDefinition->bAsDecimal = Row->bAsDecimal;
		AttributeDefinition->bAsPercentage = Row->bAsPercentage;
		if (!AttributeDefinition->ClampStrategyClass)
		{
			AttributeDefinition->ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
		}
		return true;
	}

	if (UTcsAttributeModifierDefinition* const AttributeModifierDefinition = Cast<UTcsAttributeModifierDefinition>(DefAsset))
	{
		const FTcsAttributeModifierDefRow* const Row = RowData.GetPtr<FTcsAttributeModifierDefRow>();
		if (!Row)
		{
			return false;
		}

		AttributeModifierDefinition->AttributeModifierDefId = RowName;
		AttributeModifierDefinition->ModifierName = Row->ModifierName;
		AttributeModifierDefinition->Tags = Row->Tags;
		AttributeModifierDefinition->Priority = Row->Priority;
		AttributeModifierDefinition->AttributeId = Row->AttributeId;
		AttributeModifierDefinition->ModifierMode = Row->ModifierMode;
		AttributeModifierDefinition->Operands = Row->Operands;
		AttributeModifierDefinition->ModifierType = Row->ModifierType;
		AttributeModifierDefinition->MergerType = Row->MergerType;
		if (!AttributeModifierDefinition->MergerType)
		{
			AttributeModifierDefinition->MergerType = UTcsAttrModMerger_NoMerge::StaticClass();
		}
		return true;
	}

	if (UTcsBuffDefinition* const BuffDefinition = Cast<UTcsBuffDefinition>(DefAsset))
	{
		const FTcsBuffDefRow* const Row = RowData.GetPtr<FTcsBuffDefRow>();
		if (!Row)
		{
			return false;
		}

		CopyRowToStateDefinition(*Row, RowName, *BuffDefinition);
		BuffDefinition->DurationType = Row->DurationType;
		BuffDefinition->Duration = Row->Duration;
		BuffDefinition->Period = Row->Period;
		BuffDefinition->MaxStackCount = Row->MaxStackCount;
		BuffDefinition->MergerType = Row->MergerType;
		BuffDefinition->OnStackIncrease = Row->OnStackIncrease;
		BuffDefinition->OnDurationExpired = Row->OnDurationExpired;
		BuffDefinition->BuffInstanceClass = Row->BuffInstanceClass;
		if (!BuffDefinition->MergerType)
		{
			BuffDefinition->MergerType = UTcsBuffMerger_NoMerge::StaticClass();
		}
		return true;
	}

	if (UTcsSkillDefinition* const SkillDefinition = Cast<UTcsSkillDefinition>(DefAsset))
	{
		const FTcsSkillDefRow* const Row = RowData.GetPtr<FTcsSkillDefRow>();
		if (!Row)
		{
			return false;
		}

		CopyRowToStateDefinition(*Row, RowName, *SkillDefinition);
		SkillDefinition->SkillInstanceClass = Row->SkillInstanceClass;
		SkillDefinition->SkillEntryClass = Row->SkillEntryClass;
		SkillDefinition->CooldownParamTag = Row->CooldownParamTag;
		SkillDefinition->CooldownParam = Row->CooldownParam;
		NormalizeSkillDefinitionAssetDefaults(*SkillDefinition);
		return true;
	}

	if (UTcsSkillModifierDefinition* const SkillModifierDefinition = Cast<UTcsSkillModifierDefinition>(DefAsset))
	{
		const FTcsSkillModifierDefRow* const Row = RowData.GetPtr<FTcsSkillModifierDefRow>();
		if (!Row)
		{
			return false;
		}

		SkillModifierDefinition->ModifierId = RowName;
		SkillModifierDefinition->EntrySelectorClass = Row->EntrySelectorClass;
		SkillModifierDefinition->EntrySelectorConfig = Row->EntrySelectorConfig;
		SkillModifierDefinition->TargetParamTag = Row->TargetParamTag;
		SkillModifierDefinition->TargetParamType = Row->TargetParamType;
		SkillModifierDefinition->NumericEvaluatorClass = Row->NumericEvaluatorClass;
		SkillModifierDefinition->BoolEvaluatorClass = Row->BoolEvaluatorClass;
		SkillModifierDefinition->VectorEvaluatorClass = Row->VectorEvaluatorClass;
		SkillModifierDefinition->EvaluatorConfig = Row->EvaluatorConfig;
		SkillModifierDefinition->Priority = Row->Priority;
		SkillModifierDefinition->MergePolicy = Row->MergePolicy;
		NormalizeSkillModifierStrategyDefaults(*SkillModifierDefinition);
		return true;
	}

	if (UTcsStateSlotDefinition* const StateSlotDefinition = Cast<UTcsStateSlotDefinition>(DefAsset))
	{
		const FTcsStateSlotDefRow* const Row = RowData.GetPtr<FTcsStateSlotDefRow>();
		if (!Row)
		{
			return false;
		}

		StateSlotDefinition->StateSlotDefId = RowName;
		StateSlotDefinition->SlotTag = Row->SlotTag;
		StateSlotDefinition->StateTreeStateName = Row->StateTreeStateName;
		StateSlotDefinition->ActivationMode = Row->ActivationMode;
		StateSlotDefinition->GateCloseBehavior = Row->GateCloseBehavior;
		StateSlotDefinition->PreemptionPolicy = Row->PreemptionPolicy;
		StateSlotDefinition->SamePriorityPolicy = Row->SamePriorityPolicy;
		if (!StateSlotDefinition->SamePriorityPolicy)
		{
			StateSlotDefinition->SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
		}
		return true;
	}

	return false;
}