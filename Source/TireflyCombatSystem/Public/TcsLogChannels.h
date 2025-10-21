// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



#pragma region Generic

// 战斗系统通用日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcs, Log, All);

#pragma endregion



#pragma region Attribute

// 战斗系统属性模块日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttribute, Log, All);

// 战斗系统属性修改器执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttrModExec, Log, All);

// 战斗系统属性修改器合并日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttrModMerger, Log, All);

#pragma endregion



#pragma region State

// 战斗系统状态模块日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsState, Log, All);

// 战斗系统状态合并执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsStateMerger, Log, All);

// 战斗系统状态条件执行日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsStateCondition, Log, All);

#pragma endregion



#pragma region Skill

// 战斗系统技能模块日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsSkill, Log, All);

#pragma endregion



#pragma region StateTree

// 战斗系统状态树日志频道
DECLARE_LOG_CATEGORY_EXTERN(LogTcsStateTree, Log, All);

#pragma endregion