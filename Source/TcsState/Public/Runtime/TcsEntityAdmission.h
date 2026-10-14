// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "TcsEntityAdmission.generated.h"



/**
 * 本次"重申请"的决策输入（纯 C++ 视图，不做 USTRUCT）：决策只需要"新来的这一次是谁、从哪来"。
 *
 * **为什么不做反射**：策略方法的形参不参与资产序列化，也不是脚本层要构造的东西——
 * 决策输入永远由引擎流程装配（同款取舍先例 = 物化上下文）。
 */
struct FTcsEntityAdmissionRequest
{
	// 定义身份（组键基座的一半）
	FGameplayTag DefTag;

	// 被申请方实体（组键基座的另一半）
	FTcsCombatEntityHandle Entity;

	// 本次申请的来源句柄（**无效 = 调用方未声明来源**，决策按"同来源"处理）
	FTcsSourceHandle Source;

	// 本次申请的发起者实体（调用方已在门面处归一：无效即取被申请方）
	FTcsCombatEntityHandle Instigator;
};



/**
 * 既有实例的**通用视图**（由调用方从自己域的运行态记录填充）。
 *
 * **为什么不直接传域记录（如 `const FTcsStateInstance&`）**：共享层 MUST NOT 认识两域的运行态
 * 类型（那是域语义，且两类型分属不同模块）。若用 `const void*` 传，则**默认实现无法成立**
 * ——本接口的默认 `IsSameGroup` 语义是"同实体 + 同 `DefTag`"，它必须**读得到**这两个字段。
 * ⇒ 取"通用视图"：共享层定义它认得的**最小公共字段集**，域侧负责填。
 *
 * **字段取舍判据 = "该字段是否两域共有且语义相同"**（MUST 逐条有据，MUST NOT 图省事多塞）：
 * 两域的运行态记录都持 `DefTag` 身份 / `Entity` 归属 / `Source` 来源（本仓实测：状态
 * `FTcsStateInstance` 三样俱全；技能 `FTcsCastRun` 有 `Entity` / `RunSource`，
 * 其 `DefTag` 由调用方从所属账本条目取——**不是新造信息，是调用方手上已有的**）。
 */
struct FTcsEntityAdmissionExisting
{
	// 既有实例的定义身份
	FGameplayTag DefTag;

	// 既有实例的归属实体
	FTcsCombatEntityHandle Entity;

	// 既有实例的来源句柄（"续杯 vs 叠层"的判据）
	FTcsSourceHandle Source;

	// 既有实例的发起者实体（`EGB_PerInstigator` 一类的分组政策要读它）
	FTcsCombatEntityHandle Instigator;

	// 既有实例的当前层数（两域共有的字段面；技能侧恒 1——它无层数语义）
	int32 Stacks = 1;
};



// 重申请决策的档位（内建路径的全部可能结果；策略只在"同组判定 / 接受 / 层数"三处改写结果）
enum class ETcsEntityAdmissionKind : uint8
{
	NewInstance = 0,	// 组内无实例 ⇒ 建新条（层数 1）
	Refresh = 1,		// 同来源 ⇒ 在原实例上刷新（层数不变）
	Stack = 2,			// 异来源且未满仓 ⇒ 原实例层数增加
	Replace = 3,		// 满仓 + 替换政策 ⇒ 移除原实例、建新条
	Reject = 4,			// 满仓 + 拒绝政策（或策略不接受）⇒ 实例不动、无广播
};



/**
 * 重申请决策结果（引擎流程消费；**决策只算不写**——返回目标层数与档位，由调用方写入实例，
 * 同物化器纪律"算与写分开，读一处即全貌"）。
 *
 * **为什么不再带 `ExistingHandle`**：调用方**本来就是自己找到既有实例的**（它才填得出
 * `FTcsEntityAdmissionExisting`）⇒ 句柄在调用方手上，塞进结果里是第二份真相。
 */
struct FTcsEntityAdmissionDecision
{
	ETcsEntityAdmissionKind Kind = ETcsEntityAdmissionKind::NewInstance;

	// 决策算出的**目标层数**（新建 = 1；刷新 = 原值；叠层 = 原值 + 1 或策略给的值）
	int32 NewStacks = 1;
};



