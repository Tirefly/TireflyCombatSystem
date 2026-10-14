// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Def/TcsCostTypes.h"
#include "Skill/TcsSkillEntryHandle.h"

#include "TcsCostPolicyHost.generated.h"



// 消耗策略宿主插槽（`PLN-R6.5-CC` Task 1）：宿主用脚本/蓝图定义"这个技能消耗什么、怎么扣"。
// `UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`
// = UE 原生反射分发，脚本 / 蓝图 / 任意脚本语言均可实现。
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsSkillCostPolicyHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * 消耗策略宿主契约（**宿主面**，`D5-10` 的 `ICostPolicy` 的脚本可达形态）。
 *
 * **为什么必须有它**（本契约存在的唯一理由）：`FTcsSkillCostPolicy` 是 `USTRUCT` 虚分派基类
 * ——**C++ 宿主**派生它即可，但**脚本定义的 struct 物理不可达**（无 C++ 类型 ⇒
 * `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用，判据 = `FTcsParamValueSource` 的既有实测）。
 * ⇒ 解法 = **两层结构**（同族先例 `ITcsParamSourceHost` / `ITcsTargetScorerHost`）：
 * 宿主实现本接口，`FTcsSkillCostPolicy_HostDelegate` 把它接进既有虚分派体系。
 * **内建路径（`_ResourceAttr`）保持 C++ 快路径不变**。
 *
 * **为什么是 `UINTERFACE` 而不是 `USTRUCT` 策略基类**（`D5-10` 原写的 `ICostPolicy`
 * 就是后者，**该形状对脚本侧不可达**）：
 * ① 宿主的**既有类**（角色 / 组件 / 任意 `UObject`）可直接挂接口——一个类能同时实现多个插槽，
 *    不必为每个插槽专造一个对象；
 * ② 反射分发使蓝图 / C# / Lua / TS 任一语言都可实现——`USTRUCT` 策略基类只能被 C++ 与
 *    `FInstancedStruct` 选择器表达。
 *
 * **★ 形参 MUST 只用已反射类型（UHT 硬约束，MUST 读）**：
 * - **`FTcsLearnedSkillEntry` 是"非反射 struct"**（桶内池化纯数据，台账 `SCRIPT-10`；同
 *   `FTcsStateInstance`）⇒ 拿它作 `BlueprintNativeEvent` 形参是 **UHT 硬错误**
 *   （同 `ITcsCastStateQuery` 被迫传 `FTcsCastRunHandle` 而非 `FTcsCastRun`）
 *   ⇒ **必须传已反射的 `FTcsSkillEntryHandle`**。
 * - **翻译方向（MUST 写清，防写反）**：框架侧拿到的是**富上下文**（`FTcsSkillCostContext`，
 *   含 `FTcsLearnedSkillEntry*`），转发器把它**降级**为两个已反射句柄（`Entity` + 条目句柄）
 *   再交给宿主。宿主若要读**等级 / 定义**等条目字段，**经技能门面按句柄解析**
 *   （`SCRIPT-8` 的"按句柄访问器"手法）——**MUST NOT** 期待框架把非反射字段塞进签名。
 * - `FTcsCombatEntityHandle` / `FTcsSkillEntryHandle` 均已 `USTRUCT(BlueprintType)`
 *   ⇒ 可直接用，**不需要**选择器族那套"传句柄 + 门面访问器"。
 * - ⇒ **这正是本轮两处分层的落点**：`USTRUCT` 的 C++ 虚函数可自由取富类型（见
 *   `FTcsSkillCostContext`），而脚本面只见过反射类型，翻译住在转发器里。
 *
 * **形参 MUST NOT 带 `const`**（`BlueprintNativeEvent` 上的 `const` 会让 UHT 生成错误的 thunk
 * 签名，硬规则；先例 `ITcsEntityLevelProvider::GetEntityLevel` /
 * `ITcsParamTableReader::TryGetNumericParam` / `ITcsCastStateQuery::IsInterruptibleNow`）。
 *
 * **一处该契约做不到的事（如实登记）**：它**无法**表达"这条自定义消耗要扣多少"的**参数化数值**
 * （`FTcsParamValue`）——脚本宿主要自己持有数值（或从自己的数据里取）。
 * 若日后要"脚本策略 + `FTcsParamValue` 数值"，那是**另一层**（届时可让 `_HostDelegate`
 * 也带一个 `FTcsParamValue`）。**本轮不预建**（零消费者不预建）。
 */
class ITcsSkillCostPolicyHost
{
	GENERATED_BODY()

// 检查
#pragma region Afford

public:
	/**
	 * 该实体**现在**付得起吗（门禁⑤ 的判据）。
	 *
	 * **纯查询语义（MUST）**：实现方 MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件——
	 * 本口是"裁决"，不是"执行"（与 `ITcsCastStateQuery` 同款纪律）。
	 *
	 * **拒绝面形态**：返回 `bool`，**MUST NOT** 用 `ensure` 表达"付不起"——那是**正常业务路径**。
	 *
	 * @param Entity 被扣资源的实体。
	 * @param Entry 账本条目身份（**已反射句柄**；实现方要读等级/定义须经技能门面按此解析）。
	 * @param OutReason 输出"为什么付不起"的可读原因（宿主排查用）。
	 * @return 返回是否付得起。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool CanAfford(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, FString& OutReason);

#pragma endregion


// 提交
#pragma region Commit

public:
	/**
	 * 提交消耗（**实现方自己的扣减**）。
	 *
	 * **返回 `bool` 表示"是否成功"**——底层写口可能失败（实体未注册 / 资源不存在等）
	 * ⇒ 实现方 MUST 检查并如实返回，**MUST NOT** 恒返回 true
	 * （否则 `PayCost` 会报成功而资源没扣）。
	 *
	 * **调用方（框架）已保证 `CanAfford` 通过**——但实现方 MUST NOT 依赖"提交一定成功"。
	 *
	 * **原子性由框架承担**（实现方不必自己开批）：框架在调用本口前已 `BeginBatch`、
	 * 返回后 `Commit`（见 `FTcsCostOps::CommitCost` 的注释）。
	 * **一处如实登记**：批给的是"**单次 flush + 订阅者看不到中间态**"，**不是回滚**
	 * ⇒ 多资源策略若中途失败，前面已写的部分**不会自动撤销**；
	 * 实现方若要"全有全无"，MUST 自己**先全判后扣**（框架无法代判宿主资源语义）。
	 *
	 * @param Entity 被扣资源的实体。
	 * @param Entry 账本条目身份（已反射句柄）。
	 * @param OutApplied 输出实际扣减的数量（0 = 零动作）。
	 * @return 返回是否提交成功。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool CommitCost(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, double& OutApplied);

#pragma endregion
};



/**
 * 消耗策略宿主转发器（Task 1 的落地形态）：把宿主实现接进 `FTcsSkillCostPolicy` 虚分派体系。
 *
 * **纯转发 + 空 Host 守卫**：两个虚函数都转发到 `Execute_*`；`Host` 为空时**fail-closed**
 * （`CanAfford` 返回 `false`、`CommitCost` 返回 `false` 且实扣 0）。
 *
 * **★ 空 Host 为什么是 fail-closed 而不是"放行"**：一个装了本转发器但**没接宿主对象**的配置，
 * 与"忘了配 `CostMode`"是同一类**配置缺口**。放行等于**免费放技能**——而
 * `FTcsCostOps::CanAffordCost` 的注释里已把那条形态定性为"配置了消耗却静默不生效"的最坏形态。
 * **判据一致**：转发的默认值 MUST 与"策略缺失"那条路对齐（都是拦住）。
 * （对照 `FTcsParamSource_HostDelegate` 的空 Host 返回 0 —— 那里是**数值源**，
 * 返回中性值不会让"该扣的没扣"，语义不同，**MUST NOT 照抄**。）
 *
 * **★ MUST 用 `Execute_*` 而非虚表直调**：虚表直调会**静默跳过**脚本层实现
 * （引擎的 `BlueprintNativeEvent` 分发要走 `UFunction::Invoke`）——`TcsParamSourceHost.h:116`
 * 的既有判据："`Execute_` 是必需的，不是风格选择"。
 *
 * **载体与调用点零改动**：`FTcsSkillCostConfig::CostMode` 是裸 `FInstancedStruct`，
 * 照装本转发器；既有 `GetPtr<FTcsSkillCostPolicy>()` 调用点天然命中（本类是子类）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillCostPolicy_HostDelegate : public FTcsSkillCostPolicy
{
	GENERATED_BODY()

// 宿主
#pragma region Host

public:
	// 宿主实现（空 = 配置缺口 ⇒ fail-closed：`CanAfford`/`CommitCost` 均拒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Policy|Host")
	TScriptInterface<ITcsSkillCostPolicyHost> Host;

#pragma endregion


// 策略实现
#pragma region Policy

public:
	virtual bool CanAfford(const FTcsSkillCostContext& Context, FString* OutReason = nullptr) const override;

	virtual bool CommitCost(const FTcsSkillCostContext& Context, double* OutApplied = nullptr) const override;

#pragma endregion
};
