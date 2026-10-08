// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastEvents.h"



// 事件 Tag 定义（原生声明、模块导出；声明处见头文件）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_Started, "TcsEvent.Cast.Started",
	"施法开始事件：激活成功后广播（载荷 FTcsCastEventPayload）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_PhaseChanged, "TcsEvent.Cast.PhaseChanged",
	"施法时段变化事件（产生者归 Task 5 的时段推进；本 Task 只落词与载荷）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_Completed, "TcsEvent.Cast.Completed",
	"施法完成事件（产生者归 Task 5 的自然终结；本 Task 只落词与载荷）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_Interrupted, "TcsEvent.Cast.Interrupted",
	"施法被打断事件：顶替路径在本 Task 即产生（打断的完整结算归 Task 5）");
