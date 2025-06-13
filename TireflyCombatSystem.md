# 一、属性模块

## 1.1 属性定义（待修改）

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

	//  属性拥有者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Owner;

	// 基础值
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.0f;

	// 属性值
	UPROPERTY(BlueprintReadOnly)
	float CurrentValue = 0.0f;

public:
	FTireflyAttributeInstance() {}

	FTireflyAttributeInstance(const FTireflyAttributeDefinition& AttrDef, int32 InstId, AActor* Owner)
		: AttributeDef(AttrDef), AttributeInstId(InstId), Owner(Owner), BaseValue(0.f), CurrentValue(0.f)
	{}

	FTireflyAttributeInstance(const FTireflyAttributeDefinition& AttrDef, int32 InstId, AActor* Owner, float InitValue)
		: AttributeDef(AttrDef), AttributeInstId(InstId), Owner(Owner), BaseValue(InitValue), CurrentValue(InitValue)
	{}
};
```

## 1.3 属性修改器定义

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

	// 修改器来源
	UPROPERTY(BlueprintReadOnly)
	FName SourceName = NAME_None;

	// 修改器发起者
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Instigator;

	// 修改器目标
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target;

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

	bool operator==(const FTireflyAttributeModifierInstance& Other) const
	{
		return ModifierDef.ModifierName == Other.ModifierDef.ModifierName
			&& ModifierInstId == Other.ModifierInstId
			&& ApplyTimestamp == Other.ApplyTimestamp;
	}

	bool operator!=(const FTireflyAttributeModifierInstance& Other) const
	{
		return !(*this == Other);
	}

	bool operator<(const FTireflyAttributeModifierInstance& Other) const
	{
		return ModifierDef.Priority < Other.ModifierDef.Priority;
	}

#pragma endregion
};
```

## 1.5 属性修改器执行器

```cpp
// 属性修改器执行器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器执行器
	 * 
	 * @param ModInst 属性修改器实例
	 * @param BaseValues 要修改的所有属性的基础值
	 * @param CurrentValues 要修改的所有属性的当前值
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Execute(
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues);
	virtual void Execute_Implementation(
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) {}
};
```

### a. 属性修改执行算法：加法运算

```cpp
// 属性修改器执行器：加法
UCLASS(Meta = (DisplayName = "属性修改器执行器：加法"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_Addition : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
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
// 属性修改器执行器：乘法结合律
UCLASS(Meta = (DisplayName = "属性修改器执行器：乘法结合律"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyAdditive : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
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
// 属性修改器执行器：乘法连乘
UCLASS(Meta = (DisplayName = "属性修改器执行器：乘法连乘"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyContinued : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
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

## 1.6 属性修改器合并器

```cpp
// 属性修改器合并器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeModifierMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 属性修改器合并器
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

### a. 属性修改器合并操作：不合并

```cpp
// 属性修改器合并器：不合并
UCLASS(Meta = (DisplayName = "属性修改器合并器：不合并"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_NoMerge : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override
	{
		MergedModifiers.Append(ModifiersToMerge);
	}
};
```

### b. 属性修改器合并操作：取加法和值

