// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Flow/TcsDamageFlowContext.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsDamageRecord.h"
#include "Flow/TcsFlowStepExecutorObject.h"
#include "Flow/TcsFlowTemplate.h"
#include "Handle/TcsSourceHandle.h"

#include "TcsDamageSubsystem.generated.h"



/**
 * 伤害流程门面与解释器（M9 流程管线宿主化 D7-5）：世界级子系统，持**流程模板登记表**。
 *
 * 分工：本模块 = 机制层（流程解释器 + 步骤注册表 + 流程属性黑板 + 收集事件协议）；
 * **流程阶段构成 = 项目知识**——插件只交付标准步骤库与官方默认模板（Task 4），
 * 模板由宿主/内容组装并登记。
 *
 * 非 Tickable：流程**单帧同步完成、无挂起**（09 §3）——不推进任何状态、零每帧成本。
 */
UCLASS()
class TCSDAMAGE_API UTcsDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 初始化：登记**官方默认模板**（`Default`：CollectStart → BaseDamage → Execute → Completed，D7-5 可整表替换）
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化（对齐时钟/总线/属性/效果链门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 反初始化：确定性清空模板登记表与记录缓冲（模板自持数据，无外部资源待释放）
	virtual void Deinitialize() override;

	/**
	 * GC 引用收集（**模板登记表的 GC 可见持有**，2026-09-23 修复 WAIT-8）。
	 *
	 * 为什么必须自己实现：`Templates` 是 `TMap<FGameplayTag, TUniquePtr<FTcsFlowTemplate>>`——
	 * **裸 C++ 容器不经 GC 的 `RefLink`**，`TUniquePtr` 更不是 GC 可见持有，故 GC 看不见
	 * 模板步骤里的对象引用（`FTcsFlowModify::Delegate` / `FTcsFlowDelegate::Delegate` 等
	 * `TScriptInterface<ITcsDamageFlowDelegate>`）。后果是**静默 GC**：delegate 被回收后
	 * 流程跑到该步取到空接口，表现为"公式不生效"而非崩溃，极难排查。
	 *
	 * 手法 = 逐个模板调 `FReferenceCollector::AddPropertyReferencesWithStructARO`
	 * （引擎 `UDataTable::AddReferencedObjects` 对 `RowMap` 用的同一招，`DataTable.cpp:300`）——
	 * 它递归走步骤 struct 的反射属性，且对带 `WithAddStructReferencedObjects` 的
	 * `FInstancedStruct` 会**继续递归进内层实例内存**（`InstancedStruct.cpp:506`），
	 * 故 `TArray<FInstancedStruct>` 里任意深度的对象引用都能被保活。
	 *
	 * 登记时机不受影响：本函数在 GC 期间被调，与 `RegisterTemplate` 的登记顺序无关。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#pragma endregion


// 模板登记表
#pragma region Template

public:
	/**
	 * 登记流程模板（键 = `Template.TemplateId`）。
	 * 拒绝面（ensure 提示 + 返回 false）：TemplateId 为空、同 id 重复登记（不得静默覆写）。
	 *
	 * **反射面（2026-09-24，台账 SCRIPT-8）**：`UFUNCTION()` **无 specifier** 是有意的——形参
	 * `FTcsFlowTemplate` 是 `USTRUCT()` 非 `BlueprintType`，加 `BlueprintCallable` 会被 UHT
	 * 的蓝图参数校验拒；无 specifier 时 UHT 不校验参数，而宿主脚本层照常可达（口径同
	 * `UTcsEffectSubsystem` 的 SCRIPT-1 批次）。蓝图侧不可见属接受项（R0 §9"蓝图不承诺"）。
	 *
	 * @param Template 流程模板（按值拷入登记表；TUniquePtr 持有使执行期持有的引用不随登记表增长而悬空）。
	 * @return 返回是否登记成功。
	 */
	UFUNCTION()
	bool RegisterTemplate(const FTcsFlowTemplate& Template);

	/**
	 * 注销流程模板（未登记时 Warning + false，不 ensure——正常清理路径）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier（理由同 `RegisterTemplate`）。
	 *
	 * @param TemplateId 模板 id。
	 * @return 返回是否注销成功。
	 */
	UFUNCTION()
	bool UnregisterTemplate(FGameplayTag TemplateId);

	/**
	 * 登记**宿主脚本流程步骤执行器**（2026-09-24，台账 SCRIPT-8 的流程侧插槽）。
	 *
	 * 内部把 `Executor` 包成 `TFunction` 转发进既有流程步骤注册表——**键与查表逻辑零改动**，
	 * C++ 静态自注册宏路径（`UE_DEFINE_FLOW_STEP_EXECUTOR`）原样保留（**双轨并存**）。
	 *
	 * **为什么需要它**："流程阶段构成 = 项目知识"（D7-5）——宿主自研阶段是最典型的"客制化、
	 * 只服务宿主业务、不值得进插件"的语义；而既有注册值 `TFunction` 不可反射（脚本登记不了）。
	 *
	 * **GC 可见持有**：本函数把 `Executor` 加进 `RegisteredFlowStepExecutors`（`UPROPERTY` 数组）——
	 * 同 `Templates` 的 WAIT-8 教训（裸容器持不住对象引用 ⇒ 静默回收 ⇒ "步骤不生效"而非崩溃）。
	 *
	 * 拒绝面（配置错误 → ensure + 返回 false）：`StepStruct` 为空、`Executor` 为空、
	 * 同类型重复登记（由注册表判定，保留首个）。
	 *
	 * **反射面**：`UFUNCTION()` 无 specifier（口径同 `RegisterTemplate`）。
	 *
	 * @param StepStruct 步骤 struct 的反射类型（注册表键）。
	 * @param Executor 宿主执行器（须为 `UTcsFlowStepExecutor` 派生）。
	 * @return 返回是否登记成功。
	 */
	UFUNCTION()
	bool RegisterStepExecutor(const UScriptStruct* StepStruct, UTcsFlowStepExecutor* Executor);

	/**
	 * 查询流程模板（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * @param TemplateId 模板 id。
	 * @return 返回模板；未登记返回 nullptr。
	 */
	const FTcsFlowTemplate* FindTemplate(FGameplayTag TemplateId) const;

