// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTriggerPayloadReader.generated.h"



/**
 * 载荷主体信息（触发求值期从事件载荷读出的全部内容）——**载荷能提供的就这些**。
 *
 * **为什么需要它（本类型的立项理由）**：`FTcsTriggerContext` 需要 `Caster` 与 `ClassificationTags`，
 * 但 `TcsEffect` **MUST NOT 认识任何领域载荷类型**（依赖铁律 `Core←Attribute←Effect←{Damage,…}`，
 * `04 §1`）——它不可能写 `GetPtr<FTcsDamageFlowCollectEvent>()`。故"怎么从载荷里读主体信息"
 * 这件事由**载荷类型的属主模块**登记（见 `FTcsTriggerPayloadReaderRegistry`）。
 *
 * **空标签集的语义（显式交付，不是遗漏）**：无读取器时标签集为空集，而
 * `FTcsTriggerCondition_HasAllTags` 在空集上匹配非空 Tag 数组**恒不过**（条件的既有语义）——
 * 故内容侧若要用该条件，MUST 有读取器提供标签集。
 */
USTRUCT()
struct TCSEFFECT_API FTcsTriggerPayloadInfo
{
	GENERATED_BODY()

// 主体
#pragma region Subjects

public:
	// 主体（从载荷解析；无读取器或载荷不含主体时为空句柄）
	UPROPERTY()
	FTcsCombatEntityHandle Caster;

#pragma endregion


// 分类
#pragma region Classification

public:
	// 分类标签集（`HasAllTags` 条件的匹配面；无读取器时为空集）
	UPROPERTY()
	TArray<FGameplayTag> ClassificationTags;

#pragma endregion
};



/**
 * 载荷读取器签名（**与步骤执行器/条件求值器同款形态**——按类型分派走注册表）。
 *
 * 形参：事件载荷（只读）。返回：载荷能提供的主体信息（读不出就返回默认构造）。
 */
using FTcsTriggerPayloadRead = TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct& Payload)>;



/**
 * 待解析登记项：静态初始化期只把"载荷类型 getter + 读取器"挂进表里，**不调用 getter**
 * （静态初始化期触 UObject 是雷区——引擎 `FNativeGameplayTag` 用 `GetIfAllocated()` 规避同款问题；
 * 本仓 `FTcsEffectStepExecutorEntry` / `FTcsTriggerConditionEntry` 同款处置）。
 */
struct FTcsTriggerPayloadReaderEntry
{
	// 载荷类型 getter（通常即 `&FTcsXxxEvent::StaticStruct`；首次查询时才调用）
	UScriptStruct* (*GetPayloadStruct)() = nullptr;

	// 读取器
	FTcsTriggerPayloadRead Reader;
};



/**
 * 静态自注册器（宏展开的载体）：模块静态初始化期构造 → 把本项挂入待解析表。
 * **零 UObject 触达**——反射类型延迟到注册表首次查询时才解析。
 */
struct TCSEFFECT_API FTcsTriggerPayloadReaderRegistrar
{
	FTcsTriggerPayloadReaderRegistrar(UScriptStruct* (*InPayloadStructGetter)(), FTcsTriggerPayloadRead InReader);
};



/**
 * 触发载荷读取器注册表（**按载荷反射类型分派**）：**由载荷类型的属主模块自登记**
 * （TcsDamage 的收集事件读取器住 TcsDamage）——TcsEffect 对载荷类型**零硬编码**。
 *
 * 与另两张注册表的关系（TCS 的"属主自登记"惯例，三处同构）：
 * | 注册表 | 键 | 登记方 |
 * |---|---|---|
 * | `FTcsEffectStepExecutorRegistry` | 步骤 struct | 步骤类型的领域模块 |
 * | `FTcsTriggerConditionRegistry` | 条件 struct | 条件类型的定义方（含宿主） |
 * | `FTcsTriggerPayloadReaderRegistry` | **载荷 struct** | **载荷类型的属主模块** |
 *
 * 键 = `const UScriptStruct*`（求值期唯一可得的类型身份是 `FInstancedStruct::GetScriptStruct()`）。
 * 进程级单例（读取器是代码而非世界状态），函数局部静态——单游戏线程访问（D0-4）。
 */
class TCSEFFECT_API FTcsTriggerPayloadReaderRegistry
{
// 注册
#pragma region Registration

public:
	// 进程级单例（函数局部静态——首次任一路径触达时建立）
	static FTcsTriggerPayloadReaderRegistry& Get();

	// 静态自注册入口（注册器构造调用——静态初始化期安全：只写待解析表）
	void AddPending(FTcsTriggerPayloadReaderEntry Entry);

	/**
	 * 动态注册入口（脚本层 / 测试装置 / 运行期补登）。
	 * 同类型重复登记拒绝（ensure 提示 + 保留首个登记，不静默覆写）。
	 *
	 * **注（反射面欠账）**：本入口目前是纯 C++ 面（`TFunction` 不可反射）。
	 * 该欠账与步骤执行器 / 条件求值器注册表**同批**解决（CS 调研 §7.6 的 G-2），
	 * 不在此处单独开一个反射入口（否则三处口径不一）。
	 *
	 * @param PayloadStruct 载荷 struct 反射类型。
	 * @param Reader 读取器。
	 */
	void Register(const UScriptStruct* PayloadStruct, FTcsTriggerPayloadRead Reader);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按载荷反射类型查读取器（首次调用时解析全部待解析登记项）。
	 *
	 * @param PayloadStruct 载荷 struct 反射类型。
	 * @return 返回读取器指针；未登记返回 nullptr（**不 ensure**——"载荷类型未知"由求值器
	 *         按"默认构造 + Verbose"处置，不是契约违规）。
	 */
	const FTcsTriggerPayloadRead* Find(const UScriptStruct* PayloadStruct);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（调用各 getter 取反射类型 → 建键；幂等，只跑一次）
	void ResolvePending();

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsTriggerPayloadReaderEntry> PendingEntries;

	// 已登记读取器（键 = 载荷 struct 反射类型）
	TMap<const UScriptStruct*, FTcsTriggerPayloadRead> Readers;

	// 待解析项是否已消费
	bool bPendingResolved = false;

#pragma endregion
};



/**
 * 声明一个载荷读取器的自注册器（供跨 TU 引用；与 UE_DEFINE_TRIGGER_PAYLOAD_READER 配对）。
 */
#define UE_DECLARE_TRIGGER_PAYLOAD_READER(ReaderFn) \
	extern FTcsTriggerPayloadReaderRegistrar ReaderFn##PayloadReaderRegistrar;

/**
 * 登记一个载荷读取器（仿 GAMEPLAY_TAG 模式——模块静态初始化期自登记，零 StartupModule 样板）。
 * **由载荷类型的属主模块使用**（如 TcsDamage 为自己发布的收集事件登记）。
 * 用法（属主模块的 .cpp）：`UE_DEFINE_TRIGGER_PAYLOAD_READER(FTcsDamageFlowCollectEvent, ReadDamageFlowCollectPayload)`。
 *
 * @param PayloadType 载荷 struct 类型（取 `PayloadType::StaticStruct` 作反射类型 getter）
 * @param ReaderFn 读取器函数（签名同 FTcsTriggerPayloadRead 的形参表）
 */
#define UE_DEFINE_TRIGGER_PAYLOAD_READER(PayloadType, ReaderFn) \
	FTcsTriggerPayloadReaderRegistrar ReaderFn##PayloadReaderRegistrar(&PayloadType::StaticStruct, &ReaderFn);
