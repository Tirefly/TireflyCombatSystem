// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsCastAttrCapture.generated.h"



// 属性捕获的取值方；CACF_ = CastAttrCaptureFrom。
UENUM(BlueprintType)
enum class ETcsCastAttrCaptureFrom : uint8
{
	CACF_Instigator = 0	UMETA(DisplayName = "发起者", ToolTip = "从本次施法的发起者捕获属性"),
	CACF_Target = 1		UMETA(DisplayName = "目标", ToolTip = "从本次施法的目标捕获属性"),
};



/**
 * 技能侧属性捕获声明：激活时按属性键与取值方捕获进施法运行态。
 * 与伤害流程步骤级 AttrCaptureList 是两层独立机制；本类型不交付 WAIT-7 的伤害侧捕获。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCastAttrCapture
{
	GENERATED_BODY()

// 捕获声明
#pragma region Capture

public:
	// 宿主声明的属性键。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Attr Capture")
	FGameplayTag AttrKey;

	// 取值方（发起者或目标）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Attr Capture")
	ETcsCastAttrCaptureFrom From = ETcsCastAttrCaptureFrom::CACF_Instigator;

#pragma endregion
};
