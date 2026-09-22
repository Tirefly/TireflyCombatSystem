// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsInstanceHandle.h"

#include "Chain/TcsEffectContext.h"

class UTcsEffectSubsystem;



// 链运行态句柄标签（仅供句柄模板做类型区分——与其他池句柄编译期防互串）
struct FTcsChainRunTag
{
};



// 链运行态句柄（唤醒回入口锚点；句柄配对清理——复用 TTcsInstancePool 机制）
struct FTcsChainRunHandle
{
	// 池内句柄
	TTcsInstanceHandle<FTcsChainRunTag> Inner;

	// 句柄有效性（代际校验由池在解析/释放时执行）
	bool IsValid() const
	{
		return Inner.IsValid();
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
