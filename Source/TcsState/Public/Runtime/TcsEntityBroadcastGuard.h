// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



/**
 * **统一重入广播守卫**（2026-10-14 立）：把"某句柄正在广播窗口内"这件事做成
 * **一份实现**，取代状态侧 `InRemovalBroadcastHandles` 与技能侧 `InFinishBroadcastHandles`
 * 两份同形成员。
 *
 * **它守的是什么**：事件广播是**当场同步派发**（`PublishImmediate`）⇒ 订阅者可在回调内
 * **重入终结同一句柄**。若不拦，后果是**重复广播 + 重复归还槽位**（后者更坏：槽位可能已被
 * 复用给别的实例，重复归还会把别人的槽释放掉）。
 *
 * **本仓既有语义（MUST 逐条保住）**：
 * - 重入时必须**返回 false 且静默**（MUST NOT ensure、MUST NOT 报 Warning）——
 *   它是合法的订阅者行为，不是契约违规；
 * - 只拦**同一句柄**（别的句柄在广播窗口内照常可操作）；
 * - 撤锚顺序 = **Add → 广播 → Remove**（`Remove` 必须在广播返回后执行，否则等于没守）。
 *
 * **为什么是模板而不是"统一句柄"**：三个句柄类型**刻意保持独立**（编译期护栏，见
 * `TcsInstanceHandleOps`）⇒ 守卫也按句柄类型模板化：两域各持**自己句柄类型**的实例，
 * 而**实现只有这一份**。
 *
 * @tparam THandleType 该域的展平反射句柄类型（`TSet` 键控要求 `GetTypeHash` + `operator==`——
 *         三个句柄都提供）。
 */
template <typename THandleType>
class TTcsEntityBroadcastGuard
{
// 窗口
#pragma region Window

public:
	/**
	 * 该句柄当前是否处于广播窗口内（调用方据此**静默返回 false**）。
	 *
	 * @param Handle 待判句柄。
	 * @return 返回是否正在广播。
	 */
	bool IsBroadcasting(const THandleType& Handle) const
	{
		return InFlight.Contains(Handle);
	}

	/**
	 * 进入广播窗口（**MUST 在广播调用之前**执行）。
	 *
	 * @param Handle 即将广播其事件的句柄。
	 */
	void Enter(const THandleType& Handle)
	{
		InFlight.Add(Handle);
	}

	/**
	 * 退出广播窗口（**MUST 在广播调用返回之后**执行；异常路径下也必须到达——
	 * 漏掉一次会让该句柄**永久**被判为重入，症状 = 该实例再也移除不掉）。
	 *
	 * @param Handle 已广播完事件的句柄。
	 */
	void Exit(const THandleType& Handle)
	{
		InFlight.Remove(Handle);
	}

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 清空窗口集合（门面 `Deinitialize` 的确定性清空口）。
	 *
	 * **为什么 MUST 清**：该集合只在"某次终结还没走完"的窗口内有意义；跨世界残留会让新世界里
	 * 同 `(Index, Generation)` 的实例**再也终结不掉**，症状是"放不出去也结束不了"，离根因极远。
	 */
	void Reset()
	{
		InFlight.Reset();
	}

	// 当前在窗口内的句柄数（观测与跨世界残留复核用）
	int32 Num() const
	{
		return InFlight.Num();
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 正在广播其事件的句柄集
	TSet<THandleType> InFlight;

#pragma endregion
};
