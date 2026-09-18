// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttributeBandFold.h"
#include "Attribute/TcsAttributeInstance.h"
#include "Attribute/TcsAttributeName.h"
#include "Attribute/TcsAttributeStore.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

class UTcsAttributeSubsystem;



/**
 * 属性聚合管线（M2，02 §3/§4）：值的唯一生产者与变更的唯一记账口。
 *
 * 职责：脏则重算（收集 → 折叠 → 值域收口 → 写缓存）｜事务批与唯一提交点｜读即登记依赖边 +
 * 成环拒绝｜变化广播（epsilon 1e-5）｜按来源级联摘除（扫描面含冻结暂存区）。
 *
 * 非 UObject：纯 C++ 组件，由 `UTcsAttributeSubsystem` 持有（PIMPL：门面头只前向声明）。
 *
 * **可见性口径（2026-09-18 用户拍板：表声明在 Public、实现在 Private）**——本头住 `Public/`
 * 是为了让**跨模块消费者**在不碰内部机制的前提下直接驱动某个单位的求值/事务（流程域属性容器、
 * 离屏或工具侧求值等）；实现仍留 `Private/Attribute/`（三份 `.cpp` 不随头公开）。
 *
 * 调用纪律：
 * - **推荐路径是门面**（`UTcsAttributeSubsystem` 转发同名入口）——游戏逻辑一律经门面，门面是唯一入口；
 *   直调本类是**逃生口**，只用于确无门面可用的场景（离屏工具、非 UWorld 宿主）。
 * - 两条路径操作同一份状态，MUST NOT 混用成两条记账路径（"管线是 `CachedCurrent` 唯一生产者"）。
 * - 机制面（`private:` 段：重算内核 / 依赖登记 / 求值栈等）**不属于消费契约**——外部 MUST NOT 依赖，
 *   随实现演进变更不另行通知；公开面只有下面 Read / Write 两个区的入口。
 * - **生命周期**：实例归所属门面所有、引用绑定该门面；调用方 MUST NOT 跨帧持有或在门面销毁后使用
 *   （World 切换 / Deinitialize 之后即为悬空——不要把它存成成员）。
 *
 * 导出宏：本类有 out-of-line 成员，跨模块消费必须带 `TCSATTRIBUTE_API`（纯头文件内联值类型反之——
 * 见 `TcsCore` 的 `TcsSourceHandle` 注记）。
 */
class TCSATTRIBUTE_API FTcsAttributePipeline
{
public:
	// 构造（绑定所属门面——数据宿主）
	explicit FTcsAttributePipeline(UTcsAttributeSubsystem& InOwner)
		: Owner(InOwner)
	{
	}

// 读
#pragma region Read

public:
	/**
	 * 求值当前值（脏则惰性重算）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回当前值；单位未注册或属性未定义时返回 0（不 ensure——读取是正常查询路径）。
	 */
	double EvaluateCurrent(FTcsCombatEntityHandle Unit, const FTcsAttributeName& Attribute);

	/**
	 * 读未提交候选值（不落账预览，D2-5）：批进行中返回"若此刻提交会是多少"，不做写回/标脏/广播；
	 * 无进行中的批时等同 `EvaluateCurrent`。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回候选值。
	 */
	double PeekPending(FTcsCombatEntityHandle Unit, const FTcsAttributeName& Attribute);

#pragma endregion


// 写
#pragma region Write

public:
	/**
	 * 挂修正器（标脏 + 登记；批内不立即重算/广播）。
	 *
	 * @param Unit 单位句柄。
	 * @param Modifier 修正器（Target 指向目标属性）。
	 * @return 返回是否挂载成功（单位未注册 / 目标属性未定义 = 忽略并留日志，返回 false）。
	 */
	bool ApplyModifier(FTcsCombatEntityHandle Unit, const FTcsAttrModInstance& Modifier);

