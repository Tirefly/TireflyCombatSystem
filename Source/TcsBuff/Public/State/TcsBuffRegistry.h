// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "Runtime/TcsEntityRuntimeBucket.h"
#include "State/TcsBuffInstance.h"
#include "State/TcsBuffInstanceHandle.h"
#include "State/TcsStateInstance.h"



/**
 * 状态层的 per-entity 桶与注册表（D3-1/D3-3）——**共享层模板的实例化**（2026-10-14 起）。
 *
 * **本文件不再是"实现"，只是"绑定"**：手写的槽位 / 代际 / 空闲栈 / 代际校验 / 稳定序遍历
 * 已于 2026-10-14 全部收敛到共享层的 `TTcsEntityRuntimeBucket` / `FTcsEntityRuntimeRegistry`
 * （见 `Runtime/TcsEntityRuntimeBucket.h`）。此处只把**载荷类型与句柄类型**钉进模板。
 *
 * **★ 载荷类型 = `FTcsBuffInstance`，MUST NOT 改回 `FTcsStateInstance`**（2026-10-14 拆解后新增）：
 * `FTcsStateInstance` 现在是**基底**（只装 6 个公共字段），Buff 的层数 / 时值 / 周期三个语义
 * 与 4 个域内字段都住派生类型 `FTcsBuffInstance`。桶若绑基底，**层数与时值会从桶里消失**
 * （池按值存 `TArray<TPayload>`，绑基底就是把 buff 字段丢掉）。⇒ 桶载荷**一律用派生类型**。
 * 共享机制只认基底 ⇒ 它**看不见**派生的额外字段，这正是"机制通用、语义分域"的落点。
 *
 * **为什么保留本文件与这两个名字**：域侧（`TcsStateOps*` / 门面）大量引用它们；用别名而非
 * "到处写长模板参数"既保留了可读性，也让"两域共用同一模板"这件事在类型层面**可审**
 * （别名两侧的模板实参不同、模板本体同一份）。
 *
 * **桶载体 = `TTcsInstancePool`，且注入进程级发号器**（`FTcsStateGenerationIssuer`）：
 * 桶是 per-entity 的而句柄不带实体段 ⇒ 各桶各从代际 1 起发会让"跨桶扫描式解析"（`GetState`）
 * **命中错实体**。这是本仓 2026-10-05 实测过的缺陷 ⇒ **MUST NOT** 改回池内自增。
 */
using FTcsBuffBucket = TTcsEntityRuntimeBucket<FTcsBuffInstance, FTcsBuffInstanceHandle>;

// 沿用旧名（域侧大量调用点用 `FStateBucket`；保留别名把"这是共享模板的一个实例化"这件事
// 与"调用点不必改"两件事同时做掉。**MUST NOT** 据此认为"状态侧还有一份自己的桶实现"——
// 本文件下方没有任何实现，逻辑全在共享层模板里）。
using FStateBucket = FTcsBuffBucket;



/**
 * per-entity 状态注册表：`实体 → 桶` 的登记表，持全部在册状态实例。
 *
 * **脏句柄一律拒绝（不 `ensure`）**：代际失配 / 下标越界 / 实体未注册都是**时序竞态**语义
 * （调用方手上的句柄过期了），不是配置错误——与"非法阶段迁移"的 `ensure` 口径刻意分开。
 * 拒绝 MUST NOT 影响同桶其它实例。该语义由共享层模板统一承载（见其文档）。
 */
using FTcsBuffRegistry = FTcsEntityRuntimeRegistry<FTcsBuffInstance, FTcsBuffInstanceHandle>;
