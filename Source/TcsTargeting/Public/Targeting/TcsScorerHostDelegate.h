// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ScriptInterface.h"

#include "Host/TcsTargetScorerHost.h"
#include "Targeting/TcsTargetScorerStrategy.h"

#include "TcsScorerHostDelegate.generated.h"



/**
 * 宿主评分器**转发器**（P-A 排序相位）：把 `ITcsTargetScorerHost`（UObject 反射接口）接进
 * `FTcsTargetScorerStrategy`（USTRUCT 虚分派体系）——**宿主用任意 UE 脚本语言写评分逻辑，
 * 零 C++ 改动**。
 *
 * **为什么需要转发器**：评分器基类走 C++ 虚分派，而脚本定义的 struct **没有 C++ 类型**
 * ⇒ `CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即**野函数指针**（引擎层面无解）。
 * 转发器是 USTRUCT（进虚分派体系），内部把调用**转发**到 UObject 接口（反射分派，脚本可达）。
 *
 * **配置面**：本类型可被编辑器 picker 选中（`FTcsTargetScorerStrategy` 的 `BaseStruct` 限定内），
 * 宿主在其 `Host` 字段上指定脚本实现对象。
 *
 * **双轨并存**：内置评分器（`FTcsScorerDistance`）与 C++ 策略子类走虚分派快路径、原样保留；
 * 本转发器只服务宿主扩展（**评分调用频次高于过滤器**，多一次 `UFunction::Invoke` 的开销是
 * 明示接受的取舍——规格 SHOULD 建议"评分优先 C++、插槽用于低频 / 非关键排序"）。
 */
USTRUCT(DisplayName = "宿主脚本评分器（转发）")
struct TCSTARGETING_API FTcsScorerHostDelegate : public FTcsTargetScorerStrategy
{
	GENERATED_BODY()

// 转发目标
#pragma region Host

public:
	// 宿主实现（脚本层对象；未配置时返回中性分 + Warning——见 Score 实现）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting")
	TScriptInterface<ITcsTargetScorerHost> Host;

#pragma endregion


// 评分契约（纯转发）
#pragma region Score

public:
	/**
	 * 转发到 `Host->ScoreTarget`。
	 *
	 * 降级：`Host` 未配置 / 对象无效时返回**中性分 `0.0` + Warning**（**不淘汰候选**——
	 * 与过滤器插槽"空 Host = 通过"的既有裁定同源：配置遗漏 SHOULD 表现成"这一项不区分候选"，
	 * 而不是"淘汰全部候选"；后者会让配置残缺表现成"打不到人"，比中性更难排查）。
	 * 脚本实现返回 **NaN** 时按同一条浮点边界处理：**该候选被排除**（不特殊处理）。
	 *
	 * @param Candidate 候选目标实体句柄。
	 * @param Context 链黑板（只取 `Caster` / `Instigator` 两个句柄传给脚本）。
	 * @param EntityQuery 宿主实体查询能力（**不转发给脚本**——脚本要读世界时经门面按句柄访问器取）。
	 * @return 返回评分；未配置 Host 时返回 0.0。
	 */
	virtual double Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery) const override;

#pragma endregion
};
