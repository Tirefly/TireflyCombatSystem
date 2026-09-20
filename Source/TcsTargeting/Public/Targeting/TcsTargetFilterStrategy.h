// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Chain/TcsEffectContext.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTargetFilterStrategy.generated.h"



/**
 * 目标过滤器策略抽象基类（载体同选择器：USTRUCT 反射基类 + C++ 虚函数分派；禁纯虚 + `meta=(Hidden)`
 * 的抽象约定见 `TcsTargetSelectorStrategy.h` 的说明）。
 *
 * **框架零默认 Filter**（10 §2.2）：怎么算存活、怎么算敌人、阵营如何判定都是**宿主本体论**——
 * 原枚举式 `TTF_Alive/TTF_Hostile` 正是"框架假装认识宿主语义再转手委托"的反例，v2 起改为直接暴露契约。
 * R3 竖切由测试装置实现（装置不入库，内容资产版装置随 plan2 Task 6）。
 *
 * 组合语义由执行器保证：**AND 全过 + 短路**（任一 Filter 返回 false 即淘汰该候选，不再问后续 Filter）。
 */
USTRUCT(meta = (Hidden))
struct TCSTARGETING_API FTcsTargetFilterStrategy
{
	GENERATED_BODY()

// 判定契约
#pragma region Pass

public:
	/**
	 * 候选是否通过过滤（纯判定——MUST NOT 改写候选或上下文）。
	 *
	 * @param Candidate 候选目标（**实体句柄**——需要存活/位置语义时由实现自行经注入接口询问宿主）。
	 * @param Context 链黑板（宿主语义可自取所需；框架不解释其内容）。
	 * @return 返回是否通过（中性默认实现 = 通过）。
	 */
	virtual bool Pass(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context) const
	{
		// 中性默认实现：通过（基类不可被编辑器选中；"框架零默认 Filter"指不提供任何有语义的默认实现）
		return true;
	}

#pragma endregion
};
