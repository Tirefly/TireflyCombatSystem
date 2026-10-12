// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCostEvents.h"



// 消耗事件 Tag 定义（原生声明、模块导出；声明处见头文件）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Cast_CostApplied,
	"TcsEvent.Cast.CostApplied",
	"消耗已提交事件：内建属性资源策略扣减成功后广播"
	"（CanAfford 失败不发——门禁拒绝面由 TryActivate 返回值覆盖；载荷 FTcsCostEventPayload）");
