// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Def/TcsSkillDefData.h"
#include "Def/TcsStateDef.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsSkillDef.generated.h"



/**
 * 技能定义资产：持 SkillDef 数据，复用状态族基类的 DefTag 内容身份。
 * 资产裸名已有文档契约，数据以 Data 后缀消歧，避免 UHT 去前缀引擎名冲突。
 * 定义库按类发现、按 DefTag 解析；技能定义只进缓存，不参与世界装配。
 */
UCLASS(BlueprintType)
class TCSSKILL_API UTcsSkillDef : public UTcsStateDef
{
	GENERATED_BODY()

// 主资产身份
#pragma region Identity

public:
	// 主资产类型取本类名；遮蔽基类静态成员，配合 GetPrimaryAssetId 覆写使用。
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 返回 [TcsSkillDef, DefTag.GetTagName()]，不取资产文件名。
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 定义数据
#pragma region Definition

public:
	// 技能定义数据；EditAnywhere 支持作者期配置，读取面由定义库 C++ 解析口提供。
	UPROPERTY(EditAnywhere, Category = "Tcs|Skill")
	FTcsSkillDefData SkillDef;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器内容校验：六类配置错误，身份校验由基类承担。
	 * @param Context 收集带字段与下标定位的错误。
	 * @return 有错误返回 Invalid，无错误返回 Valid。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
