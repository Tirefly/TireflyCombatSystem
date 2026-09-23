// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTriggerCondition.generated.h"



/**
 * 触发期上下文（**最小集**）：条件求值的数据面——只含本批条件真正需要的字段。
 *
 * **MUST NOT 为将来的条件预建字段**（零消费者不预建）：`AttributeCompare` 需要属性读取注入、
 * `VariableCompare` 需要变量存储、`GateCheck` 读 M5 的 `BoolSwitches`——三者今天都零消费者，
 * 各自落地时**再扩本结构**（那时它们有真实消费者，字段形状才定得准）。
 *
 * 与流程侧上下文的分工：`FTcsDamageFlowContext` 是**流程内**（三层值空间 + 黑板 + 参与者），
 * 本结构是**触发期**（事件刚到达、尚未起链）——两者不是一回事，MUST NOT 互相替换。
 *
 * **扩展方式（待落地）**：设计承诺"上下文扩展 = 结构体继承 + 源内 checked cast"
 * （PV-1 机制，`FTcsParamEvaluateContext` 先例）。**R4 不实现**——今天零消费者，
 * 且"Buff 生命周期参数（StateHandle/Stacks/Level）"这一批扩展要等 M3 落地才有真实数据源。
 */
USTRUCT()
struct TCSEFFECT_API FTcsTriggerContext
{
	GENERATED_BODY()

// 数据面
#pragma region Data

public:
	// 命中的事件 Tag（求值期由求值器填入——条件可据此区分同一行订阅多 Tag 的情形）
	UPROPERTY()
	FGameplayTag EventTag;

	// 事件携带的分类标签（如伤害流程的分类 Tag 集；`HasAllTags` 条件在此集合上匹配）
	UPROPERTY()
	TArray<FGameplayTag> ClassificationTags;

	// 主体（从事件载荷解析；取不到时为默认构造——"载荷类型未知"不是契约违规，不 ensure）
	UPROPERTY()
	FTcsCombatEntityHandle Caster;

#pragma endregion
};



/**
 * 条件求值器签名（**与流程步骤执行器同款形态**——按类型分派走注册表）。
 *
 * **为什么用注册表而不是 USTRUCT 虚分派基类（2026-09-23 决定，依据 CS 脚本调研）**：
 * 见 `TcsTriggerConditionRegistry` 的类注释（虚分派在 C# 侧物理不可达）。
 *
 * 形参：条件数据（只读，具体类型由注册键决定）+ 触发期上下文（只读）+ 注入随机值（仅概率类条件用）。
 * 返回：条件是否通过。
 */
using FTcsTriggerConditionTest = TFunction<bool(
	const FInstancedStruct& ConditionData,
	const FTcsTriggerContext& Context,
	double RandomValue)>;



/**
 * 待解析登记项：静态初始化期只把"条件类型 getter + 求值器"挂进表里，**不调用 getter**
 * （静态初始化期触 UObject 是雷区——引擎 `FNativeGameplayTag` 用 `GetIfAllocated()` 规避同款问题；
 * 本仓 `FTcsEffectStepExecutorEntry` 同款处置）。
 */
struct FTcsTriggerConditionEntry
{
	// 条件类型 getter（通常即 `&FTcsTriggerCondition_Xxx::StaticStruct`；首次查询时才调用）
	UScriptStruct* (*GetConditionStruct)() = nullptr;

	// 求值器
	FTcsTriggerConditionTest Test;
};



/**
 * 静态自注册器（宏展开的载体）：模块静态初始化期构造 → 把本项挂入待解析表。
 * **零 UObject 触达**——反射类型延迟到注册表首次查询时才解析。
 */
struct TCSEFFECT_API FTcsTriggerConditionRegistrar
{
	FTcsTriggerConditionRegistrar(UScriptStruct* (*InConditionStructGetter)(), FTcsTriggerConditionTest InTest);
};



