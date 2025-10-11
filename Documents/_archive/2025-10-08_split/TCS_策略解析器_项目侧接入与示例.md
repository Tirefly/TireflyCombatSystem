# TCS 策略解析器：项目侧接入与示例（设计文档）

本文给出“项目侧决定策略资产选择与激活”的推荐做法，包含数据结构（文档示意）、接入方式和详细示例。TCS 不内置 LayerPriority/AppliesWhen/TimeWindow 等固定字段，解析逻辑完全由项目自定义；StateManager 只负责“收集 → 评估 → 聚合”。

目录
- 1. 目标与范围
- 2. 数据结构（文档示意）
- 3. 接入方式（接口/委托/多解析器）
- 4. 数据驱动解析器示例（YAML/数据表风格）
- 5. 角色/阵营本地覆盖（Provider 示例）
- 6. 注册与生命周期（启动/关卡/实体）
- 7. 运行时交互（伪代码）
- 8. 性能与调试
- 9. 测试用例建议

——

## 1. 目标与范围
- 由项目侧决定“当前生效的策略集合”（Immunity/Cleanse）。
– 与 StateManager 解耦：解析器只产出策略句柄数组；聚合/评估仍在 StateManager。
- 支持多种维度：模式、关卡、事件、职业/阵营、角色 Archetype 等；可叠加或覆盖。

## 2. 数据结构（文档示意）

```cpp
// 评估上下文（与主设计一致）
USTRUCT(BlueprintType)
struct FTcsEvalContext { GENERATED_BODY();
  UPROPERTY() AActor* Target = nullptr;
  UPROPERTY() AActor* Source = nullptr;
  UPROPERTY() FGameplayTagContainer ContextTags; // 由项目侧提供，如 Mode.PvP/Rank.Boss/Event.*
};

// 选择器句柄（结构式 CDO）
USTRUCT(BlueprintType)
struct FTcsSelectorHandle { GENERATED_BODY();
  UPROPERTY(EditAnywhere) TSubclassOf<UTcsStateSelectorPolicy> SelectorClass;
  UPROPERTY(EditAnywhere) FInstancedStruct SelectorParams; // 与 SelectorClass::GetParamsStructType 对齐
};

// 策略句柄（结构式 CDO）：解耦为两类
USTRUCT(BlueprintType)
struct FTcsImmunityPolicyHandle { GENERATED_BODY();
  UPROPERTY(EditAnywhere) TSubclassOf<UTcsImmunityPolicy> Class; // 免疫策略类
  UPROPERTY(EditAnywhere) FInstancedStruct PolicyParams;        // 与 Class 对应的 Params
};

USTRUCT(BlueprintType)
struct FTcsCleansePolicyHandle { GENERATED_BODY();
  UPROPERTY(EditAnywhere) TSubclassOf<UTcsCleansePolicy> Class; // 净化策略类
  UPROPERTY(EditAnywhere) FInstancedStruct PolicyParams;        // 与 Class 对应的 Params
};

// 条目匹配条件（项目侧定义；示例为 TagQuery/Match 逻辑）
UENUM(BlueprintType)
enum class ETcsContextMatch : uint8 { Any, All };

USTRUCT(BlueprintType)
struct FTcsImmunityPolicyMappingEntry { GENERATED_BODY();
  UPROPERTY(EditAnywhere) FGameplayTagContainer ConditionTags;  // 触发条件（如 Mode.PvP, Rank.Boss）
  UPROPERTY(EditAnywhere) ETcsContextMatch MatchLogic = ETcsContextMatch::All; // 命中逻辑
  UPROPERTY(EditAnywhere) TArray<FTcsImmunityPolicyHandle> Policies; // 命中时追加的免疫策略
};

USTRUCT(BlueprintType)
struct FTcsCleansePolicyMappingEntry { GENERATED_BODY();
  UPROPERTY(EditAnywhere) FGameplayTagContainer ConditionTags;  // 触发条件（如 Event.*）
  UPROPERTY(EditAnywhere) ETcsContextMatch MatchLogic = ETcsContextMatch::All; // 命中逻辑
  UPROPERTY(EditAnywhere) TArray<FTcsCleansePolicyHandle> Policies; // 命中时追加的净化策略
};

// 解析器配置（数据资产示例）
USTRUCT(BlueprintType)
struct FTcsPolicyResolverConfig { GENERATED_BODY();
  UPROPERTY(EditAnywhere) TArray<FTcsImmunityPolicyMappingEntry> ImmunityEntries;
  UPROPERTY(EditAnywhere) TArray<FTcsCleansePolicyMappingEntry>  CleanseEntries;
  // 可选：合并策略（全局）：Append / OverrideByClass
};
```

