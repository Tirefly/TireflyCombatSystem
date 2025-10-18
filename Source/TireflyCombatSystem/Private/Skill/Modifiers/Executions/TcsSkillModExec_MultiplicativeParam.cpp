#include "Skill/Modifiers/Executions/TcsSkillModExec_MultiplicativeParam.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"
#include "Skill/Modifiers/TcsSkillModifierParams.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"
#include "TcsCombatSystemLogChannels.h"

void UTcsSkillModExec_MultiplicativeParam::ExecuteToEffect_Implementation(
	UTcsSkillInstance* SkillInstance,
	const FTcsSkillModifierInstance& ModInst,
	FTcsAggregatedParamEffect& InOutEffect)
{
	// 从载荷中提取参数
	const FInstancedStruct& Payload = ModInst.ModifierDef.ModifierParameter.ParamValueContainer;
	if (!Payload.IsValid() || Payload.GetScriptStruct() != FTcsModParam_Multiplicative::StaticStruct())
	{
		return;
	}

	const FTcsModParam_Multiplicative* Params = Payload.GetPtr<FTcsModParam_Multiplicative>();
	if (!Params)
	{
		return;
	}

	// 直接修改聚合效果（支持FName和Tag两种通道）
	InOutEffect.MulProd *= Params->Multiplier;

	UE_LOG(LogTcsSkill, VeryVerbose,
		TEXT("[Multiplicative Execution] Applied multiplier: %.2f (Name: %s, Tag: %s)"),
		Params->Multiplier, *Params->ParamName.ToString(), *Params->ParamTag.ToString());
}

void UTcsSkillModExec_MultiplicativeParam::Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance)
{
	// 具体乘法逻辑将在聚合阶段实现
}
