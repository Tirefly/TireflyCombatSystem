// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttrModDef.h"



// 定义主资产类型标识（Def 资产族统一：类型稳定、名取 TemplateId——见头文件说明）
const FPrimaryAssetType UTcsAttrModDef::PrimaryAssetType = FPrimaryAssetType(TEXT("TcsAttrModDef"));



FPrimaryAssetId UTcsAttrModDef::GetPrimaryAssetId() const
{
	// 名取 TemplateId 而非资产名：资产文件可自由改名/挪目录而不失联
	return FPrimaryAssetId(PrimaryAssetType, TemplateId);
}



#if WITH_EDITOR
EDataValidationResult UTcsAttrModDef::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 身份非空（TemplateId 是状态/技能定义引用本模板的唯一依据，也是主资产身份的名）
	if (TemplateId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("修正器模板缺少 TemplateId：状态/技能定义按它引用模板")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证被修饰属性名（空名无法路由到任何属性实例）
	if (Def.Target.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("修正器模板缺少被修饰的属性名：Target 为空，物化后无法路由到属性实例")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证运算数：属性换算必须给出属性名，字面量必须有数值来源
	if (Def.Operand.Kind == ETcsOperandKind::OPK_AttributeScaled)
	{
		if (Def.Operand.Attribute.IsNone())
		{
			Context.AddError(FText::FromString(TEXT("运算数种类为属性换算，但未指定被读取的属性名：请填写 Operand.Attribute")));
			Result = EDataValidationResult::Invalid;
		}
	}
	else if (!Def.Operand.Literal.Source.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("运算数种类为字面量，但数值来源为空：请在 Operand.Literal 选择数值来源")));
		Result = EDataValidationResult::Invalid;
	}

	// 值约定列白名单（D5-18 v3）：判据在数值来源自身（能力位虚函数）——不在此建立源类型名单，
	// 否则表型源（住 TcsState，本模块不可见）与宿主自定义源都会被错判
	if (Def.ValueConvention != ETcsValueConventionFlag::VCF_None)
	{
		if (!Def.Operand.Literal.Source.IsValid())
		{
			Context.AddError(FText::FromString(TEXT("配了值约定列，但数值来源为空：约定无从作用，请先配置 Operand.Literal 的数值来源")));
			Result = EDataValidationResult::Invalid;
		}
		else if (!Def.Operand.Literal.Source.Get().AllowsValueConvention())
		{
			const UScriptStruct* SourceStruct = Def.Operand.Literal.Source.GetScriptStruct();
			const FString SourceName = SourceStruct ? SourceStruct->GetName() : TEXT("<未知来源>");

			Context.AddError(FText::FromString(FString::Printf(
				TEXT("数值来源 %s 不允许配值约定列：它读到的已是规范值（再转换 = 二次转换），"
					"或约定作用在系数与乘积上无定义。系数若需百分比语义，请直接书写小数"),
				*SourceName)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 覆盖优先级只对覆盖带有意义（2026-09-18）：非 Override 带填了它不会参与折叠——
	// 给一条警告而非错误（配置本身无害，静默才是问题）
	if (Def.Op != ETcsAttributeOp::TAO_Override && Def.OverridePriority != 0)
	{
		Context.AddWarning(FText::FromString(
			TEXT("OverridePriority 只在运算带为「覆盖」时参与折叠：当前带非覆盖，该值将被忽略")));
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
