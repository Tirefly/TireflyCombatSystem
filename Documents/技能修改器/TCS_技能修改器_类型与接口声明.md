TCS 技能修改器：类型与接口声明（Header 级声明汇总）

说明
- 本文集中给出技能修改器相关的“新枚举、结构体、类”的声明，以及所有需要实现的函数原型，便于统一落地。
- 命名与导出宏沿用模块：TIREFLYCOMBATSYSTEM_API。参数名采用 FName。允许扩展型参数结构。
- 仅声明，不包含实现；默认实现类只给出需要覆写/实现的函数签名。

目录
- Skill/Modifiers 核心类型（Definition/Instance/Execution/Merger/Filter/Condition）
- 参数载荷结构（Additive/Multiplicative/Scalar/可扩展）
- 默认实现（执行器/合并器/筛选器/条件）
- 组件聚合接口（UTcsSkillComponent 侧）
- 门面子系统（UTcsSkillManagerSubsystem 侧）
- 设置（UTcsCombatSystemSettings 扩展）

一、Skill/Modifiers 核心类型（补：Skill/State 参数 ByTag API 摘要）

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierDefinition.h

    #pragma once
    #include "CoreMinimal.h"
    #include "Engine/DataTable.h"
    #include "State/TcsState.h"
    #include "TcsSkillModifierDefinition.generated.h"
    
    USTRUCT(BlueprintType)
    struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierDefinition : public FTableRowBase
    {
        GENERATED_BODY();
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") FName ModifierName = NAME_None;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") int32 Priority = 0;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") TSubclassOf<class UTcsSkillFilter> SkillFilter;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") TArray<TSubclassOf<class UTcsSkillModifierCondition>> ActiveConditions;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") FTcsStateParameter ModifierParameter;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") TSubclassOf<class UTcsSkillModifierExecution> ExecutionType;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillModifier") TSubclassOf<class UTcsSkillModifierMerger>   MergeType;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierInstance.h

    #pragma once
    #include "CoreMinimal.h"
    #include "TcsSkillModifierDefinition.h"
    #include "TcsSkillModifierInstance.generated.h"
    class UTcsSkillInstance;
    USTRUCT(BlueprintType)
    struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierInstance
    {
        GENERATED_BODY();
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SkillModifier") FTcsSkillModifierDefinition ModifierDef;
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SkillModifier") int32 SkillModInstanceId = -1;
        UPROPERTY() TObjectPtr<UTcsSkillInstance> SkillInstance = nullptr; // 可空
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SkillModifier") int64 ApplyTime = 0;
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SkillModifier") int64 UpdateTime = 0;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierExecution.h

    #pragma once
    #include "CoreMinimal.h"
    #include "UObject/Object.h"
    #include "TcsSkillModifierExecution.generated.h"
    class UTcsSkillInstance;
    UCLASS(Abstract, Blueprintable)
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierExecution : public UObject
    {
        GENERATED_BODY()
    public:
        UFUNCTION(BlueprintNativeEvent, Category="SkillModifier|Execution")
        void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) struct FTcsSkillModifierInstance& SkillModInstance);
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierMerger.h

    #pragma once
    #include "CoreMinimal.h"
    #include "UObject/Object.h"
    #include "TcsSkillModifierMerger.generated.h"
    UCLASS(Abstract, Blueprintable)
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierMerger : public UObject
    {
        GENERATED_BODY()
    public:
        UFUNCTION(BlueprintNativeEvent, Category="SkillModifier|Merger")
        void Merge(UPARAM(ref) TArray<struct FTcsSkillModifierInstance>& SourceModifiers,
                   TArray<struct FTcsSkillModifierInstance>& OutModifiers);
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillFilter.h

    #pragma once
    #include "CoreMinimal.h"
    #include "UObject/Object.h"
    #include "TcsSkillFilter.generated.h"
    class UTcsSkillInstance;
    UCLASS(Abstract, Blueprintable)
    class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter : public UObject
    {
        GENERATED_BODY()
    public:
        UFUNCTION(BlueprintNativeEvent, Category="SkillModifier|Filter")
        void Filter(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills);
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierCondition.h

    #pragma once
    #include "CoreMinimal.h"
    #include "UObject/Object.h"
    #include "TcsSkillModifierCondition.generated.h"
    class UTcsSkillInstance; class UTcsStateInstance;
    UCLASS(Abstract, Blueprintable)
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierCondition : public UObject
    {
        GENERATED_BODY()
    public:
        UFUNCTION(BlueprintNativeEvent, Category="SkillModifier|Condition")
        bool Evaluate(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance,
                      const struct FTcsSkillModifierInstance& ModInst) const;
    };

二、参数载荷结构（Name/Tag 并立）

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierParams.h

    #pragma once
    #include "CoreMinimal.h"
    #include "TcsSkillModifierParams.generated.h"
    USTRUCT(BlueprintType)
    struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Additive
    {
        GENERATED_BODY()
        UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ParamName; // 可选
        UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Param")) FGameplayTag ParamTag; // 可选
        UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.f;
    };
    USTRUCT(BlueprintType)
    struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Multiplicative
    {
        GENERATED_BODY()
        UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ParamName; // 可选
        UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Param")) FGameplayTag ParamTag; // 可选
        UPROPERTY(EditAnywhere, BlueprintReadWrite) float Multiplier = 1.f;
    };
    USTRUCT(BlueprintType)
    struct TIREFLYCOMBATSYSTEM_API FTcsModParam_Scalar
    {
        GENERATED_BODY()
        UPROPERTY(EditAnywhere, BlueprintReadWrite) float Value = 1.f;
    };

三、默认实现（声明）

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Executions/TcsSkillModExec_AdditiveParam.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierExecution.h"
    #include "TcsSkillModExec_AdditiveParam.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_AdditiveParam : public UTcsSkillModifierExecution
    {
        GENERATED_BODY()
    public:
        virtual void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Executions/TcsSkillModExec_MultiplicativeParam.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierExecution.h"
    #include "TcsSkillModExec_MultiplicativeParam.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_MultiplicativeParam : public UTcsSkillModifierExecution
    {
        GENERATED_BODY()
    public:
        virtual void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Executions/TcsSkillModExec_CooldownMultiplier.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierExecution.h"
    #include "TcsSkillModExec_CooldownMultiplier.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_CooldownMultiplier : public UTcsSkillModifierExecution
    {
        GENERATED_BODY()
    public:
        virtual void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Executions/TcsSkillModExec_CostMultiplier.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierExecution.h"
    #include "TcsSkillModExec_CostMultiplier.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_CostMultiplier : public UTcsSkillModifierExecution
    {
        GENERATED_BODY()
    public:
        virtual void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Mergers/TcsSkillModMerger_NoMerge.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierMerger.h"
    #include "TcsSkillModMerger_NoMerge.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModMerger_NoMerge : public UTcsSkillModifierMerger
    {
        GENERATED_BODY()
    public:
        virtual void Merge(UPARAM(ref) TArray<FTcsSkillModifierInstance>& SourceModifiers,
                           TArray<FTcsSkillModifierInstance>& OutModifiers) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Mergers/TcsSkillModMerger_CombineByParam.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierMerger.h"
    #include "TcsSkillModMerger_CombineByParam.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModMerger_CombineByParam : public UTcsSkillModifierMerger
    {
        GENERATED_BODY()
    public:
        virtual void Merge(UPARAM(ref) TArray<FTcsSkillModifierInstance>& SourceModifiers,
                           TArray<FTcsSkillModifierInstance>& OutModifiers) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Filters/TcsSkillFilter_ByQuery.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillFilter.h"
    #include "Skill/TcsSkillComponent.h" // 为 FTcsSkillQuery
    #include "TcsSkillFilter_ByQuery.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter_ByQuery : public UTcsSkillFilter
    {
        GENERATED_BODY()
    public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Query") FTcsSkillQuery Query;
        virtual void Filter(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Filters/TcsSkillFilter_ByDefIds.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillFilter.h"
    #include "TcsSkillFilter_ByDefIds.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter_ByDefIds : public UTcsSkillFilter
    {
        GENERATED_BODY()
    public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter") TArray<FName> SkillDefIds;
        virtual void Filter(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills) override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Conditions/TcsSkillModCond_AlwaysTrue.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierCondition.h"
    #include "TcsSkillModCond_AlwaysTrue.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_AlwaysTrue : public UTcsSkillModifierCondition
    {
        GENERATED_BODY()
    public:
        virtual bool Evaluate(AActor* Owner, UTcsSkillInstance* SkillInstance, class UTcsStateInstance* ActiveStateInstance,
                              const FTcsSkillModifierInstance& ModInst) const override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Conditions/TcsSkillModCond_SkillHasTags.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierCondition.h"
    #include "TcsSkillModCond_SkillHasTags.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_SkillHasTags : public UTcsSkillModifierCondition
    {
        GENERATED_BODY()
    public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredTags;
        UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer BlockedTags;
        virtual bool Evaluate(AActor* Owner, UTcsSkillInstance* SkillInstance, class UTcsStateInstance* ActiveStateInstance,
                              const FTcsSkillModifierInstance& ModInst) const override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Conditions/TcsSkillModCond_SkillLevelInRange.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierCondition.h"
    #include "TcsSkillModCond_SkillLevelInRange.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_SkillLevelInRange : public UTcsSkillModifierCondition
    {
        GENERATED_BODY()
    public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinLevel = 1;
        UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxLevel = 9999;
        virtual bool Evaluate(ACTOR* Owner, UTcsSkillInstance* SkillInstance, class UTcsStateInstance* ActiveStateInstance,
                              const FTcsSkillModifierInstance& ModInst) const override;
    };

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/Conditions/TcsSkillModCond_StateStageEquals.h

    #pragma once
    #include "Skill/Modifiers/TcsSkillModifierCondition.h"
    #include "State/TcsState.h" // ETcsStateStage
    #include "TcsSkillModCond_StateStageEquals.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_StateStageEquals : public UTcsSkillModifierCondition
    {
        GENERATED_BODY()
    public:
        UPROPERTY(EditAnywhere, BlueprintReadWrite) TEnumAsByte<ETcsStateStage> RequiredStage = ETcsStateStage::SS_Active;
        virtual bool Evaluate(AActor* Owner, UTcsSkillInstance* SkillInstance, class UTcsStateInstance* ActiveStateInstance,
                              const FTcsSkillModifierInstance& ModInst) const override;
    };

四、组件聚合接口（UTcsSkillComponent 扩展声明｜Name/Tag 双空间）

文件：Source/TireflyCombatSystem/Public/Skill/TcsSkillComponent.h（新增/扩展）

    USTRUCT()
    struct TIREFLYCOMBATSYSTEM_API FTcsAggregatedParamEffect
    {
        GENERATED_BODY()
        UPROPERTY() float AddSum = 0.f;
        UPROPERTY() float MulProd = 1.f;
        UPROPERTY() bool  bHasOverride = false;
        UPROPERTY() float OverrideValue = 0.f;
        UPROPERTY() float CooldownMul = 1.f;
        UPROPERTY() float CostMul = 1.f;
    };
    UPROPERTY() TArray<FTcsSkillModifierInstance> ActiveSkillModifiers;
    UPROPERTY() TMap<UTcsSkillInstance*, TMap<FName, FTcsAggregatedParamEffect>>        NameAggCache;
    UPROPERTY() TMap<UTcsSkillInstance*, TMap<FGameplayTag, FTcsAggregatedParamEffect>> TagAggCache;
    UPROPERTY() TMap<UTcsSkillInstance*, TSet<FName>>                                   DirtyNames;
    UPROPERTY() TMap<UTcsSkillInstance*, TSet<FGameplayTag>>                            DirtyTags;
    UFUNCTION(BlueprintCallable, Category="Skill|Modifiers")
    bool ApplySkillModifiers(const TArray<FTcsSkillModifierDefinition>& Defs, TArray<int32>& OutInstanceIds);
    UFUNCTION(BlueprintCallable, Category="Skill|Modifiers")
    void RemoveSkillModifierById(int32 InstanceId);
    UFUNCTION(BlueprintCallable, Category="Skill|Modifiers")
    void UpdateSkillModifiers(float DeltaTime);
    UFUNCTION(BlueprintPure, Category="Skill|Modifiers")
    bool GetAggregatedParamEffect(const UTcsSkillInstance* Skill, FName ParamName, FTcsAggregatedParamEffect& OutEffect) const;

    UFUNCTION(BlueprintPure, Category="Skill|Modifiers")
    bool GetAggregatedParamEffectByTag(const UTcsSkillInstance* Skill, FGameplayTag ParamTag, FTcsAggregatedParamEffect& OutEffect) const;
    void OnStateInstanceAdded(class UTcsStateInstance* State);
    void OnStateInstanceRemoved(class UTcsStateInstance* State);
    void OnStateStageChanged(class UTcsStateInstance* State, TEnumAsByte<ETcsStateStage> OldStage, TEnumAsByte<ETcsStateStage> NewStage);
    void OnOwnerTagsChanged();
    void OnSkillLevelChanged(UTcsSkillInstance* Skill, int32 OldLevel, int32 NewLevel);
    void MarkSkillParamDirty(UTcsSkillInstance* Skill, FName ParamName);
    void MarkSkillParamDirtyByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag);
    void RebuildAggregatedForSkill(UTcsSkillInstance* Skill); // 同时处理 Name/Tag 两套空间
    void RebuildAggregatedForSkillParam(UTcsSkillInstance* Skill, FName ParamName);
    void RebuildAggregatedForSkillParamByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag);
    void RunFilter(const FTcsSkillModifierDefinition& Def, TArray<UTcsSkillInstance*>& OutSkills) const;
    bool EvaluateConditions(AActor* Owner, UTcsSkillInstance* Skill, class UTcsStateInstance* ActiveState, const FTcsSkillModifierInstance& Inst) const;
    void MergeAndSortModifiers(TArray<FTcsSkillModifierInstance>& InOutModifiers) const;

五、门面子系统（UTcsSkillManagerSubsystem 扩展声明）

文件：Source/TireflyCombatSystem/Public/Skill/TcsSkillManagerSubsystem.h（扩展）

    UFUNCTION(BlueprintCallable, Category="Skill Manager")
    bool ApplySkillModifier(AActor* TargetActor, const TArray<FTcsSkillModifierDefinition>& Modifiers, TArray<int32>& OutInstanceIds);
    UFUNCTION(BlueprintCallable, Category="Skill Manager")
    void RemoveSkillModifierById(AActor* TargetActor, int32 InstanceId);
    UFUNCTION(BlueprintCallable, Category="Skill Manager")
    void UpdateCombatEntity(AActor* TargetActor, float DeltaTime);

六、设置（UTcsCombatSystemSettings 扩展）

文件：Source/TireflyCombatSystem/Public/TcsCombatSystemSettings.h（扩展）

    UPROPERTY(EditDefaultsOnly, Config, Category="Data")
    TSoftObjectPtr<class UDataTable> SkillModifierDefTable;

七、可选：工具与校验（如需）

文件：Source/TireflyCombatSystem/Public/Skill/Modifiers/TcsSkillModifierUtils.h

    #pragma once
    #include "CoreMinimal.h"
    #include "TcsSkillModifierDefinition.h"
    #include "TcsSkillModifierUtils.generated.h"
    UCLASS()
    class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierUtils : public UObject
    {
        GENERATED_BODY()
    public:
        UFUNCTION(BlueprintCallable, Category="SkillModifier|Validate")
        static bool ValidateDefinitionRow(const FTcsSkillModifierDefinition& Def, FString& OutError);
    };
文件：Source/TireflyCombatSystem/Public/Skill/TcsSkillInstance.h（ByTag API 摘要）

    // Bool/Vector ByTag
    UFUNCTION(BlueprintPure,   Category="Skill Instance|Parameters|ByTag") bool    GetBoolParameterByTag(FGameplayTag ParamTag, bool Default=false) const;
    UFUNCTION(BlueprintCallable,Category="Skill Instance|Parameters|ByTag") void    SetBoolParameterByTag(FGameplayTag ParamTag, bool bValue);
    UFUNCTION(BlueprintPure,   Category="Skill Instance|Parameters|ByTag") FVector  GetVectorParameterByTag(FGameplayTag ParamTag, const FVector& Default=FVector::ZeroVector) const;
    UFUNCTION(BlueprintCallable,Category="Skill Instance|Parameters|ByTag") void    SetVectorParameterByTag(FGameplayTag ParamTag, const FVector& Value);
    // Numeric ByTag
    UFUNCTION(BlueprintCallable,Category="Skill Instance|Parameters|ByTag") float   CalculateNumericParameterByTag(FGameplayTag ParamTag, AActor* Instigator=nullptr, AActor* Target=nullptr) const;
    UFUNCTION(BlueprintPure,   Category="Skill Instance|Parameters|ByTag") float   GetSnapshotParameterByTag(FGameplayTag ParamTag, float Default=0.f) const;

文件：Source/TireflyCombatSystem/Public/State/TcsStateInstance.h（ByTag API 摘要）

    UFUNCTION(BlueprintCallable,Category="State Instance|Parameters|ByTag") void   SetNumericParamByTag(FGameplayTag ParamTag, float Value);
    UFUNCTION(BlueprintPure,   Category="State Instance|Parameters|ByTag") bool   GetNumericParamByTag(FGameplayTag ParamTag, float& OutValue) const;
