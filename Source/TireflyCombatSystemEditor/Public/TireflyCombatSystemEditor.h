// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AssetCategoryPath.h"
#include "Modules/ModuleManager.h"

/**
 * TCS 编辑器模块。
 *
 * 负责注册 TCS 资产在 Content Browser 中的创建分类与编辑器侧入口。
 */
class FTireflyCombatSystemEditorModule : public IModuleInterface
{
public:
	/**
	 * 获取模块实例。
	 *
	 * @return 已加载的 TCS 编辑器模块实例。
	 */
	static FTireflyCombatSystemEditorModule& Get();

	/**
	 * 检查模块当前是否已加载。
	 *
	 * @return 模块已加载时返回 true。
	 */
	static bool IsAvailable();

	/**
	 * 获取模块名称。
	 *
	 * @return TCS 编辑器模块名。
	 */
	static FName GetModuleName();

	/**
	 * 获取 TCS 资产分类显示名。
	 *
	 * @return Content Browser 中显示的分类名称。
	 */
	static FText GetAssetCategoryDisplayName();

	/**
	 * 获取 Definition Asset 子分类显示名。
	 *
	 * @return Definition Asset 二级分类文本。
	 */
	static FText GetDefinitionAssetCategoryDisplayName();

	/**
	 * 获取 Gameplay Runtime 子分类显示名。
	 *
	 * @return Gameplay Runtime 二级分类文本。
	 */
	static FText GetGameplayRuntimeCategoryDisplayName();

	/**
	 * 获取 TCS Def 资产使用的分类路径。
	 *
	 * @return TCS Def 资产的分类路径集合。
	 */
	static TConstArrayView<FAssetCategoryPath> GetDefinitionAssetCategoryPaths();

	/**
	 * 获取 Definition Asset 工厂菜单使用的二级子菜单文本。
	 *
	 * @return Definition Asset 子菜单路径。
	 */
	static const TArray<FText>& GetDefinitionAssetCategorySubMenus();

	/**
	 * 获取 Gameplay Runtime 工厂菜单使用的二级子菜单文本。
	 *
	 * @return Gameplay Runtime 子菜单路径。
	 */
	static const TArray<FText>& GetGameplayRuntimeCategorySubMenus();

	/**
	 * 获取 Factory 菜单使用的旧式资产分类位。
	 *
	 * @return UFactory 菜单分类位掩码。
	 */
	uint32 GetRegisteredAssetCategory() const
	{
		return RegisteredAssetCategory;
	}

	/** 注册模块的编辑器扩展。 */
	virtual void StartupModule() override;

	/** 清理模块的编辑器扩展状态。 */
	virtual void ShutdownModule() override;

private:
	/** UFactory 新建菜单使用的旧式分类位。 */
	uint32 RegisteredAssetCategory = 0;
};