/**
 * 条件求值器注册表（**按类型分派**）：宿主/领域模块在自己的实现文件里自登记，
 * 求值助手按条件 struct 的**反射类型**查表分派。
 *
 * **形态依据（2026-09-23 决定）——为什么是注册表而非 `USTRUCT` 虚分派基类**：
 * 调研文档 `2026-09-23-scripting-language-ustruct-research.md` §5–§6 给出引擎级论证：
 * 虚分派依赖 vtable，而 vtable 来自 UHT 为 **C++ 类型**生成的 `TCppStructOps<T>`
 * （`Class.h:2265`）；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr`
 * （`Class.cpp:3120-3144`）→ 实例内存由 `FMemory::Memzero` 起步、**vtable 指针位为 0**
 * → `GetStructPtr<T>` 重解释后调用 = **野调用**。故虚分派在脚本侧**物理不可达**，
 * 而注册表分派（`Register(UScriptStruct*, ...)`）**可达**（§6："步骤系统已经用注册表分派
 * → 只要补一个反射可达的注册入口，C# 就能注册执行器"）。
 *
 * **与步骤执行器同构的额外收益**：TCS 内部对"按类型分派"这个问题本有两套机制
 * （步骤 = 注册表、参数源/选择器/过滤器 = 虚分派）——条件归注册表侧，使新代码不再加深该分裂。
 *
 * **内置条件也走同一注册表**（`HasAllTags` / `Chance` 经宏自登记）——**不分内外两套路径**
 * （两套路径必然导致行为分歧：一处改了另一处忘）。
 *
 * 键 = `const UScriptStruct*`（执行期唯一可得的类型身份是 `FInstancedStruct::GetScriptStruct()`）。
 * 进程级单例（条件是代码而非世界状态），函数局部静态——单游戏线程访问（D0-4）。
 */
class TCSEFFECT_API FTcsTriggerConditionRegistry
{
// 注册
#pragma region Registration

public:
	// 进程级单例（函数局部静态——首次任一路径触达时建立）
	static FTcsTriggerConditionRegistry& Get();

	// 静态自注册入口（注册器构造调用——静态初始化期安全：只写待解析表）
	void AddPending(FTcsTriggerConditionEntry Entry);

	/**
	 * 动态注册入口（脚本层 / 测试装置 / 运行期补登）。
	 * 同类型重复登记拒绝（ensure 提示 + 保留首个登记，不静默覆写）。
	 *
	 * **注（反射面欠账）**：本入口目前是纯 C++ 面（`TFunction` 不可反射）。
	 * 设计意图（D4-17 双入口之反射面）要求它可被脚本层触达——该欠账与步骤执行器注册表
	 * **同批**解决（CS 调研 §7.6 的 G-2），不在此处单独开一个反射入口（否则两处口径不一）。
	 *
	 * @param ConditionStruct 条件 struct 反射类型。
	 * @param Test 求值器。
	 */
	void Register(const UScriptStruct* ConditionStruct, FTcsTriggerConditionTest Test);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按条件反射类型查求值器（首次调用时解析全部待解析登记项）。
	 *
	 * @param ConditionStruct 条件 struct 反射类型。
	 * @return 返回求值器指针；未登记返回 nullptr（**不 ensure**——"未知条件类型"由求值助手
	 *         按"不通过 + Warning"处置）。
	 */
	const FTcsTriggerConditionTest* Find(const UScriptStruct* ConditionStruct);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（调用各 getter 取反射类型 → 建键；幂等，只跑一次）
	void ResolvePending();

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsTriggerConditionEntry> PendingEntries;

	// 已登记求值器（键 = 条件 struct 反射类型）
	TMap<const UScriptStruct*, FTcsTriggerConditionTest> Tests;

	// 待解析项是否已消费
	bool bPendingResolved = false;

#pragma endregion
};



/**
 * 声明一个条件求值器的自注册器（供跨 TU 引用；与 UE_DEFINE_TRIGGER_CONDITION_EVALUATOR 配对）。
 */
