// Copyright Tirefly. All Rights Reserved.

#include "Flow/TcsDamageFlowCollectEvent.h"

#include "StructUtils/InstancedStruct.h"
#include "Trigger/TcsTriggerPayloadReader.h"



namespace
{
	/**
	 * 收集事件载荷读取器（台账 `LEDGER-reflection` R-2 调研发现的**登记缺失**补齐——
	 * `UE_DEFINE_TRIGGER_PAYLOAD_READER` 全库此前只有定义、零登记；用户 2026-09-27 裁定并入本批）：
	 * 把"这次伤害是谁打的 / 带哪些分类标签"从载荷译成触发求值器能用的主体信息。
	 *
	 * **`Caster` ← 攻击方**（`FTcsDamageFlowContext::Attacker`）——收集协议里"谁发起的"由触发行侧的
	 * `Instigator` 承载，不是本字段的语义；`ClassificationTags` 直通。
	 *
	 * 载荷缺失或 `Context` 空指针 → 返回**默认构造**（空句柄 + 空标签集）：不崩溃、不编造主体。
	 */
	FTcsTriggerPayloadInfo ReadDamageFlowCollectPayload(const FInstancedStruct& Payload)
	{
		const FTcsDamageFlowCollectEvent* Event = Payload.GetPtr<FTcsDamageFlowCollectEvent>();
		if (!Event || !Event->Context)
		{
			return FTcsTriggerPayloadInfo();
		}

		FTcsTriggerPayloadInfo Info;
		Info.Caster = Event->Context->Attacker;
		Info.ClassificationTags = Event->Context->ClassificationTags;
		return Info;
	}
}

// 属主模块自登记（TcsDamage 为自己发布的载荷登记读取器；模块静态初始化期登记——零 UObject 触达）
UE_DEFINE_TRIGGER_PAYLOAD_READER(FTcsDamageFlowCollectEvent, ReadDamageFlowCollectPayload)