合并辅助（伪代码）
```cpp
template<typename THandle>
static void UpsertByKey(TArray<THandle>& Out, const FName& Key, const THandle& H)
{
  for (int32 i=Out.Num()-1; i>=0; --i) {
    const FName OtherKey = Out[i].PolicyKey.IsNone() ? FName(Out[i].Class ? Out[i].Class->GetName() : TEXT("")) : Out[i].PolicyKey;
    if (OtherKey == Key) { Out[i] = H; return; }
  }
  Out.Add(H);
}

template<typename THandle>
static FName DerivePolicyKey(const THandle& H)
{
  FString S = H.Class ? H.Class->GetName() : TEXT("None");
  // 将选择器与参数序列化为稳定文本（见下文“键生成与规范”）
  S += TEXT("|"); S += CanonSelector(H);
  return FName(*FString::Printf(TEXT("Pol_%08x"), FCrc::StrCrc32(*S)));
}

template<typename THandle>
static FString CanonSelector(const THandle& H)
{
  // 按项目实际结构实现：Tag/Id 排序、Exceptions 规范化、数值参数格式化等
  return TEXT("<CanonSelector>");
}
```

### 键生成与规范（OverrideByKey）
- 显式键（推荐）：
  - 采用语义化命名：`Domain.Scope.Feature`，如 `Event.Cleanse.Standard`、`Mode.PvP.StunImmune`；
  - 便于在配置与审计时推断覆盖关系。
- 自动派生键（兜底）：
  - 由 `ClassName + Selector(类名 + 规范化 Tag/Id 集合) + 关键参数摘要` 拼接后取哈希作为短键；
  - 规范化建议：
    - GameplayTagContainer：字符串化后按字典序排序，去重；
    - Exceptions：按 ParentTag 排序；每项内 AllowedIds/BlockedIds 分别排序；
    - Id 集合：字典序排序；
    - 数值参数：裁剪到固定小数位（如 Chance 保留 3 位）；
  - 哈希：可用 CRC32/xxHash/CityHash；保证同配置下跨会话稳定。
- 冲突处理与诊断：
  - 同键重复时采用“后覆盖前”（解析器/条目顺序决定）；
  - 建议在合并阶段输出 Trace：`MergeOverride key=... old=ClassA new=ClassB` 便于审计。

说明
- 上述结构为“文档示意”，不代表插件实现；项目可自由裁剪（例如用 DataTable/DT_RowHandle）。
- 免疫与净化的句柄、映射条目与配置完全解耦，便于不同生命周期与合并规则。

## 3. 接入方式（接口/委托/多解析器）

接口型解析器（推荐）
```cpp
UINTERFACE(BlueprintType)
class UTcsPolicyResolver : public UInterface { GENERATED_BODY() };
class ITcsPolicyResolver { GENERATED_BODY()
public:
  virtual void CollectImmunityPolicies(const FTcsEvalContext& Ctx, TArray<FTcsImmunityPolicyHandle>& Out) const = 0;
  virtual void CollectCleansePolicies (const FTcsEvalContext& Ctx, TArray<FTcsCleansePolicyHandle>& Out) const = 0;
};
```

委托型接入（可并存）
```cpp
// 在 StateManager 中提供注册委托/回调的入口（文档示意）
DECLARE_DELEGATE_TwoParams(FTcsCollectImmunityPoliciesDelegate, const FTcsEvalContext& /*Ctx*/, TArray<FTcsImmunityPolicyHandle>& /*Out*/);
DECLARE_DELEGATE_TwoParams(FTcsCollectCleansePoliciesDelegate,  const FTcsEvalContext& /*Ctx*/, TArray<FTcsCleansePolicyHandle>&  /*Out*/);
// StateManager 成员：OnCollectImmunityPolicies, OnCollectCleansePolicies（类型如上）
```

多解析器聚合（可选）
- StateManager 维护解析器数组；Resolve 时按注册顺序依次收集并合并（Append 或按项目规则覆盖）。
- 典型组合：全局解析器 + 关卡解析器 + 角色本地 Provider（见第5节）。

## 4. 数据驱动解析器示例

