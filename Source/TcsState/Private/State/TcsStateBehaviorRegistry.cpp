// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateBehaviorRegistry.h"

#include "Def/TcsBuffDef.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateBehaviorFragment.h"
#include "State/TcsStateBehaviorHandler.h"
#include "TcsStateLogChannel.h"



void FTcsStateBehaviorRegistry::SetHandler(UTcsStateBehaviorHandler* InHandler)
{
	Handler = InHandler;
}

void FTcsStateBehaviorRegistry::Reset(UTcsEventBusSubsystem* Bus)
{
	if (Bus && Bus->IsInitialized())
	{
		for (const TPair<FGameplayTag, FTcsEventSubscriptionHandle>& Pair : TagSubscriptions)
		{
			Bus->Unsubscribe(Pair.Value);
		}
	}

	TagSubscriptions.Reset();
	TagInstances.Reset();
	InstanceInterests.Reset();
	Handler = nullptr;
}

bool FTcsStateBehaviorRegistry::AddInstance(
	FTcsStateHandle Handle,
	const FTcsBuffDef& Def,
	UTcsEventBusSubsystem* Bus)
{
	if (InstanceInterests.Contains(Handle))
	{
		return true;
	}

	TArray<FGameplayTag> Interests;
	for (int32 FragmentIndex = 0; FragmentIndex < Def.Fragments.Num(); ++FragmentIndex)
	{
		const FTcsStateBehaviorFragment* Fragment = Def.Fragments[FragmentIndex].GetPtr<FTcsStateBehaviorFragment>();
		if (!Fragment)
		{
			// 只在接线时报告一次：空载荷与错误类型都降级为无行为，不逐事件刷 Warning。
			UE_LOG(LogTcsState, Warning,
				TEXT("状态行为片段未登记：Fragments[%d] %s（句柄=%d/%d）"),
				FragmentIndex, Def.Fragments[FragmentIndex].IsValid() ? TEXT("类型不符") : TEXT("载荷为空"),
				Handle.Index, Handle.Generation);
			continue;
		}

		for (const FGameplayTag& EventTag : Fragment->Interests)
		{
			if (!EventTag.IsValid())
			{
				UE_LOG(LogTcsState, Warning,
					TEXT("状态行为兴趣未登记：Fragments[%d] 含无效事件 Tag（句柄=%d/%d）"),
					FragmentIndex, Handle.Index, Handle.Generation);
				continue;
			}
			Interests.AddUnique(EventTag);
		}
	}

	if (Interests.IsEmpty())
	{
		return true;
	}

	if (!Handle.IsValid() || !Bus || !Bus->IsInitialized() || !Handler.IsValid())
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("状态行为订阅未登记：实例、总线或共享 Handler 不可得（句柄=%d/%d）"),
			Handle.Index, Handle.Generation);
		return false;
	}

	InstanceInterests.Add(Handle);
	for (const FGameplayTag& EventTag : Interests)
	{
		if (!TagSubscriptions.Contains(EventTag))
		{
			const FTcsEventSubscriptionHandle Subscription = Bus->Subscribe(
				EventTag, Handler.Get(), ETcsEventDispatch::EED_Immediate);
			if (!Subscription.IsValid())
			{
				// 回滚本次已登记的兴趣：共享订阅仍有其它实例时不会被退掉。
				RemoveInstance(Handle, Bus);
				return false;
			}
			TagSubscriptions.Add(EventTag, Subscription);
		}

		TagInstances.FindOrAdd(EventTag).Add(Handle);
		InstanceInterests.FindChecked(Handle).Add(EventTag);
	}

	return true;
}

bool FTcsStateBehaviorRegistry::RemoveInstance(
	FTcsStateHandle Handle,
	UTcsEventBusSubsystem* Bus)
{
	TArray<FGameplayTag> Interests;
	if (!InstanceInterests.RemoveAndCopyValue(Handle, Interests))
	{
		return false;
	}

	for (const FGameplayTag& EventTag : Interests)
	{
		TArray<FTcsStateHandle>* Instances = TagInstances.Find(EventTag);
		if (Instances)
		{
			Instances->RemoveSingle(Handle);
			if (!Instances->IsEmpty())
			{
				continue;
			}
		}

		TagInstances.Remove(EventTag);
		FTcsEventSubscriptionHandle Subscription;
		if (TagSubscriptions.RemoveAndCopyValue(EventTag, Subscription) && Bus && Bus->IsInitialized())
		{
			Bus->Unsubscribe(Subscription);
		}
	}

	return true;
}

void FTcsStateBehaviorRegistry::CollectInterests(
	FGameplayTag EventTag,
	TArray<FTcsStateHandle>& OutHandles) const
{
	if (const TArray<FTcsStateHandle>* Instances = TagInstances.Find(EventTag))
	{
		OutHandles.Append(*Instances);
	}
}

bool FTcsStateBehaviorRegistry::IsInterested(
	FTcsStateHandle Handle,
	FGameplayTag EventTag) const
{
	const TArray<FGameplayTag>* Interests = InstanceInterests.Find(Handle);
	return Interests && Interests->Contains(EventTag);
}

int32 FTcsStateBehaviorRegistry::GetSubscriptionCount() const
{
	return TagSubscriptions.Num();
}

int32 FTcsStateBehaviorRegistry::GetInstanceCount() const
{
	return InstanceInterests.Num();
}
