// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <atomic>

#include "TcsCombatEntityHandle.generated.h"



/**
 * 战斗实体身份句柄（PV-1 边界让步记录）：全插件通用实体键，也是 Core 唯一持有的战斗实体词汇
 * （上下文/门面住 Core 而 Core 不得反向依赖域模块，故实体身份由 Core 承载——02 §2.2a）。
 *
 * 与 TTcsInstanceHandle 的区别：本句柄**无代际段**——Id 单调递增且永不复用，悬空表现为"查不到"
 * 而非"误命中回收槽"；代际校验只属于池的槽位复用语义。
 * 与 FTcsSourceHandle 的区别（语义隔离，编译期不可互换）：Source = 归属来源/级联撤销锚点，
 * 本句柄 = 被修饰或被查询的主体。
 *
 * **反射性（2026-09-18 升格）**：本句柄是**身份词汇**（token 化，非裸 uint64），需出现在
 * 反射载荷里——属性变更事件 `FTcsAttributeChangedEvent.Unit`（走总线的 FInstancedStruct 载荷
 * MUST 反射可见），且 PV-1 规划的上下文 `Subject` 字段同样是 UPROPERTY。故本结构为
 * `USTRUCT(BlueprintType)` 并带模块导出宏（反射类型有 UHT 生成符号 → 按导出宏纪律必须带宏；
 * 纯内联的 `FTcsSourceHandle` 反而不能带——两者差异正是该纪律的两面）。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsCombatEntityHandle
{
	GENERATED_BODY()

	// 实体 Id（0 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Combat Entity")
	int64 Id = 0;

	// 句柄有效性
	bool IsValid() const
	{
		return Id != 0;
	}

	// 相等比较（容器键控/配对清理用）
	friend bool operator==(const FTcsCombatEntityHandle& A, const FTcsCombatEntityHandle& B)
	{
		return A.Id == B.Id;
	}

	// 哈希（TMap 键控用）
	friend uint32 GetTypeHash(const FTcsCombatEntityHandle& Handle)
	{
		return GetTypeHash(Handle.Id);
	}
};



/**
 * 实体身份发号器：进程内原子递增发号（首个分配为 1），永不复用。
 * 发号方归属：R3 由 M2 门面（UTcsAttributeSubsystem::RegisterUnit）发号，M6 世界注册表落地后
 * 移交发号（句柄类型与消费者签名不变——句柄类型先行是为了避免一次类型迁移摊到全部下游）。
 * 与 FTcsSourceHandleRegistry 同形式：只发号，不做注销登记。
 */
struct FTcsCombatEntityHandleRegistry
{
	// 分配新实体 Id（原子递增，进程内唯一）
	FTcsCombatEntityHandle Allocate()
	{
		FTcsCombatEntityHandle Handle;
		Handle.Id = NextId.fetch_add(1, std::memory_order_relaxed) + 1;
		return Handle;
	}

private:
	// 下一实体 Id（原子计数；0 保留无效）
	std::atomic<int64> NextId{0};
};
