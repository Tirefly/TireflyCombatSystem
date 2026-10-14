// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffBehaviorFragment.h"



void FTcsBuffBehaviorFragment::OnStateEvent(
	const FGameplayTag& /*EventTag*/,
	const FInstancedStruct& /*Payload*/,
	const FTcsBuffBehaviorContext& /*Ctx*/) const
{
	// 中性默认：无行为。具体策略由宿主提供，不在基类下玩法结论。
}
