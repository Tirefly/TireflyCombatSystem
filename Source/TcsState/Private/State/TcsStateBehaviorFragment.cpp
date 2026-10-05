// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateBehaviorFragment.h"



void FTcsStateBehaviorFragment::OnStateEvent(
	const FGameplayTag& /*EventTag*/,
	const FInstancedStruct& /*Payload*/,
	const FTcsStateBehaviorContext& /*Ctx*/) const
{
	// 中性默认：无行为。具体策略由宿主提供，不在基类下玩法结论。
}
