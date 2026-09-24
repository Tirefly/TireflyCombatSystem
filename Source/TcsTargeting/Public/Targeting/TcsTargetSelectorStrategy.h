// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Chain/TcsEffectContext.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Host/TcsEntityQuery.h"

#include "TcsTargetSelectorStrategy.generated.h"



/**
 * 目标选择器策略抽象基类（D4-4 v2 策略模式 / D3-7 v3 载体 = USTRUCT 反射基类 + C++ 虚函数分派）。
 *
 * **选谁是宿主/内容语义，框架只立契约**（10 §2.2 同款纪律）：R3 不提供任何框架默认选择器——
 * 框架侧只交付本契约与 `SelectTargets` 步骤/执行器，竖切的选择器由宿主/测试装置提供。
 *
 * **"抽象"由约定达成（禁纯虚）**：UHT 为每个 USTRUCT 无条件生成 `TCppStructOps<T>`（构造路径需可默认
 * 构造），抽象类报 C2259；且 `PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 开启时展开为 `= 0`
 * （`CoreMiscDefines.h:100-102`）——同一份代码会在 Development 编不过、Shipping 能过。故基类提供
 * **中性默认实现**，并以 `meta = (Hidden)` 使其不可被编辑器类型 picker 选中（picker 按 Hidden 元数据过滤，
 * 引擎实证 `SInstancedStructPicker.cpp:104`）。同款先例：本仓 `FTcsParamValueSource`。
 *
 * 扩展方式 = **C++ 新 struct 子类**（宿主本体论语义零框架改动；BP 通道放弃，R0 §9"蓝图不承诺"承责）。
 * 配置面经 `FInstancedStruct` 内嵌编辑（2026-09-24 换型）——`BaseStruct` 限定由手写 metadata 提供
 * 自动写入（引擎实证 `UhtStructProperty.cs:634`；属性上再显式写 `meta=(BaseStruct=…)` 会报错）。
 */
USTRUCT(meta = (Hidden))
struct TCSTARGETING_API FTcsTargetSelectorStrategy
{
	GENERATED_BODY()

// 解析契约
#pragma region Resolve

public:
	/**
	 * 解析目标集。**只填充不清空**——调用方（SelectTargets 执行器）负责清空 OutTargets，
	 * 本方法 MUST NOT 假设其为空。
	 *
	 * @param Context 链黑板（Caster / Instigator / EventPayload / Targets / Variables）。
	 * @param EntityQuery 宿主注入的实体查询能力；**MAY 为 nullptr**（未注入）——此时 MUST 降级为
	 *                    可产出的结果并留 Warning 日志，MUST NOT 解引用空指针。
	 * @param OutTargets 目标集出参（**实体句柄**——与 `Context.Targets` 同型；句柄无生命周期语义，
	 *                   需要定位/存活时经 `EntityQuery` 询问宿主）。
	 */
	virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<FTcsCombatEntityHandle>& OutTargets) const
	{
		// 中性默认实现：不产出任何目标（基类不可被编辑器选中；运行侧取到它属代码缺陷）
	}

#pragma endregion
};