#pragma endregion


// 执行
#pragma region Execution

public:
	/**
	 * 执行流程：按模板顺序**同步单帧**走完（无挂起、无延迟步骤）。
	 * 起流程时分配 `Context.FlowSource`（每流程唯一——作用域修改器的级联摘除锚点）。
	 * 中止语义：未登记模板 / 未知步骤类型 / 步骤执行器返回 false → 中止剩余步骤 + Error/Warning
	 * （已产生的副作用不回滚——止于未来）；模板只读，执行 MUST NOT 改写共享模板数据。
	 *
	 * @param TemplateId 模板 id。
	 * @param Context 流程上下文（调用方提供；本函数就地使用，不持引用跨帧）。
	 * @return 返回流程是否走完全部步骤。
	 */
	bool RunTemplate(FGameplayTag TemplateId, FTcsDamageFlowContext& Context);

#pragma endregion


// 收集事件协议
#pragma region Collect

public:
	/**
	 * 广播收集事件（步骤边界调用）：经总线**立即通道**同步派发——响应方在返回前收到，
	 * 可在回调内 `Submit` 黑板修正（**收集 ≠ 消费**，D7-4：消费归 Execute 裁决步骤）。
	 * 载荷为**上下文指针包装**（进程内瞬态，MUST NOT 跨帧持有）。
	 *
	 * @param EventTag 收集事件 Tag（原生声明，`Tcs.Event.Damage.*`）。
	 * @param Context 流程上下文（派发期有效）。
	 */
	void PublishCollectEvent(FGameplayTag EventTag, FTcsDamageFlowContext& Context);

#pragma endregion


// 内核
#pragma region Core

public:
	/**
	 * 追加一条伤害记录（`Completed` 步骤调用）：发号序号 → 写环形缓冲 → 经总线**立即通道**
	 * 发布原生事件 `Tcs.Event.Damage.Recorded`（载荷 = 记录本身；记录是结果快照，消费方无须上下文）。
	 *
	 * @param Record 记录（序号与时刻若为空由本函数补齐——步骤只填业务字段）。
	 */
	void AppendRecord(FTcsDamageRecord& Record);

	/**
	 * 读回最近记录（**旧 → 新序**；用于统计/回放/装置断言）。
	 *
	 * @param OutRecords 输出数组（先清空）。
	 */
	void GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const;

private:
	// 模板登记表（键 = TemplateId；TUniquePtr 地址稳定持有）
	TMap<FGameplayTag, TUniquePtr<FTcsFlowTemplate>> Templates;

	// 宿主脚本流程步骤执行器（**UPROPERTY 持有是必需的**：流程步骤注册表是裸 C++ 容器，
	// 不经 GC 的 RefLink——不持有则脚本执行器被静默回收，表现为"步骤不生效"而非崩溃。
	// 与 Templates 的 WAIT-8 修复同款形态）
	UPROPERTY()
	TArray<TObjectPtr<UTcsFlowStepExecutor>> RegisteredFlowStepExecutors;

	// 流程来源发号器（每流程唯一 FlowSource）
	FTcsSourceHandleRegistry FlowSourceRegistry;

	// 记录环形缓冲（容量 = GTcsDamageRecordBufferSize；旧→新序读回）
	TArray<FTcsDamageRecord> RecordBuffer;

	// 记录序号发生器（单调递增——回放依赖序）
	uint64 NextRecordSequence = 0;

#pragma endregion
};
