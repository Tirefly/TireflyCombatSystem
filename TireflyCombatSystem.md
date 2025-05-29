# Attribute模块代码

## 枚举类型

### ETireflyAttributeRangeType
```cpp
// 属性范围类型
UENUM(BlueprintType)
enum class ETireflyAttributeRangeType : uint8
{
	None  = 0		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute has no limit."),
	Static = 1		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is a constant numeric value."),
	Dynamic = 2		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is dynamic, which is affected by another attribute value.")
};
```

### ETireflyAttributeModifierMode
```cpp
// 修改器修改属性的方式
UENUM(BlueprintType)
enum class ETireflyAttributeModifierMode : uint8
{
	BaseValue			UMETA(ToolTip = "The base value of the attribute."),
	CurrentValue		UMETA(ToolTip = "The current value, modified by skill or buff, of the attribute.")
};
```

## 结构体

### FTireflyAttributeRange
```cpp
// 属性范围
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeRange
{
	GENERATED_BODY()

#pragma region MinValue

public:
	// 最小值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	ETireflyAttributeRangeType MinValueType = ETireflyAttributeRangeType::None;

	// 最小值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETireflyAttributeRangeType::Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	float MinValue = 0.f;

	// 最小值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETireflyAttributeRangeType::Dynamic",  EditConditionHides,
		GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	FName MinValueAttribute = NAME_None;

#pragma endregion

#pragma region MaxValue

public:
	// 最大值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	ETireflyAttributeRangeType MaxValueType = ETireflyAttributeRangeType::None;

	// 最大值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETireflyAttributeRangeType::Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	float MaxValue = 0.f;

	// 最大值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETireflyAttributeRangeType::Dynamic",  EditConditionHides,
		GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	FName MaxValueAttribute = NAME_None;

#pragma endregion
};
```

### FTireflyAttributeDefinition
```cpp
// 属性定义表
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 属性数值范围
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTireflyAttributeRange AttributeRange;

	// 属性类别
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeCategory = FString("");

	// 属性缩写（用于公式）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeAbbreviation = FString("");

	// 属性名（最好使用 StringTable）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText AttributeName;

	// 属性描述（最好使用 StringTable）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText AttributeDescription;

	// 是否在UI中显示
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bShowInUI = true;

	// 属性图标
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UTexture2D> Icon;

	// 是否显示为小数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bAsDecimal = true;

	// 是否显示为百分比
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bAsPercentage = false;
};
```

### FTireflyAttributeInstance
```cpp
// 属性实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeInstance
{
	GENERATED_BODY()

public:
	// 属性定义
	UPROPERTY(BlueprintReadOnly)
	FTireflyAttributeDefinition AttributeDef;

	// 属性实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 AttributeInstId = -1;

	// 基础值
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.0f;

	// 属性值
	UPROPERTY(BlueprintReadOnly)
	float CurrentValue = 0.0f;

	//  属性拥有者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Owner;
};
```

### FTireflyAttributeModifierDefinition
```cpp
// 属性修改器定义
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeModifierDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 修改器名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FName ModifierName = NAME_None;

	// 修改器标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FGameplayTagContainer Tags;

	// 修改器优先级（值越小，优先级越高）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	int32 Priority = 0;

	// 修改器要修改的属性
	UPROPERTY(Meta = (GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FName AttributeName = NAME_None;

	// 修改器修改属性的方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	ETireflyAttributeModifierMode ModifierMode = ETireflyAttributeModifierMode::CurrentValue;

	// 修改器操作数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TArray<FName> OperandNames;

	// 修改器执行器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTireflyAttributeModifierExecution> ModifierType;

	// 修改器合并器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTireflyAttributeModifierMerger> MergerType;
};
```

### FTireflyAttributeModifierInstance
```cpp
// 属性修改器实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeModifierInstance
{
	GENERATED_BODY()

#pragma region Variables

public:
	// 修改器定义
	UPROPERTY(BlueprintReadOnly)
	FTireflyAttributeModifierDefinition ModifierDef;

	// 修改器实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 ModifierInstId = -1;

	// 修改器来源对象
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> SourceObject;

	// 修改器操作数
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> Operands;

	// 修改器应用时间戳
	UPROPERTY(BlueprintReadOnly)
	int64 ApplyTimestamp = -1;

	// 修改器最新更新时间戳
	UPROPERTY(BlueprintReadOnly)
	int64 UpdateTimestamp = -1;

#pragma endregion


#pragma region Constructors

public:
	FTireflyAttributeModifierInstance(){}

#pragma endregion


#pragma region Functions

public:
	bool IsValid() const
	{
		return ModifierDef.ModifierName != NAME_None
			&& ModifierInstId >= 0
			&& ApplyTimestamp >= 0;
	}

#pragma endregion
};
```

