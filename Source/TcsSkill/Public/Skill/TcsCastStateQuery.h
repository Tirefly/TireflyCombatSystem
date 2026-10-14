// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Skill/TcsSkillInstanceHandle.h"

#include "TcsCastStateQuery.generated.h"



// 施法状态查询契约（宿主实现：这个施法现在能不能打断 / 能不能移动）——
// `ECastQueryMode::CQM_Custom` 档的唯一载体（UINTERFACE + UFUNCTION(BlueprintNativeEvent)
// = UE 原生反射分发，脚本/蓝图/任意脚本语言均可实现）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsCastStateQuery : public UInterface
{
	GENERATED_BODY()
};

/**
 * 施法状态查询契约（宿主面，D5-1 查询契约的 `CQM_Custom` 档）：宿主回答"**这个施法现在**
 * 能不能被打断 / 能不能移动"。
 *
 * **为什么必须是 `UINTERFACE` 而不是 USTRUCT 策略基类**（本需求明文，MUST 记录）：
 * ① 宿主的**既有类**可直接挂接口（一个类可挂多个插槽），不必为每个插槽专造一个对象；
 * ② 反射分发使蓝图 / C# / Lua / TS 任一语言都可实现——USTRUCT 策略基类只能被 C++ 与
 *    `FInstancedStruct` 选择器表达。
 *
 * **宿主可达性（谁在什么时候调它）**：宿主**不直接调本接口**。唯一合法入口是门面上的
 * `UTcsSkillSubsystem::IsInterruptibleNow` / `CanMoveNow`——它按 `ECastQueryMode` 三档分派，
 * 只有 `CQM_Custom` 一档会走到本接口。宿主 MUST NOT 自行读 `FTcsSkillDefData` 的开关字段来代判
 * （那会在三档之间制造**第二真相**）。
 *
 * **形参 MUST 是已反射句柄**（`FTcsSkillInstanceHandle`）：`FTcsSkillInstance` 自身**非反射**（池内池化纯数据，
 * 无 `USTRUCT`）⇒ 拿它作 `BlueprintNativeEvent` 形参是 **UHT 硬错误**。句柄已反射 ⇒ 脚本面可达。
 *
 * **⚠ 本轮（R6 Task 5）本接口"可声明、不可求值"（如实登记，MUST NOT 被误读）**：
 * `CQM_Custom` 档的**求值**归 `R6.5-g`，本轮该档 MUST **具名报出"未实现"并回退 `DefSwitches` 档**。
 * 本接口此刻落地的是**形状**（宿主可据此提前实现），而"谁来求值 `CastQueryFragment`"这件事本身
 * 尚未定形——`CastQueryFragment` 是 `FInstancedStruct`（可编的**策略结构体**），而本接口是
 * **UObject 接口**（可被既有类实现），两者**不是同一种载体**：到底由宿主把实现对象注入门面，
 * 还是让片段持一个指向实现对象的引用，属 `R6.5-g` 的契约裁决。
 * **⇒ 本接口本轮零调用点，这是需求明文允许的形态**（同族先例 = `ITcsChainRedirectResolver`
 * 的"注入点可留形状，求值 MUST 具名报未实现"）。
 */
class ITcsCastStateQuery
{
	GENERATED_BODY()

// 查询契约
#pragma region Query

public:
	/**
	 * 当前是否可以打断这次施法。
	 *
	 * **不带 `const`**：`BlueprintNativeEvent` 上的 `const` 会让 UHT 生成错误的 thunk 签名，
	 * 是硬规则（先例 `ITcsEntityLevelProvider::GetEntityLevel` / `ITcsParamTableReader::TryGetNumericParam`）。
	 *
	 * **纯查询语义（MUST）**：实现方 MUST NOT 改运行态、MUST NOT 发事件、MUST NOT 动账本——
	 * 本接口是"裁决"，不是"执行"。
	 *
	 * @param Run 施法运行态句柄（在飞；悬空句柄由调用方在分派前挡掉）。
	 * @return 返回当前是否可打断。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool IsInterruptibleNow(FTcsSkillInstanceHandle Run);

	/**
	 * 当前是否允许移动（宿主移动系统询问）。
	 *
	 * 约束同上（不带 `const`、纯查询语义）。
	 *
	 * @param Run 施法运行态句柄（在飞）。
	 * @return 返回当前是否允许移动。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool CanMoveNow(FTcsSkillInstanceHandle Run);

#pragma endregion
};
