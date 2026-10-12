// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCooldownEvents.h"



// 冷却事件 Tag 定义（原生声明、模块导出；声明处见头文件）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_CooldownStarted,
	"TcsEvent.Cast.CooldownStarted",
	"冷却开始事件：起冷成功后广播（自动档与手动档同一条路径；载荷 FTcsCooldownEventPayload）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_CooldownUpdated,
	"TcsEvent.Cast.CooldownUpdated",
	"冷却被改变事件：调整剩余时长或清轨后广播（载荷 FTcsCooldownEventPayload）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_CooldownEnded,
	"TcsEvent.Cast.CooldownEnded",
	"冷却结束事件：到期堆回调（权威侧——到期是时轴上的事实）广播（载荷 FTcsCooldownEventPayload）");
