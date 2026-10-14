// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/PrimaryAssetId.h"

#include "Asset/TcsAssetLoadPolicy.h"



/**
 * 主资产注册项（**类型擦除**——裁定③）。
 *
 * 本结构是各域向加载层登记自己的 Def 资产族的**唯一载体**，它只收引擎级词汇：
 * `FPrimaryAssetType` + `UClass*` + 扫描路径 + 策略 + 两个引擎扫描开关。
 * **MUST NOT** 认识任何业务 Def 类型——否则本模块就必须住到那些类型之上，
 * 各域（含层级最低的 `TcsAttribute`）无法再向它**向下注册**（本会话已实测该环）。
 *
 * **为什么基类延迟解析**（存 getter 而非直接存 `UClass*`）：
 * 登记发生在各域 .cpp 的**静态初始化期**（`UE_DEFINE_PRIMARY_ASSET_TYPE` 展开的注册器构造），
 * 该时点 UObject 设施未就绪——引擎 `FNativeGameplayTag` 用同款手法规避（只存 getter、首次查询才调用）。
 * 解析后 `Type` / `BaseClass` 被填充，字段形态与 Interfaces 声明一致。
 */
struct TCSASSETBUTLER_API FTcsPrimaryAssetEntry
{
	// 基类 getter（通常即 `&UMyDef::StaticClass`；**静态初始化期只存指针，不调用**）
	UClass* (*GetBaseClass)() = nullptr;

	// 扫描路径（主资产的搜索根；引擎按 FString 路径寻资产，与包路径同形）
	TArray<FString> ScanPaths;

	// 加载策略（本结构只声明归属；扫描归属由 UTcsAssetManager 统一决定，与策略无关）
	ETcsAssetLoadPolicy Policy = ETcsAssetLoadPolicy::OnDemand;

	// 是否含蓝图子类（引擎扫描开关，透传 ScanPathsForPrimaryAssets）
	bool bHasBlueprintClasses = false;

	// 是否仅编辑器有效（引擎扫描开关；非编辑器环境该类型被整条跳过）
	bool bIsEditorOnly = false;

	// 主资产类型标识（**解析后有效**——解析期由 BaseClass 的反射名派生，零字符串面）
	FPrimaryAssetType Type;

	// 基类（**解析后有效**）
	UClass* BaseClass = nullptr;

	// 本项是否已解析成功（区分"未解析"与"解析失败"——后者 BaseClass 仍为空但已消费）
	bool bResolved = false;
};



/**
 * 静态自注册器（宏展开的载体）：模块静态初始化期构造 → 把本项挂入待解析表。
 * **零 UObject 触达**——反射类型延迟到注册表首次查询时才解析。
 */
struct TCSASSETBUTLER_API FTcsPrimaryAssetRegistrar
{
	FTcsPrimaryAssetRegistrar(FTcsPrimaryAssetEntry InEntry);
};



/**
 * 登记一个主资产类型（各域在自己的 .cpp 里自注册，**零 ini、零字符串面**）。
 *
 * 用法（Def 资产类实现文件，紧随 `PrimaryAssetType` 定义之后）：
 * ```
 * UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsAttributeDef, { TEXT("/Game/TcsDev") }, ETcsAssetLoadPolicy::PreloadAll)
 * ```
 *
 * 形态照既有四枚 `UE_DEFINE_*` 一族（`UE_DEFINE_EFFECT_STEP_EXECUTOR` 等）：
 * 静态注册器 + 构造期登记。**类型名由 `StaticClass()` 反射派生**，故改名时本处无需同步修改。
 *
 * @param DefClass 主资产基类（取 `DefClass::StaticClass` 作基类 getter）
 * @param ScanPathsInit 扫描路径初值（`TArray<FString>` 的初始化列表，如 `{ TEXT("/Game/X") }`）
 * @param Policy 加载策略
 */
#define UE_DEFINE_PRIMARY_ASSET_TYPE(DefClass, ScanPathsInit, Policy) \
	static FTcsPrimaryAssetRegistrar DefClass##PrimaryAssetRegistrar( \
		FTcsPrimaryAssetEntry{ &DefClass::StaticClass, ScanPathsInit, Policy, false, false, FPrimaryAssetType(), nullptr, false });