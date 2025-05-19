// Copyright Epic Games, Inc. All Rights Reserved.

#include "TireflyCombatSystemModule.h"

#define LOCTEXT_NAMESPACE "FTireflyCombatSystemModule"

void FTireflyCombatSystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FTireflyCombatSystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTireflyCombatSystemModule, TireflyCombatSystem)