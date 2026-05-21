// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "StateTreeFactory.h"

#include "TcsRuntimeAssetFactories.generated.h"

/**
 * State Component StateTree 资产创建工厂。
 *
 * 负责创建预设为 `UStateTreeComponentSchema` 的 StateTree 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsStateComponentStateTreeFactory : public UStateTreeFactory
{
	GENERATED_BODY()

public:
	/** 初始化 State Component StateTree 工厂。 */
	UTcsStateComponentStateTreeFactory();

	/** @return 固定使用组件 Schema 并跳过额外配置。 */
	virtual bool ConfigureProperties() override;

	/** @return 工厂是否显示在新建资产菜单中。 */
	virtual bool ShouldShowInNewMenu() const override;

	/** @return 工厂所属的新建资产菜单分类。 */
	virtual uint32 GetMenuCategories() const override;

	/** @return 工厂所属的新建资产菜单二级子分类。 */
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;

	/** @return 菜单中显示的资产名称。 */
	virtual FText GetDisplayName() const override;

	/** @return 菜单中显示的资产说明。 */
	virtual FText GetToolTip() const override;

	/** @return 新建资产的默认名称。 */
	virtual FString GetDefaultNewAssetName() const override;
};

/**
 * State Instance StateTree 资产创建工厂。
 *
 * 负责创建预设为 `UTcsStateTreeSchema_StateInstance` 的 StateTree 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsStateInstanceStateTreeFactory : public UStateTreeFactory
{
	GENERATED_BODY()

public:
	/** 初始化 State Instance StateTree 工厂。 */
	UTcsStateInstanceStateTreeFactory();

	/** @return 固定使用 StateInstance Schema 并跳过额外配置。 */
	virtual bool ConfigureProperties() override;

	/** @return 工厂是否显示在新建资产菜单中。 */
	virtual bool ShouldShowInNewMenu() const override;

	/** @return 工厂所属的新建资产菜单分类。 */
	virtual uint32 GetMenuCategories() const override;

	/** @return 工厂所属的新建资产菜单二级子分类。 */
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;

	/** @return 菜单中显示的资产名称。 */
	virtual FText GetDisplayName() const override;

	/** @return 菜单中显示的资产说明。 */
	virtual FText GetToolTip() const override;

	/** @return 新建资产的默认名称。 */
	virtual FString GetDefaultNewAssetName() const override;
};

/**
 * Skill Instance 蓝图资产创建工厂。
 *
 * 负责创建以 `UTcsSkillInstance` 为父类的 Blueprint 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsSkillInstanceBlueprintFactory : public UBlueprintFactory
{
	GENERATED_BODY()

public:
	/** 初始化 Skill Instance 蓝图工厂。 */
	UTcsSkillInstanceBlueprintFactory();

	/** @return 固定父类为 `UTcsSkillInstance` 并跳过父类选择器。 */
	virtual bool ConfigureProperties() override;

	/** @return 工厂是否显示在新建资产菜单中。 */
	virtual bool ShouldShowInNewMenu() const override;

	/** @return 工厂所属的新建资产菜单分类。 */
	virtual uint32 GetMenuCategories() const override;

	/** @return 工厂所属的新建资产菜单二级子分类。 */
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;

	/** @return 菜单中显示的资产名称。 */
	virtual FText GetDisplayName() const override;

	/** @return 菜单中显示的资产说明。 */
	virtual FText GetToolTip() const override;

	/** @return 新建资产的默认名称。 */
	virtual FString GetDefaultNewAssetName() const override;
};