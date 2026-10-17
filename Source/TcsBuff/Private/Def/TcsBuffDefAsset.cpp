// Copyright Tirefly. All Rights Reserved.

#include "Def/TcsBuffDefAsset.h"

#include "Asset/TcsPrimaryAssetEntry.h"
#include "Parameter/TcsParamValueSource.h"
#include "State/TcsBuffBehaviorFragment.h"
#include "TcsValueConvention.h"



// 主资产类型取值 = 类名（派生族各自遮蔽基类静态成员——见头文件说明）
const FPrimaryAssetType UTcsBuffDefAsset::PrimaryAssetType(TEXT("TcsBuffDefAsset"));

// 向资产加载层自注册本域的主资产类型（2026-10-15 PLN-REFACTOR-1 Task 6 补）。
//
// **为什么 MUST 登记**（实测，不是预建）：本类于 Task 4 自 `TcsState` 搬入 `TcsBuff`，
// 而旧的定义库发现走 `IAssetRegistry::GetAssetsByClass(当前类路径)` —— 类换模块后该查询
// **静默返回 0**（`CoreRedirects` 只在加载期映射，不改写 AssetRegistry 索引记录的类路径）。
// 实测后果：PIE 装置 `Tcs.Test.State.Run` 由 **26/0 退为 6/9**（S2「buff 定义 未命中」
// 及其下游 S5~S12 连锁 FAIL）。⇒ 改走按 `FPrimaryAssetType` 匹配的官方口，故本处 MUST 登记。
//
// 扫描路径 = 宿主内容根；类型名由 `StaticClass()` 反射派生（零字符串面）。
UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsBuffDefAsset, { TEXT("/Game") }, ETcsAssetLoadPolicy::OnDemand)



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

	// 错误六：关系表的**自指**（2026-10-16 R5.5-e1 新增）
	// 判据：`ForbiddenTags` 含自己的主体词 ⇒ 自我否定的门禁、**永不可满足**。
	// 注意主体词 = 本定义的 `DefTag`（平铺数组的语义：主体即本定义自己）。
	if (DefTag.IsValid() && BuffDef.ForbiddenTags.Contains(DefTag))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("ForbiddenTags 含自己的 DefTag（%s）：自指的门禁永不可满足，该状态永远无法施加——请删掉这个词"),
			*DefTag.ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// 错误七：关系表的**双向强夺**（互指取消）
	// 判据：`TagsToCancel` 与对方互相包含 ⇒ 两条声明各自都合法（都是独占策略），
	// 但同时成立时结果**依赖施加顺序**、作者无法预测 ⇒ 报错。
	// **本规则 MUST NOT 被推广成「ForbiddenTags 与 TagsToCancel 同命中即错」**——
	// 那个组合是「在位者赢」的**合法**表达（见 state-relation-check 能力的《填法即策略》需求）。
	// **单资产内可判的那一半**：自己的 `TagsToCancel` 含自己的 DefTag（自取消）同样无意义。
	if (DefTag.IsValid() && BuffDef.TagsToCancel.Contains(DefTag))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("TagsToCancel 含自己的 DefTag（%s）：自取消无意义（施加的瞬间就把自己摘掉）——请删掉这个词"),
			*DefTag.ToString())));
		Result = EDataValidationResult::Invalid;
	}

	// **跨资产的双向强夺**（A 取消 B 且 B 取消 A）在**单资产 IsDataValid 里判不了**：
	// 该规则需要同时看到两个定义，而本接口的契约是「判这一份资产」。
	// ⇒ 它归**定义库的发现期**（`UTcsDefinitionSubsystem`，能看到全部已发现定义）——
	// **本变更如实登记为未落面**（MUST NOT 假装已在作者期拦下）。

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

	// 行为片段的两条作者期规则：空载荷与类型不符分别报出，运行期降级不代替配置检查。
	for (int32 FragmentIndex = 0; FragmentIndex < BuffDef.Fragments.Num(); ++FragmentIndex)
	{
		const FInstancedStruct& Fragment = BuffDef.Fragments[FragmentIndex];
		if (!Fragment.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Fragments[%d] 载荷为空：请选择行为片段，或删除该空项"), FragmentIndex)));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (!Fragment.GetScriptStruct()->IsChildOf(FTcsBuffBehaviorFragment::StaticStruct()))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Fragments[%d] 类型不符：%s 不是状态行为片段"),
				FragmentIndex, *Fragment.GetScriptStruct()->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 错误五：有限时值却没有时长来源（Finite 的到期条目依赖它——配空即"以为有限、实际永不到期"）
	if (BuffDef.DurationPolicy == EBuffDurationPolicy::EBDP_Finite && !BuffDef.DurationTime.Source.IsValid())
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
	if (BuffDef.PeriodRefresh != ETcsBuffPeriodRefresh::EBPR_Keep && BuffDef.Period == 0.0)
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
