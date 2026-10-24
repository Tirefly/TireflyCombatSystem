// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "State/TcsStateLiveView.h"

#include "TcsStateLiveQuery.generated.h"



// 在飞运行态查询契约（共享层定义、状态域实现——见下类注释）
UINTERFACE(MinimalAPI)
class UTcsStateLiveQuery : public UInterface
{
	GENERATED_BODY()
};

/**
 * **在飞运行态查询契约**（2026-10-16 `R5.5-e1` 立）：共享层定义、**状态域向下注册**实现。
 *
 * **它解决什么问题**（MUST 读，这是本类型存在的唯一理由）：技能侧判关系表时要读**状态域**的在跑实例
 * （类别词 + 它的 `TagsToBlock`）；而 `TcsSkill` **MUST NOT 依赖 `TcsBuff`**
 * （`PLN-REFACTOR-1` 验收明文：Build.cs 与 include 双向核——两域是兄弟，不是父子）。
 * ⇒ 互通口 MUST 经**共同下层**（`TcsState`）中转，MUST NOT 让任一域认识另一域。
 *
 * **为什么由「域自身注册」而不是「宿主注入」**（后者同样是类型擦除，但方向相反）：
 * 这不是宿主能力——宿主没有理由知道「插件内部两个域要互通」，那是**框架自身机制的缺失**
 * （`PLN-REFACTOR-1` 把两域拆开后没留下互通口）。若要求宿主接线，则每个宿主都要接一次，
 * 而**漏接的表现是关系门静默失效**——本仓最忌的失效形态。故本契约由域侧自己注册。
 *
 * **与 `ITcsEntityQuery` 的分工**：那个是「读世界」（遍历 / 定位 / 存活——**宿主本体论**，
 * 框架不认识「世界有哪些实体」）；本契约读的是**框架自己的运行态**（哪些状态实例在跑），
 * 故实现方是域而非宿主。
 *
 * **未注册时的降级语义（MUST 明确，防静默失效）**：注册方未就位时，调用方 MUST 走
 * 「**无在跑实例**」路径（关系门恒过的保守侧），并**留一条具名日志**——
 * MUST NOT 静默当作「查询结果为真」，也 MUST NOT 因此拒绝（那会让缺注册变成随机的业务失败）。
 *
 * C++ 专用面：形参含 `TFunctionRef`，蓝图不可表达（同 `ITcsEntityQuery` / `ForEachState` 的先例）。
 */
class ITcsStateLiveQuery
{
	GENERATED_BODY()

// 查询
#pragma region Query

public:
	/**
	 * 稳定序遍历某实体**当前在册**的状态运行态，逐个交出它的**关系行视图**。
	 *
	 * **为什么交「一对」而不是只交其中一半**（★ 契约形状经三次修正，前两版各少一半，MUST 读）：
	 * 技能侧要判的是**两个方向**——① 「在跑状态的类别词是否落进我的禁区」（有一个词面就够）；
	 * ② 「在跑状态是否用 `TagsToBlock` 覆盖我」（这要读**状态那一侧的行**，是施动侧的反向查找）。
	 * 只交类别词时方向 ② **无法表达** ⇒ 只能落成「未落面」。交行 = 一次把两面都给到。
	 *
	 * **为什么交行而不是交实例句柄 / 定义句柄**：
	 * ① 调用方要判的就是这两面，判据面只需**词**；
	 * ② 交句柄会把共享层变成「能操作另一域运行态」的通道，超出本契约的读用途；
	 * ③ 交行与 `FTcsEntityAdmissionExisting` 的手法一致——共享层定义它认得的**最小公共字段集**，
	 *    而 `FTcsRelationRow` 恰好就是共享层自己定义的那个纯值类型（不是域类型）。
	 *
	 * **遍历期间 MUST NOT 增删实例**（会改槽位与空闲栈）——调用方若需增删，先收集再动（两段式）。
	 *
	 * @param Entity 实体句柄（未注册该实体时零次回调，属正常路径）。
	 * @param Visitor 访问者（参数为该实例的**完整视图**——类别词集 + 关系行；返回是否继续）。
	 */
	virtual void ForEachLiveState(
		FTcsCombatEntityHandle Entity,
		TFunctionRef<bool(const FTcsLiveStateView&)> Visitor) = 0;

	/**
	 * 某实体当前在册的运行态实例数（观测与装置断言用；也给「反向查找扫描了几条」留读数）。
	 *
	 * **消费者登记（`ARCH-00` §3.3 的 B 档「装置观测口」两件事，MUST 写全）**：
	 * ① **消费者 = 装置断言**——今天**尚未接线**（`TcsDev` 里零调用）；
	 * ② **该装置可整体摘除**（`TcsDev` 头注释明写"可整体摘除"）⇒ 摘除时本口**同时归零**。
	 * ⚠ 这意味着今天它**没有任何调用者**，且**契约从未受检验**——保留它的判据不是"有人用"，
	 * 而是「`ForEachLiveState` 的配套读数，删了会让契约只剩单向面」（对称性）。
	 * 若将来仍无装置接线，MUST 按「零消费者不预建」重新评估是否删除。
	 *
	 * @param Entity 实体句柄。
	 * @return 在册实例数（未注册实体返回 0）。
	 */
	virtual int32 GetLiveCount(FTcsCombatEntityHandle Entity) const = 0;

#pragma endregion
};
