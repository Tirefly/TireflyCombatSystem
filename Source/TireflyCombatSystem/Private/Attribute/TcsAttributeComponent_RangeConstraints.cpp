// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TcsLogChannels.h"


bool UTcsAttributeComponent::TryBuildDeclaredRangeConstraintDependents(
	TMap<FName, TSet<FName>>& OutDependents) const
{
	OutDependents.Reset();
	OutDependents.Reserve(Attributes.Num());

	TArray<FName> DeclaredDependencies;
	DeclaredDependencies.Reserve(4);

	// 瀵规瘡涓睘鎬ц闂€滄垜鐨?Clamp 缁撴灉渚濊禆鍝簺灞炴€р€濓紝鍐嶅弽鍚戝缓鍥炬垚鈥滆皝鍙樺寲鍚庨渶瑕侀噸鏂版鏌ユ垜鈥濄€?
	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;
		if (!Attribute.AttributeDef || !Attribute.AttributeDef->ClampStrategyClass)
		{
			return false;
		}

		UTcsAttributeClampStrategy* StrategyCDO = Attribute.AttributeDef->ClampStrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();
		if (!StrategyCDO)
		{
			return false;
		}

		DeclaredDependencies.Reset();
		if (!StrategyCDO->CollectDependentAttributes(AttributeName, Attribute.AttributeDef, Attribute.AttributeDef->ClampStrategyConfig, DeclaredDependencies))
		{
			// 鍙鏈変竴涓瓥鐣ヤ笉缁欏嚭瀹屾暣澹版槑锛屽氨涓嶈兘璇佹槑灞€閮ㄤ紶鎾棴鍖呮纭紝鍙兘鍥為€€鍒板叏灞€ fixpoint銆?
			return false;
		}

		for (const FName DependencyAttribute : DeclaredDependencies)
		{
			// 蹇界暐绌轰緷璧栥€佽嚜渚濊禆鍜岀粍浠跺唴涓嶅瓨鍦ㄧ殑灞炴€э紝閬垮厤鏋勯€犲嚭鏃犳晥浼犳挱杈广€?
			if (DependencyAttribute.IsNone() || DependencyAttribute == AttributeName || !Attributes.Contains(DependencyAttribute))
			{
				continue;
			}

			OutDependents.FindOrAdd(DependencyAttribute).Add(AttributeName);
		}
	}

	return true;
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(
	const TSet<FName>& DirtyAttributes,
	bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(&DirtyAttributes, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(nullptr, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraintsInternal(
	const TSet<FName>* DirtyAttributes,
	bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_EnforceAttributeRangeConstraints);

	const int32 MaxIterations = 8; // 闃叉鏃犻檺寰幆
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 宸ヤ綔闆嗘壙鎺ユ暣杞紶鎾殑涓棿鎬併€?
	// 鍦ㄦ渶缁堟敹鏁涘墠锛屼笉鐩存帴鍐欏洖 Attributes锛岄伩鍏嶅崐鏇存柊鐘舵€佹薄鏌撳悗缁緷璧栬В鏋愩€?
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;
	WorkingBaseValues.Reserve(Attributes.Num());
	WorkingCurrentValues.Reserve(Attributes.Num());

	// 鍒濆鍖栧伐浣滈泦
	for (auto& Pair : Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	TMap<FName, TSet<FName>> DeclaredDependents;
	const bool bTryLocalPropagation = DirtyAttributes && !DirtyAttributes->IsEmpty();
	const bool bUseDeclaredLocalPropagation = bTryLocalPropagation && TryBuildDeclaredRangeConstraintDependents(DeclaredDependents);

	if (bUseDeclaredLocalPropagation)
	{
		// 浠庢湰杞剰灞炴€у嚭鍙戯紝鍙部鈥滃凡澹版槑渚濊禆瀹冧滑鐨勫睘鎬р€濈户缁墿鏁ｏ紝閬垮厤鏃犳剰涔夌殑鍏ㄨ〃鎵弿銆?
		TSet<FName> PendingAttributes = *DirtyAttributes;

		while (!PendingAttributes.IsEmpty() && Iteration < MaxIterations)
		{
			Iteration++;
			bool bAnyChangedThisPass = false;

			TArray<FName> AttributesToProcess;
			AttributesToProcess.Reserve(PendingAttributes.Num());
			// 鍏堝喕缁撴湰杞澶勭悊鐨勮妭鐐癸紝鍐嶆妸鏂拌剰鑺傜偣鐣欏埌涓嬩竴杞紝閬垮厤杈归亶鍘嗚竟鎵╅槦鍒楀鑷磋涔夋贩涔便€?
			for (const FName PendingAttribute : PendingAttributes)
			{
				AttributesToProcess.Add(PendingAttribute);
			}
			PendingAttributes.Empty();

			for (const FName AttributeName : AttributesToProcess)
			{
				if (!Attributes.Contains(AttributeName))
				{
					continue;
				}

				bool bAttributeChanged = false;

				float OldBase = WorkingBaseValues.FindRef(AttributeName);
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues.Add(AttributeName, NewBase);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				float OldCurrent = WorkingCurrentValues.FindRef(AttributeName);
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues.Add(AttributeName, NewCurrent);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				// 鍙湁褰撳墠灞炴€у湪杩欎竴杞湡鐨勮 Clamp 鏀瑰姩浜嗭紝鎵嶆湁蹇呰缁х画鍞ら啋瀹冪殑澹版槑寮忎緷璧栦笅娓搞€?
				if (bAttributeChanged)
				{
					if (const TSet<FName>* Dependents = DeclaredDependents.Find(AttributeName))
					{
						for (const FName DependentAttribute : *Dependents)
						{
							PendingAttributes.Add(DependentAttribute);
						}
					}
				}
			}

			if (!bAnyChangedThisPass && PendingAttributes.IsEmpty())
			{
				break;
			}
		}

		if (!PendingAttributes.IsEmpty())
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] Declared local propagation did not converge for entity '%s', fallback to full fixpoint."),
				*FString(__FUNCTION__),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

			// 灞€閮ㄤ紶鎾笉鏀舵暃鏃讹紝涓嶇户缁部鐢ㄤ腑闂村伐浣滈泦锛岀洿鎺ュ洖鍒扮粍浠跺綋鍓嶅凡鎻愪氦鐘舵€侀噸鏂拌窇鍏ㄥ眬 fixpoint銆?
			// 杩欐牱鍙互閬垮厤鎶娾€滄湭璇佹槑姝ｇ‘鐨勫眬閮ㄤ腑闂存€佲€濆甫杩涗繚瀹堣矾寰勩€?
			WorkingBaseValues.Reset();
			WorkingCurrentValues.Reset();
			WorkingBaseValues.Reserve(Attributes.Num());
			WorkingCurrentValues.Reserve(Attributes.Num());
			for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
			{
				WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
				WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
			}

			Iteration = 0;
			bAnyChanged = true;
		}
		else
		{
			bAnyChanged = false;
		}
	}

	if (!bUseDeclaredLocalPropagation || bAnyChanged)
	{
		// 鏈彁渚涜剰灞炴€с€佺瓥鐣ユ湭澹版槑渚濊禆锛屾垨灞€閮ㄤ紶鎾湭鏀舵暃鏃讹紝鍥為€€鍒扮幇鏈夊叏灞€ fixpoint銆?
		Iteration = 0;
		bAnyChanged = true;
		while (bAnyChanged && Iteration < MaxIterations)
		{
			bAnyChanged = false;
			Iteration++;

			// 杩欓噷鏁呮剰鍥炲埌鍏ㄨ〃鎵弿銆?
			// 褰撲緷璧栭棴鍖呮湭鐭ユ椂锛屽彧鏈夐噸澶嶆鏌ユ墍鏈夊睘鎬э紝鎵嶈兘淇濆畧鍦伴€艰繎绋冲畾鎬併€?
			for (auto& Pair : Attributes)
			{
				FName AttributeName = Pair.Key;

				// 闃舵1: Clamp BaseValue锛屼娇鐢?WorkingBaseValues 瑙ｆ瀽鍔ㄦ€佽寖鍥?
				float OldBase = WorkingBaseValues[AttributeName];
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues[AttributeName] = NewBase;
					bAnyChanged = true;
				}

				// 闃舵2: Clamp CurrentValue锛屼娇鐢?WorkingCurrentValues 瑙ｆ瀽鍔ㄦ€佽寖鍥?
				float OldCurrent = WorkingCurrentValues[AttributeName];
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues[AttributeName] = NewCurrent;
					bAnyChanged = true;
				}
			}
		}
	}

	// 妫€鏌ユ槸鍚︽敹鏁?
	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Max iterations reached for entity '%s', possible circular dependency"),
			*FString(__FUNCTION__),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d Iterations=%d ReachedMaxIterations=%s"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		Iteration,
		Iteration >= MaxIterations ? TEXT("true") : TEXT("false"));

	// 鎵€鏈?Clamp 浼犳挱瀹屾垚鍚庡啀缁熶竴鎻愪氦锛岄伩鍏嶅崐鏇存柊鐘舵€佹薄鏌撳悗缁緷璧栬В鏋愬拰骞挎挱缁撴灉銆?
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (auto& Pair : Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

		// 鎻愪氦闃舵鍙礋璐ｆ妸鏀舵暃鍚庣殑宸ヤ綔闆嗗啓鍥炵粍浠讹紝骞惰ˉ鍙戝彉鏇?杈圭晫浜嬩欢锛屼笉鍐嶅弬涓庝緷璧栨帹瀵笺€?
		// 杩欐牱鈥滄眰鍊间紶鎾€濆拰鈥滃澶栧彲瑙佺姸鎬佹彁浜も€濅袱涓樁娈垫槸瑙ｈ€︾殑銆?
		// 鎻愪氦 BaseValue
		float NewBase = WorkingBaseValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
		{
			float OldBase = Attribute.BaseValue;
			Attribute.BaseValue = NewBase;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldBase;
			Payload.NewValue = NewBase;
			BaseChangePayloads.Add(Payload);

			// 妫€娴嬫槸鍚﹁揪鍒拌竟鐣?
			float RangeMin = NewBase;
			float RangeMax = NewBase;
			ClampAttributeValueInRange(AttributeName, NewBase, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewBase, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewBase, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldBase, NewBase, BoundaryValue);
			}
		}

		// 鎻愪氦 CurrentValue
		float NewCurrent = WorkingCurrentValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
		{
			float OldCurrent = Attribute.CurrentValue;
			Attribute.CurrentValue = NewCurrent;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldCurrent;
			Payload.NewValue = NewCurrent;
			CurrentChangePayloads.Add(Payload);

			// 妫€娴嬫槸鍚﹁揪鍒拌竟鐣?
			float RangeMin = NewCurrent;
			float RangeMax = NewCurrent;
			ClampAttributeValueInRange(AttributeName, NewCurrent, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewCurrent, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewCurrent, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldCurrent, NewCurrent, BoundaryValue);
			}
		}
	}

	// 骞挎挱浜嬩欢
	if (bBroadcastEvents && BaseChangePayloads.Num() > 0)
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (bBroadcastEvents && CurrentChangePayloads.Num() > 0)
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}
	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}
}
