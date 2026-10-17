// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "State/TcsStateParamTableReader.h"



// 生命期
void UTcsStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 快照参数表反射壳：门面持有单例 ⇒ 物化路径零 per-apply 分配。
	// 初始**不绑定**任何快照（未绑定 = 一切查询落 miss）——绑定只在物化作用域内发生。
	ParamTableReader = NewObject<UTcsStateParamTableReader>(this, TEXT("TcsStateParamTableReader"));
}

bool UTcsStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界（Game/PIE/GamePreview）——参数表壳是运行时设施，与时钟/总线/属性/效果链门面同口径
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsStateSubsystem::Deinitialize()
{
	// 壳随本对象回收；此处只把绑定清掉，避免壳被回收前仍持着某个域的栈上快照指针。
	if (ParamTableReader)
	{
		ParamTableReader->Bind(nullptr);
	}

	Super::Deinitialize();
}



// 内核
UTcsStateParamTableReader* UTcsStateSubsystem::GetParamTableReader()
{
	return ParamTableReader;
}


ITcsStateLiveQuery* UTcsStateSubsystem::GetLiveQuery() const
{
	return LiveQuery;
}

void UTcsStateSubsystem::SetLiveQuery(ITcsStateLiveQuery* InQuery)
{
	LiveQuery = InQuery;
}
