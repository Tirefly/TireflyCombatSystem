// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTriggerPayloadReader.generated.h"



/**
 * 载荷主体信息（触发求值期从事件载荷读出的单位、实例主体与分类内容）。
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

	/**
	 * 事件所属的实例主体（运行期反射值；状态事件装 `FTcsStateHandle`）。
	 *
	 * TcsEffect 不认识内层的领域类型：路由先要求主体类型完全相同，再用反射结构比较值。
	 * 空主体表示非实例局部事件（Damage / 未登记读取器的自定义载荷等），维持既有 Tag 路由；
	 * 只有载荷与行都声明主体时才进行实例匹配。该字段与内容侧 `EventPayloadFilter` 无关。
	 */
	UPROPERTY()
	FInstancedStruct Subject;

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
 * 动态登记项的**寿命信息**（2026-09-29，DEC-04 裁定 ⑤；修反射册 R-2 跨世界寿命缺陷）。
 *
 * 与另两张注册表同款：仅**动态**登记需要——它把世界级 GC 对象塞进了**进程级**注册表，
 * 而注册表比世界活得久 ⇒ 必须能判"这条登记还属不属于当前世界"。
 * **静态自注册项不进本表**（属主模块登记的领域读取器全走自注册，永不过期）。
 *
 * 失效判据（任一成立即失效）：① `Object` 弱引用为空（对象已被 GC）；② `World` 弱引用为空（世界已销毁）；③ `World` ≠ 查询方世界。
 */
struct FTcsTriggerPayloadReaderLifetime
{
	// 宿主读取器对象（弱引用——注册表 MUST NOT 强持有）
	TWeakObjectPtr<UObject> Object;

	// 登记时所在的世界（`nullptr` = 无世界上下文，此时跳过世界校验）
	TWeakObjectPtr<const UWorld> World;
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
 * （TcsDamage 的收集事件、TcsState 的生命周期事件读取器各住自己的模块）——
 * TcsEffect 对载荷类型**零硬编码**。
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
	 * 该欠账与条件求值器**同批**、作为反射册 R-2 的独立提案解决（2026-09-29 口径更新：
	 * 原写"三处同批"，因步骤执行器侧已由 SCRIPT-8 先行落地而口径过期）。
	 *
	 * @param PayloadStruct 载荷 struct 反射类型。
	 * @param Reader 读取器。
	 * @param LifetimeObject 宿主读取器对象（弱引用记录；空 = 不做对象寿命校验）。
	 * @param LifetimeWorld 登记时所在的世界（弱引用记录；空 = 不做世界校验）。
	 */
	void Register(
		const UScriptStruct* PayloadStruct,
		FTcsTriggerPayloadRead Reader,
		UObject* LifetimeObject = nullptr,
		const UWorld* LifetimeWorld = nullptr);

	/**
	 * 移除一条**动态**登记（2026-09-29，DEC-04 裁定 ⑤）。静态自注册项 MUST NOT 被移除
	 * ——它们是代码而非登记，本入口对它们返回 false。
	 *
	 * @return 是否真的移除了动态条目。
	 */
	bool Unregister(const UScriptStruct* PayloadStruct);

	/**
	 * 取全部**动态**登记项的键（静态自注册项不在其列）。
	 *
	 * @return 动态登记项的键数组（顺序不保证）。
	 */
	TArray<const UScriptStruct*> GetDynamicKeys() const;

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按载荷反射类型查读取器（首次调用时解析全部待解析登记项）。
	 *
	 * **寿命校验**（2026-09-29，DEC-04 裁定 ⑤）：动态登记项在对象已被 GC、世界已销毁、
	 * 或与传入世界不一致时 MUST 视为未命中——返回 nullptr、移除该条目、留 Warning 日志。
	 * **注意与"未注册载荷类型"的处置区分**：后者走"默认构造 + **Verbose**"（不是契约违规），
	 * 而前者是**世界已更换**——两者 MUST NOT 混为一谈。
	 *
	 * @param PayloadStruct 载荷 struct 反射类型。
	 * @param World 调用方所在的世界；**调用方持有世界时 MUST 传入**。空 = 跳过世界校验。
	 * @return 返回读取器指针；未登记或已失效返回 nullptr（**不 ensure**——"载荷类型未知"由求值器
	 *         按"默认构造 + Verbose"处置，不是契约违规）。
	 */
	const FTcsTriggerPayloadRead* Find(const UScriptStruct* PayloadStruct, const UWorld* World = nullptr);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（调用各 getter 取反射类型 → 建键；幂等，只跑一次）
	void ResolvePending();

	// 记录/覆盖一条动态登记的寿命信息（登记成功后调用）
	void RecordLifetime(const UScriptStruct* PayloadStruct, UObject* LifetimeObject, const UWorld* LifetimeWorld);

	// 既有动态登记是否失效（并移除之）；返回 true 表示"可被替换"
	bool DiscardIfStale(const UScriptStruct* PayloadStruct, const UWorld* World);

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsTriggerPayloadReaderEntry> PendingEntries;

	// 已登记读取器（键 = 载荷 struct 反射类型）
	TMap<const UScriptStruct*, FTcsTriggerPayloadRead> Readers;

	// 动态登记项的寿命信息（键同上；**只含动态项**——静态自注册项不在此表，故永不过期）
	TMap<const UScriptStruct*, FTcsTriggerPayloadReaderLifetime> Lifetimes;

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
