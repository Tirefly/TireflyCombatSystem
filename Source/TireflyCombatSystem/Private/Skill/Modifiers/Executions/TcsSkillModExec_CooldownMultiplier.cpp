#include "Skill/Modifiers/Executions/TcsSkillModExec_CooldownMultiplier.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"
#include "Skill/Modifiers/TcsSkillModifierParams.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"
#include "TcsCombatSystemLogChannels.h"

void UTcsSkillModExec_CooldownMultiplier::ExecuteToEffect_Implementation(
	UTcsSkillInstance* SkillInstance,
	const FTcsSkillModifierInstance& ModInst,
	FTcsAggregatedParamEffect& InOutEffect)
{
	// 从载荷中提取参数
	const FInstancedStruct& Payload = ModInst.ModifierDef.ModifierParameter.ParamValueContainer;
	if (!Payload.IsValid() || Payload.GetScriptStruct() != FTcsModParam_Scalar::StaticStruct())
	{
		return;
	}

	const FTcsModParam_Scalar* Params = Payload.GetPtr<FTcsModParam_Scalar>();
	if (!Params)
	{
		return;
	}

	// 应用冷却倍率
	InOutEffect.CooldownMultiplier *= Params->Value;

	UE_LOG(LogTcsSkill, VeryVerbose,
		TEXT("[Cooldown Multiplier Execution] Applied cooldown multiplier: %.2f"),
		Params->Value);
}

void UTcsSkillModExec_CooldownMultiplier::Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance)
{
	// 聚合阶段统一处理冷却倍率
}
