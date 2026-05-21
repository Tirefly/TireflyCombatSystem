// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "TcsDefAssetFactories.generated.h"

/**
 * Attribute Def 资产创建工厂。
 *
 * 负责在编辑器中直接创建 canonical `UTcsAttributeDefinition` 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsAttributeDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** 初始化 Attribute Def 工厂。 */
	UTcsAttributeDefinitionFactory();

	/**
	 * 创建新的 Attribute Def 资产。
	 *
	 * @param InClass 调用方请求创建的类型。
	 * @param InParent 新资产的 Outer。
	 * @param InName 新资产名称。
	 * @param Flags 新资产对象标志。
	 * @param Context 额外上下文对象。
	 * @param Warn 输出警告的反馈上下文。
	 * @param CallingContext 调用方上下文名称。
	 * @return 新建的 canonical Attribute Def 资产。
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;

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
 * Attribute Modifier Def 资产创建工厂。
 *
 * 负责在编辑器中直接创建 canonical `UTcsAttributeModifierDefinition` 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsAttributeModifierDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** 初始化 Attribute Modifier Def 工厂。 */
	UTcsAttributeModifierDefinitionFactory();

	/**
	 * 创建新的 Attribute Modifier Def 资产。
	 *
	 * @param InClass 调用方请求创建的类型。
	 * @param InParent 新资产的 Outer。
	 * @param InName 新资产名称。
	 * @param Flags 新资产对象标志。
	 * @param Context 额外上下文对象。
	 * @param Warn 输出警告的反馈上下文。
	 * @param CallingContext 调用方上下文名称。
	 * @return 新建的 canonical Attribute Modifier Def 资产。
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;

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
 * Buff Def 资产创建工厂。
 *
 * 负责在编辑器中直接创建 canonical `UTcsBuffDefinition` 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsBuffDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** 初始化 Buff Def 工厂。 */
	UTcsBuffDefinitionFactory();

	/**
	 * 创建新的 Buff Def 资产。
	 *
	 * @param InClass 调用方请求创建的类型。
	 * @param InParent 新资产的 Outer。
	 * @param InName 新资产名称。
	 * @param Flags 新资产对象标志。
	 * @param Context 额外上下文对象。
	 * @param Warn 输出警告的反馈上下文。
	 * @param CallingContext 调用方上下文名称。
	 * @return 新建的 canonical Buff Def 资产。
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;

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
 * State Slot Def 资产创建工厂。
 *
 * 负责在编辑器中直接创建 canonical `UTcsStateSlotDefinition` 资产。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsStateSlotDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** 初始化 State Slot Def 工厂。 */
	UTcsStateSlotDefinitionFactory();

	/**
	 * 创建新的 State Slot Def 资产。
	 *
	 * @param InClass 调用方请求创建的类型。
	 * @param InParent 新资产的 Outer。
	 * @param InName 新资产名称。
	 * @param Flags 新资产对象标志。
	 * @param Context 额外上下文对象。
	 * @param Warn 输出警告的反馈上下文。
	 * @param CallingContext 调用方上下文名称。
	 * @return 新建的 canonical State Slot Def 资产。
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;

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