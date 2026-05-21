// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsBuffTypes.generated.h"



namespace TcsBuffRemovalReasons
{
	static const FName Expired(TEXT("Expired"));
	static const FName MergedOut(TEXT("MergedOut"));
	static const FName StackDepleted(TEXT("StackDepleted"));
}



UENUM(BlueprintType)
enum ETcsBuffDurationType : uint8
{
	SDT_None = 0		UMETA(DisplayName = "None", ToolTip = "无持续时间"),
	SDT_Duration		UMETA(DisplayName = "Duration", ToolTip = "有持续时间"),
	SDT_Infinite		UMETA(DisplayName = "Infinite", ToolTip = "无限持续时间"),
};