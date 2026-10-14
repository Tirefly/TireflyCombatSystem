// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TcsAssetButlerSettings.generated.h"



/**
 * 资产加载层设置（裁定① 的"编辑器期间 PrimaryAsset 的相关机制"）。
 *
 * 在项目设置中呈现为独立 "Tcs" 分类（与 UTcsDeveloperSettings 同款）。
 *
 * **★ 红线（MUST 读）**：本类的字段**MUST NOT 影响模拟语义**——
 * 它们只表达"资产怎么被找到、怎么被预热"这类**编辑器/加载期**规则。
 * 一旦某个值能改变战斗结果（如伤害公式系数、堆叠上限），它就该住**对应域的定义资产**，
 * 而不是这里：设置是**进程级单例**、无版本、不随内容回滚，把它接进模拟语义
 * 会让"同一份内容在不同机器上算出不同结果"。
 *
 * **零消费者不预建**：本类当前只登记"路径排除"这一条**确有消费面**的项
 * （`UTcsAssetManager` 用它过滤扫描路径）；策略默认值等其余项待有真实消费方再加。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tcs Asset Butler"))
class TCSASSETBUTLER_API UTcsAssetButlerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

// 开发者设置分类
#pragma region Config

public:
	// 项目设置分类名：与 UTcsDeveloperSettings 同住 "Tcs" 分类
	virtual FName GetCategoryName() const override;

#pragma endregion


// 扫描规则
#pragma region ScanRules

public:
	/**
	 * 从扫描路径中排除的目录前缀（宿主可改）。
	 *
	 * 用途：宿主把**测试/临时**内容放在某目录下时，用它把该目录整片排除出主资产扫描
	 * （避免测试资产的类型注册进正式类型表）。**前缀匹配**，不是精确路径。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Scan",
		Meta = (DisplayName = "扫描排除目录前缀"))
	TArray<FString> ExcludedScanPathPrefixes;

#pragma endregion
};
