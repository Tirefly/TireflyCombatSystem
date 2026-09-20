// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsEffectChain.generated.h"



/**
 * 效果链定义（D4-3）：**链 = 数据（句子）**，语义 = 词汇（C++）——分离宪法 R0 §8。
 * 链是共享 Const 内容：解释器执行期间只读，不做运行时步骤级补丁（04 §5 Authoring 边界）。
 *
 * 字段说明：
 * - `ChainId`：链的唯一标识，也是**登记表的键**（`UTcsEffectSubsystem::RegisterChain` 不另传 id）；
 * - `Steps`：有序步骤数组，元素类型无公共基类（D4-16）——类型合法性由执行器注册表在执行期判定；
 * - `MaxStepsPerFrame`：单帧步数熔断上限（自激/循环链的失控护栏，超限 ensure + 断链），默认 64。
 */
USTRUCT()
struct TCSEFFECT_API FTcsEffectChain
{
	GENERATED_BODY()

// 身份与步骤
#pragma region Definition

public:
	// 链唯一标识（= 登记表键；空 id 拒绝登记）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect")
	FName ChainId;

	// 有序步骤数组（步骤类型由执行器注册表分派；未知类型在执行期断链并留日志）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect")
	TArray<FInstancedStruct> Steps;

	// 单次进入执行的步数上限（熔断护栏——正常链路远低于此值）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect", meta = (ClampMin = "1"))
	int32 MaxStepsPerFrame = 64;

#pragma endregion
};
