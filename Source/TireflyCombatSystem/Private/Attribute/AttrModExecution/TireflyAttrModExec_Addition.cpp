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
	
	switch (ModInst.ModifierDef.ModifierMode)
	{
	case ETireflyAttributeModifierMode::BaseValue:
		{
			for (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				*BaseValue += Pair.Value;
			}
			break;
		}
	case ETireflyAttributeModifierMode::CurrentValue:
		{
			for  (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				*CurrentValue += Pair.Value;
			}
			break;
		}
	}
}
