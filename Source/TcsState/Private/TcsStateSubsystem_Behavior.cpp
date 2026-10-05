// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "State/TcsStateBehaviorFragment.h"
#include "State/TcsStateEvents.h"
#include "UObject/GCObject.h"



namespace
{
	/** 回调期间强持片段副本与事件载荷内的对象引用，Def 注销 + 手动 GC 也不会破坏当前调用。 */
	class FTcsStateBehaviorCallbackScope final : public FGCObject
	{
	public:
		FTcsStateBehaviorCallbackScope(
			UTcsStateSubsystem* InOwner,
			FInstancedStruct& InFragment,
			FInstancedStruct& InPayload)
			: Owner(InOwner), Fragment(InFragment), Payload(InPayload)
		{
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Owner);
			Fragment.AddStructReferencedObjects(Collector);
			Payload.AddStructReferencedObjects(Collector);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FTcsStateBehaviorCallbackScope");
		}

	private:
		TObjectPtr<UTcsStateSubsystem> Owner;
		FInstancedStruct& Fragment;
		FInstancedStruct& Payload;
	};
}



int32 UTcsStateSubsystem::GetBehaviorSubscriptionCount() const
{
	return BehaviorRegistry.GetSubscriptionCount();
}

int32 UTcsStateSubsystem::GetBehaviorInstanceCount() const
{
	return BehaviorRegistry.GetInstanceCount();
}

void UTcsStateSubsystem::DispatchBehaviorEvent(
	FGameplayTag EventTag,
	const FInstancedStruct& Payload)
{
	TArray<FTcsStateHandle> Handles;
	BehaviorRegistry.CollectInterests(EventTag, Handles);
	if (Handles.IsEmpty())
	{
		return;
	}

	// 固定本次事件的值快照；状态载荷按实例身份筛选，其它载荷仍向全部兴趣实例派发。
	FInstancedStruct EventSnapshot = Payload;
	const FTcsStateEventPayload* StatePayload = EventSnapshot.GetPtr<FTcsStateEventPayload>();
	const bool bStateEvent = StatePayload != nullptr;
	const FTcsStateHandle EventHandle = StatePayload ? StatePayload->Handle : FTcsStateHandle();

	for (const FTcsStateHandle& Handle : Handles)
	{
		if (bStateEvent && !(EventHandle == Handle))
		{
			continue;
		}

		// 固定本次遍历的上限，回调中新添加的片段不加入当前事件；实际内容每次重新解析。
		int32 FragmentCount = 0;
		{
			const FTcsStateInstance* Instance = GetState(Handle);
			const FTcsBuffDef* Def = Instance ? GetRegisteredStateDef(Instance->DefTag) : nullptr;
			FragmentCount = Def ? Def->Fragments.Num() : 0;
		}

		for (int32 FragmentIndex = 0; FragmentIndex < FragmentCount; ++FragmentIndex)
		{
			// 前一回调可以移除本实例、其它实例或全清登记。逐回调检查，不把快照当活性保证。
			if (!BehaviorRegistry.IsInterested(Handle, EventTag))
			{
				break;
			}

			FTcsStateBehaviorContext Ctx;
			FInstancedStruct CallbackFragment;
			{
				const FTcsStateInstance* Instance = GetState(Handle);
				const FTcsBuffDef* Def = Instance ? GetRegisteredStateDef(Instance->DefTag) : nullptr;
				if (!Instance || !Def || !Def->Fragments.IsValidIndex(FragmentIndex))
				{
					break;
				}

				const FTcsStateBehaviorFragment* Fragment = Def->Fragments[FragmentIndex].GetPtr<FTcsStateBehaviorFragment>();
				if (!Fragment || !Fragment->Interests.Contains(EventTag))
				{
					// 无效载荷已在接线时报过一次 Warning；这里不随每次事件重复报告。
					continue;
				}

				Ctx.Subsystem = this;
				Ctx.Handle = Handle;
				Ctx.Unit = Instance->Unit;
				Ctx.DefTag = Instance->DefTag;
				Ctx.Stacks = Instance->Stacks;
				Ctx.Level = Instance->Level;
				CallbackFragment = Def->Fragments[FragmentIndex];
			}

			// 回调使用副本：注销 Def 不会释放正在执行的片段内存或 vtable。
			FTcsStateBehaviorCallbackScope CallbackScope(this, CallbackFragment, EventSnapshot);
			const FTcsStateBehaviorFragment* Fragment = CallbackFragment.GetPtr<FTcsStateBehaviorFragment>();
			if (Fragment)
			{
				Fragment->OnStateEvent(EventTag, EventSnapshot, Ctx);
			}
		}
	}
}
