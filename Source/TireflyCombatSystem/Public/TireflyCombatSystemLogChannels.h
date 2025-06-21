// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



// 战斗系统通用日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcs, Log, All);

// 战斗系统属性模块日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttribute, Log, All);

// 战斗系统属性修改器执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttrModExec, Log, All);

// 战斗系统属性修改器合并日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttrModMerger, Log, All);

// 战斗系统状态模块日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsState, Log, All);

// 战斗系统状态合并执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsStateMerger, Log, All);

// 战斗系统状态条件执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsStateCondition, Log, All);