// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsEffectTriggerDefAsset.h"

#include "Asset/TcsPrimaryAssetEntry.h"



// 主资产类型取值 = **类名**（族内一致：UTcsAttributeDef → "TcsAttributeDef"、
// UTcsEffectChainDef → "TcsEffectChainDef"；2026-09-21 用户收口）
const FPrimaryAssetType UTcsEffectTriggerDefAsset::PrimaryAssetType(TEXT("TcsEffectTriggerDefAsset"));

// 向资产加载层自注册本域的主资产类型（PLN-REFACTOR-1 Task 8 随搬迁一并落地）。
// 扫描路径 = 宿主内容根；类型名由 StaticClass() 反射派生（零字符串面，类改名不必同步本处或任何 ini）。
//
// **策略 = `PreloadAll`（2026-10-26 用户裁定）**：本类是**装配型**——`UTcsDefinitionSubsystem::SeedWorld`
// 在世界初始化时逐条把它登记进该世界的门面，而 `OnDemand` 承诺"零加载"⇒ 缓存为空 ⇒ 装配 0 条 ⇒
// 门面每一次查询都返回 nullptr，而启动期**零 Error**（症状是"功能全失效"，日志却只刷"定义未在本世界登记"，
// 排障方向是错的）。⇒ 装配型类型 MUST NOT 用 `OnDemand`；定义库对它做硬拒（计入失败清单 + 不就绪）。
// 宿主若要按需加载，可用 `UTcsAssetButlerSettings::LoadPolicyByType` 覆盖为 `PreloadSelected`，但**不得**为 OnDemand。
UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsEffectTriggerDefAsset, { TEXT("/Game") }, ETcsAssetLoadPolicy::PreloadAll)



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