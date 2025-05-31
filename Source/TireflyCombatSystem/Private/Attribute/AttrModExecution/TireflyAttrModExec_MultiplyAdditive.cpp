// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TireflyAttrModExec_MultiplyAdditive.h"

#include "TireflyCombatSystemLogChannels.h"



void UTireflyAttrModExec_MultiplyAdditive::Execute_Implementation(
	const FTireflyAttributeModifierInstance& ModInst,
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
		case ETireflyAttributeModifierMode::BaseValue:
			{
				*BaseValue *= (1.f + *Magnitude);
				break;
			}
		case ETireflyAttributeModifierMode::CurrentValue:
			{
				*CurrentValue *= (1.f + *Magnitude);
				break;
			}
		}
	}
}
