// Copyright Tirefly. All Rights Reserved.

#include "Def/TcsStateDef.h"



// 主资产类型取值 = 类名（族内一致：UTcsAttributeDef → "TcsAttributeDef"、
// UTcsEffectChainDef → "TcsEffectChainDef"；2026-09-21 用户收口）
const FPrimaryAssetType UTcsStateDef::PrimaryAssetType(TEXT("TcsStateDef"));



FPrimaryAssetId UTcsStateDef::GetPrimaryAssetId() const
{
	// 名取 DefTag 的 FName 形态（FPrimaryAssetId 的 name 位是 FName——引擎类型约束）
	return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());
}



#if WITH_EDITOR
EDataValidationResult UTcsStateDef::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 身份非空：空 DefTag 让 [PrimaryAssetType, DefTag] 失去意义，定义库也必拒绝登记
	if (!DefTag.IsValid())
	{
		Context.AddError(FText::FromString(
			TEXT("定义身份 DefTag 为空：资产无法按 [PrimaryAssetType, DefTag] 解析，定义库也不会登记它")));
		Result = EDataValidationResult::Invalid;
	}

	// 本类带校验规则且未发现问题 → 明确报 Valid（引擎基类默认返回 NotValidated，
	// 语义是"没有规则"，会让编辑器把已校验通过的资产显示为"未验证"）
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif
