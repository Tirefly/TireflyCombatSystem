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
 *
 * **`BlueprintType`（2026-09-24 放宽，台账 SCRIPT-8 连带）**：本句柄要出现在**宿主脚本插槽**的签名里
 * （`UTcsStepExecutor::Execute` 的形参），而 UHT 对 `BlueprintNativeEvent` 强制全部形参蓝图可表达
 * （`UhtFunction.cs:859`/`:1043-1053`）——非 `BlueprintType` 则插槽编译不过。
 * 原"MUST NOT 加"的顾虑是"成为 `BlueprintCallable` 的合法形参 ⇒ 意外扩大蓝图承诺面"，**代价为零**：
 * 消费本句柄的门面方法仍为 `UFUNCTION()` 无 specifier（蓝图不可见），蓝图能"看见类型"却**无门面可调**；
 * 插槽接口确实蓝图可实现，但那是 R0 §9 已接受的"恰好蓝图也能用"，不是新增承诺。
 * 先例 = `FTcsCombatEntityHandle`（同为展平字段的反射 + `BlueprintType` 句柄）。
 */
USTRUCT(BlueprintType)
struct TCSEFFECT_API FTcsChainRunHandle
{
	GENERATED_BODY()

	// 池内槽位索引（-1 = 无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Effect|Run")
	int32 Index = -1;

	// 代际计数（Free 时 +1，旧句柄凭失配判悬空）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Effect|Run")
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

	// 相等比较（**身份 = Index + Generation**——等待者集合的配对摘除用；先例 = `FTcsEventSubscriptionHandle`）
	friend bool operator==(const FTcsChainRunHandle& A, const FTcsChainRunHandle& B)
	{
		return A.Index == B.Index && A.Generation == B.Generation;
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
 * 嵌套纪律（2026-10-03，提案 `add-chain-primitives-and-target-sorting` D-2——两条由门面实施）：
 * - **共享预算**：`MaxStepsPerFrame` 是"一次**最外层**进入一份"，嵌套起链的子链步数**计入父链**；
 *   子链 MUST NOT 各自获得新预算（否则 `A→B→A→B` 自激既绕过预算、又直接吃调用栈——不是性能问题）；
 * - **深度上限**：另有一条**固定常量**的嵌套深度护栏（`TcsEffectSubsystem.cpp`）。它是**调用栈护栏**
 *   而非行为语义，且预算值由链定义给出、作者可配到很大 ⇒ 它 MUST NOT 做成链定义字段。
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


// 黑板
#pragma region Blackboard

public:
	// 链黑板（起链时装配；随运行态释放而清理）
	FTcsEffectContext Context;

	// 宿主门面（唤醒回入口的持有者；弱引用——运行态生命周期不长于子系统）
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;

	// 自身句柄（唤醒源按它重入 + 代际校验）
	FTcsChainRunHandle Self;

#pragma endregion


// 挂起锚（三路唤醒源 + 等待方登记）
#pragma region Suspend

public:
	/**
	 * **配平纪律**（三个锚共用）：锚由**步骤**在首入时装、在**被唤醒**时配平清除；门面唤醒时
	 * **不清除任何锚**——锚的在场与否就是步骤自辨"首入 / 被唤醒"的唯一判据（先例 = `WaitDelay`）。
	 * 唯一例外是 `ReleaseRun`：运行态释放时无条件全清（解锚清单见门面 `.cpp`）。
	 */

	// 定时锚：等待中的到期条目句柄（定时类步骤用；M5 打断/取消复用）
	FTcsTimeEntryHandle PendingExpiry;

	// 定时锚是否有效（与 `PendingExpiry` 成对——门面解锚清单的一项）
	bool bHasPendingExpiry = false;

	// 事件锚：正在等的事件 Tag（**无效 tag = 未在等事件**）。等待在门面侧另有登记表
	// （共享订阅 + 计数配对 + 等待者集合，见 `UTcsEffectSubsystem` 的事件等待表）
	FGameplayTag PendingEventTag;

	/**
	 * 订阅侧命中标记：**门面投递载荷时置位、步骤配平清除**。
	 *
	 * **它是"命中唤醒"与"超时唤醒"的唯一判据**——两路各自只留自己的锚（上面那个 `bool` 与
	 * 本标记），而锚本身分不出是哪一路唤醒的（命中与超时都可能在锚在场时发生）。
	 */
	bool bPendingEventHit = false;

	/**
	 * 子链锚：在等的子链运行态（**有效 = 本运行态在等它结束**）。
	 *
	 * 唤醒时步骤据 `IsRunActive` **复核**再完成——`ResumeRun` 是脚本层可调的门面方法，
	 * 一次来路不明的唤醒 MUST NOT 骗过本锚：子链仍活动 ⇒ 本次是无效唤醒，继续挂起。
	 */
	FTcsChainRunHandle PendingChildRun;

	/**
	 * 等待方锚（**反向登记**：谁在等本运行态结束——子链完成唤醒用）。
	 *
	 * 挂在**子**运行态上而非父侧维护等待列表：`ReleaseRun` 是链结束的**唯一汇聚点**，通知天然
	 * 覆盖全部四条结束路径（走完 / 熔断 / 链定义不可解析 / 未知步骤类型），**含异常结束**——
	 * 不通知 = 父永久挂起 = 死链（D-3）。
	 *
	 * 单等待方即可：`ExecuteChain` 每次分配**新**运行态 ⇒ 一条运行态至多一个起链者；
	 * 将来出现"多父等待"（如 `Parallel` 原语）再改集合。
	 */
	FTcsChainRunHandle ParentRun;

#pragma endregion
};
