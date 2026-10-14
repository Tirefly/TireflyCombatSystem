// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Asset/TcsPrimaryAssetEntry.h"



/**
 * 主资产类型注册表（**类型擦除**——裁定③）。
 *
 * 进程级单例（主资产类型是代码而非世界状态），函数局部静态——单游戏线程访问（D0-4）。
 *
 * **唯一性判据 MUST 对齐引擎**（裁定 E5 / 验收红线③）：引擎在
 * `UAssetManager::ScanPrimaryAssetTypesFromConfig` 对同一 `PrimaryAssetType` 重复出现报
 * `Error`，并明言 "Only a single entry per type is supported"。
 * 本注册表 **MUST NOT** 自建第二套判据——它就是同一句话的实现，报错文案亦对齐。
 */
class TCSASSETBUTLER_API FTcsPrimaryAssetRegistry
{
// 注册
#pragma region Registration

public:
	// 进程级单例（函数局部静态——首次任一路径触达时建立）
	static FTcsPrimaryAssetRegistry& Get();

	// 登记入口（静态自注册器构造调用；也供运行期补登——两条路径同一套唯一性判据）
	void Register(FTcsPrimaryAssetEntry Entry);

	/**
	 * 撤销一个类型的登记。
	 *
	 * @param Type 主资产类型。
	 * @return 是否真的移除了该类型（含其解析结果）。
	 */
	bool Unregister(const FPrimaryAssetType& Type);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 遍历全部**已解析**的注册项（首次调用时解析全部待解析项）。
	 * 解析失败的项不进遍历面（已在解析期逐条 Error，不静默保留半成品）。
	 *
	 * @param Visitor 逐项访问回调。
	 */
	void ForEach(TFunctionRef<void(const FTcsPrimaryAssetEntry&)> Visitor);

	// 已解析注册项数（就绪日志与装置读数用；首次调用会触发解析）
	int32 Num();

	// 按类型查注册项（未登记返回 nullptr）
	const FTcsPrimaryAssetEntry* Find(const FPrimaryAssetType& Type);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析一条登记项（调用 getter 取基类 → 由反射名派生类型名；失败则留 Error 并标记未解析）
	void ResolveEntry(FTcsPrimaryAssetEntry& Entry);

	// 把一条已解析项加入注册表（唯一性判据在此——同类型重复即 Error 并保留首个）
	void AddResolved(FTcsPrimaryAssetEntry Entry);

	// 解析全部待解析登记项（幂等，只跑一次）
	void ResolvePending();

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsPrimaryAssetEntry> PendingEntries;

	// 已解析注册项（键 = 主资产类型）
	TMap<FPrimaryAssetType, FTcsPrimaryAssetEntry> Entries;

	// 待解析项是否已消费
	bool bPendingResolved = false;

#pragma endregion
};
