// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "TcsStateSubsystem.generated.h"

class UTcsStateParamTableReader;
class ITcsStateLiveQuery;



/**
 * 状态共享层门面（**2026-10-14 Task 4 拆分后**）：`TcsState` 与 `TcsBuff` 两域共用的**唯一读口**。
 *
 * **为什么拆出那么薄的一个类**：本类的前身（`UTcsStateSubsystem` 原体）承担了两件事——
 * ① **参数快照读取壳的持有者**（跨域设施：技能侧把自己的 run 快照绑上去求值引用类时长与参数链行，
 * 状态侧把实例快照绑上去做修正器物化）；② Buff 专属的施加 / 移除 / 堆叠 / 时值 / 行为订阅全体。
 * Task 4 把 ② 整体搬进新模块 `TcsBuff`（`UTcsBuffSubsystem`），而 ① **两域都要** ⇒ 留在共享层。
 *
 * **为什么读口只有这一个**（不是"三组读口"——★ 该判据曾被写错，见 `PLN-REFACTOR-1` Task 4
 * 《开工前置》的《一处判据自我纠正》）：按**代码级**消费者实测，`TcsSkill` 对本类的依赖
 * **只有 `GetParamTableReader()`**（`TcsCastOps.cpp` / `TcsCastOps_Phase.cpp` 各一处）；
 * 等级读口、事件与时钟读口的**代码级**消费者全部在 Buff 域（修正器物化路径 + 四个等级源 +
 * 门面注入口 + LAC 装置），故它们随 `UTcsBuffSubsystem` 走。
 * ⇒ **本类的存在判据 = "这个设施是否被两个域在代码级使用"**，本类只剩这一条成立。
 *
 * **与 `UTcsBuffSubsystem` 的分工**：本类**不持任何域数据**（无注册表、无桶、无定义表）；
 * 它是"跨域共享设施的持有者"。Buff 门面从本类取壳，**依赖方向单向**：`TcsBuff` → `TcsState`。
 *
 * **非 Tickable**：到期推进归 M0 时钟泵（`UTcsClockSubsystem` 的到期堆），本门面零每帧成本。
 *
 * **文件落点**：门面头与日志通道住 `Public/` 根（`cpp-module-structure` 规格）。
 */
UCLASS()
class TCSSTATE_API UTcsStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	/**
	 * 初始化：建**快照参数表反射壳**（`ParamTableReader`）——**两域共用同一份壳**。
	 *
	 * **为什么在初始化期就建**（而不是第一次物化时懒建）：壳是门面持有的**单例**，
	 * 建成之后物化路径**零分配**；而物化发生在施加流程里（可能正持着实例指针），
	 * 那个位置不该出现"可能触发 GC 的分配"这一类副作用。
	 *
	 * **为什么壳只有一份、且住共享层**：`FTcsStateSnapshotScope` 的绑定是**壳上的单例状态**
	 * （栈式保存/恢复）。若两域各持一份壳，则"当前绑定"会有两处真相，嵌套物化时的恢复顺序
	 * 将取决于调用方先取到哪一份——那是本仓反复拦的"第二处查找语义"。
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——参数表壳是运行时设施（对齐时钟/总线/属性/效果链门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 反初始化：清空壳持有的绑定（壳本身随本对象回收；域侧数据由各自的域门面自行清理）
	virtual void Deinitialize() override;

#pragma endregion


// 内核
#pragma region Core

public:
	/**
	 * 取快照参数表反射壳（**跨域唯一读口**）。
	 *
	 * 它是"把某份快照当参数表用"的载体：调用方构造 `FTcsStateSnapshotScope` 完成绑定/恢复，
	 * 再把 `Scope.GetTable()` 装进求值上下文（`FTcsParamEvaluateContext::ParamTable`）。
	 *
	 * **绑定纪律不在壳里、在作用域里**：绑定/恢复由 `FTcsStateSnapshotScope`（RAII，栈式）负责，
	 * 见 `State/TcsStateParamTableReader.h`——调用方 MUST NOT 手工 `Bind` 跨过可能广播的调用
	 * （那会让"当前绑定"变成一处隐式的跨帧状态）。
	 *
	 * @return 返回壳（初始化后非空；未初始化返回 `nullptr`，物化侧按"无参数表"降级）。
	 */
	UTcsStateParamTableReader* GetParamTableReader();

	/**
	 * 取在飞运行态查询契约（**R5.5-e1 新增**；未注册返回 nullptr）。
	 *
	 * **它为什么住共享层**：技能侧判关系表 `TagsToBlock` 时要读状态域的在飞实例，而
	 * `TcsSkill` MUST NOT 依赖 `TcsBuff`（两域是兄弟）⇒ 互通口 MUST 经共同下层中转。
	 * 本门面就是那个下层（它已是两域唯一的共用设施持有者）。
	 *
	 * **未注册是合法状态**（非错误）：调用方 MUST 走「无在飞实例」的保守路径并留具名日志，
	 * MUST NOT 静默当真（判据见 `ITcsStateLiveQuery` 的类注释）。
	 *
	 * @return 返回契约指针（未注册返回 `nullptr`）。
	 */
	ITcsStateLiveQuery* GetLiveQuery() const;

	/**
	 * **注册**在飞运行态查询契约——由**状态域自己**在初始化期调用（`UTcsBuffSubsystem::Initialize`）。
	 *
	 * **为什么是「域向下注册」而不是「宿主注入」**：这不是宿主能力（宿主没有理由知道插件内部
	 * 两个域要互通），而是框架自身机制的缺失（`PLN-REFACTOR-1` 把两域拆开后没留互通口）。
	 * 若要求宿主接线，则每个宿主都要接一次，而**漏接的表现是关系门静默失效**。
	 *
	 * @param InQuery 实现对象（域子系统自身；须实现 `ITcsStateLiveQuery`）。
	 */
	void SetLiveQuery(ITcsStateLiveQuery* InQuery);

private:
	// 在飞运行态查询契约（域侧注册；未注册 = nullptr 是合法状态）
	ITcsStateLiveQuery* LiveQuery = nullptr;

	// 快照参数表反射壳（`Initialize` 建的单例；绑定由 `FTcsStateSnapshotScope` 作用域负责）
	UPROPERTY()
	TObjectPtr<UTcsStateParamTableReader> ParamTableReader;

#pragma endregion
};
