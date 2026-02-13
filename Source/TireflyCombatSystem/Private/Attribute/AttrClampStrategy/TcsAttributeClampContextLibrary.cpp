// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrClampStrategy/TcsAttributeClampContextLibrary.h"

#include "GameplayTagAssetInterface.h"
#include "Attribute/TcsAttributeComponent.h"


AActor* UTcsAttributeClampContextLibrary::GetOwnerActor(const FTcsAttributeClampContextBase& Context)
{
	return Context.AttributeComponent ? Context.AttributeComponent->GetOwner() : nullptr;
}

bool UTcsAttributeClampContextLibrary::GetOtherAttributeValue(
	const FTcsAttributeClampContextBase& Context,
	FName OtherAttributeName,
	float& OutValue)
{
	// 优先从工作集读取（两段式 Clamp 时使用）
	if (Context.WorkingValues)
	{
		if (const float* Value = Context.WorkingValues->Find(OtherAttributeName))
		{
			OutValue = *Value;
			return true;
		}
	}

	// 否则从已提交值读取
	if (Context.AttributeComponent)
	{
		return Context.AttributeComponent->GetAttributeValue(OtherAttributeName, OutValue);
	}

	return false;
}

bool UTcsAttributeClampContextLibrary::OwnerHasTag(
	const FTcsAttributeClampContextBase& Context,
	FGameplayTag Tag)
{
	AActor* Owner = GetOwnerActor(Context);
	if (!Owner)
	{
		return false;
	}

	// 假设 Owner 实现了 IGameplayTagAssetInterface
	if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Owner))
	{
		return TagInterface->HasMatchingGameplayTag(Tag);
	}

	return false;
}

FName UTcsAttributeClampContextLibrary::GetAttributeName(const FTcsAttributeClampContextBase& Context)
{
	return Context.AttributeName;
}
