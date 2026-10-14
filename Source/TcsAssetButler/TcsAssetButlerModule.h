// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"



/**
 * TcsAssetButler 模块类：资产加载层（PrimaryAsset 机制）的模块壳。
 *
 * 职责边界（裁定① 用户原话）：只承载 PrimaryAsset 的类型注册表 / 运行期加载三策略 /
 * 异步默认 + 同步逃生口 / 加载防重 / 编辑器期规则，**不再负责其他任何内容**。
 * 本类空体、零注册——登记发生在各域 .cpp 的静态初始化期（UE_DEFINE_PRIMARY_ASSET_TYPE）。
 */
class FTcsAssetButlerModule : public IModuleInterface
{
};
