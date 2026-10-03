// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsEffectTriggerDefAsset.h"



// 主资产类型取值 = **类名**（族内一致：UTcsAttributeDef → "TcsAttributeDef"、
// UTcsEffectChainDef → "TcsEffectChainDef"；2026-09-21 用户收口）
const FPrimaryAssetType UTcsEffectTriggerDefAsset::PrimaryAssetType(TEXT("TcsEffectTriggerDefAsset"));



FPrimaryAssetId UTcsEffectTriggerDefAsset::GetPrimaryAssetId() const
{
	// 名取 TriggerTag 的 FName 形态（FPrimaryAssetId 的 name 位是 FName——引擎类型约束）
	return FPrimaryAssetId(PrimaryAssetType, TriggerTag.GetTagName());
}



#if WITH_EDITOR
EDataValidationResult UTcsEffectTriggerDefAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 身份非空：空 TriggerTag 让 [PrimaryAssetType, TriggerTag] 失去意义，且定义库必拒绝登记
	if (!TriggerTag.IsValid())
	{
		Context.AddError(NSLOCTEXT("TcsEffectTriggerDefAsset", "EmptyTriggerTag",
			"触发身份 TriggerTag 为空——资产无法按 [PrimaryAssetType, TriggerTag] 解析，且不会被登记。"));
		Result = EDataValidationResult::Invalid;
	}

	// 事件 Tag 非空：触发行按事件 Tag 路由，空 Tag 订阅不了任何事件
	if (!Def.EventTag.IsValid())
	{
		Context.AddError(NSLOCTEXT("TcsEffectTriggerDefAsset", "EmptyEventTag",
			"定义不完整：Def.EventTag 为空——触发行按事件 Tag 路由，空 Tag 无从订阅（定义库会拒绝登记该行）。"));
		Result = EDataValidationResult::Invalid;
	}

	// 链 id 非空：命中后无链可执行（且 ExecuteChain 的拒绝面会按事件逐次刷 Error）
	if (!Def.EffectChainId.IsValid())
	{
		Context.AddError(NSLOCTEXT("TcsEffectTriggerDefAsset", "EmptyEffectChainId",
			"定义不完整：Def.EffectChainId 为空——触发行命中后无链可执行（定义库会拒绝登记该行）。"));
		Result = EDataValidationResult::Invalid;
	}

	// 无错时提升为 Valid：基类默认返回 NotValidated（Obj.cpp:6096），不提升会让"已校验通过"显示为"未验证"
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif
