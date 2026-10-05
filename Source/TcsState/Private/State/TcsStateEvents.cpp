// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateEvents.h"

#include "Trigger/TcsTriggerPayloadReader.h"



namespace
{
	/**
	 * 状态生命周期载荷的读取器：属主模块把单位身份与实例主体交给下层的通用触发路由。
	 *
	 * Caster 与 Subject 分工不同：Caster 供条件求值；Subject 只限制实例局部事件的投递范围。
	 * 直接读载荷快照，不回查状态门面——移除事件中实例可能已进入释放流程。
	 *
	 * @param Payload 状态生命周期事件载荷。
	 * @return 返回主体信息；载荷类型不符时返回默认构造（不 ensure）。
	 */
	FTcsTriggerPayloadInfo TcsStateEvents_ReadTriggerPayload(const FInstancedStruct& Payload)
	{
		FTcsTriggerPayloadInfo Info;
		const FTcsStateEventPayload* StateEvent = Payload.GetPtr<FTcsStateEventPayload>();
		if (!StateEvent)
		{
			return Info;
		}

		Info.Caster = StateEvent->Instigator.IsValid() ? StateEvent->Instigator : StateEvent->Unit;
		Info.Subject = FInstancedStruct::Make<FTcsStateHandle>(StateEvent->Handle);
		return Info;
	}
}

// 继 TcsDamage 收集事件读取器之后，状态载荷也由自己的模块登记；TcsEffect 不依赖 TcsState。
UE_DEFINE_TRIGGER_PAYLOAD_READER(FTcsStateEventPayload, TcsStateEvents_ReadTriggerPayload)
