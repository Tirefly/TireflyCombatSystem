#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsSkillModifierParams.generated.h"

/**
 * 加法型参数载荷
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Additive
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FName ParamName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier", meta = (Categories = "Param"))
	FGameplayTag ParamTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float Magnitude = 0.f;
};

/**
 * 乘法型参数载荷
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Multiplicative
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FName ParamName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier", meta = (Categories = "Param"))
	FGameplayTag ParamTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float Multiplier = 1.f;
};

/**
 * 标量参数载荷（例如全局倍率）
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Scalar
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float Value = 1.f;
};
