// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeDef.h"

#include "Asset/TcsPrimaryAssetEntry.h"



// 定义主资产类型标识（Def 资产族统一：类型稳定、名取 tag 的 FName 形态——见头文件说明）
const FPrimaryAssetType UTcsAttributeDef::PrimaryAssetType = FPrimaryAssetType(TEXT("TcsAttributeDef"));

// 主资产类型自注册（2026-10-17 补，ARCH-04 D-2 / ARCH-03 U-1）：
// 类型名由 `StaticClass()` 反射派生（零字符串面，类改名无需同步改本处）。
//
// **策略已由 OnDemand 改为 `PreloadAll`（2026-10-26 用户裁定）**：本类是**装配型**——
// `UTcsDefinitionSubsystem::SeedWorld` 逐世界把它登记进 `UTcsAttributeSubsystem`，而 `OnDemand`
// 承诺"零加载"⇒ 缓存为空 ⇒ 装配 0 条 ⇒ `FindAttributeDef` 恒空 ⇒ `AddAttribute` 零定义 ⇒
// **实体零属性**（伤害系数、修正器全无落点），而启动期**零 Error**。
// 装配型类型 MUST NOT 用 `OnDemand`；定义库对它硬拒（计入失败清单 + 不就绪）。
//
// 原注释说「属性定义是宿主/定义库需要时才拿，不是启动期全量预热」——那句话描述的是**开发期观察**，
// 不是**可接受的运行语义**：装配型定义一进入世界就被门面全量持有，等价于"全程必需"。
UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsAttributeDef, { TEXT("/Game") }, ETcsAssetLoadPolicy::PreloadAll)



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
