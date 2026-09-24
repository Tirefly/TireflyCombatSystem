// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsInstanceHandle.h"

#include "Chain/TcsEffectContext.h"

#include "TcsChainRun.generated.h"

class UTcsEffectSubsystem;



// 链运行态句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
struct FTcsChainRunTag
{
};



/**
 * 链运行态句柄（唤醒回入口锚点；句柄配对清理——复用 TTcsInstancePool 机制）。
 *
 * **反射性（2026-09-24 升格，提案 `add-scripting-reflection-surface`）**：本句柄是脚本层与宿主
 * 消费"起链结果"的唯一身份词——脚本层调 `ExecuteChain` 接住它、再传回 `IsRunActive`/`ResumeRun`
 * 查询与唤醒。
 *
 * **展平形态（2026-09-24 实测修正）**：字段**直接存 `Index`/`Generation`**，不内嵌
 * `TTcsInstanceHandle<T>`——原因是后者**是模板类型、无法作 `UPROPERTY`**，导致 C# 侧生成空壳
 * （`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒
 * **代际失配、句柄往返失效**（实测：`Allocate()` 给 `{Index=0, Generation=1}`，C# 传回
 * `{0, 0}`，`IsValid` 判 false）。展平后可反射 ⇒ 往返成立。
 * 先例 = `FTcsCombatEntityHandle`（同为展平的 `int64 Id`）。
 *
 * **`Index` 用 `int32` 而非 `uint32`**：UHT 不支持 `uint32` 作属性类型（`int64` 亦同源约束），
 * 故取 `int32`；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**，
 * 经 `GetInner`/`SetInner` 转换无损。
 */
USTRUCT()
struct TCSEFFECT_API FTcsChainRunHandle
{
	GENERATED_BODY()

	// 池内槽位索引（-1 = 无效）
	UPROPERTY()
	int32 Index = -1;

	// 代际计数（Free 时 +1，旧句柄凭失配判悬空）
	UPROPERTY()
	int32 Generation = 0;

	// 句柄有效性（代际校验由池在解析/释放时执行）
	bool IsValid() const
	{
		return Index != -1;
	}

	/**
	 * 转为池句柄（唯一转换点——避免各处自行拼装）。
	 *
	 * `static_cast` 保证位模式一致：`-1` → `0xFFFFFFFF`（池的 InvalidIndex）。
	 */
	TTcsInstanceHandle<FTcsChainRunTag> GetInner() const
	{
		TTcsInstanceHandle<FTcsChainRunTag> Inner;
		Inner.Index = static_cast<uint32>(Index);
		Inner.Generation = static_cast<uint32>(Generation);
		return Inner;
	}

	// 从池句柄赋值（唯一转换点，与 GetInner 对称）
	void SetInner(const TTcsInstanceHandle<FTcsChainRunTag>& Inner)
	{
		Index = static_cast<int32>(Inner.Index);
		Generation = static_cast<int32>(Inner.Generation);
	}
};



/**
 * 链运行态（D4-3/D4-17）：池化实例——控制流状态住数据、不活在调用栈里，故挂起可以跨帧。
 *
 * 持有纪律（引擎事实 2026-09-17：池元素住 TArray 连续缓冲，扩容即搬移）：
 * - 解释器 **MUST NOT 跨步骤执行缓存本结构指针**——每次入器前按句柄重解析；
 * - 步骤执行器 **MUST NOT 在自己持有本结构/上下文引用期间触发新的链执行**（池扩容会搬移运行态）——
 *   需要嵌套起链时，先取所需数据、结束本步后再起（RunSubChain 类步骤照此办理）。
 *
 * 身份纪律：本结构持 `ChainId` **而非链定义指针**——链定义登记表的增删/搬移都不使运行中的链悬空
 * （每步入器按 id 重解析；注销有活动运行态的链会被门面拒绝）。
 */
struct FTcsChainRun
{
// 身份与控制流
#pragma region Control

public:
	// 链 id（登记表键；每步入器按 id 重解析定义）
	FGameplayTag ChainId;

	// 下一待执行步序号（挂起时停驻原值——重入即"再来一次本步"）
	int32 PC = 0;

#pragma endregion


// 黑板与唤醒锚
#pragma region Runtime

public:
	// 链黑板（起链时装配；随运行态释放而清理）
	FTcsEffectContext Context;

	// 宿主门面（唤醒回入口的持有者；弱引用——运行态生命周期不长于子系统）
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;

	// 自身句柄（唤醒源按它重入 + 代际校验）
	FTcsChainRunHandle Self;

	// 挂起锚：等待中的到期条目句柄（定时类步骤用；M5 打断/取消复用）
	FTcsTimeEntryHandle PendingExpiry;

	// 挂起锚是否有效——步骤据此自辨"首入 / 被唤醒"（门面唤醒时不清除，由步骤配平）
	bool bHasPendingExpiry = false;

#pragma endregion
};
