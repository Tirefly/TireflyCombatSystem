// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsResolveMode.generated.h"



/**
 * 定义解析的加载模式（用户 2026-10-26 裁定第 13/14 条）。
 *
 * **默认是 `Async`**：插件默认路径必须异步——插件无法预知宿主资产规模，同步会阻塞游戏线程。
 * `Sync` 是**逃生口**，只给「必须在当帧拿到、且明确知道代价」的调用点，调用点 MUST 各自说明代价。
 *
 * **两者对「已在内存」是同一行为**：都会直接返回资产指针。差别只在「不在内存」时——
 * `Async` 返回 `nullptr` 并登记加载（完成时触发调用方的 `FStreamableDelegate`），
 * `Sync` 阻塞当帧加载后返回。MUST NOT 把 `Async` 读成「永远返回 nullptr」：
 * 那会强行给已在内存的资产加一帧延迟，是纯粹的损失。
 */
UENUM(BlueprintType)
enum class ETcsResolveMode : uint8
{
	Async UMETA(DisplayName = "异步（默认）", ToolTip = "不在内存时返回 nullptr 并登记一次性加载，加载完成后触发调用方给的代理；已在内存则直接返回指针并同步触发代理。"),

	Sync UMETA(DisplayName = "同步逃生口", ToolTip = "阻塞当帧同步加载后直接返回资产指针。只给必须在当帧拿到、且明确知道代价的调用点。")
};