```cpp
// 属性修改器合并器：取加法和值
UCLASS(Meta = (DisplayName = "属性修改器合并器：取加法和值"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseAdditiveSum : public UTireflyAttributeModifierMerger
{
	GENERATED_BODY()

public:
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers) override
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

### c. 属性修改器合并操作：取操作数最大

```cpp
// 属性修改器合并器：取操作数最大
UCLASS(Meta = (DisplayName = "属性修改器合并器：取操作数最大"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModMerger_UseMaximum : public UTireflyAttributeModifierMerger
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

### d. 属性修改器合并操作：取操作数最小

```cpp
// 属性修改器合并器：取操作数最小
UCLASS(Meta = (DisplayName = "属性修改器合并器：取操作数最小"))
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

### e. 属性修改器合并操作：取最新

```cpp
// 属性修改器合并器：取最新
UCLASS(Meta = (DisplayName = "属性修改器合并器：取最新"))
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

### f. 属性修改器合并操作：取最旧

```cpp
// 属性修改器合并器：取最旧
UCLASS(Meta = (DisplayName = "属性修改器合并器：取最旧"))
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

## 1.7 属性更新事件

```cpp
// 属性值改变事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTireflyAttributeChangeDelegate,
	const TArray<FTireflyAttributeChangeEventPayload>&, Payloads);

// 属性变化事件数据
USTRUCT(BlueprintType)
struct FTireflyAttributeChangeEventPayload
{
	GENERATED_BODY()

public:
	// 属性名
	UPROPERTY(BlueprintReadOnly)
	FName AttributeName = NAME_None;
	
	// 属性新值
	UPROPERTY(BlueprintReadOnly)
	float NewValue = 0.f;

	// 属性旧值
	UPROPERTY(BlueprintReadOnly)
	float OldValue = 0.f;

	// 变化来源记录
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> ChangeSourceRecord;

public:
	FTireflyAttributeChangeEventPayload() {}

	FTireflyAttributeChangeEventPayload(
		FName InAttrName,
		float InNewVal,
		float InOldVal,
		const TMap<FName, float>& InChangeSourceRecord)
	{
		AttributeName = InAttrName;
		NewValue = InNewVal;
		OldValue = InOldVal;
		ChangeSourceRecord = InChangeSourceRecord;
	}
};
```

## 1.8 属性组件

```cpp
// 属性组件，保存战斗实体的属性相关数据
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


#pragma region Attribute

public:
	//  获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const
	{
		if (const FTireflyAttributeInstance* AttrInst = Attributes.Find(AttributeName))
		{
			OutValue = AttrInst->CurrentValue;
			return true;
		}

		return false;
	}

	//  获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const
	{
		if (const FTireflyAttributeInstance* AttrInst = Attributes.Find(AttributeName))
		{
			OutValue = AttrInst->BaseValue;
			return true;
		}

		return false;
	}

	// 获取所有属性的当前值
	TMap<FName, float> GetAttributeValues() const
	{
		TMap<FName, float> AttributeValues;
		for (const auto& AttrInst : Attributes)
		{
			AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
		}
	
		return AttributeValues;
	}

	// 获取所有属性的基础值
	TMap<FName, float> GetAttributeBaseValues() const
	{
		TMap<FName, float> AttributeValues;
		for (const auto& AttrInst : Attributes)
		{
			AttributeValues.Add(AttrInst.Key, AttrInst.Value.BaseValue);
		}
	
		return AttributeValues;
	}

	// 广播属性当前值改变事件
	void BroadcastAttributeValueChangeEvent(const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const
	{
		if (!Payloads.IsEmpty() && OnAttributeValueChanged.IsBound())
		{
			OnAttributeValueChanged.Broadcast(Payloads);
		}
	}

	// 广播属性基础值改变事件
	void BroadcastAttributeBaseValueChangeEvent(const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const
	{
		if (!Payloads.IsEmpty() && OnAttributeBaseValueChanged.IsBound())
		{
			OnAttributeBaseValueChanged.Broadcast(Payloads);
		}
	}

public:
	// 战斗实体的所有属性实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTireflyAttributeInstance> Attributes;

	// 战斗实体的所有属性修改器实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TArray<FTireflyAttributeModifierInstance> AttributeModifiers;

	// 属性当前值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute")
	FTireflyAttributeChangeDelegate OnAttributeValueChanged;

	// 属性基础值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute")
	FTireflyAttributeChangeDelegate OnAttributeBaseValueChanged;

#pragma endregion
};
```

## 1.9 属性管理器子系统

```cpp
// 属性管理器子系统，所有战斗实体执行属性相关逻辑的入口
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

#pragma region Attribute

public:
	// 给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	void AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"), *FString(__FUNCTION__));
			return;
		}
	
		const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>();
		const UDataTable* AttributeDefTable = Settings->AttributeDefTable.LoadSynchronous();
		if (!IsValid(AttributeDefTable))
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"), *FString(__FUNCTION__));
			return;
		}

		const auto AttrDef = AttributeDefTable->FindRow<FTireflyAttributeDefinition>(AttributeName, FString(__FUNCTION__));
		if (!AttrDef)
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
				*FString(__FUNCTION__),
				*AttributeName.ToString());
			return;
		}

		FTireflyAttributeInstance AttrInst = FTireflyAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity, InitValue);
		AttributeComponent->Attributes.Add(AttributeName, AttrInst);
	}

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	void AddAttributes(
		AActor* CombatEntity,
		const TArray<FName>& AttributeNames)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] CombatEntity does not have an AttributeComponent"), *FString(__FUNCTION__));
			return;
		}
	
		const UTireflyCombatSystemSettings* Settings = GetDefault<UTireflyCombatSystemSettings>();
		const UDataTable* AttributeDefTable = Settings->AttributeDefTable.LoadSynchronous();
		if (!IsValid(AttributeDefTable))
		{
			UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable in TcsDevSettings is not valid"), *FString(__FUNCTION__));
			return;
		}

		for (const FName AttributeName : AttributeNames)
		{
			const auto AttrDef = AttributeDefTable->FindRow<FTireflyAttributeDefinition>(AttributeName, FString(__FUNCTION__));
			if (!AttrDef)
			{
				UE_LOG(LogTcsAttribute, Error, TEXT("[%s] AttributeDefTable does not contain AttributeName %s"),
					*FString(__FUNCTION__),
					*AttributeName.ToString());
				continue;
			}

			FTireflyAttributeInstance AttrInst = FTireflyAttributeInstance(*AttrDef, ++GlobalAttributeInstanceIdMgr, CombatEntity);
			AttributeComponent->Attributes.Add(AttributeName, AttrInst);
		}
	}

