// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateEvents.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateOps.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



// 状态生命周期事件 Tag（**原生声明**：广播面是框架契约，
// 若由宿主配置则宿主漏配即静默破坏广播面——故不可下放给 ini，见 gameplay-tag-governance）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_Applied, "TcsEvent.State.Applied",
	"状态施加成功（新实例）：载荷 FTcsStateEventPayload");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_Refreshed, "TcsEvent.State.Refreshed",
	"状态刷新（重复施加命中同组成活实例）：载荷 FTcsStateEventPayload");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_StackChanged, "TcsEvent.State.StackChanged",
	"状态层数变化：载荷 FTcsStateEventPayload（R5 Task 5 起可达）");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_Expired, "TcsEvent.State.Expired",
	"状态到期（自然结束）：载荷 FTcsStateEventPayload，Cause = Expired");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_Removed, "TcsEvent.State.Removed",
	"状态被移除 / 取消 / 单位注销：载荷 FTcsStateEventPayload，Cause 由调用方给定");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_State_Periodic, "TcsEvent.State.Periodic",
	"状态周期到点：载荷 FTcsStateEventPayload（产生者归 R5 Task 3）");



// 广播
void UTcsStateSubsystem::BroadcastStateEvent(
	FGameplayTag EventTag,
	const FTcsStateInstance& Instance,
	EStateRemoveCause Cause)
{
	UTcsEventBusSubsystem* Bus = GetEventBus();
	if (!Bus)
	{
		// 世界拆解期（总线已回收）——时序而非配置错误，故 Warning 不 ensure
		UE_LOG(LogTcsState, Warning, TEXT("状态事件丢弃：事件总线不可得（Tag=%s 定义=%s）"),
			*EventTag.ToString(), *Instance.DefTag.ToString());
		return;
	}

	FTcsStateEventPayload Payload;
	Payload.Handle = Instance.Handle;
	Payload.DefTag = Instance.DefTag;
	Payload.Source = Instance.Source;
	Payload.Instigator = Instance.Instigator;
	Payload.Stacks = Instance.Stacks;
	Payload.Level = Instance.Level;
	Payload.Cause = Cause;

	FInstancedStruct PayloadStruct;
	PayloadStruct.InitializeAs<FTcsStateEventPayload>(Payload);

	// 立即通道：同一提交内到达——订阅者可在本次调用返回前读到实例（"先广播后释放槽位"的意义所在），
	// 装置也能在同一帧断言（帧末通道会让断言晚一拍）
	Bus->PublishImmediate(EventTag, PayloadStruct);

	UE_LOG(LogTcsState, Verbose, TEXT("状态事件派发：Tag=%s 句柄=%d/%d 单位=%lld 层数=%d 等级=%d"),
		*EventTag.ToString(), Instance.Handle.Index, Instance.Handle.Generation,
		Instance.Unit.Id, Instance.Stacks, Instance.Level);
}
