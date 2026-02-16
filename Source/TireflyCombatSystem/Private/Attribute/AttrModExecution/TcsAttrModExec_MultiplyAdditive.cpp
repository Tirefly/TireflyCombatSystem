// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TcsAttrModExec_MultiplyAdditive.h"

#include "Attribute/TcsAttributeModifierDefinitionAsset.h"
#include "TcsLogChannels.h"



void UTcsAttrModExec_MultiplyAdditive::Execute_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	TMap<FName, float>& BaseValues,
	TMap<FName, float>& CurrentValues)
{
	// 检查修改器定义 DataAsset
	if (!ModInst.ModifierDefAsset)
	{
		UE_LOG(LogTcsAttrModExec, Error, TEXT("[%s] ModifierDefAsset is null for ModifierId: %s"),
			*FString(__FUNCTION__),
			*ModInst.ModifierId.ToString());
		return;
	}

	const UTcsAttributeModifierDefinitionAsset* ModDef = ModInst.ModifierDefAsset;
	const FName& AttrToMod = ModDef->AttributeName;
	float* BaseValue = BaseValues.Find(AttrToMod);
	float* CurrentValue = CurrentValues.Find(AttrToMod);
	if (!BaseValue || !CurrentValue)
	{
		UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] Attribute '%s' not found."),
			*FString(__FUNCTION__),
			*AttrToMod.ToString());
		return;
	}

	if (const float* Magnitude = ModInst.Operands.Find(FName("Magnitude")))
	{
		switch (ModDef->ModifierMode)
		{
		case ETcsAttributeModifierMode::AMM_BaseValue:
			{
				*BaseValue *= (1.f + *Magnitude);
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				*CurrentValue *= (1.f + *Magnitude);
				break;
			}
		}
	}
}
