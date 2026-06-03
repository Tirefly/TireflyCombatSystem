// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateInstance.h"

#include "Buff/TcsBuffComponent.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"



UTcsStateInstance::UTcsStateInstance()
{
}

UWorld* UTcsStateInstance::GetWorld() const
{
	// 优先从 Owner Actor 获取 World
	if (Owner.IsValid())
	{
		return Owner->GetWorld();
	}

	// 回退：尝试从 Outer 获取
	if (const AActor* OuterActor = Cast<AActor>(GetOuter()))
	{
		return OuterActor->GetWorld();
	}

	return nullptr;
}

void UTcsStateInstance::Initialize(
	const UTcsStateDefinition* InStateDef,
	FName InStateDefId,
	AActor* InOwner,
	AActor* InInstigator,
	int32 InInstanceId,
	int32 InLevel)
{
	bInitialized = false;

	Owner = nullptr;
	OwnerController = nullptr;
	OwnerStateCmp.Reset();
	OwnerBuffCmp.Reset();
	OwnerAttributeCmp.Reset();
	OwnerSkillCmp.Reset();

	Instigator = nullptr;
	InstigatorController = nullptr;
	InstigatorStateCmp.Reset();
	InstigatorBuffCmp.Reset();
	InstigatorAttributeCmp.Reset();
	InstigatorSkillCmp.Reset();

	StateDef = InStateDef;
	StateDefId = InStateDefId;
	StateInstanceId = InInstanceId;
	Level = InLevel;

	// 验证状态定义 DataAsset
	if (!InStateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid StateDefinitionAsset for StateDefId: %s"),
			*FString(__FUNCTION__),
			*InStateDefId.ToString());
		return;
	}

	// 初始化状态Owner和其状态组件、属性组件、技能组件
	Owner = InOwner;
	if (!IsValid(InOwner) || !InOwner->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Owner is invalid"), *FString(__FUNCTION__));
		return;
	}
	OwnerController = Owner->GetInstigatorController();
	OwnerStateCmp = ITcsEntityInterface::Execute_GetStateComponent(InOwner);
	OwnerBuffCmp = ITcsEntityInterface::Execute_GetBuffComponent(InOwner);
	OwnerAttributeCmp = ITcsEntityInterface::Execute_GetAttributeComponent(InOwner);
	OwnerSkillCmp = ITcsEntityInterface::Execute_GetSkillComponent(InOwner);

	if (!OwnerStateCmp.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Owner %s has no valid StateComponent via TcsEntityInterface"),
			*FString(__FUNCTION__),
			*InOwner->GetName());
		return;
	}

	// 初始化状态Instigator和其状态组件、属性组件、技能组件
	Instigator = InInstigator;
	if (!IsValid(InInstigator) || !InInstigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Instigator is invalid"), *FString(__FUNCTION__));
		return;
	}
	InstigatorController = Instigator->GetInstigatorController();
	InstigatorStateCmp = ITcsEntityInterface::Execute_GetStateComponent(InInstigator);
	InstigatorBuffCmp = ITcsEntityInterface::Execute_GetBuffComponent(InInstigator);
	InstigatorAttributeCmp = ITcsEntityInterface::Execute_GetAttributeComponent(InInstigator);
	InstigatorSkillCmp = ITcsEntityInterface::Execute_GetSkillComponent(InInstigator);

	// 清理参数缓存
	NumericParameters.Reset();
	NumericParametersTag.Reset();
	BoolParameters.Reset();
	BoolParametersTag.Reset();
	VectorParameters.Reset();
	VectorParametersTag.Reset();

	InitializeRuntimeParameters();

	// 参数由 UTcsStateManagerSubsystem::EvaluateAndApplyStateParameters 在创建实例时统一评估并写入，
	// 此处不再重复调用 InitParameterValues / InitParameterTagValues。

	bInitialized = true;
}

bool UTcsStateInstance::SetCurrentStage(ETcsStateStage InStage)
{
	// 相同阶段无需处理
	if (Stage == InStage)
	{
		return false;
	}

	// 合法转换白名单矩阵（行=From，列=To）：
	//              Inactive  Active  HangUp  Pause  Expired
	// Inactive  [    -       true    true    true    true  ]
	// Active    [   true      -      true    true    true  ]
	// HangUp    [   false    true     -      true    true  ]
	// Pause     [   false    true    false    -      true  ]
	// Expired   [   false   false    false   false    -    ]  ← 终态
	static const bool ValidTransitions[5][5] =
	{
		/*            Inactive  Active  HangUp  Pause  Expired */
		/* Inactive */{ false,   true,   true,   true,   true  },
		/* Active   */{ true,    false,  true,   true,   true  },
		/* HangUp   */{ false,   true,   false,  true,   true  },
		/* Pause    */{ false,   true,   false,  false,  true  },
		/* Expired  */{ false,   false,  false,  false,  false },
	};

	const int32 FromIdx = static_cast<int32>(Stage);
	const int32 ToIdx = static_cast<int32>(InStage);
	if (!ValidTransitions[FromIdx][ToIdx])
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("[%s] Illegal stage transition: %s -> %s. State=%s Id=%d"),
			*FString(__FUNCTION__),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(Stage)),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(InStage)),
			*GetStateDefId().ToString(),
			GetInstanceId());
		return false;
	}

	Stage = InStage;
	return true;
}


