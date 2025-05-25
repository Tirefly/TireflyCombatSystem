// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TireflyAttrModExec_MultiplyAdditive.h"

#include "TireflyCombatSystemLogChannels.h"



void UTireflyAttrModExec_MultiplyAdditive::Execute_Implementation(
	AActor* Instigator,
	AActor* Target,
	UObject* SourceObject,
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
	
	switch (ModInst.ModifierDef.ModifierMode)
	{
	case ETireflyAttributeModifierMode::BaseValue:
		{
			float MultiplyOperand = 1.f;
			for (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				MultiplyOperand += Pair.Value;
			}
			*BaseValue *= MultiplyOperand;
			break;
		}
	case ETireflyAttributeModifierMode::CurrentValue:
		{
			float MultiplyOperand = 1.f;
			for  (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				MultiplyOperand += Pair.Value;
			}
			*CurrentValue *= MultiplyOperand;
			break;
		}
	}
}