示例资产（YAML 表达，仅文档）
```yaml
ResolverConfig:
  ImmunityEntries:
    - ConditionTags: [Mode.PvP]
      MatchLogic: All
      Policies:
        - Class: UTcsImmunity_Simple
          PolicyParams:
            Selector:
              Class: UTcsSelector_ByTags
              Params:
                IncludeTags: [State.CC.*]
                MatchLogic: Any
                ExcludeSubtrees: []
                Exceptions: []
                MaxPriority: 999
            Chance: 1.0
            ReasonTag: Rules.PvP.StunImmune

    - ConditionTags: [Rank.Boss]
      MatchLogic: All
      Policies:
        - Class: UTcsImmunity_Simple
          PolicyParams:
            Selector:
              Class: UTcsSelector_ByTags
              Params:
                IncludeTags: [State.CC.Stun, State.CC.Break]
                MatchLogic: Any
                ExcludeSubtrees: []
                Exceptions: []
                MaxPriority: 999
            Chance: 1.0
            ReasonTag: Rules.Boss.StaggerImmune

    - ConditionTags: [Archetype.Assassin]
      MatchLogic: All
      Policies:
        - Class: UTcsImmunity_Simple
          PolicyParams:
            Selector:
              Class: UTcsSelector_ByIds
              Params:
                AllowedIds: ["定身", "沉默"]
                BlockedIds: []
            Chance: 1.0
            ReasonTag: Rules.Assassin.Immune

  CleanseEntries:
    - ConditionTags: [Event.Halloween]
      MatchLogic: All
      Policies:
        - Class: UTcsCleanse_Standard
          PolicyParams:
            Selector:
              Class: UTcsSelector_ByTags
              Params:
                IncludeTags: [State.Debuff.Poison, State.Debuff.Curse]
                MatchLogic: Any
                ExcludeSubtrees: []
                Exceptions: []
                MaxPriority: 100
            MaxRemoveCount: 1
```

合并语义（建议）
- 默认 Append：同类策略并存，由 StateManager 在执行时做“免疫短路（阻断优先）”。
- 若项目希望“后者覆盖前者”，可在 ResolverConfig 设置全局 MergeMode，或在条目上设置 `MergeModeOverride`：
  - OverrideByClass：按策略类合并，后者覆盖前者；
  - OverrideByKey：按 `PolicyKey` 合并，后者覆盖前者；未提供 `PolicyKey` 时由系统基于“类名+选择器+参数指纹”派生稳定键（见下文）。

### 解析器实现草案

```cpp
class UTcsPolicyResolver_DataDriven : public UObject, public ITcsPolicyResolver {
  GENERATED_BODY()
public:
  UPROPERTY(EditAnywhere) FTcsPolicyResolverConfig Config;

  virtual void CollectImmunityPolicies(const FTcsEvalContext& Ctx, TArray<FTcsImmunityPolicyHandle>& Out) const override {
    for (const auto& E : Config.ImmunityEntries) {
      const bool any = Ctx.ContextTags.HasAny(E.ConditionTags);
      const bool all = Ctx.ContextTags.HasAll(E.ConditionTags);
      const bool hit = (E.MatchLogic==ETcsContextMatch::Any ? any : all);
      if (!hit) continue;
      // 合并：按条目覆盖或配置默认
      const ETcsMergeMode Mode = (E.MergeModeOverride != ETcsMergeMode::Append) ? E.MergeModeOverride : Config.ImmunityMergeMode;
      if (Mode == ETcsMergeMode::Append) { Out.Append(E.Policies); }
      else if (Mode == ETcsMergeMode::OverrideByClass) {
        for (const auto& H : E.Policies) {
          const FName Key = FName(H.Class ? H.Class->GetName() : TEXT(""));
          UpsertByKey(Out, Key, H);
        }
      } else /* OverrideByKey */ {
        for (const auto& H : E.Policies) {
          const FName Key = !H.PolicyKey.IsNone() ? H.PolicyKey : DerivePolicyKey(H);
          UpsertByKey(Out, Key, H);
        }
      }
    }
  }
  virtual void CollectCleansePolicies(const FTcsEvalContext& Ctx, TArray<FTcsCleansePolicyHandle>& Out) const override {
    for (const auto& E : Config.CleanseEntries) {
      const bool any = Ctx.ContextTags.HasAny(E.ConditionTags);
      const bool all = Ctx.ContextTags.HasAll(E.ConditionTags);
      const bool hit = (E.MatchLogic==ETcsContextMatch::Any ? any : all);
      if (!hit) continue;
      const ETcsMergeMode Mode = (E.MergeModeOverride != ETcsMergeMode::Append) ? E.MergeModeOverride : Config.CleanseMergeMode;
      if (Mode == ETcsMergeMode::Append) { Out.Append(E.Policies); }
      else if (Mode == ETcsMergeMode::OverrideByClass) {
        for (const auto& H : E.Policies) {
          const FName Key = FName(H.Class ? H.Class->GetName() : TEXT(""));
          UpsertByKey(Out, Key, H);
        }
      } else /* OverrideByKey */ {
        for (const auto& H : E.Policies) {
          const FName Key = !H.PolicyKey.IsNone() ? H.PolicyKey : DerivePolicyKey(H);
          UpsertByKey(Out, Key, H);
        }
      }
    }
  }
};
```

