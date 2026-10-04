// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateModifierMaterializer.h"

#include "Attribute/TcsAttrModDef.h"
#include "Def/TcsBuffDef.h"
#include "Host/TcsEntityLevelProvider.h"
#include "Parameter/TcsParamValue.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateOps.h"
#include "State/TcsStateParamTableReader.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"
#include "TcsValueConvention.h"



void FTcsStateModifierMaterializer::Materialize(
	UTcsStateSubsystem& Subsystem,
	const FTcsBuffDef& Def,
	const FTcsStateInstance& Instance,
	TArray<FTcsAttrModInstance>& Out)
{
	// 重建语义：先清空（刷新路径与首挂共用本函数——"条数与数值恒与最近一次快照一致"靠这一句成立）
	Out.Reset();

	if (Def.ModifierRows.Num() == 0)
	{
		return;
	}

	// 快照绑定作用域：进入时把该实例的快照装进反射壳、退出时恢复上一份绑定（嵌套按栈恢复）。
	// 作用域覆盖整段求值——期间任何引用类操作数读到的都是**这个实例的**快照。
	FTcsStateSnapshotScope Scope(Subsystem.GetParamTableReader(), &Instance.ParamSnapshot);

	// 上下文：与快照构建**同一处装配**（`MakeContext` 是唯一装配点）——等级类源拿到同一个
	// `EffectiveLevel`、宿主源拿到同一个主体，不会出现"快照一个档、操作数另一个档"的双口径。
	// `ParamTable` 位 = 该实例的快照（D3-19："物化点从施加方 ParamSnapshot 解析"）。
	FTcsStateEvaluateContext Ctx;
	FTcsStateOps::MakeContext(
		Ctx, Subsystem, Instance.Unit, Instance.Instigator, Instance.Level, Scope.GetTable());

	Out.Reserve(Def.ModifierRows.Num());

	for (const TSoftObjectPtr<UTcsAttrModDef>& Row : Def.ModifierRows)
	{
		// ① 解析模板：先取**已加载对象**（装置/宿主可能持有瞬态模板），仅在为空时才同步加载。
		// 顺序有意如此：软引用命中已加载对象时 MUST NOT 触发一次无谓的加载请求。
		UTcsAttrModDef* ModDef = Row.Get();
		if (!ModDef)
		{
			ModDef = Row.LoadSynchronous();
		}

		if (!ModDef)
		{
			// 单条解析失败不中断物化：状态已经生效，缺一条修正器不是"施加失败"（挂点侧留 Warning）
			UE_LOG(LogTcsState, Warning, TEXT("修正器物化跳过：模板无法解析（定义=%s 单位=%lld）"),
				*Instance.DefTag.ToString(), Instance.Unit.Id);
			continue;
		}

		const FTcsAttrModDefTableRow& Template = ModDef->Def;

		// ② 操作数：`OPK_AttributeScaled` 按设计**不物化**（保持 live 求值），属性与系数原样进账本
		FTcsAttrModOperand Operand;
		Operand.Kind = Template.Operand.Kind;
		Operand.Attribute = Template.Operand.Attribute;
		Operand.Coefficient = Template.Operand.Coefficient;

		if (Template.Operand.Kind == ETcsOperandKind::OPK_Literal)
		{
			const double RawValue = Template.Operand.Literal.Evaluate(Ctx);

			// ③ 值约定：物化边界唯一一次转换。能力位判据归**源自身声明**（虚分派，PV-10）——
			// 引用源/换算源返回 false（前者读到的已是规范值、后者约定作用对象有歧义）。
			const FTcsParamValueSource* ValueSource = Template.Operand.Literal.Source.GetPtr<FTcsParamValueSource>();
			const bool bAllowConvention = ValueSource && ValueSource->AllowsValueConvention();

			Operand.Literal = bAllowConvention
				? FTcsValueConvention::ConvertToCanonical(RawValue, static_cast<int32>(Template.ValueConvention))
				: RawValue;
		}

		// ④ 装配（字段映射的唯一声明处；`Source` = 状态实例的来源句柄 ⇒ 一次级联摘除全清）
		Out.Add(FTcsAttrModInstance::MakeFromDef(Template, Operand, Instance.Source));
	}

	UE_LOG(LogTcsState, Log, TEXT("状态修正器物化：定义=%s 单位=%lld 模板=%d 条目=%d 来源=%llu"),
		*Instance.DefTag.ToString(), Instance.Unit.Id, Def.ModifierRows.Num(), Out.Num(), Instance.Source.Id);
}
