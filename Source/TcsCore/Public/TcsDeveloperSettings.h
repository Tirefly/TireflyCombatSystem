// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TcsDeveloperSettings.generated.h"

/**
 * TcsCore 开发者设置（空壳）：承载后续模块级配置项，在项目设置中呈现为独立 "Tcs" 分类
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tcs"))
class TCSCORE_API UTcsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

// 开发者设置分类
#pragma region Config

public:
	// 项目设置分类名：独立 "Tcs" 分类
	virtual FName GetCategoryName() const override;

#pragma endregion
};