protected:
	// 获取战斗实体的属性组件
	static class UTireflyAttributeComponent* GetAttributeComponent(const AActor* CombatEntity)
	{
		if (!IsValid(CombatEntity))
		{
			return nullptr;
		}

		if (CombatEntity->Implements<UTireflyCombatEntityInterface>())
		{
			return ITireflyCombatEntityInterface::Execute_GetAttributeComponent(CombatEntity);
		}

		return CombatEntity->FindComponentByClass<UTireflyAttributeComponent>();
	}

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

#pragma endregion
	

#pragma region AttribtueModifier

public:	
	// 给战斗实体应用多个属性修改器
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent) || Modifiers.IsEmpty())
		{
			return;
		}

		TArray<FTireflyAttributeModifierInstance> ModifiersToExecute;// 对属性Base值执行操作的属性修改器
		TArray<FTireflyAttributeModifierInstance> ModifiersToApply;// 对属性Current值应用的属性修改器
		const int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();

		// 区分好修改属性Base值和Current值的两种修改器
		for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
		{
			switch (Modifier.ModifierDef.ModifierMode)
			{
			case ETireflyAttributeModifierMode::BaseValue:
				{
					ModifiersToExecute.Add(Modifier);
					break;
				}
			case ETireflyAttributeModifierMode::CurrentValue:
				{
					// 添加到应用列表并设置应用时间戳
					Modifier.ApplyTimestamp = UtcNow;
					Modifier.UpdateTimestamp = UtcNow;
					ModifiersToApply.Add(Modifier);
					break;
				}
			}
		}

		// 先执行针对属性Current值的修改器
		if (!ModifiersToExecute.IsEmpty())
		{
			RecalculateAttributeBaseValues(CombatEntity, ModifiersToExecute);
		}
	
		// 再执行针对属性Base值的修改器
		if (!ModifiersToApply.IsEmpty())
		{
			// 把以经营用过但有改变的属性修改器更新一下，并从待应用列表中移除
			for (FTireflyAttributeModifierInstance& Modifier : AttributeComponent->AttributeModifiers)
			{
				if (ModifiersToApply.Contains(Modifier))
				{
					Modifier.UpdateTimestamp = UtcNow;
					ModifiersToApply.Remove(Modifier);
				}
			}
			AttributeComponent->AttributeModifiers.Append(ModifiersToApply);
		}
	
		// 无论如何，都要重新计算属性Current值
		RecalculateAttributeCurrentValues(CombatEntity);
	}

	// 从战斗实体身上移除多个属性修改器
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			return;
		}

		bool bModified = false;
		for (const FTireflyAttributeModifierInstance& Modifier : Modifiers)
		{
			if (AttributeComponent->AttributeModifiers.Remove(Modifier) > 0)
			{
				bModified = true;
			}
		}

		if (bModified)
		{
			RecalculateAttributeCurrentValues(CombatEntity);
		}
	}

	// 处理战斗实体的属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			return;
		}

		bool bModified = false;
		const int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();
		for (FTireflyAttributeModifierInstance& Modifier : Modifiers)
		{
			if (AttributeComponent->AttributeModifiers.Contains(Modifier))
			{
				const int32 ModifierInstId = AttributeComponent->AttributeModifiers.Find(Modifier);
				Modifier.UpdateTimestamp = UtcNow;
				AttributeComponent->AttributeModifiers[ModifierInstId] = Modifier;
			
				bModified = true;
			}
		}

		if (bModified)
		{
			RecalculateAttributeCurrentValues(CombatEntity);
		}
	}