#define UE_DECLARE_TRIGGER_CONDITION_EVALUATOR(TestFn) \
	extern FTcsTriggerConditionRegistrar TestFn##ConditionRegistrar;

/**
 * 登记一个条件求值器（仿 GAMEPLAY_TAG 模式——模块静态初始化期自登记，零 StartupModule 样板）。
 * 用法（条件实现 .cpp）：`UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_Chance, TestTriggerConditionChance)`。
 *
 * @param ConditionType 条件数据 struct 类型（取 `ConditionType::StaticStruct` 作反射类型 getter）
 * @param TestFn 求值器函数（签名同 FTcsTriggerConditionTest 的形参表）
 */
#define UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(ConditionType, TestFn) \
	FTcsTriggerConditionRegistrar TestFn##ConditionRegistrar(&ConditionType::StaticStruct, &TestFn);



/**
 * 条件：触发上下文分类 Tag 集须含全部给定 Tag（D4-5 条件最小集之 `HasAllTags`）。
 *
 * **纯数据 struct、零虚函数**（D4-16：步骤/条件都是纯数据；行为经注册表外置）——
 * 这也正是脚本友好的形态（CS 调研 §7.2：纯数据 USTRUCT 可被 C# 定义并挂进数组）。
 *
 * 命名与流程侧的分工：`TcsDamage` 已有 `FTcsConditionHasAllTags`（**流程步骤**用，上下文是
 * `FTcsDamageFlowContext`）。本类型是**触发期**版本（上下文是 `FTcsTriggerContext`）——
 * `TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律），且两者上下文形状本就不同，故各持一份。
 * **两处 MUST NOT 混用**（各自注释互相指名）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsTriggerCondition_HasAllTags
{
	GENERATED_BODY()

	// 要求的 Tag（**全部**命中才通过；空数组 = 无条件通过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger")
	TArray<FGameplayTag> Tags;
};



/**
 * 条件：概率通过（D4-5 条件最小集之 `Chance`）。
 *
 * **确定性纪律的显式例外（D0-1）**：概率型条件天然依赖随机源——故本条件的随机值**由调用方注入**
 * （`EvaluateTriggerConditions` 的 `RandomValue` 入参，取值 [0,1)）；宿主未注入随机源时传固定值
 * （默认 0.0），判定即退化为确定性可复现。**MUST NOT 在求值器内部直接取随机数**
 * （否则同输入不同输出、回放失效）。
 *
 * 与流程侧 `FTcsConditionChance` 的分工同 `FTcsTriggerCondition_HasAllTags`（各持一份，不混用）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsTriggerCondition_Chance
{
	GENERATED_BODY()

	// 通过概率 [0,1]（`RandomValue < Probability` 即通过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Probability = 0.0;
};



/**
 * 触发条件求值助手（D4-5 条件最小集；04 §3 四道门的最后一道）。
 *
 * 分派走 `FTcsTriggerConditionRegistry`（按类型查表）——**宿主新增条件类型零插件改动**
 * （写一个纯数据 struct + 一行自注册宏即可）。
 *
 * 语义：全部条件通过 → `true`；任一不过 → `false`（**短路**——后续条件不再求值）；
 * **未注册的条件类型 → 视为不过 + Warning 日志**（MUST NOT 静默通过：静默会让"条件类型
 * 未注册/写错"表现成"条件通过"，与"静默失败最难查"同源）。
 *
 * @param Conditions 行携带的条件数组（可空 = 无条件通过）。
 * @param Context 触发期上下文（条件按此求值）。
 * @param RandomValue [0,1) 随机值（仅概率类条件用；未注入随机源时传固定值以保证可复现）。
 * @return 返回是否全部条件通过。
 */
bool TCSEFFECT_API EvaluateTriggerConditions(
	const TArray<FInstancedStruct>& Conditions,
	const FTcsTriggerContext& Context,
	double RandomValue = 0.0);