## 类

### UTireflyAttributeModifierExecution
```cpp
// 属性修改器执行算法
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器执行算法
	 * 
	 * @param Instigator 属性修改流程的发起者
	 * @param Target 属性修改流程的目标
	 * @param ModInst 属性修改器实例
	 * @param BaseValues 要修改的所有属性的基础值
	 * @param CurrentValues 要修改的所有属性的当前值
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Execute(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues);
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) {}
};
```

### UTireflyAttributeModifierMerger
```cpp
// 属性修改器合并算法
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器合并算法
	 * 
	 * @param ModifiersToMerge 要合并的属性修改器
	 * @param MergedModifiers 合并后的属性修改器
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Merge(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) {}
};
```

### UTireflyAttrModExec_Addition
```cpp
// 属性修改器：加法
UCLASS(Meta = (DisplayName = "属性修改器：加法"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_Addition : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
```

#### 实现: UTireflyAttrModExec_Addition
```cpp
// 属性修改器：加法
void UTireflyAttrModExec_Addition::Execute_Implementation(
	AActor* Instigator,
	AActor* Target,
	const FTireflyAttributeModifierInstance& ModInst,
	UPARAM(ref) TMap<FName, float>& BaseValues,
	UPARAM(ref) TMap<FName, float>& CurrentValues)
{
	const FName& AttrToMod = ModInst.ModifierDef.AttributeName;
	float* BaseValue = BaseValues.Find(AttrToMod);
	float* CurrentValue = CurrentValues.Find(AttrToMod);
	if (!BaseValue || !CurrentValue)
	{
		UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] Attribute '%s' not found."),
			*FString(__FUNCTION__),
			*AttrToMod.ToString());
		return;
	}

	switch (ModInst.ModifierDef.ModifierMode)
	{
	case ETireflyAttributeModifierMode::BaseValue:
		{
			for (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				*BaseValue += Pair.Value;
			}
			break;
		}
	case ETireflyAttributeModifierMode::CurrentValue:
		{
			for  (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				*CurrentValue += Pair.Value;
			}
			break;
		}
	}
}
```

### UTireflyAttrModExec_MultiplyAdditive
```cpp
// 属性修改器：乘法结合律
UCLASS(Meta = (DisplayName = "属性修改器：乘法结合律"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyAdditive : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
```

#### 实现: UTireflyAttrModExec_MultiplyAdditive
```cpp
// 属性修改器：乘法结合律
void UTireflyAttrModExec_MultiplyAdditive::Execute_Implementation(
	AActor* Instigator,
	AActor* Target,
	const FTireflyAttributeModifierInstance& ModInst,
	TMap<FName, float>& BaseValues,
	TMap<FName, float>& CurrentValues)
{
	const FName& AttrToMod = ModInst.ModifierDef.AttributeName;
	float* BaseValue = BaseValues.Find(AttrToMod);
	float* CurrentValue = CurrentValues.Find(AttrToMod);
	if (!BaseValue || !CurrentValue)
	{
		UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] Attribute '%s' not found."),
			*FString(__FUNCTION__),
			*AttrToMod.ToString());
		return;
	}

	switch (ModInst.ModifierDef.ModifierMode)
	{
	case ETireflyAttributeModifierMode::BaseValue:
		{
			float MultiplyOperand = 1.f;
			for (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				MultiplyOperand += Pair.Value;
			}
			*BaseValue *= MultiplyOperand;
			break;
		}
	case ETireflyAttributeModifierMode::CurrentValue:
		{
			float MultiplyOperand = 1.f;
			for  (const TPair<FName, float>& Pair : ModInst.Operands)
			{
				MultiplyOperand += Pair.Value;
			}
			*CurrentValue *= MultiplyOperand;
			break;
		}
	}
}
```

### UTireflyAttrModExec_MultiplyContinued
```cpp
// 属性修改器：乘法连乘
UCLASS(Meta = (DisplayName = "属性修改器：乘法连乘"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyContinued : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
```

