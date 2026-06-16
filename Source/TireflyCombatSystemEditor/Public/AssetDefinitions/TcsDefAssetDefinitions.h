// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"

#include "TcsDefAssetDefinitions.generated.h"

/**
 * Attribute Def 资产定义。
 *
 * 负责声明 Attribute Def 在 Content Browser 中的显示信息与分类。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsAttributeDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	/** @return Asset 菜单中显示的名称。 */
	virtual FText GetAssetDisplayName() const override;

	/** @return Asset 缩略图与标签使用的颜色。 */
	virtual FLinearColor GetAssetColor() const override;

	/** @return 该资产定义对应的运行时资产类。 */
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	/** @return 该资产在 Content Browser 中的分类路径。 */
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/**
 * Attribute Modifier Def 资产定义。
 *
 * 负责声明 Attribute Modifier Def 在 Content Browser 中的显示信息与分类。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsAttributeModifierDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	/** @return Asset 菜单中显示的名称。 */
	virtual FText GetAssetDisplayName() const override;

	/** @return Asset 缩略图与标签使用的颜色。 */
	virtual FLinearColor GetAssetColor() const override;

	/** @return 该资产定义对应的运行时资产类。 */
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	/** @return 该资产在 Content Browser 中的分类路径。 */
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/**
 * Buff Def 资产定义。
 *
 * 负责声明 Buff Def 在 Content Browser 中的显示信息与分类。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsBuffDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	/** @return Asset 菜单中显示的名称。 */
	virtual FText GetAssetDisplayName() const override;

	/** @return Asset 缩略图与标签使用的颜色。 */
	virtual FLinearColor GetAssetColor() const override;

	/** @return 该资产定义对应的运行时资产类。 */
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	/** @return 该资产在 Content Browser 中的分类路径。 */
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/**
 * State Slot Def 资产定义。
 *
 * 负责声明 State Slot Def 在 Content Browser 中的显示信息与分类。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsStateSlotDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override;

	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/**
 * Skill Modifier Def 资产定义。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsSkillModifierDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override;

	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/**
 * Skill Def 资产定义。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UAssetDefinition_TcsSkillDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;

	virtual FLinearColor GetAssetColor() const override;

	virtual TSoftClassPtr<UObject> GetAssetClass() const override;

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};