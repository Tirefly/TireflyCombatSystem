// Copyright Tirefly. All Rights Reserved.

#include "TireflyCombatSystemModule.h"

#include "TcsConsoleCommands.h"

#define LOCTEXT_NAMESPACE "FTireflyCombatSystemModule"

void FTireflyCombatSystemModule::StartupModule()
{
	TcsConsoleCommandRuntime::RegisterConsoleCommands();
}

void FTireflyCombatSystemModule::ShutdownModule()
{
	TcsConsoleCommandRuntime::UnregisterConsoleCommands();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTireflyCombatSystemModule, TireflyCombatSystem)