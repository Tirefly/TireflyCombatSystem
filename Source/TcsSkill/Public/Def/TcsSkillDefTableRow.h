// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "Def/TcsSkillDefData.h"

#include "TcsSkillDefTableRow.generated.h"



/**
 * 技能定义表行：只服务作者期批量编辑与往返保真，运行期读取资产而非 DataTable。
 * DefTag 是内容身份；RowName 是编辑期定位，两者不要求同名。
 * 完整字段形状只在 FTcsSkillDefData 声明，本行组合持有。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillDefTableRow : public FTableRowBase
{
	GENERATED_BODY()

// 内容身份
#pragma region Identity

public:
	// 内容身份，与资产 DefTag 对应，独立于编辑期 RowName。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def")
	FGameplayTag DefTag;

#pragma endregion


// 定义数据
#pragma region Definition

public:
	// 完整技能定义配置。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def")
	FTcsSkillDefData SkillDef;

#pragma endregion
};
