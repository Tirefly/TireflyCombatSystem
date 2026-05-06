// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TcsAttrModExec_Addition.h"

#include "Attribute/TcsAttributeModifierDefinition.h"
#include "TcsLogChannels.h"



void UTcsAttrModExec_Addition::Execute_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	UPARAM(ref) TMap<FName, float>& BaseValues,
	UPARAM(ref) TMap<FName, float>& CurrentValues)
{
	// 检查修改器定义 DataAsset
	if (!ModInst.ModifierDef)
	{
		UE_LOG(LogTcsAttrModExec, Error, TEXT("[%s] ModifierDef is null for ModifierId: %s"),
			*FString(__FUNCTION__),
			*ModInst.ModifierId.ToString());
		return;
	}

	const UTcsAttributeModifierDefinition* ModDef = ModInst.ModifierDef;
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
				*BaseValue += *Magnitude;
				break;
			}
		case ETcsAttributeModifierMode::AMM_CurrentValue:
			{
				*CurrentValue += *Magnitude;
				break;
			}
		}
	}
}
