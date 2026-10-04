// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/TcsParamValueSource.h"
#include "TcsParamEnumerableSource.generated.h"



/**
 * 可枚举值来源基类（PV-10，2026-10-04 落地）：在值来源策略之上补"档位/表项"这一可选能力，
 * 供等级数组源一类"按等级取档"的源声明自己的档位口径。
 *
 * **索引解析的唯一真相在源**：展示/视图层 MUST NOT 自行推算下标（防"tooltip 高亮 10%、
 * 实打 7.5%"的双口径）；`Evaluate` 与 `GetIndexForLevel` MUST 同源（同一档位口径）。
 *
 * 抽象手法与基类同款（D3-7 v3 / StateTree 同构）：默认实现返回中性值 + `meta=(Hidden)`
 * 让编辑器类型 picker 不列本基类——UHT 为 USTRUCT 无条件生成 TCppStructOps 需要可默认构造，
 * 纯虚 (=0) 无法编译。能力探测一律走虚分派，消费方 MUST NOT 建立"源类型 × 可枚举"中心名单。
 *
 * 本基类只承担**"等级 → 档位下标"这一个中性入口**（PV-10 原案的另一半 `Enumerate(Level, OutValues,
 * OutCurrentIndex)` 与其派生形态"表项总数"**均未建**——它们的消费者是展示层的 Series 视图
 * `FTcsParamView_Series`（TcsNotation，R8），按"零消费者不预建"留白，落地时随真实消费方补）。
 */
USTRUCT(meta = (Hidden))
struct TCSCORE_API FTcsParamEnumerableSource : public FTcsParamValueSource
{
	GENERATED_BODY()

// 档位查询
#pragma region Index

public:
	/**
	 * 把等级解析为档位下标（索引解析的唯一真相在本源）。
	 *
	 * @param Level 生效等级（来自 `FTcsParamEvaluateContext::EffectiveLevel`）。
	 * @return 返回档位下标；INDEX_NONE = 该等级无对应档（中性默认）。
	 */
	virtual int32 GetIndexForLevel(int32 Level) const
	{
		return INDEX_NONE;
	}

#pragma endregion
};
