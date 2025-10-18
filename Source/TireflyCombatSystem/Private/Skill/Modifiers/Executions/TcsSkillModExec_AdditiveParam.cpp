#include "Skill/Modifiers/Executions/TcsSkillModExec_AdditiveParam.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"
#include "Skill/Modifiers/TcsSkillModifierParams.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"
#include "TcsLogChannels.h"

void UTcsSkillModExec_AdditiveParam::ExecuteToEffect_Implementation(
	UTcsSkillInstance* SkillInstance,
	const FTcsSkillModifierInstance& ModInst,
	FTcsAggregatedParamEffect& InOutEffect)
{
	// 从载荷中提取参数
	const FInstancedStruct& Payload = ModInst.ModifierDef.ModifierParameter.ParamValueContainer;
	if (!Payload.IsValid() || Payload.GetScriptStruct() != FTcsModParam_Additive::StaticStruct())
	{
		return;
	}

	const FTcsModParam_Additive* Params = Payload.GetPtr<FTcsModParam_Additive>();
	if (!Params)
	{
		return;
	}

	// 直接修改聚合效果（支持FName和Tag两种通道）
	InOutEffect.AddSum += Params->Magnitude;

	UE_LOG(LogTcsSkill, VeryVerbose,
		TEXT("[Additive Execution] Applied magnitude: %.2f (Name: %s, Tag: %s)"),
		Params->Magnitude, *Params->ParamName.ToString(), *Params->ParamTag.ToString());
}

void UTcsSkillModExec_AdditiveParam::Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance)
{
	// 具体叠加逻辑将在 UTcsSkillComponent 聚合阶段实现
}
