# 一、属性模块

## 1.1 属性定义

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

### a. 属性数值范围类型

```cpp
// 属性范围类型
UENUM(BlueprintType)
enum class ETireflyAttributeRangeType : uint8
{
	None  = 0		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute has no limit."),
	Static = 1		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is a constant numeric value."),
	Dynamic = 2		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is dynamic, which is affected by another attribute value."),
};
```

### b. 属性数值范围结构

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

## 1.2 属性实例

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

## 1.3 属性修改器

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

### a. 属性修改器操作方式

```cpp
// 修改器修改属性的方式
UENUM(BlueprintType)
enum class ETireflyAttributeModifierMode : uint8
{
	BaseValue			UMETA(ToolTip = "The base value of the attribute."),
	CurrentValue		UMETA(ToolTip = "The current value, modified by skill or buff, of the attribute."),
};
```

## 1.4 属性修改器实例

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

## 1.5 属性修改器执行算法

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

### a. 属性修改执行算法：加法运算

```cpp
// 属性修改器执行算法：加法
UCLASS(Meta = (DisplayName = "属性修改器执行算法：加法"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_Addition : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override
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

		if (const float* Magnitude = ModInst.Operands.Find(FName("Magnitude")))
		{
			switch (ModInst.ModifierDef.ModifierMode)
			{
			case ETireflyAttributeModifierMode::BaseValue:
				{
					*BaseValue += *Magnitude;
					break;
				}
			case ETireflyAttributeModifierMode::CurrentValue:
				{
					*CurrentValue += *Magnitude;
					break;
				}
			}
		}
	}
};
```

### b. 属性修改执行算法：乘法结合律

```cpp
// 属性修改器执行算法：乘法结合律
UCLASS(Meta = (DisplayName = "属性修改器执行算法：乘法结合律"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyAdditive : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override
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

		if (const float* Magnitude = ModInst.Operands.Find(FName("Magnitude")))
		{
			switch (ModInst.ModifierDef.ModifierMode)
			{
			case ETireflyAttributeModifierMode::BaseValue:
				{
					*BaseValue *= (1.f + *Magnitude);
					break;
				}
			case ETireflyAttributeModifierMode::CurrentValue:
				{
					*CurrentValue *= (1.f + *Magnitude);
					break;
				}
			}
		}
	}
};
```

### c. 属性修改执行算法：乘法连乘

```cpp
// 属性修改器执行算法：乘法连乘
UCLASS(Meta = (DisplayName = "属性修改器执行算法：乘法连乘"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyContinued : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override
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

		if (const float* Magnitude = ModInst.Operands.Find(FName("Magnitude")))
		{
			switch (ModInst.ModifierDef.ModifierMode)
			{
			case ETireflyAttributeModifierMode::BaseValue:
				{
					*BaseValue *= *Magnitude;
					break;
				}
			case ETireflyAttributeModifierMode::CurrentValue:
				{
					*CurrentValue *= *Magnitude;
					break;
				}
			}
		}
	}
};
```

## 1.6 属性修改器合并算法

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

### a. 属性修改器合并操作：取加法和值

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
	{
		if (ModifiersToMerge.Num() <= 1)
		{
			MergedModifiers.Append(ModifiersToMerge);
			return;
		}

		const FName MagnitudeKey = FName("Magnitude");
		FTireflyAttributeModifierInstance& MergedModifier = ModifiersToMerge[0];
		float* MagnitudeToMerge = MergedModifier.Operands.Find(MagnitudeKey);
	
		for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
		{
			if (const float* Magnitude = Modifier.Operands.Find(MagnitudeKey))
			{
				*MagnitudeToMerge += *Magnitude;
			}
		}

		MergedModifiers.Add(MergedModifier);
	}
};
```

### b. 属性修改器合并操作：取操作数最大

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
	{
		if (ModifiersToMerge.IsEmpty())
		{
			return;
		}

		const FName MagnitudeKey = FName("Magnitude");
		int32 MaxIndex = 0;
		const float* MaxMagnitude = ModifiersToMerge[0].Operands.Find(MagnitudeKey);

		for (int32 i = 1; i < ModifiersToMerge.Num(); ++i)
		{
			const float* CurrentMagnitude = ModifiersToMerge[i].Operands.Find(MagnitudeKey);
			if (CurrentMagnitude && (!MaxMagnitude || *CurrentMagnitude > *MaxMagnitude))
			{
				MaxMagnitude = CurrentMagnitude;
				MaxIndex = i;
			}
		}

		MergedModifiers.Add(ModifiersToMerge[MaxIndex]);
	}
};
```

### c. 属性修改器合并操作：取操作数最小

```cpp
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMinimum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override
	{
		if (ModifiersToMerge.IsEmpty())
		{
			return;
		}

		const FName MagnitudeKey = FName("Magnitude");
		int32 MinIndex = 0;
		const float* MinMagnitude = ModifiersToMerge[0].Operands.Find(MagnitudeKey);

		for (int32 i = 1; i < ModifiersToMerge.Num(); ++i)
		{
			const float* CurrentMagnitude = ModifiersToMerge[i].Operands.Find(MagnitudeKey);
			if (CurrentMagnitude && (!MinMagnitude || *CurrentMagnitude < *MinMagnitude))
			{
				MinMagnitude = CurrentMagnitude;
				MinIndex = i;
			}
		}

		MergedModifiers.Add(ModifiersToMerge[MinIndex]);
	}
};
```

### d. 属性修改器合并操作：取最新

```cpp
// 属性修改器合并操作：取最新
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取最新"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseNewest : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override
	{
		if (ModifiersToMerge.IsEmpty())
		{
			return;
		}

		int32 NewestModIndex = 0;
		for (int32 i = 1; i < ModifiersToMerge.Num(); i++)
		{
			if (ModifiersToMerge[i].ApplyTimestamp > ModifiersToMerge[NewestModIndex].ApplyTimestamp)
			{
				NewestModIndex = i;
			}
		}

		MergedModifiers.Add(ModifiersToMerge[NewestModIndex]);
	}
};
```

### e. 属性修改器合并操作：取最旧

```cpp
// 属性修改器合并操作：取最旧
UCLASS(Meta = (DisplayName = "属性修改器合并操作：取最旧"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseOldest : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override
	{
		if (ModifiersToMerge.IsEmpty())
		{
			return;
		}
	
		int32 OldestModIndex = 0;
		for (int32 i = 1; i < ModifiersToMerge.Num(); i++)
		{
			if (ModifiersToMerge[i].ApplyTimestamp < ModifiersToMerge[OldestModIndex].ApplyTimestamp)
			{
				OldestModIndex = i;
			}
		}

		MergedModifiers.Add(ModifiersToMerge[OldestModIndex]);
	}
};
```

## 1.7 属性组件

```cpp
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTireflyAttributeComponent() {}

protected:
	virtual void BeginPlay() override {}

#pragma endregion
};
```

## 1.8 属性管理器子系统

```cpp
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
};
```