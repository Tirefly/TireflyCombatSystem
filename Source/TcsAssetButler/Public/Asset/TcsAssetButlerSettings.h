// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"

#include "Asset/TcsAssetLoadPolicy.h"

#include "TcsAssetButlerSettings.generated.h"



/**
 * 一组主资产身份（`TMap` 值侧的**包装壳**，2026-10-26）。
 *
 * **为什么需要这个壳**：UHT 硬拒「容器作 `TMap` 的值」——
 * `TMap<FPrimaryAssetType, TArray<FPrimaryAssetId>>` 会直接编译失败
 * （`Error: The type 'TArray<FPrimaryAssetId>' can not be used as a value in a TMap`）。
 * 包一层 `USTRUCT` 是引擎环境下表达"类型 → 一组身份"的标准形态，
 * **语义与直接嵌套完全一致**：项目设置里仍按类型分组显示、逐个加资产。
 *
 * MUST NOT 因为"多了一层壳"就把它拆成扁平数组——扁平表会让同一类型重复填键，
 * 填错时不报错、只是静默少加载几个资产。
 */
USTRUCT(BlueprintType)
struct TCSASSETBUTLER_API FTcsAssetIdList
{
	GENERATED_BODY()

	// 该类型下要预载的主资产身份
	UPROPERTY(Config, EditAnywhere, Category = "Tcs|Asset",
		Meta = (DisplayName = "选定的主资产"))
	TArray<FPrimaryAssetId> AssetIds;
};



/**
 * 资产加载层设置（裁定① 的"编辑器期间 PrimaryAsset 的相关机制"）。
 *
 * 在项目设置中呈现为独立 "Tcs" 分类（与 UTcsDeveloperSettings 同款）。
 *
 * **★ 红线（MUST 读）**：本类的字段**MUST NOT 影响模拟语义**——
 * 它们只表达"资产怎么被找到、怎么被预热"这类**编辑器/加载期**规则。
 * 一旦某个值能改变战斗结果（如伤害公式系数、堆叠上限），它就该住**对应域的定义资产**，
 * 而不是这里：设置是**进程级单例**、无版本、不随内容回滚，把它接进模拟语义
 * 会让"同一份内容在不同机器上算出不同结果"。
 *
 * **策略默认值住域、覆盖点住此**（2026-10-26 用户裁定）：本类的 `LoadPolicyByType` 是**覆盖面**，
 * 未列出的类型回落该域 `UE_DEFINE_PRIMARY_ASSET_TYPE` 宏的声明值。**MUST NOT** 在此再设一个
 * 「全局默认策略」字段——那会让宏里的值变成看得见、不起作用的假真相（`grep` 到的是谎报）。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tcs Asset Butler"))
class TCSASSETBUTLER_API UTcsAssetButlerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

// 开发者设置分类
#pragma region Config

public:
	// 项目设置分类名：与 UTcsDeveloperSettings 同住 "Tcs" 分类
	virtual FName GetCategoryName() const override;

#pragma endregion


// 扫描规则
#pragma region ScanRules

public:
	/**
	 * 从扫描路径中排除的目录前缀（宿主可改）。
	 *
	 * 用途：宿主把**测试/临时**内容放在某目录下时，用它把该目录整片排除出主资产扫描
	 * （避免测试资产的类型注册进正式类型表）。**前缀匹配**，不是精确路径。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Scan",
		Meta = (DisplayName = "扫描排除目录前缀"))
	TArray<FString> ExcludedScanPathPrefixes;

#pragma endregion


// 加载策略覆盖
#pragma region LoadPolicy

public:
	/**
	 * 按 Def 类型覆盖加载策略（**覆盖，不是取代**）。
	 *
	 * 未列出的类型回落到该类型在 `UE_DEFINE_PRIMARY_ASSET_TYPE` 里的声明值——
	 * **域最懂自己的默认值**，设置只做宿主级覆盖。这是本仓既有的第三态「契约默认」。
	 *
	 * **键是 `FPrimaryAssetType`**（用户 2026-10-26 裁定第 4 条）：与 `UAssetManager` 的
	 * `AssetTypeMap` 同键，宿主在编辑器里选类型时与项目设置里看到的类型名一致。
	 *
	 * **★ 本字段值 MUST NOT 让「装配型类型」退化成 `OnDemand`** —— 定义库启动期会硬拒
	 * （计入失败清单 + Error + 不就绪），理由见 `ETcsAssetLoadPolicy` 头注释的「装配型禁令」段。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "LoadPolicy",
		Meta = (DisplayName = "按类型的加载策略覆盖"))
	TMap<FPrimaryAssetType, ETcsAssetLoadPolicy> LoadPolicyByType;

	/**
	 * `PreloadSelected` 档的选定清单（**补 `ARCH-03` `U-5` 的缺口**）。
	 *
	 * 值用 `FPrimaryAssetId` 而非路径：与加载口 `LoadPrimaryAssets` 的入参一致，
	 * 且 `FPrimaryAssetId` 本就是资产的规范身份（资产文件可改名/挪目录而不失联）。
	 *
	 * **清单为空的后果**：该类型按「零资产预载」处理并留**具名 Warning**——
	 * MAY NOT 静默当作"已预载完成"，MAY NOT 退化为 `PreloadAll`（那会让"我配的清单没生效"
	 * 表现为"怎么全量加载了"，与「假策略」同罪）。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "LoadPolicy",
		Meta = (DisplayName = "按类型的选定预载清单"))
	TMap<FPrimaryAssetType, FTcsAssetIdList> SelectedAssetsByType;

#pragma endregion


// 生效策略
#pragma region EffectivePolicy

public:
	/**
	 * 取某类型的**生效策略**：`LoadPolicyByType` 有该键则用它，否则回落 `Fallback`（即宏声明值）。
	 *
	 * 判定逻辑只此一处——定义库与任何未来的消费方 MUST 调它，MUST NOT 各自再写一遍
	 * （两处判据 = 两套真相）。
	 *
	 * @param Type 主资产类型。
	 * @param Fallback 该类型在 `UE_DEFINE_PRIMARY_ASSET_TYPE` 里的声明值。
	 * @return 返回生效策略。
	 */
	ETcsAssetLoadPolicy ResolveLoadPolicy(FPrimaryAssetType Type, ETcsAssetLoadPolicy Fallback) const;

#pragma endregion
};
