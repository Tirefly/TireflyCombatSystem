// Copyright Tirefly. All Rights Reserved.

#include "Flow/TcsFlowKeys.h"



// 黑板契约键（框架词汇——步骤之间的接口，见头文件说明）
// **每个键带 `DevComment`**（编辑器 tag 树 tooltip 的载体）：语义 / 谁声明谁消费 / （若为验证词）退役判据
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_BaseDamage, "DamageFlowKey.BaseDamage",
	"伤害量唯一契约键（基值以 Add 落在它上面、收集到的修正同键折叠）；由插件 TcsDamage 原生声明·标准步骤库与宿主步骤共同读写");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_Executed, "DamageFlowKey.Executed",
	"本次实际执行量结果键（Override 语义）；由插件 TcsDamage 原生声明·Execute 步写入·Completed 步读出");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_Absorbed, "DamageFlowKey.Absorbed",
	"本次被护盾吸收量结果键（Override 语义）；由插件 TcsDamage 原生声明·Execute 步写入·Completed 步读出");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_Kill, "DamageFlowKey.Kill",
	"击杀记账键（本次事务提交后目标生命 ≤ 0·死亡规则仍归宿主）；由插件 TcsDamage 原生声明·Execute 步写入");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_Hit, "DamageFlowKey.Hit",
	"命中结果键（1 = 命中·0 = 未命中）；由插件 TcsDamage 原生声明·Hit 步写入·宿主步骤可读");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_Crit, "DamageFlowKey.Crit",
	"暴击结果键（> 0 = 暴击）；由插件 TcsDamage 原生声明·Crit 步写入·Completed 步读出");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_ExecuteCandidates, "DamageFlowKey.ExecuteCandidates",
	"免疫/减伤候选落点键（PreExecute 收集 ↔ Execute 裁决之间的传递通道）；由插件 TcsDamage 原生声明·宿主在收集期提交候选");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_HitRate, "DamageFlowKey.HitRate",
	"命中率输入键（宿主响应方可在 Hit 收集事件后改写·Hit 步读回）；由插件 TcsDamage 原生声明·宿主写入");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowKey_CritRate, "DamageFlowKey.CritRate",
	"暴击率输入键（宿主响应方可在 Crit 收集事件后改写·Crit 步读回）；由插件 TcsDamage 原生声明·宿主写入");

// 官方默认流程模板（框架自带）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_DamageFlowTemplate_Default, "DamageFlowTemplate.Default",
	"官方默认流程模板 id（四步：CollectStart → BaseDamage → Execute → Completed）；由插件 TcsDamage 原生声明·宿主注册模板或调用链原语时可直接引用或另配");
