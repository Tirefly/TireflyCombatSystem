// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeDef.h"



// 定义主资产类型标识（Def 资产族统一：类型稳定、名取 tag 的 FName 形态——见头文件说明）
const FPrimaryAssetType UTcsAttributeDef::PrimaryAssetType = FPrimaryAssetType(TEXT("TcsAttributeDef"));



FPrimaryAssetId UTcsAttributeDef::GetPrimaryAssetId() const
{
	// 名取 DefTag 的 FName 形态而非资产名：资产文件可自由改名/挪目录而不失联。
	// FPrimaryAssetId 的 name 位是 FName（引擎类型约束），tag 须经 GetTagName() 转换。
	return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());
}



#if WITH_EDITOR
EDataValidationResult UTcsAttributeDef::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 验证身份（无效 DefTag 会让 [PrimaryAssetType, DefTag] 失去意义——DefLibrary 无法按 tag 解析到本资产）
	if (!DefTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("属性定义资产缺少有效的 DefTag：它是主资产身份的名，也是属性身份")));
		Result = EDataValidationResult::Invalid;
	}

	// 本资产带校验规则且未发现问题 → 明确报 Valid（引擎基类默认返回 NotValidated，
	// 语义是"没有规则"，会让编辑器把已校验通过的资产显示为"未验证"）
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif
