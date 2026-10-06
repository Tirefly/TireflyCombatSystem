// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "TcsEffectLogChannel.h"
#include "Trigger/TcsTriggerCondition.h"
#include "Trigger/TcsTriggerPayloadReader.h"



// 宿主脚本插槽的登记与撤销（条件求值器 / 载荷读取器；R-2 后段，2026-10-06）
//
// **为什么这四个口单独成文件**：门面 .cpp 已过 300 行（风格上限），而这一组是内聚的一块——
// "两个宿主插槽 × 登记/撤销"的对称四口，判据与寿命纪律完全相同，拆出来不割裂任何既有逻辑。
//
// **本文件只管这两个插槽**：同族的第三个插槽（步骤执行器）的 `RegisterStepExecutor` 仍在
// `TcsEffectSubsystem.cpp` 里——它属 `Chain` 时代的既有代码，没有撤销口、寿命手法也不同
// （见 `UnregisterConditionEvaluator` 的注释），**不为套用风格而搬动无关代码**。
//
// **为什么必须有撤销口**（与步骤执行器插槽的关键差异）：`RegisterStepExecutor` 没有配对撤销口
// ⇒ 同类型在本世界只能登记一次、**同一 PIE 会话无法重臂**；而条件/载荷两张注册表本就支持撤销
// ⇒ 缺了门面口，"重新布置一次探针"会被自己的重复登记门挡住，表现为**装置不可重跑**而非配置错误。
//
// **强持有数组为何不在撤销时收缩**：见 `UnregisterConditionEvaluator` 内的注释（过度持有无害、
// 误删致命——后者是 WAIT-8 形态）。

// 登记宿主脚本条件求值器（R-2 后段；形态见声明处注释——**不造 USTRUCT 转发器**）
bool UTcsEffectSubsystem::RegisterConditionEvaluator(
	const UScriptStruct* ConditionStruct,
	TScriptInterface<ITcsTriggerConditionEvaluator> Evaluator)
{
	ensure(IsInGameThread());

	UObject* EvaluatorObject = Evaluator.GetObject();

	// 拒绝面（配置错误 → ensure + false）。空对象与空键都**不静默吞掉**：
	// 静默会让"插槽没接上"表现成"该条件恒不过"，比 ensure 更难查
	if (!ConditionStruct)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterConditionEvaluator: ConditionStruct 为空"));
		return false;
	}
	if (!EvaluatorObject)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterConditionEvaluator: 宿主求值器对象为空（条件类型 %s）"),
			*ConditionStruct->GetName());
		return false;
	}

	// 同键**有效**登记已存在 ⇒ 拒绝并保留首个（配置错误 → ensure + false）。
	//
	// **为什么在门面显式判、不靠注册表返回值**：注册表的 `Register` 是 `void` ⇒ 拒绝与成功在
	// 调用侧**不可分辨**；而本口的返回值是宿主脚本唯一的判据面（`tasks.md 5.7` 的"重复登记被拒"
	// 负对照正是靠它——没有它，"登记失败"只能靠翻日志里的 ensure 才知道）。
	//
	// `Find` 顺带把寿命校验做了：既有登记已失效（对象已回收 / 旧世界残留）时它返回 nullptr
	// **并移除该条目** ⇒ 本处放行，注册表随后的 `Add` 走的就是替换路径（与 2026-09-29 的
	// "失效条目 MUST 可替换、否则毒化后续 PIE"口径一致）。
	if (FTcsTriggerConditionRegistry::Get().Find(ConditionStruct, GetWorld()))
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterConditionEvaluator: 条件类型 %s 重复登记——保留首个登记"),
			*ConditionStruct->GetName());
		return false;
	}

	// 包成 TFunction 转发进既有注册表（键与查表逻辑零改动——内置条件的 C++ 快路径原样保留）
	// 捕获裸指针：本对象同时被下面的 UPROPERTY 数组强持有（防静默回收）**与**注册表的弱引用寿命
	// 信息（防跨世界残留）——两者互补，缺一都有洞（GC 可见持有见 RegisteredConditionEvaluators）
	FTcsTriggerConditionTest Forwarder = [EvaluatorObject](
		const FInstancedStruct& ConditionData,
		const FTcsTriggerContext& Context,
		double RandomValue)
	{
		// 转发到宿主实现（**反射分派**——脚本层可达）。
		// `Execute_Test` 是 UINTERFACE 的 BlueprintNativeEvent 静态助手（接口**必须**经它；
		// 与 UCLASS 不同——后者的命名函数自带分派，见 RegisterStepExecutor 的说明）：
		// 虚表直调会**静默跳过**脚本层实现
		return ITcsTriggerConditionEvaluator::Execute_Test(EvaluatorObject, ConditionData, Context, RandomValue);
	};

	// 记录寿命（**对象取自 TScriptInterface**：它本身不是 `UObject*`，不显式交出就落成"不做对象寿命校验"）。
	// 注册表的拒绝门据此判"同世界活对象重复"——失效条目会被替换而非拒绝
	FTcsTriggerConditionRegistry::Get().Register(ConditionStruct, MoveTemp(Forwarder), EvaluatorObject, GetWorld());

	// GC 可见持有（WAIT-8 教训：裸注册表持不住对象引用 ⇒ 静默回收 ⇒ 表现为"条件不生效"而非崩溃）
	RegisteredConditionEvaluators.AddUnique(Evaluator);

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 条件类型 %s 已登记宿主求值器（%s）"),
		*ConditionStruct->GetName(), *EvaluatorObject->GetClass()->GetName());
	return true;
}



// 登记宿主脚本载荷读取器（R-2 后段；形态与寿命纪律同 RegisterConditionEvaluator）
bool UTcsEffectSubsystem::RegisterPayloadReader(
	const UScriptStruct* PayloadStruct,
	TScriptInterface<ITcsTriggerPayloadReader> Reader)
{
	ensure(IsInGameThread());

	UObject* ReaderObject = Reader.GetObject();

	if (!PayloadStruct)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterPayloadReader: PayloadStruct 为空"));
		return false;
	}
	if (!ReaderObject)
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterPayloadReader: 宿主读取器对象为空（载荷类型 %s）"),
			*PayloadStruct->GetName());
		return false;
	}

	// 同键**有效**登记已存在 ⇒ 拒绝并保留首个（理由与门面返回值语义同 `RegisterConditionEvaluator`）。
	// `Find` 会顺带清掉已失效的旧登记，故"旧世界残留"不会把新登记挡在门外
	if (FTcsTriggerPayloadReaderRegistry::Get().Find(PayloadStruct, GetWorld()))
	{
		ensureMsgf(false, TEXT("UTcsEffectSubsystem::RegisterPayloadReader: 载荷类型 %s 重复登记——保留首个登记"),
			*PayloadStruct->GetName());
		return false;
	}

	// 包成 TFunction 转发进既有注册表（属主模块的静态自注册路径原样保留——**双轨并存**）。
	// **"每事件一次"语义**：本转发器只替换"读取器"这一层，读取点仍在求值器里被各行共用一次
	// （MUST NOT 因本插槽把读取挪进每行循环——那会让宿主读取器被按行重复调用）
	FTcsTriggerPayloadRead Forwarder = [ReaderObject](const FInstancedStruct& Payload)
	{
		return ITcsTriggerPayloadReader::Execute_Read(ReaderObject, Payload);
	};

	FTcsTriggerPayloadReaderRegistry::Get().Register(PayloadStruct, MoveTemp(Forwarder), ReaderObject, GetWorld());

	RegisteredPayloadReaders.AddUnique(Reader);

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 载荷类型 %s 已登记宿主读取器（%s）"),
		*PayloadStruct->GetName(), *ReaderObject->GetClass()->GetName());
	return true;
}



// 撤销宿主登记（与两个 RegisterXxx 配对；只撤动态登记，不动内置/属主模块的静态自注册）
bool UTcsEffectSubsystem::UnregisterConditionEvaluator(const UScriptStruct* ConditionStruct)
{
	ensure(IsInGameThread());

	if (!ConditionStruct)
	{
		return false;
	}

	const bool bRemoved = FTcsTriggerConditionRegistry::Get().Unregister(ConditionStruct);

	if (bRemoved)
	{
		// **强持有数组刻意不在这里收缩**（与 `Deinitialize` 的整体 `Reset()` 不同）。
		//
		// 理由是不对称代价：多留一个引用只是**多活一会儿**（无害的过度持有，对象随门面一起回收）；
		// 而误删一个仍被别的动态键引用的对象，会让那条键**静默失效**——正是 WAIT-8 的形态
		// （表现为"条件不生效"而不是崩溃，最难查）。且键 → 对象的反查需要注册表开出寿命表访问面，
		// 为一个良性收益扩反射/公开面不划算。
		//
		// 同一宿主对象登记多个键时，撤销其中一个键不会波及其余键的保活——这正是本处不收缩的收益。
		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 条件类型 %s 的宿主求值器登记已撤销"),
			*ConditionStruct->GetName());
	}

	return bRemoved;
}



bool UTcsEffectSubsystem::UnregisterPayloadReader(const UScriptStruct* PayloadStruct)
{
	ensure(IsInGameThread());

	if (!PayloadStruct)
	{
		return false;
	}

	const bool bRemoved = FTcsTriggerPayloadReaderRegistry::Get().Unregister(PayloadStruct);

	if (bRemoved)
	{
		// 同 `UnregisterConditionEvaluator`：强持有数组不在此收缩（过度持有无害，误删致命）
		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 载荷类型 %s 的宿主读取器登记已撤销"),
			*PayloadStruct->GetName());
	}

	return bRemoved;
}
