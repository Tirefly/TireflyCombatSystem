// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeDef.h"

#include "Asset/TcsPrimaryAssetEntry.h"



// 定义主资产类型标识（Def 资产族统一：类型稳定、名取 tag 的 FName 形态——见头文件说明）
const FPrimaryAssetType UTcsAttributeDef::PrimaryAssetType = FPrimaryAssetType(TEXT("TcsAttributeDef"));

// 主资产类型自注册（2026-10-17 补，ARCH-04 D-2 / ARCH-03 U-1）：
// 类型名由 `StaticClass()` 反射派生（零字符串面，类改名无需同步改本处）。
// **策略用 OnDemand**：属性定义今天是"宿主/定义库需要时才拿"，不是"启动期全量预热"——
// 与已注册的三个 Def 族（`TcsBuffDefAsset` / `TcsEffectChainDef` / `TcsEffectTriggerDefAsset`）同档。
// ⚠ 本注册**只是发现路径的前置**：`UTcsAttributeDef` 的插件侧 `DiscoverAttributeDefs` 仍未建
// （ARCH-04 U-1），今天仍只有宿主 `TcsDev` 在扫。注册在这里的意义是**把"未登记"这个失败面关掉**
// ——`FTcsPrimaryAssetQuery::GetAssetsForDefClass` 对未登记类型直接返回失败且报 Warning。
UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsAttributeDef, { TEXT("/Game") }, ETcsAssetLoadPolicy::OnDemand)



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
