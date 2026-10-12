// Copyright Tirefly. All Rights Reserved.

#include "Param/TcsSkillParamKeys.h"



// 冷却缩减契约键定义（原生声明、模块导出；声明处与"为什么住本模块"见头文件）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsSkillParam_CooldownPct,
	"TcsStateParam.CooldownPct",
	"冷却缩减（CDR）契约键：本键上的修正不进普通参数折叠，而是作为系数作用于该次冷却各轨的 "
	"Duration（快照于冷却触发时刻，中途变化不追溯）；由拥有冷却语义的 TcsSkill 原生声明，"
	"宿主零配置即生效");

// 消耗修正契约键定义（2026-10-12 用户裁定：Cost 走与 CDR 同构的"参数键折叠"通道）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsSkillParam_CostAmount,
	"TcsStateParam.CostAmount",
	"消耗修正契约键：本键上的修正经五带折叠作用于 `FTcsCostConfig::Amount` 的求值结果 "
	"（**初值 = Amount 本身的求值**，不是 0）⇒ 百分比减免用 TAO_PercentAdd、固定值减免用 "
	"TAO_FlatAdd；由拥有消耗语义的 TcsSkill 原生声明，宿主零配置即生效");
