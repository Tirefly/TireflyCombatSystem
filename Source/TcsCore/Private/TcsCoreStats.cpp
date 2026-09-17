// Copyright Tirefly. All Rights Reserved.

#include "TcsCoreStats.h"

#include "HAL/IConsoleManager.h"
#include "Pool/TcsInstancePool.h"

// 跨池已分配槽位总数（仅游戏线程读写——池进入点已断言 D0-4）
static int32 GTcsCorePoolSlotsTotal = 0;

FAutoConsoleVariableRef GTcsCorePoolSlotsCVar(
	TEXT("Tcs.Core.PoolSlots"),
	GTcsCorePoolSlotsTotal,
	TEXT("TCS 全部实例池当前已分配槽位总数（池 Allocate/Free 维护，只读参考值）"),
	ECVF_Default);

void FTcsCorePoolStats::AddSlotCount()
{
	++GTcsCorePoolSlotsTotal;
}

void FTcsCorePoolStats::RemoveSlotCount()
{
	--GTcsCorePoolSlotsTotal;
}

int32 FTcsCorePoolStats::GetSlotCount()
{
	return GTcsCorePoolSlotsTotal;
}

// 到期堆有效条目总数（仅游戏线程读写——堆进入点已断言 D0-4）
static int32 GTcsCoreHeapDepthTotal = 0;

FAutoConsoleVariableRef GTcsCoreHeapDepthCVar(
	TEXT("Tcs.Core.ExpiryHeapDepth"),
	GTcsCoreHeapDepthTotal,
	TEXT("TCS 到期堆当前有效条目总数（入堆/移出维护，只读参考值）"),
	ECVF_Default);

void FTcsCoreHeapStats::AddDepth()
{
	++GTcsCoreHeapDepthTotal;
}

void FTcsCoreHeapStats::RemoveDepth()
{
	--GTcsCoreHeapDepthTotal;
}

int32 FTcsCoreHeapStats::GetDepth()
{
	return GTcsCoreHeapDepthTotal;
}

// 编译期实例化锚点：Task 1 暂无运行时消费者，显式实例化确保池模板全量编译检查
// （Allocate/Free/Resolve/IsValid/ForEach 全部过编译；Task 2 起由真实消费者取代本锚点）
namespace TcsCorePoolAnchor
{
	struct FAnchorTag {};

	struct FAnchorInstance
	{
		int32 Dummy = 0;
	};

	template class TTcsInstancePool<FAnchorInstance, FAnchorTag>;
}
