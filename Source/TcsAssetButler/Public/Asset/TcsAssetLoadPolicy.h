// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsAssetLoadPolicy.generated.h"



/**
 * 主资产加载策略（裁定①：运行期加载三策略）。
 *
 * **本枚举只声明策略归属，不驱动扫描**——扫描归属由 UTcsAssetManager::StartInitialLoading()
 * 统一决定（全部注册项一律登记扫描路径，与策略无关）；策略的消费面是 FTcsAssetLoader
 * 与各域的预载决策点。
 *
 * **一行注释的教训（2026-10-15 实测）**：枚举值**上方**的 `//` 注释会被 UHT 自动登记为
 * 该值的 `ToolTip` 元数据；此处再显式写 `ToolTip` 即撞 UHT 的元数据一致性检查
 * （`Error: Metadata key 'ToolTip' first seen with value '…' then '…'`）。
 * ⇒ 说明文字 MUST 只出现在一处（本例取显式 `ToolTip`）。
 */
UENUM()
enum class ETcsAssetLoadPolicy : uint8
{
	PreloadAll UMETA(DisplayName = "预载全部", ToolTip = "世界/游戏启动期即把该类型下的全部主资产加载进内存；适用于数量少且全程必需的定义（如属性定义）。"),

	PreloadSelected UMETA(DisplayName = "预载选定", ToolTip = "只加载注册项显式列出的那些主资产；适用于体量大、按关卡或模式挑选的定义族。"),

	OnDemand UMETA(DisplayName = "按需加载", ToolTip = "不预载，首次按 FPrimaryAssetId 解析时加载；适用于体量大且非全程必需的资产。")
};
