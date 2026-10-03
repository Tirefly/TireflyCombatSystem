// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "Trigger/TcsEffectTrigger.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsEffectTriggerDefAsset.generated.h"



/**
 * 触发定义资产（**系统级规则的内容载体**）：一条"全局常驻、与任何 Def 无关"的触发规则一个资产
 * （如"任何单位死亡时触发某链"）——供策划在编辑器里**零 C++** 创作触发规则。
 *
 * **类名为何带 `Asset` 后缀（2026-10-04 UHT 实证，本族的明示例外）**：Def 资产族命名标准是
 * `<Family>Def`（不带后缀），但本族的 `<Family>Def` 这个名字**已被 Task 1 交付的数据 struct
 * `FTcsEffectTriggerDef` 占用**，而 UHT 按"去前缀后的引擎名"判重——实测报错：
 * `Error: Class 'UTcsEffectTriggerDef' shares engine name 'TcsEffectTriggerDef' with struct
 * 'FTcsEffectTriggerDef'`（UHT 阶段即失败，不是编译期）。故本类取 `Asset` 后缀消歧，
 * 标准条文已补"`<Family>Def` 已被数据类型占用时，资产类加 `Asset` 后缀"的限定语（`openspec/project.md`）。
 *
 * **与内联位的关系**：Buff / Skill 的行为走**内联**（`TArray<FTcsEffectTriggerDef>` 进各自的 Def，
 * 施加时登记、`Source` = 状态实例句柄）——那两个 Def 随 M3/M5 落地时自然成立；本类只承载
 * **独立资产**这一条载体（定义类型本身已是纯配置，故可直接内联）。
 *
 * **与 `FTcsEffectTriggerInstance` 的分层**（2026-09-23 用户拍板）：本资产持**定义侧**（纯配置、
 * 零运行期字段 ⇒ 可入库、可复制）；运行期簿记（`Source` / `Self`）只住实例。
 *
 * **谁登记它**：`UTcsDefinitionSubsystem` 在发现期校验并缓存、在**世界装配期**把它登记成该世界的
 * 触发行（`Source` = 定义库来源句柄）——这是 `effect-trigger` 规格"**定义加载期登记**（全局常驻规则）
 * → `Source` = 系统/DefLibrary 来源句柄"那条的实现落点。**只发现不登记 = 资产零消费者**（策划填了不生效）。
 */
UCLASS(BlueprintType)
class TCSINTEGRATION_API UTcsEffectTriggerDefAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	/**
	 * 主资产类型标识（Def 资产族统一约定，2026-09-17）：**显式声明而非靠类名派生**——值取类名
	 * （族内一致：`UTcsAttributeDef` → `"TcsAttributeDef"`、`UTcsEffectChainDef` → `"TcsEffectChainDef"`；
	 * 本类 → `"TcsEffectTriggerDefAsset"`）。注册到 AssetManager 的 `PrimaryAssetTypesToScan` 属 M6 轮
	 * （不注册也能解析 id；当前发现走 `IAssetRegistry::GetAssetsByClass`，见台账 INTEG-3）。
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 触发身份（= 定义库的解析键与去重键；**内容身份**，不是资产名。词住 `EffectTriggerDef` 根）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	FGameplayTag TriggerTag;

public:
	/**
	 * 覆写主资产身份：**名取 `TriggerTag`**（不取资产名）——于是资产文件叫什么、放哪个目录，
	 * 都与 `[PrimaryAssetType, TriggerTag]` 解析无关。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, TriggerTag]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 定义数据
#pragma region Definition

public:
	/**
	 * 触发定义数据（纯配置：`EventTag` / `Conditions` / `EffectChainId` / `Priority` /
	 * `ExecutionGate` / `InterruptPriority` / `GateTags` / `bConditionMissIsSilent` / `EventPayloadFilter`）。
	 *
	 * **无 `BlueprintReadOnly`**：本字段的读取面是定义库的 `ResolveTriggerDef`（C++ 面）；
	 * 细节面板可编辑性只看 `CPF_Edit`（`PropertyEditorHelpers.cpp:407`），与 BlueprintType 无关，
	 * 故 `EditAnywhere` 已足够（与链资产的 `Chain` 字段同款取舍）。
	 */
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger")
	FTcsEffectTriggerDef Def;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验（三条必填，均为登记/触发的硬前置）：
	 * - `TriggerTag` 无效 → Invalid（资产无法按 `[PrimaryAssetType, TriggerTag]` 解析，且不会被登记）；
	 * - `Def.EventTag` 无效 → Invalid（触发行按事件 Tag 路由，空 Tag 无从订阅）；
	 * - `Def.EffectChainId` 无效 → Invalid（命中后无链可执行）。
	 * 三者都会让 `RegisterTriggerRow` 走 ensure 拒绝面，故 MUST 在编辑器提前报出（不留给运行期红字）。
	 *
	 * **无错时把 `NotValidated` 提升为 `Valid`**：`UObject::IsDataValid` 基类默认返回 `NotValidated`
	 * （`Obj.cpp:6096`），不提升会让"已校验通过"在编辑器里显示为"未验证"，且装置断言 `== Valid`
	 * 必然失败。链资产缺这一段属台账 `TOOLS-2` ③（校验矩阵轮），本类不复制该缺陷。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误；无错 = Valid）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
