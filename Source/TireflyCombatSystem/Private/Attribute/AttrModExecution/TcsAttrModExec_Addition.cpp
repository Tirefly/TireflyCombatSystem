// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TcsAttrModExec_Addition.h"

#include "TcsCombatSystemLogChannels.h"



void UTcsAttrModExec_Addition::Execute_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	UPARAM(ref) TMap<FName, float>& BaseValues,
	UPARAM(ref) TMap<FName, float>& CurrentValues)
{
	const FName& AttrToMod = ModInst.ModifierDef.AttributeName;
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
		switch (ModInst.ModifierDef.ModifierMode)
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
