// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"

#include "Flow/TcsDamageFlowContextView.h"

#include "TcsFlowStepExecutorObject.generated.h"



/**
 * 流程步骤执行器插槽（2026-09-24，台账 S-8）：**让宿主用任意 UE 脚本语言定义流程步骤行为，
 * 零 C++ 改动**——"流程阶段构成 = 项目知识"（D7-5）的直接兑现：宿主自研的阶段（如
 * "先算护盾再生再结算"）正是最典型的"客制化、只服务宿主业务、不值得进插件"的语义。
 *
 * **为什么需要它**：既有流程执行器签名 `FTcsFlowStepExecute = TFunction<...>` 是**注册表的值**——
 * 键（`UScriptStruct*`）可反射而 `TFunction` 不可，脚本层无法登记（与 Effect 侧同款缺口）。
 * 本基类**替代 `TFunction` 作注册值**，由门面包成 `TFunction` 转发进既有注册表
 * （**键与查表逻辑零改动**、C++ 自注册宏路径原样保留）。
 *
 * **形参用视图不用原上下文**：`FTcsDamageFlowContext` 是**纯 C++ struct（无 `USTRUCT`）**，
 * 出现在 `UFUNCTION` 签名里会让 UHT 报 `Unable to find 'struct' with name ...`。
 * `FTcsDamageFlowContextView` 是它的可反射数据面投影（不含黑板——黑板深处嵌 `TFunction`，物理不可反射）。
 *
 * **写回限制（明示）**：视图是**只读投影**——脚本步骤若要修正流程值，MUST 走收集事件协议
 * （`UTcsDamageSubsystem::PublishCollectEvent` + 响应内 `Submit`），**MUST NOT** 期望改视图生效
 * （视图无反向写回，避免双真相）。
 *
 * **GC 可见持有**：登记后门面以 `UPROPERTY` 数组持有本类实例（同 `Templates` 的 T-8 教训——
 * 裸容器持不住对象引用 ⇒ 静默回收 ⇒ 表现为"步骤不生效"而非崩溃）。
 *
 * **文件名注意**：UHT 要求全项目头文件名唯一——`TcsFlowStepExecutor.h` 已被既有注册表头占用，
 * 故本文件名为 `TcsFlowStepExecutorObject.h`。
 */
UCLASS(Abstract, Blueprintable)
class TCSDAMAGE_API UTcsFlowStepExecutor : public UObject
{
	GENERATED_BODY()

// 执行入口
#pragma region Execute

public:
	/**
	 * 执行一个流程步骤（语义与 `FTcsFlowStepExecute` 完全一致）。
	 *
	 * **返回值表达中止**：`true` = 本步成功、流程继续；`false` = **中止流程剩余步骤**
	 * （流程失败/打断；已产生的副作用不回滚——止于未来）。
	 *
	 * @param StepData 本步数据（`FInstancedStruct`——脚本层可定义纯数据 struct 并真进反射）。
	 * @param Context 流程上下文**反射只读视图**（参与者/参数/分类 Tag/请求字段）。
	 * @return 返回流程是否继续。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	bool Execute(const FInstancedStruct& StepData, const FTcsDamageFlowContextView& Context);

	// 中性默认实现：返回 true（抽象基类不可被实例化；派生类 SHOULD 覆写）
	virtual bool Execute_Implementation(const FInstancedStruct& StepData, const FTcsDamageFlowContextView& Context);

#pragma endregion
};
