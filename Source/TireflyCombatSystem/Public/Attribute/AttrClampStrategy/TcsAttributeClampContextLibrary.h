// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "TcsAttributeClampContextLibrary.generated.h"


/**
 * 属性 Clamp 上下文辅助库
 *
 * 提供蓝图友好的辅助函数，用于在自定义 Clamp 策略中访问上下文信息。
 *
 * 使用场景：
 * - 在蓝图中创建自定义 Clamp 策略时，使用这些函数访问 Owner Actor、其他属性值等
 * - 在 C++ 中也可以使用这些函数，但直接访问 Context 字段更高效
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeClampContextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 获取 Owner Actor
	 *
	 * @param Context Clamp 上下文
	 * @return Owner Actor（可能为 nullptr）
	 */
	UFUNCTION(BlueprintPure, Category = "TireflyCombatSystem|Attribute|Clamp",
		Meta = (DisplayName = "Get Owner Actor"))
	static AActor* GetOwnerActor(const FTcsAttributeClampContextBase& Context);

	/**
	 * 获取其他属性值
	 *
	 * 优先从工作集读取（如果 Resolver 可用），否则从已提交值读取。
	 * 在两段式 Clamp 过程中，Resolver 会返回工作集中的值。
	 *
	 * @param Context Clamp 上下文
	 * @param OtherAttributeName 其他属性名称
	 * @param OutValue 输出：属性值
	 * @return 是否成功读取
	 */
	UFUNCTION(BlueprintPure, Category = "TireflyCombatSystem|Attribute|Clamp",
		Meta = (DisplayName = "Get Other Attribute Value"))
	static bool GetOtherAttributeValue(
		const FTcsAttributeClampContextBase& Context,
		FName OtherAttributeName,
		float& OutValue);

	/**
	 * 检查 Owner 是否有特定标签
	 *
	 * @param Context Clamp 上下文
	 * @param Tag 要检查的 Gameplay Tag
	 * @return Owner 是否有该标签
	 */
	UFUNCTION(BlueprintPure, Category = "TireflyCombatSystem|Attribute|Clamp",
		Meta = (DisplayName = "Owner Has Tag"))
	static bool OwnerHasTag(
		const FTcsAttributeClampContextBase& Context,
		FGameplayTag Tag);

	/**
	 * 获取属性名称
	 *
	 * @param Context Clamp 上下文
	 * @return 当前正在 Clamp 的属性名称
	 */
	UFUNCTION(BlueprintPure, Category = "TireflyCombatSystem|Attribute|Clamp",
		Meta = (DisplayName = "Get Attribute Name"))
	static FName GetAttributeName(const FTcsAttributeClampContextBase& Context);
};
