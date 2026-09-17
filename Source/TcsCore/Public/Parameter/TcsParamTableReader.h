// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TcsParamTableReader.generated.h"



// 参数表只读访问接口（TcsCore 反射面；宿主/UnrealSharp 可实现以提供自定义参数表——PV-1）
UINTERFACE(MinimalAPI)
class UTcsParamTableReader : public UInterface
{
	GENERATED_BODY()
};

/**
 * 参数表只读访问接口（PV-1）：参数源求值时读取数值参数的统一通道。
 * 实现方必须保证只读（零副作用）；miss 返回 false，由调用源落 Fallback。
 */
class ITcsParamTableReader
{
	GENERATED_BODY()

// 参数读取契约
#pragma region Query

public:
	/**
	 * 读取数值参数。
	 *
	 * @param Key 参数键。
	 * @param OutValue 输出读到的规范值（miss 时内容未定义，调用方不得使用）。
	 * @return 返回是否命中；false = miss，源落 Fallback。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool TryGetNumericParam(FName Key, double& OutValue);

#pragma endregion
};
