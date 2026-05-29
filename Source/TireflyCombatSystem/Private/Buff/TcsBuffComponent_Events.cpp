// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "State/TcsStateComponent.h"

void UTcsBuffComponent::BroadcastBuffRuntimeDeltaBatchEvent(
	const TArray<FTcsBuffRuntimeDeltaEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnBuffRuntimeDelta.IsBound())
	{
		OnBuffRuntimeDelta.Broadcast(Payloads);
	}
}

void UTcsBuffComponent::BroadcastBuffRemovedBatchEvent(
	const TArray<FTcsBuffRemovedEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnBuffRemoved.IsBound())
	{
		OnBuffRemoved.Broadcast(Payloads);
	}
}

void UTcsBuffComponent::BeginPublicEventBatch()
{
	// 嵌套链路只累计深度，真正的提交动作留给最外层批次统一收口。
	++PublicEventBatchDepth;
}

void UTcsBuffComponent::EndPublicEventBatch()
{
	// 防守式收尾：如果外部成对调用失衡，直接清空挂起结果，避免脏事件跨帧残留。
	if (PublicEventBatchDepth <= 0)
	{
		PendingBuffRuntimeDeltaEvents.Reset();
		PendingBuffRemovedEvents.Reset();
		PublicEventBatchDepth = 0;
		return;
	}

	--PublicEventBatchDepth;
	if (PublicEventBatchDepth == 0)
	{
		// 销毁路径不再对外广播，避免在宿主回收阶段触发不稳定的重入访问。
		if (IsBeingDestroyed())
		{
			PendingBuffRuntimeDeltaEvents.Reset();
			PendingBuffRemovedEvents.Reset();
			return;
		}

		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::FlushPendingPublicEvents()
{
	if (PendingBuffRuntimeDeltaEvents.IsEmpty() && PendingBuffRemovedEvents.IsEmpty())
	{
		return;
	}

	TArray<FTcsBuffRuntimeDeltaEventPayload> RuntimePayloads = MoveTemp(PendingBuffRuntimeDeltaEvents);
	TArray<FTcsBuffRemovedEventPayload> RemovedPayloads = MoveTemp(PendingBuffRemovedEvents);
	PendingBuffRuntimeDeltaEvents.Reset();
	PendingBuffRemovedEvents.Reset();

	// 过滤掉已经失效的 Buff，或者虽然占了一个槽位但最终没有留下任何可见变化的空载荷。
	RuntimePayloads.RemoveAll([](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return !IsValid(Payload.BuffInstance.Get())
			|| (!Payload.bStackCountChanged
				&& !Payload.bMaxStackCountChanged
				&& !Payload.bPeriodChanged
				&& !Payload.bDurationRefreshed);
	});

	RemovedPayloads.RemoveAll([](const FTcsBuffRemovedEventPayload& Payload)
	{
		return !IsValid(Payload.BuffInstance.Get());
	});

	// 保持先 RuntimeDelta、后 Removed 的公共顺序，便于外部观察同批最终态。
	BroadcastBuffRuntimeDeltaBatchEvent(RuntimePayloads);
	BroadcastBuffRemovedBatchEvent(RemovedPayloads);
}

FTcsBuffRuntimeDeltaEventPayload* UTcsBuffComponent::FindPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return nullptr;
	}

	return PendingBuffRuntimeDeltaEvents.FindByPredicate([BuffInstance](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

FTcsBuffRemovedEventPayload* UTcsBuffComponent::FindPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return nullptr;
	}

	return PendingBuffRemovedEvents.FindByPredicate([BuffInstance](const FTcsBuffRemovedEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

bool UTcsBuffComponent::HasPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance) const
{
	if (!IsValid(BuffInstance))
	{
		return false;
	}

	return PendingBuffRemovedEvents.ContainsByPredicate([BuffInstance](const FTcsBuffRemovedEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

void UTcsBuffComponent::DiscardPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	PendingBuffRuntimeDeltaEvents.RemoveAll([BuffInstance](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

void UTcsBuffComponent::QueueBuffStackChangedEvent(
	UTcsBuffInstance* BuffInstance,
	int32 OldStackCount,
	int32 NewStackCount)
{
	// 一旦同批次已经确定移除，这个 Buff 的运行时变化对外就不再有意义。
	if (!IsValid(BuffInstance) || OldStackCount == NewStackCount || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bStackCountChanged)
	{
		// 只保留本批次第一次看到的旧值，后续更新继续覆盖 New 值即可形成 Old -> Final。
		PendingPayload->bStackCountChanged = true;
		PendingPayload->OldStackCount = OldStackCount;
	}

	PendingPayload->NewStackCount = NewStackCount;
}

void UTcsBuffComponent::QueueBuffMaxStackCountChangedEvent(
	UTcsBuffInstance* BuffInstance,
	int32 OldMaxStackCount,
	int32 NewMaxStackCount)
{
	// 同批移除优先级高于运行时变化，避免对外暴露已失效的中间态。
	if (!IsValid(BuffInstance) || OldMaxStackCount == NewMaxStackCount || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bMaxStackCountChanged)
	{
		// 最大叠层同样只记录第一次旧值，最后一次新值由后续写入覆盖。
		PendingPayload->bMaxStackCountChanged = true;
		PendingPayload->OldMaxStackCount = OldMaxStackCount;
	}

	PendingPayload->NewMaxStackCount = NewMaxStackCount;
}

void UTcsBuffComponent::QueueBuffPeriodChangedEvent(
	UTcsBuffInstance* BuffInstance,
	float OldPeriod,
	float NewPeriod)
{
	// Removed 已经挂起时，不再为该 Buff 累积周期变化结果。
	if (!IsValid(BuffInstance) || OldPeriod == NewPeriod || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bPeriodChanged)
	{
		// 周期变化也按 Old -> Final 收敛，外部无需感知中间多次改写。
		PendingPayload->bPeriodChanged = true;
		PendingPayload->OldPeriod = OldPeriod;
	}

	PendingPayload->NewPeriod = NewPeriod;
}

void UTcsBuffComponent::QueueBuffDurationRefreshedEvent(
	UTcsBuffInstance* BuffInstance,
	float NewDuration)
{
	// 持续时间只关心本批次对外可见的最终剩余值，因此每次直接覆盖即可。
	if (!IsValid(BuffInstance) || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	PendingPayload->bDurationRefreshed = true;
	PendingPayload->NewDuration = NewDuration;
}

void UTcsBuffComponent::QueueBuffRemovedEvent(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	// Removed 是该 Buff 在本批次里的终态结果，之前累计的 RuntimeDelta 全部失效。
	DiscardPendingBuffRuntimeDeltaEvent(BuffInstance);

	FTcsBuffRemovedEventPayload* PendingPayload = FindPendingBuffRemovedEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRemovedEvents.Add_GetRef(FTcsBuffRemovedEventPayload(BuffInstance, RemovalReason));
		return;
	}

	if (PendingPayload->RemovalReason.IsNone() || !RemovalReason.IsNone())
	{
		// 后续补来的有效 RemovalReason 可以覆盖之前的空原因，保留更完整的终态语义。
		PendingPayload->RemovalReason = RemovalReason;
	}
}

void UTcsBuffComponent::NotifyBuffStackChanged(UTcsBuffInstance* BuffInstance, int32 OldStackCount, int32 NewStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldStackCount == NewStackCount)
	{
		return;
	}

	QueueBuffStackChangedEvent(BuffInstance, OldStackCount, NewStackCount);
	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);

	// 没有显式批次时，沿用“本次调用立即可见”的旧行为，直接提交当前结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffMaxStackCountChanged(UTcsBuffInstance* BuffInstance, int32 OldMaxStackCount, int32 NewMaxStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldMaxStackCount == NewMaxStackCount)
	{
		return;
	}

	QueueBuffMaxStackCountChangedEvent(BuffInstance, OldMaxStackCount, NewMaxStackCount);
	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);

	// 外层没有批处理包裹时，立刻把这一次变化对外提交出去。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffPeriodChanged(UTcsBuffInstance* BuffInstance, float OldPeriod, float NewPeriod)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldPeriod == NewPeriod)
	{
		return;
	}

	QueueBuffPeriodChangedEvent(BuffInstance, OldPeriod, NewPeriod);
	// 周期变化本身不影响 merge group，但仍要在非批处理路径里立即提交公共结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffDurationRefreshed(UTcsBuffInstance* BuffInstance, float NewDuration)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	QueueBuffDurationRefreshedEvent(BuffInstance, NewDuration);
	// 持续时间刷新同样允许在无批次时直接向外暴露最终结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffRemoved(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	QueueBuffRemovedEvent(BuffInstance, RemovalReason);
	// Removed 没有外层批次时也要立即提交，保持移除回调的现有时效性。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}
