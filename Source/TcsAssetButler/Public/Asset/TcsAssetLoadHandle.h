// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"



/**
 * 主资产加载句柄（**句柄计数**的载体——裁定① 的"加载防重"）。
 *
 * 语义：一个 `FPrimaryAssetId` 在进程内**只加载一次**；每次 `FTcsAssetLoader::LoadAsync`
 * 请求同一 id 时**复用**同一条底层 `FStreamableHandle` 并把本计数 +1，
 * 释放时 -1；归零才真正释放底层句柄。
 *
 * **为什么自己做计数**：`FStreamableHandle` 自身没有公开的引用计数查询，
 * 而多个消费者（各域定义库、将来的按需加载点）会各自请求同一份定义资产——
 * 谁先释放都不会打断别人。
 */
struct TCSASSETBUTLER_API FTcsAssetLoadHandle
{
	// 受管的主资产身份（无效 = 本句柄为空壳）
	FPrimaryAssetId AssetId;

	// 释放本句柄（计数 -1；归零才真正释放底层句柄）
	void Release();

	// 本句柄是否有效（持有过加载请求）
	bool IsValid() const;
};