protected:
	// 重新计算战斗实体的属性基值
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTireflyAttributeModifierInstance>& Modifiers)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			return;
		}

		// 按类型整理所有属性修改器，方便后续执行修改器合并
		TArray<FTireflyAttributeModifierInstance> MergedModifiers;
		MergeAttributeModifiers(CombatEntity, Modifiers, MergedModifiers);
		// 按照优先级对属性修改器进行排序
		MergedModifiers.Sort();

		// 属性修改事件记录
		TMap<FName, FTireflyAttributeChangeEventPayload> ChangeEventPayloads;

		// 执行对属性基础值的修改计算
		TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
		for (const FTireflyAttributeModifierInstance& Modifier : MergedModifiers)
		{
			if (!Modifier.ModifierDef.ModifierType)
			{
				UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type"),
					*FString(__FUNCTION__),
					*Modifier.ModifierDef.ModifierName.ToString());
				return;
			}

			// 缓存属性基础值的上一次修改最终值
			TMap<FName, float> BaseValuesCached = BaseValues;

			// 执行修改器
			auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
			Execution->Execute(Modifier, BaseValues, BaseValues);

			// 记录属性修改过程
			float* NewValue = BaseValues.Find(Modifier.ModifierDef.AttributeName);
			float* OldValue = BaseValuesCached.Find(Modifier.ModifierDef.AttributeName);
			if (NewValue && OldValue)
			{
				FTireflyAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(Modifier.ModifierDef.AttributeName);
				Payload.AttributeName = Modifier.ModifierDef.AttributeName;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceName);
				PayloadValue += *NewValue - *OldValue;
			}
		}
	
		// 对修改后的属性基础值进行范围修正，然后更新属性基础值
		for (TPair<FName, float>& Pair : BaseValues)
		{
			if (FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
			{
				ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
				if (FMath::IsNearlyEqual(Attribute->BaseValue, Pair.Value))
				{
					continue;
				}

				// 记录属性修改事件的最终结果
				if (FTireflyAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
				{
					Payload->NewValue = Pair.Value;
					Payload->OldValue = Attribute->BaseValue;
				}

				// 把属性基础值的最终修改赋值
				Attribute->BaseValue = Pair.Value;
			}
		}

		// 属性基础值更新广播
		if (!ChangeEventPayloads.IsEmpty())
		{
			TArray<FTireflyAttributeChangeEventPayload> Payloads;
			AttributeComponent->BroadcastAttributeBaseValueChangeEvent(Payloads);
		}
	}

	// 重新计算战斗实体的属性当前值
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity)
	{
		UTireflyAttributeComponent* AttributeComponent = GetAttributeComponent(CombatEntity);
		if (!IsValid(AttributeComponent))
		{
			return;
		}

		// 按类型整理所有属性修改器，方便后续执行修改器合并
		TArray<FTireflyAttributeModifierInstance> MergedModifiers;
		MergeAttributeModifiers(CombatEntity, AttributeComponent->AttributeModifiers, MergedModifiers);
		// 按照优先级对属性修改器进行排序
		MergedModifiers.Sort();

		// 属性修改事件记录
		TMap<FName, FTireflyAttributeChangeEventPayload> ChangeEventPayloads;
		int64 UtcNow = FDateTime::UtcNow().ToUnixTimestamp();

		// 先声明用于更新计算的临时属性值容器
		TMap<FName, float> BaseValues = AttributeComponent->GetAttributeBaseValues();
		TMap<FName, float> CurrentValues = BaseValues;

		// 执行属性修改器的修改计算
		for (const FTireflyAttributeModifierInstance& Modifier : MergedModifiers)
		{
			if (!Modifier.ModifierDef.ModifierType)
			{
				UE_LOG(LogTcsAttrModExec, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierExecution type. Entity: %s"),
					*FString(__FUNCTION__), 
					*Modifier.ModifierDef.ModifierName.ToString(),
					CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
				continue;
			}

			// 缓存属性当前值的上一次修改最终值
			TMap<FName, float> CurrentValuesCached = CurrentValues;

			// 执行修改器
			auto Execution = Modifier.ModifierDef.ModifierType->GetDefaultObject<UTireflyAttributeModifierExecution>();
			Execution->Execute(Modifier, BaseValues, CurrentValues);

			// 记录属性修改过程，需要属性修改器的更新时间为最新
			float* NewValue = CurrentValues.Find(Modifier.ModifierDef.AttributeName);
			float* OldValue = CurrentValuesCached.Find(Modifier.ModifierDef.AttributeName);
			if (NewValue && OldValue && Modifier.UpdateTimestamp == UtcNow)
			{
				FTireflyAttributeChangeEventPayload& Payload = ChangeEventPayloads.FindOrAdd(Modifier.ModifierDef.AttributeName);
				Payload.AttributeName = Modifier.ModifierDef.AttributeName;
				float& PayloadValue = Payload.ChangeSourceRecord.FindOrAdd(Modifier.SourceName);
				PayloadValue += *NewValue - *OldValue;
			}
		}

		// 对修改后的属性当前值进行范围修正，然后更新属性当前值
		for (TPair<FName, float>& Pair : CurrentValues)
		{
			if (FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(Pair.Key))
			{
				ClampAttributeValueInRange(AttributeComponent, Pair.Key, Pair.Value);
				if (FMath::IsNearlyEqual(Attribute->CurrentValue, Pair.Value))
				{
					continue;
				}

				// 记录属性修改事件的最终结果
				if (FTireflyAttributeChangeEventPayload* Payload = ChangeEventPayloads.Find(Pair.Key))
				{
					Payload->NewValue = Pair.Value;
					Payload->OldValue = Attribute->CurrentValue;
				}

				// 把属性当前值的最终修改赋值
				Attribute->CurrentValue = Pair.Value;
			}
		}

		// 属性当前值更新广播
		if (!ChangeEventPayloads.IsEmpty())
		{
			TArray<FTireflyAttributeChangeEventPayload> Payloads;
			AttributeComponent->BroadcastAttributeValueChangeEvent(Payloads);
		}
	}

	// 属性修改器合并
	static void MergeAttributeModifiers(
		const AActor* CombatEntity,	
		const TArray<FTireflyAttributeModifierInstance>& Modifiers,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
	{
		// 按类型整理所有属性修改器，方便后续执行修改器合并
		TMap<FName, TArray<FTireflyAttributeModifierInstance>> ModifiersToMerge;
		for (const FTireflyAttributeModifierInstance& Modifier : Modifiers)
		{
			ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);
		}
	
		// 执行修改器合并
		for (TPair<FName, TArray<FTireflyAttributeModifierInstance>>& Pair : ModifiersToMerge)
		{
			if (Pair.Value.IsEmpty() || !Pair.Value[0].ModifierDef.MergerType)
			{
				UE_LOG(LogTcsAttrModMerger, Warning, TEXT("[%s] AttrModDef %s has no valid AttributeModifierMerger type. Entity: %s"),
					*FString(__FUNCTION__), 
					*Pair.Key.ToString(),
					CombatEntity ? *CombatEntity->GetName() : TEXT("Unknown"));
				continue;
			}

			auto Merger = Pair.Value[0].ModifierDef.MergerType->GetDefaultObject<UTireflyAttributeModifierMerger>();
			Merger->Merge(Pair.Value, MergedModifiers);
		}
	}

	// 将属性的给定值限制在指定范围内
	static void ClampAttributeValueInRange(UTireflyAttributeComponent* AttributeComponent, const FName& AttributeName, float& NewValue)
	{
		if (!IsValid(AttributeComponent))
		{
			return;
		}

		const FTireflyAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
		if (!Attribute)
		{
			return;
		}
		const FTireflyAttributeRange& Range = Attribute->AttributeDef.AttributeRange;

		// 计算属性范围的最小值
		float MinValue = NewValue;
		switch (Range.MinValueType)
		{
		case ETireflyAttributeRangeType::None:
			{
				break;
			}
		case ETireflyAttributeRangeType::Static:
			{
				MinValue = Range.MinValue;
				break;
			}
		case ETireflyAttributeRangeType::Dynamic:
			{
				if (!AttributeComponent->GetAttributeValue(Range.MinValueAttribute, MinValue))
				{
					UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
						*FString(__FUNCTION__),
						*AttributeComponent->GetOwner()->GetName(),
						*Range.MinValueAttribute.ToString(),
						*AttributeName.ToString());
					return;
				}
				break;
			}
		}

		// 计算属性范围的最大值
		float MaxValue = NewValue;
		switch (Range.MaxValueType)
		{
		case ETireflyAttributeRangeType::None:
			{
				break;
			}
		case ETireflyAttributeRangeType::Static:
			{
				MaxValue = Range.MaxValue;
				break;
			}
		case ETireflyAttributeRangeType::Dynamic:
			{
				if (!AttributeComponent->GetAttributeValue(Range.MaxValueAttribute, MaxValue))
				{
					UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
						*FString(__FUNCTION__),
						*AttributeComponent->GetOwner()->GetName(),
						*Range.MaxValueAttribute.ToString(),
						*AttributeName.ToString());
					return;
				}
				break;
			}
		}

		// 属性值范围修正
		NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	}

#pragma endregion
};
```