#### 实现: UTireflyAttrModExec_MultiplyContinued
```cpp
// 属性修改器：乘法连乘
void UTireflyAttrModMerger_UseAdditiveSum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.Num() <= 1)
	{
		MergedModifiers.Append(ModifiersToMerge);
		return;
	}

	FTireflyAttributeModifierInstance MergedModifier = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		for (const TPair<FName, float>& Pair : Modifier.Operands)
		{
			if (float* OperandValue = MergedModifier.Operands.Find(Pair.Key))
			{
				*OperandValue += Pair.Value;
			}
		}
	}

	MergedModifiers.Add(MergedModifier);
}
```

### UTireflyAttrModMerger_UseAdditiveSum
```cpp
// 属性修改器合并操作：取加法和值
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取加法和值"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseAdditiveSum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
```

#### 实现: UTireflyAttrModMerger_UseAdditiveSum
```cpp
// 属性修改器合并操作：取操作数最大
void UTireflyAttrModMerger_UseMaximum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance MaxMagnitudeMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		const float* BiggestMagnitude = MaxMagnitudeMod.Operands.Find(FName("Magnitude"));
		const float* ModifierMagnitude = Modifier.Operands.Find(FName("Magnitude"));
		if (BiggestMagnitude && ModifierMagnitude)
	{
			if (*ModifierMagnitude > *BiggestMagnitude)
			{
				MaxMagnitudeMod = Modifier;
			}
		}
	}

	MergedModifiers.Add(MaxMagnitudeMod);
}
```

### UTireflyAttrModMerger_UseMaximum
```cpp
// 属性修改器合并操作：取操作数最大
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取操作数最大"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMaximum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
```

#### 实现: UTireflyAttrModMerger_UseMaximum
```cpp
// 属性修改器合并操作：取最小
void UTireflyAttrModMerger_UseMinimum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance MinMagnitudeMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		const float* BiggestMagnitude = MinMagnitudeMod.Operands.Find(FName("Magnitude"));
		const float* ModifierMagnitude = Modifier.Operands.Find(FName("Magnitude"));
		if (BiggestMagnitude && ModifierMagnitude)
	{
			if (*ModifierMagnitude < *BiggestMagnitude)
			{
				MinMagnitudeMod = Modifier;
			}
		}
	}

	MergedModifiers.Add(MinMagnitudeMod);
}
```

### UTireflyAttrModMerger_UseMinimum
```cpp
// 属性修改器合并操作：取最小
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMinimum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
```

#### 实现: UTireflyAttrModMerger_UseMinimum
```cpp
// 属性修改器合并操作：取最小
void UTireflyAttrModMerger_UseMinimum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance MinMagnitudeMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		const float* BiggestMagnitude = MinMagnitudeMod.Operands.Find(FName("Magnitude"));
		const float* ModifierMagnitude = Modifier.Operands.Find(FName("Magnitude"));
		if (BiggestMagnitude && ModifierMagnitude)
	{
			if (*ModifierMagnitude < *BiggestMagnitude)
			{
				MinMagnitudeMod = Modifier;
			}
		}
	}

	MergedModifiers.Add(MinMagnitudeMod);
}
```

### UTireflyAttrModMerger_UseNewest
```cpp
// 属性修改器合并操作：取最新
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取最大"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseNewest : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
```

#### 实现: UTireflyAttrModMerger_UseNewest
```cpp
// 属性修改器合并操作：取最新
void UTireflyAttrModMerger_UseNewest::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance NewestMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (Modifier.ApplyTimestamp > NewestMod.ApplyTimestamp)
		{
			NewestMod = Modifier;
		}
	}

	MergedModifiers.Add(NewestMod);
}
```

### UTireflyAttrModMerger_UseOldest
```cpp
// 属性修改器合并操作：取最旧
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取最旧"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseOldest : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override;
};
```

#### 实现: UTireflyAttrModMerger_UseOldest
```cpp
// 属性修改器合并操作：取最旧
void UTireflyAttrModMerger_UseOldest::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance OldestMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (Modifier.ApplyTimestamp < OldestMod.ApplyTimestamp)
		{
			OldestMod = Modifier;
		}
	}

	MergedModifiers.Add(OldestMod);
}
```

### UTireflyAttributeComponent
```cpp
// 属性组件
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTireflyAttributeComponent();

protected:
	virtual void BeginPlay() override;

#pragma endregion
};
```

#### 实现: UTireflyAttributeComponent
```cpp
// 属性组件实现
inline UTireflyAttributeComponent::UTireflyAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

inline void UTireflyAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}
```

### UTireflyAttributeManagerSubsystem
```cpp
// 属性管理子系统
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
};