	/**
	 * 改写基础值（事务的"改基值"写操作类，02 §2.2a/§4）：标脏并按事务纪律重算/广播
	 * （批内只标脏、提交期统一处理；批外立即生效）。
	 * 单位未注册或属性无实例时忽略并留日志（返回 false）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @param NewBaseValue 新的基础值。
	 * @return 返回是否改写成功。
	 */
	bool SetBaseValue(FTcsCombatEntityHandle Unit, const FTcsAttributeName& Attribute, double NewBaseValue);

	/**
	 * 按来源级联摘除（D2-2）：扫描该单位全部属性的修正器槽位**与冻结暂存区**，
	 * 摘除 Source 匹配的条目 → 标脏 → 重算 → 按变更规则广播。
	 *
	 * @param Unit 单位句柄。
	 * @param Source 来源句柄。
	 * @return 返回摘除的修正器条数（0 = 无匹配，属正常路径）。
	 */
	int32 RemoveBySource(FTcsCombatEntityHandle Unit, const FTcsSourceHandle& Source);

	/**
	 * 开始一次变更批（嵌套计数：内层提交不触发 flush，最外层提交才触发）。
	 *
	 * @param Unit 单位句柄。
	 */
	void BeginBatch(FTcsCombatEntityHandle Unit);

	/**
	 * 提交变更批：对全部脏属性重算并按 epsilon 比较后广播（每个属性最多一次）。
	 * 唯一提交点：重算失败（如成环）的属性**不写回**（保持提交前的缓存值）。
	 *
	 * @param Unit 单位句柄。
	 */
	void Commit(FTcsCombatEntityHandle Unit);

#pragma endregion


// 内部
#pragma region Core

private:
	// 重算内核（写回 + 清脏 + 广播）：返回是否发生实质变化
	bool Recalculate(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store, FTcsAttributeInstance& Instance, bool bInBatch);

	// 重算全部脏属性（多轮扫描至收敛；提交尾与按来源摘除后共用）
	void FlushDirty(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store);

	// 只算不写（PeekPending 用——评估候选值但不落账）
	double ComputeCandidate(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance);

	// 收集 + 折叠 + 值域收口（重算与候选值共用；会登记依赖边）
	double ComputeFoldedValue(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance);

	// 值域收口（Clamp / Wrap / Custom 逃逸位）
	double ApplyValueDomain(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance, double Value);

	// 边界解析（ABM_Static 取静态值；ABM_Dynamic 先按管线求值——其依赖同样"读即登记"）
	void ResolveBound(FTcsCombatEntityHandle Unit, FTcsAttributeStore& Store, const FTcsAttributeName& ForAttribute,
		const FTcsAttributeBound& Bound, bool& bOutHasValue, double& OutValue);

	// 读即登记依赖边（被读者 → 读者）；成环则拒绝该边并 ensure
	void RegisterDependency(FTcsAttributeStore& Store, const FTcsAttributeName& ReadAttribute, const FTcsAttributeName& ReaderAttribute);

	// 依赖传播：被读属性变化 → 把其读者全部标脏
	void MarkDependentsDirty(FTcsAttributeStore& Store, const FTcsAttributeName& ChangedAttribute);

	// 变更广播（epsilon 1e-5 判定；未变不广播）
	void BroadcastChange(FTcsCombatEntityHandle Unit, const FTcsAttributeName& Attribute, double OldValue, double NewValue) const;

	// 求值栈防重入（同一属性在单次求值链中只允许出现一次；命中 = 环）
	bool PushEvalStack(const FTcsAttributeName& Attribute);
	void PopEvalStack(const FTcsAttributeName& Attribute);

	// 所属门面（数据宿主与单位注册表）
	UTcsAttributeSubsystem& Owner;

	// 求值栈（防重入：AttributeScaled 链上的环会在 RegisterDependency 处被拒，此处为兜底）
	TArray<FTcsAttributeName> EvalStack;

#pragma endregion
};
