// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Engine/StreamableManager.h"

#include "Asset/TcsAssetLoadHandle.h"



/**
 * 主资产加载器（裁定①：运行期加载三策略 · 异步默认 + 同步逃生口 + 加载防重）。
 *
 * **异步默认**：`LoadAsync` 走 `UAssetManager::LoadPrimaryAssets`（持 `FStreamableHandle`）。
 * `FStreamableHandle` 的**请求生命周期由引擎管理**，其内部已含"已在内存直接完成"的快路径，
 * 故本类不另测"是否已在内存"——那是引擎的判据，自己再判会造出第二套真相。
 *
 * **同步逃生口**：`LoadSync` 走 `FStreamableManager::LoadSynchronous`，
 * 只给"必须在当帧拿到、且明确知道代价"的调用点（如定义库在启动期一次性装配）。
 *
 * **防重**：同一 `FPrimaryAssetId` 只加载一次 + 句柄计数（见 `FTcsAssetLoadHandle`）。
 *
 * **策略不在此处分支**：`ETcsAssetLoadPolicy` 是**各域对自身类型的预载归属声明**，
 * 消费点是各域的预载决策（"哪些类型该在启动期全部预热"），不是本类的加载路径开关——
 * 本类对三个策略提供同一条加载路径（裁定①："只负责 PrimaryAsset 的运行期加载三方案"，
 * 三方案指**异步 / 同步 / 防重**这三条机制，策略是使用者的选择）。
 *
 * 进程级单例（加载状态不属于任何世界），函数局部静态——单游戏线程访问（D0-4）。
 */
class TCSASSETBUTLER_API FTcsAssetLoader
{
// 单例
#pragma region Singleton

public:
	// 进程级单例
	static FTcsAssetLoader& Get();

#pragma endregion


// 加载
#pragma region Loading

public:
	/**
	 * 异步加载一个主资产（默认路径）。
	 *
	 * @param AssetId 主资产身份。
	 * @param BundleNames 要加载的 bundle 名（空 = 只加载主资产本体）。
	 * @param OnComplete 完成回调（引擎在**下一帧**调用，已加载完成时亦然）。
	 * @return 返回加载句柄（无效 id 时返回空壳）。
	 */
	FTcsAssetLoadHandle LoadAsync(
		const FPrimaryAssetId& AssetId,
		const TArray<FName>& BundleNames = TArray<FName>(),
		FStreamableDelegate OnComplete = FStreamableDelegate());

	/**
	 * 同步加载一个主资产（**逃生口**——阻塞当帧，仅限明确知道代价的调用点）。
	 *
	 * @param AssetId 主资产身份。
	 * @param BundleNames 要加载的 bundle 名。
	 * @return 返回加载到的对象；失败返回 nullptr。
	 */
	UObject* LoadSync(const FPrimaryAssetId& AssetId, const TArray<FName>& BundleNames = TArray<FName>());

#pragma endregion


// 查询
#pragma region Query

public:
	// 当前在册（未释放完）的加载项数（诊断读数用）
	int32 NumTrackedLoads() const;

#pragma endregion


// 句柄侧
#pragma region Handle

public:
	// 释放一次请求（计数 -1；归零摘除在册项并释放底层句柄）——由 FTcsAssetLoadHandle::Release 调用
	void ReleaseRequest(const FPrimaryAssetId& AssetId);

#pragma endregion


// 内核
#pragma region Core

private:
	// 一条在册加载记录（底层句柄 + 请求计数）
	struct FLoadRecord
	{
		// 底层流加载句柄（引擎所有；本类只持引用）
		TSharedPtr<FStreamableHandle> Handle;

		// 当前请求数（归零即可从在册表摘除）
		int32 RefCount = 0;
	};

	// 取或建一条加载记录（同 id 复用——**防重的唯一落点**）
	TSharedPtr<FStreamableHandle> AcquireHandle(const FPrimaryAssetId& AssetId, const TArray<FName>& BundleNames);

	// 在册加载表（键 = 主资产身份）
	TMap<FPrimaryAssetId, FLoadRecord> Loads;

#pragma endregion
};