## 5. 角色/阵营本地覆盖（Provider 示例）

接口（示意）
```cpp
UINTERFACE(BlueprintType)
class UTcsPolicyProvider : public UInterface { GENERATED_BODY() };
class ITcsPolicyProvider { GENERATED_BODY()
public:
  virtual void GetLocalImmunityPolicies (TArray<FTcsImmunityPolicyHandle>& Out) const = 0;
  virtual void GetLocalCleansePolicies  (TArray<FTcsCleansePolicyHandle>& Out) const = 0;
};
```

用法
- 某些 Pawn/Component 实现 Provider 接口，提供本地策略（如 Boss 专属、职业专属）。
- StateManager 在 Resolve 前先询问 Target 是否实现 Provider，拿到本地策略再叠加解析器输出。

## 6. 注册与生命周期（启动/关卡/实体）

启动（模块或 GameInstance 初始化）
- 构建全局解析器（如 UTcsPolicyResolver_DataDriven），加载 ResolverConfig，并 `StateManager.SetPolicyResolver(GlobalResolver)`；
- 或注册委托：`StateManager.OnCollectImmunityPolicies.BindUObject(...)`。

关卡/世界（WorldSubsystem 或 LevelScript）
- 在 BeginPlay/地图切换事件中替换/叠加解析器（例如为 PvP 地图注入 `Mode.PvP` Tag）；
- 当 ContextTags 变化较大时，调用 `StateManager.InvalidatePolicies(Context)` 触发缓存失效（见第8节）。

实体（角色/阵营）
- 对关键实体（Boss/职业）实现 Provider；
- 解析顺序建议：Provider → 全局解析器 → 关卡解析器（或相反，按项目语义），最终合并为一个策略集合。

## 7. 运行时交互（伪代码）

```cpp
bool UTcsStateManagerSubsystem::CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& Out)
{
  TArray<FTcsImmunityPolicyHandle> Policies;
  // 1) Provider（可选）
  if (auto* Prov = Cast<ITcsPolicyProvider>(Ctx.Target)) { Prov->GetLocalImmunityPolicies(Policies); }
  // 2) 解析器（可为多个）
  for (ITcsPolicyResolver* R : Resolvers) { R->CollectImmunityPolicies(Ctx, Policies); }

  // 3) 聚合：命中免疫即短路（阻断），否则允许
  bool blocked = false; FGameplayTag reason;
  for (const auto& H : Policies) {
    if (!H.Class) continue;
    auto* CDO = H.Class->GetDefaultObject<UTcsImmunityPolicy>();
    FTcsImmunityDecision D; if (CDO->Evaluate(Def, Ctx, H.PolicyParams, D)) {
      if (!D.bAllowed) { blocked = true; reason = D.ReasonTag; break; }
      reason = D.ReasonTag.IsValid() ? D.ReasonTag : reason;
    }
  }
  Out.bAllowed = !blocked; Out.ReasonTag = reason;
  return Out.bAllowed;
}
```

## 8. 性能与调试
- 缓存：按 `ContextTags`（或其 Hash）缓存解析结果；Context 改变时局部失效。
- 选择器：CDO + FInstancedStruct 参数；ByIds 用 `TSet<FName>`；ByTags 可预编译 TagQuery（若使用）。
- 调试：保留 `tcs.policies.trace on|off [N]`；输出当前解析器、合并前/后策略数量、评估轨迹（详见策略设计文档）。

## 9. 测试用例建议
- 解析命中：Any/All 逻辑；多条目叠加；ImmunityEntries/CleanseEntries 顺序与合并模式验证。
- 聚合正确性：遇免疫短路阻断；ReasonTag 记录。
- 变更响应：切换关卡/模式/事件时缓存失效；角色 Provider 热插拔。
- 稳定性：零策略/空解析器/空 Provider 下的回退路径；大量策略下的性能。

——

附：相关文档
- TCS_状态关系规则系统设计_规则资产与状态管理器.md（策略与选择器设计）
- TCS_调用顺序图_关键模块.md（交互序列总览）
