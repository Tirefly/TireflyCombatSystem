// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierApplication.h"



void FTcsAttributeModifierApplicationResult::Reset()
{
	ModifierDefId = NAME_None;
	ApplicationMode = ETcsAttributeModifierApplicationMode::AMAM_None;
	SourceHandle = FTcsSourceHandle();
	bSucceeded = false;
	Failure = ETcsAttributeModifierApplicationFailure::AMAF_None;
	OperationResults.Reset();
}
