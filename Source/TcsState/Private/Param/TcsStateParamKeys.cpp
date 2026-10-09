// Copyright Tirefly. All Rights Reserved.

#include "Param/TcsStateParamKeys.h"



// 等级契约键定义（原生声明、模块导出；声明处与"为什么住本模块"见头文件）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsStateParam_Level, "TcsStateParam.Level",
	"生效等级修正契约键（框架契约词，D3-11）：挂在本键上的修正不进普通参数折叠，而是累加进 "
	"EffectiveLevel = clamp(0, LevelBase + Σ)；由拥有等级语义的 TcsState 原生声明，宿主零配置即生效");
