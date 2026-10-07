// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsSkillEnums.generated.h"



// 施法实例化方式；CI_ = CastInstancing。裸枚举名为已批准的公开契约。
UENUM(BlueprintType)
enum class ECastInstancing : uint8
{
	CI_InstancePerExecution = 0	UMETA(DisplayName = "每次执行一实例", ToolTip = "每次激活创建独立施法运行态"),
	CI_InstancePerEntity = 1	UMETA(DisplayName = "每实体一实例", ToolTip = "同一实体上的该技能复用单一施法运行态"),
};



// 施法状态查询方式；CQM_ = CastQueryMode。
UENUM(BlueprintType)
enum class ECastQueryMode : uint8
{
	CQM_DefSwitches = 0	UMETA(DisplayName = "定义开关", ToolTip = "读取定义上的可打断与可移动开关"),
	CQM_PhaseTable = 1	UMETA(DisplayName = "时段表", ToolTip = "读取当前施法时段的可打断与可移动开关"),
	CQM_Custom = 2		UMETA(DisplayName = "自定义", ToolTip = "由宿主提供的施法查询片段决定"),
};



// 主效果链的启动时点；MCS_ = MainChainStart。
UENUM(BlueprintType)
enum class EMainChainStart : uint8
{
	MCS_OnCastStarted = 0	UMETA(DisplayName = "施法开始", ToolTip = "施法开始时启动主效果链"),
	MCS_OnPhaseEnter = 1		UMETA(DisplayName = "进入指定时段", ToolTip = "进入 MainChainStartPhaseTag 指定的时段时启动主效果链"),
	MCS_OnCastCompleted = 2	UMETA(DisplayName = "施法完成", ToolTip = "施法完成时启动主效果链"),
	MCS_Custom = 3			UMETA(DisplayName = "自定义", ToolTip = "主效果链的启动时点由宿主编排决定"),
};
