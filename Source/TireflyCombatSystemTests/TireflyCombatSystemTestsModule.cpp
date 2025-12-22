// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystemTestsModule.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"



#define LOCTEXT_NAMESPACE "FTireflyCombatSystemModule"



void FTireflyCombatSystemTestsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FTireflyCombatSystemTestsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTireflyCombatSystemTestsModule, TireflyCombatSystem)