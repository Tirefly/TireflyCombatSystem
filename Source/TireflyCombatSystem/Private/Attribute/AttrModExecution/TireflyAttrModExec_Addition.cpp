// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TireflyAttrModExec_Addition.h"

#include "TireflyCombatSystemLogChannels.h"



void UTireflyAttrModExec_Addition::Execute_Implementation(
	AActor* Instigator,
	AActor* Target,
	const FTireflyAttributeModifierInstance& ModInst,
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
		case ETireflyAttributeModifierMode::BaseValue:
			{
				*BaseValue += *Magnitude;
				break;
			}
		case ETireflyAttributeModifierMode::CurrentValue:
			{
				*CurrentValue += *Magnitude;
				break;
			}
		}
	}
}
