// Copyright Tirefly. All Rights Reserved.

#include "Def/TcsBuffDefAsset.h"

#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"



// 主资产类型取值 = 类名（派生族各自遮蔽基类静态成员——见头文件说明）
const FPrimaryAssetType UTcsBuffDefAsset::PrimaryAssetType(TEXT("TcsBuffDefAsset"));



FPrimaryAssetId UTcsBuffDefAsset::GetPrimaryAssetId() const
{
	// 名取 DefTag 的 FName 形态（FPrimaryAssetId 的 name 位是 FName——引擎类型约束）
	return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());
}



#if WITH_EDITOR
// 内部函数：描述配置的作者期校验（判据声明处 = state-def-asset 能力的「描述配置载体」需求）
namespace
{
	// 返回是否报出过错误；错误消息 MUST 点名出问题的条目下标与字段（可操作定位）
	bool ValidateBuffDefDescriptions(
		const TArray<FTcsDescriptionEntry>& Descriptions,
		FDataValidationContext& Context)
	{
		bool bHasError = false;

		for (int32 EntryIndex = 0; EntryIndex < Descriptions.Num(); ++EntryIndex)
		{
			const FTcsDescriptionEntry& Entry = Descriptions[EntryIndex];

			if (Entry.DescriptionId.IsNone())
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Descriptions[%d] 缺少 DescriptionId：描述入口靠它区分（\"Tip\" / \"Codex\" / \"LevelUpPreview\"…）"),
					EntryIndex)));
				bHasError = true;
			}

			if (Entry.TextKey.IsNone())
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Descriptions[%d] 缺少 TextKey：文案原文住 StringTable，TextKey 是它的键"), EntryIndex)));
				bHasError = true;
			}

			// 槽名逐个查：空槽名与同条目内重复都要报（槽名 = 文案里占位符的名字）
			TSet<FName> SeenSlotNames;
			for (int32 SlotIndex = 0; SlotIndex < Entry.Views.Num(); ++SlotIndex)
			{
				const FName SlotName = Entry.Views[SlotIndex].SlotName;
				if (SlotName.IsNone())
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("Descriptions[%d].Views[%d] 缺少 SlotName：槽名要与 StringTable 文案里的占位符同名"),
						EntryIndex, SlotIndex)));
					bHasError = true;
					continue;
				}

				bool bAlreadyInSet = false;
				SeenSlotNames.Add(SlotName, &bAlreadyInSet);
				if (bAlreadyInSet)
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("Descriptions[%d] 内槽名重复（%s）：同一条文案里同名槽位会互相覆盖"),
						EntryIndex, *SlotName.ToString())));
					bHasError = true;
				}
			}
		}

		return bHasError;
	}
}
#endif



#if WITH_EDITOR
EDataValidationResult UTcsBuffDefAsset::IsDataValid(FDataValidationContext& Context) const
{
	// 基类先判身份（DefTag），派生段只判定义数据
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 错误一：参数行键重复（同键两行会让快照写入互相覆盖——静默丢一行是最坏结果）
	TSet<FGameplayTag> SeenParamKeys;
	for (const FTcsNumericParamRow& Row : BuffDef.Params)
	{
		bool bAlreadyInSet = false;
		SeenParamKeys.Add(Row.Key, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Params 里出现重复键（%s）：同键两行会让快照写入互相覆盖，请删掉一条"), *Row.Key.ToString())));
			Result = EDataValidationResult::Invalid;
		}

		// 错误五：值约定白名单（R5 Task 3）——判据走**虚分派**读该行数值来源的能力位
		// （MUST NOT 建"源类型 × 可配约定"的中心名单：宿主自定义源自行声明、零公共代码改动）。
		// 运行期不拦（快照构建对这类行只按"不转换"降级），故作者期这一道门是唯一暴露点。
		if (Row.ValueConvention != ETcsValueConventionFlag::VCF_None)
		{
			const FTcsParamValueSource* Source = Row.Base.Source.GetPtr<FTcsParamValueSource>();
			if (Source && !Source->AllowsValueConvention())
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("参数行 %s 配了值约定列，但该行的数值来源不允许值约定（引用类源 / 域侧换算源读到的已是规范值，"
						"再转即二次转换）：请把值约定改回「无」，或把数值来源换成书写值即结果的那类源"),
					*Row.Key.ToString())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	// 错误二：修正器引用行有空引用（空引用在物化期无从解析，且它正是"引用位留了没填"的形态）
	for (int32 RowIndex = 0; RowIndex < BuffDef.ModifierRows.Num(); ++RowIndex)
	{
		if (BuffDef.ModifierRows[RowIndex].IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("ModifierRows[%d] 是空引用：apply 时按模板物化，空引用无从解析"), RowIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 错误三/四：内联触发行的两个硬前置（登记期会走 ensure 拒绝面，故在编辑器提前报出）
	for (int32 TriggerIndex = 0; TriggerIndex < BuffDef.Triggers.Num(); ++TriggerIndex)
	{
		const FTcsEffectTriggerDef& Trigger = BuffDef.Triggers[TriggerIndex];

		if (!Trigger.EventTag.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Triggers[%d].EventTag 无效：触发行按事件 Tag 路由，空 Tag 无从订阅"), TriggerIndex)));
			Result = EDataValidationResult::Invalid;
		}

		if (!Trigger.EffectChainId.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Triggers[%d].EffectChainId 无效：命中后无链可执行"), TriggerIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 错误五：有限时值却没有时长来源（Finite 的到期条目依赖它——配空即"以为有限、实际永不到期"）
	if (BuffDef.DurationPolicy == EDurationPolicy::EDP_Finite && !BuffDef.DurationTime.Source.IsValid())
	{
		Context.AddError(FText::FromString(
			TEXT("DurationPolicy 为「有限」但 DurationTime 的数值来源为空：请选择数值来源，或把 DurationPolicy 改为「无限」")));
		Result = EDataValidationResult::Invalid;
	}

	// 错误六：描述配置项缺项（规则只在「描述配置载体」需求声明一处，此处不重复判据）
	if (ValidateBuffDefDescriptions(BuffDef.Descriptions, Context))
	{
		Result = EDataValidationResult::Invalid;
	}

	// 警告一：负周期（配置无害但不得静默——负值不会产生任何周期条目）
	if (BuffDef.Period < 0.0)
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("Period 为负（%f）：不会产生周期条目，请改成 0（无周期）或正数"), BuffDef.Period)));
	}

	// 警告二：配了周期刷新却没配周期（该轴在无周期时不起任何作用）
	if (BuffDef.PeriodRefresh != ETcsPeriodRefresh::EPR_Keep && BuffDef.Period == 0.0)
	{
		Context.AddWarning(FText::FromString(
			TEXT("PeriodRefresh 非「保持」但 Period 为 0：该轴在没有周期时不起作用（可忽略或清回「保持」）")));
	}

	// 无错无警时提升为 Valid：基类默认返回 NotValidated（Obj.cpp:6096），不提升会让"已校验通过"显示为"未验证"
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif
