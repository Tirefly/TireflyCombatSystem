// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepRunSubChain.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"



namespace
{
	/**
	 * 子链原语执行器：取齐数据 → 交给门面侧的**共用实现**（起链 + 按需挂起 + 唤醒配平）。
	 *
	 * **步内起链纪律（D-4 的落点）**：本执行器收到的 `Context` / `Run` 是运行态**池元素**的引用，
	 * 而起链可能使池扩容搬移 ⇒ 起链**前**把要用的数据全取成局部值（链 id / bWait / 自身句柄），
	 * 起链**后 MUST NOT 再触碰** `StepData` / `Context` / `Run`。故本函数一次性取齐后立即转交门面，
	 * 门面按**句柄**自行取黑板快照——从头到尾没有引用被递到起链之后。
	 */
	ETcsStepResult ExecuteStepRunSubChain(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepRunSubChain* Step = StepData.GetPtr<FTcsStepRunSubChain>();
		UTcsEffectSubsystem* Facade = Run.Owner.Get();
		if (!Step || !Facade)
		{
			// 载荷类型不符 / 门面已回收（世界拆解期）：按完成处理（不挂起——挂起需要可靠的唤醒源）
			UE_LOG(LogTcsEffect, Warning, TEXT("RunSubChain[%s]: 步骤载荷类型不符或门面不可得——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 取齐数据（值拷贝）：此后不得再用 `Step` / `Context` / `Run`
		const FGameplayTag ChainId = Step->ChainId;
		const bool bWait = Step->bWait;
		const FTcsChainRunHandle Self = Run.Self;
		const FString SelfChainName = Run.ChainId.ToString();

		// 空 id = 未配置：留痕但**不挂起**。空 id 的口径住在各步骤里（`Branch` 的空支是正常配置、
		// 不记 Warning），故不塞进共用实现
		if (!ChainId.IsValid())
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("RunSubChain[%s]: ChainId 为空——不起链（本步按完成处理）"),
				*SelfChainName);
			return ETcsStepResult::TSR_Completed;
		}

		return Facade->RunChildChainStep(Self, ChainId, bWait);
	}
}

// 自注册（本模块内；机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepRunSubChain, ExecuteStepRunSubChain)
