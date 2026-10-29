// Copyright Tirefly. All Rights Reserved.

#include "Def/TcsSkillDef.h"

#include "Asset/TcsPrimaryAssetEntry.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



// 主资产类型取类名，派生资产必须同时覆写身份函数。
const FPrimaryAssetType UTcsSkillDef::PrimaryAssetType(TEXT("TcsSkillDef"));

// 主资产类型自注册（2026-10-26 用户裁定第 7 条）。
//
// **为什么补登记**：`GetPrimaryAssetId()` 早已覆写，只差注册。不登记的后果是
// `FTcsPrimaryAssetQuery::GetAssetsForDefClass` 直接判"未登记"并返回失败 ⇒
// `DiscoverDefs` 把它记进失败清单 ⇒ 按「失败即永不就绪」口径**整个 TCS 停摆**。
// 登记后它才是一个可被发现、可按策略加载的独立 `FPrimaryAssetType`（与 `UTcsBuffDefAsset` 同层）。
//
// **策略 = `PreloadAll`**：本类是**装配型**（`SeedWorld` 逐世界 `RegisterSkillDef`），
// 同 `UTcsBuffDefAsset` / `UTcsAttributeDef` 的理由——装配型 MUST NOT 用 `OnDemand`。
UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsSkillDef, { TEXT("/Game") }, ETcsAssetLoadPolicy::PreloadAll)



FPrimaryAssetId UTcsSkillDef::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());
}

#if WITH_EDITOR
EDataValidationResult UTcsSkillDef::IsDataValid(
	FDataValidationContext& Context) const
{
	// 基类先判 DefTag 身份；派生段只判批准规格的六类内容规则。
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 同表同键会覆盖：数值表与布尔表分别判重复，不跨表添加约束。
	TSet<FGameplayTag> SeenParamKeys;
	for (int32 RowIndex = 0; RowIndex < SkillDef.Params.Num(); ++RowIndex)
	{
		const FGameplayTag Key = SkillDef.Params[RowIndex].Key;
		bool bAlreadyInSet = false;
		SeenParamKeys.Add(Key, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Params[%d].Key 重复（%s）：同一参数键只能配置一行"),
				RowIndex, *Key.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}

	TSet<FGameplayTag> SeenSwitchKeys;
	for (int32 RowIndex = 0; RowIndex < SkillDef.BoolSwitches.Num(); ++RowIndex)
	{
		const FGameplayTag Key = SkillDef.BoolSwitches[RowIndex].Key;
		bool bAlreadyInSet = false;
		SeenSwitchKeys.Add(Key, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.BoolSwitches[%d].Key 重复（%s）：同一开关键只能配置一行"),
				RowIndex, *Key.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 空引用无模板可解析；此处不强制加载或重复定义库的发现规则。
	for (int32 RowIndex = 0; RowIndex < SkillDef.ModifierRows.Num(); ++RowIndex)
	{
		if (SkillDef.ModifierRows[RowIndex].IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.ModifierRows[%d] 是空引用：请选择修正器模板或删除该行"), RowIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 空时段表合法（瞬发）；仅检查已配置时段的时长来源是否为空。
	for (int32 PhaseIndex = 0; PhaseIndex < SkillDef.Phases.Num(); ++PhaseIndex)
	{
		if (!SkillDef.Phases[PhaseIndex].Duration.Source.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Phases[%d].Duration.Source 为空：请选择时长参数来源"), PhaseIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 内联触发行的事件路由与效果链身份都是登记前置。
	for (int32 TriggerIndex = 0; TriggerIndex < SkillDef.Triggers.Num(); ++TriggerIndex)
	{
		const FTcsEffectTriggerDef& Trigger = SkillDef.Triggers[TriggerIndex];
		if (!Trigger.EventTag.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Triggers[%d].EventTag 无效：触发行无法订阅事件"), TriggerIndex)));
			Result = EDataValidationResult::Invalid;
		}

		if (!Trigger.EffectChainId.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Triggers[%d].EffectChainId 无效：触发行没有可执行的效果链身份"), TriggerIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (!SkillDef.CastChainId.IsValid())
	{
		Context.AddError(FText::FromString(
			TEXT("SkillDef.CastChainId 无效：请选择主效果链身份")));
		Result = EDataValidationResult::Invalid;
	}

	// 关系表的两条自指错误（2026-10-16 R5.5-e1；与状态侧同款同判据）
	// 判据与跨资产部分的归属说明见 UTcsBuffDefAsset::IsDataValid 的同段注释。
	if (DefTag.IsValid() && SkillDef.ForbiddenTags.Contains(DefTag))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("ForbiddenTags 含自己的 DefTag（%s）：自指的门禁永不可满足，该技能永远无法激活"),
			*DefTag.ToString())));
		Result = EDataValidationResult::Invalid;
	}

	if (DefTag.IsValid() && SkillDef.TagsToCancel.Contains(DefTag))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("TagsToCancel 含自己的 DefTag（%s）：自取消无意义（激活的瞬间就把自己取消掉）——请删掉这个词"),
			*DefTag.ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// 描述仅校验批准规格中的入口与文本键；视图机制归 R8。
	for (int32 EntryIndex = 0; EntryIndex < SkillDef.Descriptions.Num(); ++EntryIndex)
	{
		const FTcsDescriptionEntry& Entry = SkillDef.Descriptions[EntryIndex];
		if (Entry.DescriptionId.IsNone())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Descriptions[%d].DescriptionId 为空：请配置描述入口标识"), EntryIndex)));
			Result = EDataValidationResult::Invalid;
		}

		if (Entry.TextKey.IsNone())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("SkillDef.Descriptions[%d].TextKey 为空：请配置 StringTable 文本键"), EntryIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 已有校验规则的合法资产必须明确返回 Valid，不保留引擎基类的 NotValidated。
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif
