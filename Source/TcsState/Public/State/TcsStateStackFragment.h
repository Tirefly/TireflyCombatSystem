// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateHandle.h"
#include "Runtime/TcsEntityAdmission.h"

#include "TcsStateStackFragment.generated.h"

struct FTcsBuffInstance;



/**
 * 本次施加的决策输入（**纯 C++ 视图，不做 USTRUCT**）：共存决策只需要"新来的这一次是谁、从哪来"，
 * 既有实例的一切（定义 / 实体 / 来源 / 层数 / 等级）从 `Existing` 读。
 *
 * 为什么不做反射：策略方法的形参不参与资产序列化，也不是脚本层要构造的东西——决策输入永远由
 * 引擎流程装配（同款取舍先例 = 物化上下文）。
 */
struct FTcsStateStackRequest
{
	// 定义身份（组键基座的一半）
	FGameplayTag DefTag;

	// 被施加方实体（组键基座的另一半）
	FTcsCombatEntityHandle Entity;

	// 本次施加的来源句柄（**无效 = 调用方未声明来源**，共存决策按"同来源"处理——见下方决策种类）
	FTcsSourceHandle Source;

	// 本次施加的发起者实体（调用方已在门面处归一：无效即取被施加方）
	FTcsCombatEntityHandle Instigator;
};



// 共存决策的种类（内建路径的全部可能结果；`Custom` 位只在"同组判定 / 接受 / 层数"三处改写结果）
enum class ETcsStateStackDecisionKind : uint8
{
	NewInstance = 0,	// 组内无实例 ⇒ 建新条（层数 1）
	Refresh = 1,		// 同来源 ⇒ 在原实例上刷新（层数不变）
	Stack = 2,			// 异来源且未满仓 ⇒ 原实例层数 +1
	Replace = 3,		// 满仓 + 替换政策 ⇒ 移除原实例、建新条
	Reject = 4,			// 满仓 + 拒绝政策（或 Custom 不接受）⇒ 实例不动、无广播
};



/**
 * 共存决策结果（引擎流程消费；不反射）。
 *
 * `NewStacks` 是**决策算出的目标层数**（新建 = 1；刷新 = 原值；叠层 = 原值 + 1 或 Custom 给的值），
 * 由调用方写入实例——**决策只算不写**（纯函数式，同物化器的纪律：算与写分开，读一处即全貌）。
 */
struct FTcsStateStackDecision
{
	ETcsStateStackDecisionKind Kind = ETcsStateStackDecisionKind::NewInstance;
	FTcsStateHandle ExistingHandle;
	int32 NewStacks = 1;
};



/**
 * 堆叠共存决策 Fragment（D3-7 v3 的"决策 Fragment"深度）：**只在本策略选中 `EGB_Custom` 时参与**。
 *
 * **★ 自 2026-10-14 起它是共享层统一决策接口的「状态侧薄型」**：`FTcsEntityAdmission`
 * 提供三方法（`IsSameGroup` / `ShouldAccept` / `ResolveStacks`）与中性默认实现，本类型
 * **继承它、不再自行声明那三个方法**（⇒ 决策空间的契约只有一份；技能侧同款由另一组薄型承载）。
 * **保留本类型名的理由**：① 它是 Def 资产 `CustomDecision` 的**作者面**类型（宿主子类化它）；
 * ② 状态域的默认语义与共享层默认**逐字相同**（同实体 + 同 `DefTag`；恒接受；同来源不变、异来源 +1），
 * 故继承默认实现即可保持行为不变。
 *
 * **载体纪律**：策略实例归 **Def 资产**持有（`FStateStackPolicy::CustomDecision`），
 * `FTcsBuffInstance` MUST NOT 持有任何策略载体（值语义记录是池化与操作复制的地基）。
 *
 * **抽象手法**（全仓统一，2026-09-16 实证）：USTRUCT 反射基类 + **中性默认实现** + `meta = (Hidden)`——
 * MUST NOT 用 `= 0`（UHT 为每个 USTRUCT 生成 `TCppStructOps<T>`，构造路径需要可默认构造）与
 * `PURE_VIRTUAL`（Development 编不过、Shipping 致命错误体，最坏的一类失败）。
 *
 * **框架零具体策略**：分组词与业务判据是宿主词汇，本类只给中性默认 + 逃逸位；
 * 具体策略（如"按元素分组""按技能族共享层数"）由宿主实现并挂进 Def 资产。
 */
USTRUCT(meta = (Hidden))
struct TCSSTATE_API FTcsStateStackDecisionFragment : public FTcsEntityAdmission
{
	GENERATED_BODY()

	// 三个决策方法**继承自 `FTcsEntityAdmission`**（中性默认：同实体 + 同 `DefTag` / 恒接受 /
	// 同来源不变·异来源 +1）。宿主子类化本类型即可覆盖它们——**MUST NOT** 在本类型里再声明一份。
};
