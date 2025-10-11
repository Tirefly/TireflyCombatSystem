// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TcsAttrModExec_MultiplyAdditive.h"

#include "TcsCombatSystemLogChannels.h"



void UTcsAttrModExec_MultiplyAdditive::Execute_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	TMap<FName, float>& BaseValues,
	TMap<FName, float>& CurrentValues)
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
