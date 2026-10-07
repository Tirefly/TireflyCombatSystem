// Copyright Tirefly. All Rights Reserved.

#include "Def/TcsSkillDef.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



// 主资产类型取类名，派生资产必须同时覆写身份函数。
const FPrimaryAssetType UTcsSkillDef::PrimaryAssetType(TEXT("TcsSkillDef"));



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
