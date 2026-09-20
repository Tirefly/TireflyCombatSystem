// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsStepWaitDelay.generated.h"



/**
 * 等待步骤（D4-16 本模块 9 个原语之一；挂起-恢复协议的首个实证载体）。
 *
 * 语义：本步在**战斗时钟**（FTcsClock.Elapsed 时轴）上等待 `Seconds` 秒——首入即入到期堆并返回
 * `TSR_Running`（PC 停驻本步、零每帧成本），到期由时钟泵回调唤醒重入，本步随即判为完成、链续走。
 * 时间基准是战斗时钟（ScaledDt）而非实时：`slomo` 减速时等待同比例拉长、`pause` 冻结期间不兑现。
 *
 * 执行器住 `Private/Chain/TcsStepWaitDelay.cpp`（+ 一行宏自注册）——它是"步骤类型 + 执行器 + 自登记"
 * 的最小完整样本，领域模块（TcsDamage/TcsTargeting）的步骤照此形状办理。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepWaitDelay
{
	GENERATED_BODY()

	// 等待时长（战斗时间秒；负值按 0 处理）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|WaitDelay", meta = (ClampMin = "0.0"))
	double Seconds = 0.5;
};
