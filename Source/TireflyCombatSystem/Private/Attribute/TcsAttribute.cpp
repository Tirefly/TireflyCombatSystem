// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttribute.h"

#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"


FTcsAttributeDefinition::FTcsAttributeDefinition()
{
	// 设置默认 Clamp 策略为线性 Clamp
	// 这确保了所有属性定义（包括现有的和新创建的）都有一个有效的 Clamp 策略
	ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
}
FTcsAttributeInstance::FTcsAttributeInstance(const UTcsAttributeDefinitionAsset& AttrDefAsset, int32 InstId, AActor* Owner)
	: AttributeInstId(InstId), Owner(Owner), BaseValue(0.f), CurrentValue(0.f)
{
	// TODO: [DataAsset Migration] Phase 1.6 - 临时实现，将 DataAsset 转换为 struct
	// 在 Phase 1.6 中，AttributeDef 字段将改为 TSoftObjectPtr<UTcsAttributeDefinitionAsset>
	AttributeDef.AttributeRange = AttrDefAsset.AttributeRange;
	AttributeDef.ClampStrategyClass = AttrDefAsset.ClampStrategyClass;
	AttributeDef.ClampStrategyConfig = AttrDefAsset.ClampStrategyConfig;
	AttributeDef.AttributeCategory = AttrDefAsset.AttributeCategory;
	AttributeDef.AttributeAbbreviation = AttrDefAsset.AttributeAbbreviation;
	AttributeDef.AttributeTag = AttrDefAsset.AttributeTag;
	AttributeDef.AttributeName = AttrDefAsset.AttributeName;
	AttributeDef.AttributeDescription = AttrDefAsset.AttributeDescription;
	AttributeDef.bShowInUI = AttrDefAsset.bShowInUI;
	AttributeDef.Icon = AttrDefAsset.Icon;
	AttributeDef.bAsDecimal = AttrDefAsset.bAsDecimal;
	AttributeDef.bAsPercentage = AttrDefAsset.bAsPercentage;
}
FTcsAttributeInstance::FTcsAttributeInstance(const UTcsAttributeDefinitionAsset& AttrDefAsset, int32 InstId, AActor* Owner, float InitValue)
	: AttributeInstId(InstId), Owner(Owner), BaseValue(InitValue), CurrentValue(InitValue)
{
	// TODO: [DataAsset Migration] Phase 1.6 - 临时实现，将 DataAsset 转换为 struct
	// 在 Phase 1.6 中，AttributeDef 字段将改为 TSoftObjectPtr<UTcsAttributeDefinitionAsset>
	AttributeDef.AttributeRange = AttrDefAsset.AttributeRange;
	AttributeDef.ClampStrategyClass = AttrDefAsset.ClampStrategyClass;
	AttributeDef.ClampStrategyConfig = AttrDefAsset.ClampStrategyConfig;
	AttributeDef.AttributeCategory = AttrDefAsset.AttributeCategory;
	AttributeDef.AttributeAbbreviation = AttrDefAsset.AttributeAbbreviation;
	AttributeDef.AttributeTag = AttrDefAsset.AttributeTag;
	AttributeDef.AttributeName = AttrDefAsset.AttributeName;
	AttributeDef.AttributeDescription = AttrDefAsset.AttributeDescription;
	AttributeDef.bShowInUI = AttrDefAsset.bShowInUI;
	AttributeDef.Icon = AttrDefAsset.Icon;
	AttributeDef.bAsDecimal = AttrDefAsset.bAsDecimal;
	AttributeDef.bAsPercentage = AttrDefAsset.bAsPercentage;
}