/**
 * **统一重申请决策接口**（2026-10-14 立）：表达"同组则刷新 / 叠层，异组则新建 / 拒绝 / 替换"
 * 这一**决策空间**，供两域各挂自己的默认策略。
 *
 * **它取代了两份同型实现**：状态侧的五轴堆叠决策树（`FTcsStateStackDecisionFragment`）
 * 与技能侧**内联在激活算子里**的 `ECastInstancing` 四路分流（`TcsCastOps.cpp`）。
 * 两者是**同型**的决策空间——各写一份即漂移源。
 *
 * **抽象手法**（全仓统一，2026-09-16 实证）：`USTRUCT(meta = (Hidden))` + **中性默认实现**——
 * MUST NOT 用 `= 0`（UHT 为每个 USTRUCT 生成 `TCppStructOps<T>`，构造路径需可默认构造）
 * 与 `PURE_VIRTUAL`（`CHECK_PUREVIRTUALS` 开启时展开为 `= 0`、关闭时才是致命错误体
 * ⇒ Development 编不过、Shipping 能过，最坏的一类失败）。
 *
 * **载体纪律**：策略实例 MUST 归 **Def 资产**持有；运行态实例 MUST NOT 持有任何策略载体
 * （值语义记录是池化与操作复制的地基）。
 *
 * **框架零具体策略**：分组词与业务判据是宿主词汇，本接口只给中性默认 + 逃逸位；
 * 具体策略（如"按元素分组""按技能族共享层数"）由宿主实现并挂进 Def 资产。
 */
USTRUCT(meta = (Hidden))
struct TCSSTATE_API FTcsEntityAdmission
{
	GENERATED_BODY()

	virtual ~FTcsEntityAdmission() = default;

// 决策
#pragma region Decision

public:
	/**
	 * 是否与既有实例同组（内置分组档为 `Custom` 时接管分组判定）。
	 *
	 * 默认实现 = 内置"不分组"语义（**同实体 + 同 `DefTag`**）。
	 *
	 * @param Request 本次申请的决策输入。
	 * @param Existing 桶内既有实例的通用视图。
	 * @return 返回是否同组。
	 */
	virtual bool IsSameGroup(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const;

	/**
	 * 是否接受这次申请（拒绝 ⇒ 实例不动、**任何事件都不广播**）。
	 *
	 * 默认实现 = 恒接受（容量与溢出政策仍在引擎侧照常生效——策略只表达它真正在乎的那一档判断）。
	 *
	 * @return 返回是否接受。
	 */
	virtual bool ShouldAccept(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const;

	/**
	 * 层数决策（默认 = 内置规则：同来源不变、异来源 +1；未声明来源按同来源处理）。
	 *
	 * @return 返回本次申请之后的目标层数。
	 */
	virtual int32 ResolveStacks(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const;

#pragma endregion
};



/**
 * **技能侧默认策略：每次执行一实例**（对应 `ECastInstancing::CI_InstancePerExecution`）。
 *
 * **为什么用"恒不同组"表达而不是特例代码**：该档的语义就是"每次激活独立成实例"
 * ⇒ `IsSameGroup` 恒 false 即**由字段表达语义**，不需要在算子里写一条 `if (档位 == X)`。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsEntityAdmissionAlwaysNew : public FTcsEntityAdmission
{
	GENERATED_BODY()

	virtual bool IsSameGroup(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const override;
};



/**
 * **技能侧默认策略：每实体一实例 + 驳回**（对应 `CI_InstancePerEntity` + `bRetriggerOnActive = false`）。
 *
 * `IsSameGroup` 走默认（同实体 + 同 `DefTag`）；`ShouldAccept` 恒 false ⇒ 该档的语义是
 * "同一实体上已有在飞的同技能实例 ⇒ **驳回**"。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsEntityAdmissionPerEntityReject : public FTcsEntityAdmission
{
	GENERATED_BODY()

	virtual bool ShouldAccept(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const override;
};



/**
 * **技能侧默认策略：每实体一实例 + 顶替**（对应 `CI_InstancePerEntity` + `bRetriggerOnActive = true`）。
 *
 * 语义 = 接受这次申请（`ShouldAccept` 恒 true）而同组 ⇒ 决策落到"替换"档
 * （由调用方按 `Replace` 走"终止旧的、建新的"）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsEntityAdmissionPerEntityReplace : public FTcsEntityAdmission
{
	GENERATED_BODY()

	virtual bool ShouldAccept(const FTcsEntityAdmissionRequest& Request, const FTcsEntityAdmissionExisting& Existing) const override;
};
