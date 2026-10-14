// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



/**
 * 共享层的**进程级分配代际发号器**（2026-10-14 立）——供 per-entity 桶注入 `TTcsInstancePool`
 * 的第三模板参 `TGenerationIssuer`。
 *
 * **为什么必须"进程级"而不是"每桶各自 +1"**：桶是 **per-entity** 的，而句柄**不带实体段**；
 * 若各桶各从代际 1 起发，则两个实体各自的首实例会得到**相同的 `(Index, Generation)`**，
 * 于是"跨桶扫描式解析"（`FTcsBuffOps::Find` 就是这种——`GetState(Handle)` 无实体入参）
 * 会**命中错实体**。这是本仓 **2026-10-05 实测过**的缺陷（`decisions-log.md:257` /
 * `implementation-log.md:281`：修法即"出线进程唯一发号器"）。
 *
 * **契约（MUST 逐条守）**：
 * - 每次分配取**不复用的正奇数**（奇 = 在册、偶 = 空闲，池的奇偶簿记依赖它）；
 * - 释放把该槽代际 `+1` 置偶（由池执行），**再次分配取新号**、MUST NOT 复用旧号；
 * - **世界拆解 MUST NOT 复位**（复位会让新世界与上一世界撞车——那正是级联摘除会误伤别人的形态）；
 * - `int32` 正值空间耗尽时**明确失败**（`Fatal`），MUST NOT 回绕复用。
 *
 * **无状态 + 出线（MUST）**：计数器本体住本模块的一个 `.cpp`（匿名 namespace 变量），
 * **MUST NOT** 用头内联 `static` / inline 变量——模块化构建下每个引用它的 DLL 各留一份，
 * 于是各模块各自从 1 起发号（症状随构建配置漂移，属最难察觉的一类）。
 */
struct TCSSTATE_API FTcsStateGenerationIssuer
{
	/**
	 * 发出该槽本次分配的代际。
	 *
	 * @param PriorGeneration 该槽当前代际（**全新槽传 0**，否则传释放后的偶数值）——本实现忽略它
	 *        （进程级发号与槽位无关），保留形参是为了满足 `TGenerationIssuer` 的静态接口契约。
	 * @return 返回**大于 0 的奇数**。
	 */
	static uint32 IssueAllocation(uint32 PriorGeneration);
};
