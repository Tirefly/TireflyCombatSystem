// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsEffectChainDef.h"



// 主资产类型取值 = **类名**（族内一致：UTcsAttributeDef → "TcsAttributeDef"、UTcsAttrModDef → "TcsAttrModDef"；
// 2026-09-21 用户收口——原写 "TcsEffectChain" 是族内唯一例外）
const FPrimaryAssetType UTcsEffectChainDef::PrimaryAssetType(TEXT("TcsEffectChainDef"));



FPrimaryAssetId UTcsEffectChainDef::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, ChainId);
}



#if WITH_EDITOR
EDataValidationResult UTcsEffectChainDef::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 身份非空：空 ChainId 让 [PrimaryAssetType, ChainId] 失去意义，且登记必被拒（提前在编辑器报出）
	if (ChainId.IsNone())
	{
		Context.AddError(NSLOCTEXT("TcsEffectChainDef", "EmptyChainId",
			"链身份 ChainId 为空——资产无法按 [PrimaryAssetType, ChainId] 解析，且不会被登记。"));
		Result = EDataValidationResult::Invalid;
	}

	// 双真相禁令：Chain.ChainId 与 ChainId 必须一致（不一致时读者无从判断以哪个为准）
	if (!Chain.ChainId.IsNone() && Chain.ChainId != ChainId)
	{
		Context.AddError(FText::Format(
			NSLOCTEXT("TcsEffectChainDef", "ChainIdMismatch",
				"双真相：资产身份 ChainId（{0}）与链数据 Chain.ChainId（{1}）不一致——请消歧（两者必须取同一值）。"),
			FText::FromName(ChainId), FText::FromName(Chain.ChainId)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
