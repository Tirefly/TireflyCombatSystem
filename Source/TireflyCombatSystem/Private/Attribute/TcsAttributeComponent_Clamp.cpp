// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "TcsLogChannels.h"


void UTcsAttributeComponent::ClampAttributeValueInRange(
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue,
	const TMap<FName, float>* WorkingValues)
{
	const FTcsAttributeInstance* Attribute = Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTcsAttributeRange& Range = Attribute->AttributeDef->AttributeRange;

	// 鍏堣В鏋愭渶灏忚竟鐣屻€傝嫢浼犳挱杩囩▼浼犲叆浜?WorkingValues锛屽垯浼樺厛璇诲彇宸ヤ綔闆嗛噷鐨勫€欓€夊€硷紝
	// 杩欐牱鍚屼竴杞?fixpoint 鍙互鐪嬪埌鈥滃皻鏈彁浜ゅ埌缁勪欢浣嗗凡缁忓湪鏈疆鎺ㄥ鍑虹殑鏂板€尖€濄€?
	float MinValue = TNumericLimits<float>::Lowest();
	switch (Range.MinValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MinValue = Range.MinValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MinValueAttribute))
				{
					MinValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 鏈€澶ц竟鐣屼笌鏈€灏忚竟鐣屽悓鐞嗭紝涔熶紭鍏堣鍙栧伐浣滈泦锛屼繚璇佷紶鎾樁娈电殑渚濊禆瑙ｆ瀽鍩轰簬鏈€鏂板€欓€夌姸鎬併€?
	float MaxValue = TNumericLimits<float>::Max();
	switch (Range.MaxValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MaxValue = Range.MaxValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MaxValueAttribute))
				{
					MaxValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 鍒拌繖閲?Min/Max 宸茬粡瑙ｆ瀽瀹屾瘯锛岀湡姝ｂ€滃浣曞湪鑼冨洿鍐呬慨姝ｂ€濈殑绛栫暐浜ょ粰 ClampStrategy銆?
	// 渚濊禆澹版槑灞炰簬 CollectDependentAttributes 鐨勮亴璐ｏ紝涓嶅湪杩欓噷鎺ㄥ銆?
	TSubclassOf<UTcsAttributeClampStrategy> StrategyClass = Attribute->AttributeDef->ClampStrategyClass;
	if (StrategyClass)
	{
		UTcsAttributeClampStrategy* StrategyCDO = StrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();

		FTcsAttributeClampContextBase Context(
			// Context retains its non-const reflected component field for existing Blueprint readers.
			const_cast<UTcsAttributeComponent*>(this),
			AttributeName,
			Attribute->AttributeDef,
			Attribute,
			WorkingValues
		);

		const FInstancedStruct& Config = Attribute->AttributeDef->ClampStrategyConfig;

		NewValue = StrategyCDO->Clamp(NewValue, MinValue, MaxValue, Context, Config);

		UE_LOG(LogTcsAttribute, Verbose, TEXT("[%s] Attribute %s clamped using strategy %s: Value=%f, Min=%f, Max=%f"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*StrategyClass->GetName(),
			NewValue,
			MinValue,
			MaxValue);
	}
	else
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ClampStrategyClass is null for attribute %s, using FMath::Clamp as fallback"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	}

	if (OutMinValue)
	{
		*OutMinValue = MinValue;
	}
	if (OutMaxValue)
	{
		*OutMaxValue = MaxValue;
	}
}
