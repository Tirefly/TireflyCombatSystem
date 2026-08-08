// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"



class UTcsBuffInstance;



// Ongoing AttributeModifier 可自动观察的依赖类型
enum class ETcsAttributeModifierDependencyType : uint8
{
	AMDT_AttributeCurrentValue = 0,
	AMDT_BuffNumericStateParamEffectiveValue,
};



/** Ongoing AttributeModifier 的单个运行时依赖标识。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierDependencyKey
{
	// 依赖类型。
	ETcsAttributeModifierDependencyType Type = ETcsAttributeModifierDependencyType::AMDT_AttributeCurrentValue;

	// Attribute CurrentValue 依赖的本地 Attribute Id。
	FName AttributeId = NAME_None;

	// Buff Numeric StateParam 依赖的 Buff 实例身份。
	FObjectKey SourceBuffObjectKey;

	// Buff Numeric StateParam 依赖的参数标签。
	FGameplayTag StateParamTag;

	/** 创建本地 Attribute CurrentValue 依赖键。 */
	static FTcsAttributeModifierDependencyKey MakeAttributeCurrentValue(FName InAttributeId);

	/** 创建本地 Buff Numeric StateParam effective 值依赖键。 */
	static FTcsAttributeModifierDependencyKey MakeBuffNumericStateParam(
		const UTcsBuffInstance& BuffInstance,
		FGameplayTag InStateParamTag);

	bool operator==(const FTcsAttributeModifierDependencyKey& Other) const
	{
		return Type == Other.Type &&
			AttributeId == Other.AttributeId &&
			SourceBuffObjectKey == Other.SourceBuffObjectKey &&
			StateParamTag == Other.StateParamTag;
	}

	friend uint32 GetTypeHash(const FTcsAttributeModifierDependencyKey& Key)
	{
		uint32 Hash = GetTypeHash(static_cast<uint8>(Key.Type));
		Hash = HashCombineFast(Hash, GetTypeHash(Key.AttributeId));
		Hash = HashCombineFast(Hash, GetTypeHash(Key.SourceBuffObjectKey));
		return HashCombineFast(Hash, GetTypeHash(Key.StateParamTag));
	}
};



/** Ongoing 父实例成功提交时保存的依赖及其已观察版本。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierDependencyRecord
{
	// 已观察的依赖键。
	FTcsAttributeModifierDependencyKey Key;

	// 本轮求值时依赖生产者的版本。
	uint64 ObservedRevision = 0;
};
