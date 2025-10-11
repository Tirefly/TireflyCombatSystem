# TCS 阶段划分：暂缓设计（策略解析器 / 免疫 / 净化）

说明
- 本文聚合当前仍处于设计讨论阶段、暂不落地的内容：策略解析器、免疫策略、净化策略、选择器策略及其在管线中的调用与示例。
- 与实现阶段直接相关的“非策略部分”（顶层 StateTree、槽位、实例、激活模式、合并器、计时/抢占/排队/调试等）已迁移至：TCS_阶段划分_即将开发_非策略部分.md。
- 本文保留原有设计文档的全部策略相关信息，并补充跨文档的引用段落，便于后续继续推进。

---

# 策略系统设计全文
# TCS 状态策略系统设计（新版）：策略资产与状态管理器

本版将“规则层”简化为“策略层”，把依赖、互斥、Gate、清槽、打断等编排全部交给顶层 StateTree；规则层仅保留“免疫（Immunity）”与“净化（Cleanse）”。与“抗性/韧性”等渐进型数值相关的功能交由项目侧属性系统处理，不在策略层表达。

## 0. 变更摘要
- 逻辑编排回归 StateTree：依赖、互斥、Gate、清槽、打断由 StateTree 的 EnterCondition、TransitionRule、OnEnter、OnExit 表达。
- 策略化配置：引入策略类 UTcsImmunityPolicy 与 UTcsCleansePolicy，支持运行时动态评估（可含几率与上下文）。
- 同槽策略统一：ActivationMode 使用 'PriorityOnly'、'PriorityHangUp'、'AllActive' 三种语义。
- 轻量前置 CheckImmunity：仅做免疫判定（允许/拒绝），不在策略层返回“缩放/抗性”。
- 废弃：FTcsSlotRelationRow、Dependencies、Exclusions、RuleSet（如需审计可做导出工具）。

## 1. 架构与职责
- 顶层 StateTree（唯一逻辑中枢）
  - 负责域级 Gate 与时序、依赖判断、互斥、清槽/打断、跨槽关系；
  - 通过子状态树复用通用域（CC 域、Channel 域、元素域等）；
  - 可直接调用 Cleanse 策略执行净化。
- 槽位与组件（UTcsStateComponent）
  - 存储、优先级、激活；
  - 同槽策略统一于 ActivationMode：'PriorityOnly'、'PriorityHangUp'、'AllActive'；
  - 同 DefId 合并由 UTcsStateMerger_* 处理（UseNewest、UseOldest、Stack、NoMerge）。
- 状态管理器子系统（UTcsStateManagerSubsystem）
  - 提供策略相关 API：CheckImmunity / Cleanse（在此进行策略聚合与评估）；
  - 注册项目侧策略解析器/提供者（由项目决定当前生效的策略集合）；
  - 仅关注 Immunity/Resist 与 Cleanse；依赖/互斥/清槽/打断由顶层 StateTree 表达。

## 2. 调用顺序（新版）
- 轻量前置（可选）：StateManager.CheckImmunity 调用 ImmunityPolicy.Evaluate；
  - 若拒绝：返回阻断（Immunity/Context）；若允许：直接进入分配（不返回“缩放/抗性”）。
- 分配：Component.AssignStateToStateSlot；
  - 同 DefId 合并（UTcsStateMerger_*）→ ActivationMode（PriorityOnly 或 PriorityHangUp 或 AllActive）→ 排序与激活；
- 顶层时序：StateTree 的条件与任务完成 Gate/清槽/打断/依赖判断；必要时调用 CleansePolicy。
  

## 3. 数据结构（结构式 CDO）
- 选择器策略（结构式 CDO）
  - 作用：用于免疫/净化的“命中范围”判定（仅两类：按 Id、按 Tag）。
  - 基类（仅文档示意，非实现）：

```cpp
// 视为文档中的接口示意：CDO 不携带数据；参数通过 FInstancedStruct 传入
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UTcsStateSelectorPolicy : public UObject {
  GENERATED_BODY()
public:
  virtual UScriptStruct* GetParamsStructType() const PURE_VIRTUAL(UTcsStateSelectorPolicy::GetParamsStructType, return nullptr;);

  // 注意：直接传 StateDef（而非中间 View），与 CheckImmunity 的调用点一致
  UFUNCTION(BlueprintNativeEvent)
  bool MatchesDef(const FTcsStateDefinition& StateDef, const FTcsEvalContext& Ctx, const FInstancedStruct& Params) const;

  virtual bool ValidateParams(const FInstancedStruct& Params) const {
    return Params.IsValid() && Params.GetScriptStruct() == GetParamsStructType();
  }
};

// 选择器句柄：策略参数中以此形式引用具体选择器 + 其参数
USTRUCT(BlueprintType)
struct FTcsSelectorHandle {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere) TSubclassOf<UTcsStateSelectorPolicy> SelectorClass;
  UPROPERTY(EditAnywhere) FInstancedStruct SelectorParams; // 与 SelectorClass::GetParamsStructType 对齐
  bool MatchesDef(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx) const;
};
```

- 内建选择器（仅文档示意，非实现）
  - ByIds（UTcsSelector_ByIds）：只做精确 Id 判断；不参与优先级
  - ByTags（UTcsSelector_ByTags）：包含/排除/例外 + 优先级上限（仅 Tag 选择器考虑优先级）

```cpp
// — ByIds —
USTRUCT(BlueprintType)
struct FTcsSelectorByIdsParams {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere) TSet<FName> AllowedIds; // 为空则不命中
  UPROPERTY(EditAnywhere) TSet<FName> BlockedIds; // 先黑后白
};

// — ByTags —
UENUM(BlueprintType)
enum class ETcsTagMatchLogic : uint8 { Any, All };

USTRUCT(BlueprintType)
struct FTcsTagIdExceptions {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, meta=(Categories="State")) FGameplayTag ParentTag; // 例外的作用域（父级）
  UPROPERTY(EditAnywhere) TSet<FName> AllowedIds; // 白名单打洞
  UPROPERTY(EditAnywhere) TSet<FName> BlockedIds; // 局部黑名单
};

USTRUCT(BlueprintType)
struct FTcsSelectorByTagsParams {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, meta=(Categories="State")) FGameplayTagContainer IncludeTags; // 命中集合（父级即含后代）
  UPROPERTY(EditAnywhere) ETcsTagMatchLogic MatchLogic = ETcsTagMatchLogic::Any; // Any/All
  UPROPERTY(EditAnywhere, meta=(Categories="State")) FGameplayTagContainer ExcludeSubtrees; // 排除整棵子树
  UPROPERTY(EditAnywhere) TArray<FTcsTagIdExceptions> Exceptions; // 按父级限定的 Id 例外
  UPROPERTY(EditAnywhere) int32 MaxPriority = INT32_MAX; // 仅 Tag 选择器考虑
};
```

- 策略类（结构式 CDO，文档示意）
  - 免疫：UTcsImmunityPolicy
    - Evaluate(const FTcsStateDefinition& StateDef, const FTcsEvalContext& Ctx, const FInstancedStruct& PolicyParams, FTcsImmunityDecision& Out)
  - 净化：UTcsCleansePolicy
    - Apply(const FTcsEvalContext& Ctx, const FInstancedStruct& PolicyParams) → int32 CleanedCount
  - 公共类型：

```cpp
USTRUCT(BlueprintType)
struct FTcsImmunityDecision { GENERATED_BODY();
  UPROPERTY() bool bAllowed = true;        // false 表示免疫命中（阻断）
  UPROPERTY() FGameplayTag ReasonTag;      // 诊断/调试用
};

// 示例参数：免疫（简单阻断）
USTRUCT(BlueprintType)
struct FTcsImmunity_SimpleParams { GENERATED_BODY();
  UPROPERTY(EditAnywhere) FTcsSelectorHandle Selector; // 命中范围
  UPROPERTY(EditAnywhere) float Chance = 1.f;   // 可选：0..1 几率（若不需要，设为1）
  UPROPERTY(EditAnywhere) FGameplayTag ReasonTag;
};

// 示例参数：净化（标准移除/减层）
USTRUCT(BlueprintType)
struct FTcsCleanseParams { GENERATED_BODY();
  UPROPERTY(EditAnywhere) FTcsSelectorHandle Selector; // 命中范围
  UPROPERTY(EditAnywhere) int32 MaxRemoveCount = -1;   // -1 表示移除所有命中的状态
};
```

## 4. 状态管理器中的策略 API（文档示意）
- Initialize：在 UTcsStateManagerSubsystem 中注册项目侧“策略解析器/提供者”（由项目决定如何选择/激活策略集）
- 主要接口（伪代码，签名示意）：

```cpp
class UTcsStateManagerSubsystem {
public:
  // 免疫判定（轻量前置）
  bool CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& OutDecision) const;

  // 应用状态（内部可先调用 CheckImmunity）
  bool ApplyState(AActor* Target, const FTcsStateDefinition& Def, AActor* Source, const FTcsEvalContext& Ctx, /*out*/FTcsStateHandle& OutHandle);

  // 净化：遍历项目侧提供的净化策略；可叠加任务内联 Selector+Params
  int32 Cleanse(AActor* Target, const FTcsEvalContext& Ctx, const FTcsSelectorHandle& InlineSelector, const FTcsCleanseParams& InlineParams) const;

  // 解析器/委托接入（项目侧）
  void AddPolicyResolver(ITcsPolicyResolver* Resolver);
  void ClearPolicyResolvers();
  FTcsCollectPoliciesDelegate OnCollectImmunityPolicies; // 可选
  FTcsCollectPoliciesDelegate OnCollectCleansePolicies;  // 可选
};
```

- 说明：依赖、互斥、Gate 不在策略层合并，统一由 StateTree 表达；策略可按上下文启用/禁用；建议“先硬规则后软规则”。

## 5. 策略选择与激活（项目侧）
- 原则：TCS 不规定策略资产的选择/激活方式；由项目自行决定（模式、关卡、角色类别、难度、活动等）。
- 接入方式（示意，非实现）：

```cpp
// 方式A：项目侧“策略解析器”接口
UINTERFACE(BlueprintType) class UTcsPolicyResolver : public UInterface { GENERATED_BODY() };
class ITcsPolicyResolver { GENERATED_BODY()
public:
  virtual void CollectImmunityPolicies(const FTcsEvalContext& Ctx, TArray<FTcsImmunityPolicyHandle>& Out) const = 0;
  virtual void CollectCleansePolicies (const FTcsEvalContext& Ctx, TArray<FTcsCleansePolicyHandle>&  Out) const = 0;
};

// 方式B：在子系统中注册委托/函数回调（项目启动时注入）
// e.g. FTcsCollectImmunityPoliciesDelegate OnCollectImmunityPolicies; FTcsCollectCleansePoliciesDelegate OnCollectCleansePolicies;
```

- 解析样例（由项目自定）：
  - 按 `ContextTags`（如 Mode.PvP / Rank.Boss / Event.*）拼装策略集；
  - 按“角色 Archetype/职业/阵营”等选择不同策略包；
  - 运行时可热切换或叠加（不要求 LayerPriority/TimeWindow 等固定字段）。
- 槽位关系（推荐做法）
  - 不再使用独立表；在 StateTree 的 OnEnter/OnExit 任务中显式编排关闭/打开 Gate、清槽/打断；
  - 如需审计/可视化，可做导出工具将节点动作聚合成报告。
- 元素关系（模板）
  - Counter：如 '水克火' → 关闭 'StateSlot.Debuff.Fire' Gate，打开 'StateSlot.Buff.Water.Resonance' Gate；
  - Generate：如 '木生火' → 打开 'StateSlot.Buff.Fire.Resonance' Gate（带窗口时长）。

## 6. 使用示例（片段）
- ApplyState（轻量前置）
  - 调用 StateManager.CheckImmunity；若拒绝则不创建实例；若允许直接进入分配（不做“缩放/抗性”）。
- AssignStateToStateSlot（合并 + 同槽策略 + 激活）
  - 合并 UTcsStateMerger_* → 按 ActivationMode 处理同槽：
    - PriorityOnly：仅最高优先级 Active，低优先 Cancel；
    - PriorityHangUp：仅最高优先级 Active，低优先 HangUp（暂停 Tick，不 Stop/Exit），高优先退出时恢复；
    - AllActive：并发 Active；
  - UpdateStateSlotActivation。
- 顶层 Cleanse 任务（YAML 示例，仅文档）

```yaml
Task: Cleanse
Params:
  # 选择器策略句柄（结构式 CDO）
  Selector:
    Class: UTcsSelector_ByTags
    Params:
      IncludeTags: [State.Debuff.*, State.CC.Slow, State.CC.Root]
      MatchLogic: Any
      ExcludeSubtrees: []
      Exceptions: []   # 可按父级Tag增加 AllowedIds/BlockedIds
      MaxPriority: 100 # 仅 Tag 选择器考虑

  # 标准净化参数（示例）
  MaxRemoveCount: -1      # -1 表示移除所有命中的状态
```

## 7. 多策略/上下文管理（分层 + 上下文 + 调试）
- 激活：由项目侧策略解析器/回调决定当前生效的策略集合；
- 顺序：仅硬规则（免疫命中即阻断）；
- 追踪：记录最近 N 次 CheckImmunity 的 ReasonTag；提供控制台/面板查看当前激活 Policies 与评估顺序。

## 8. 迁移建议
- 先接入 CheckImmunity（仅免疫判定）；
- 逐步把互斥/净化从技能/蓝图迁到 StateTree/策略层；
- 用子状态树减少 StateTree 碎片化，保持复用与可读性。

## 9. 免疫评估与调试（建议）
- 判定：策略仅返回“命中免疫 → 阻断/允许 + ReasonTag”；不返回缩放/抗性。
- 聚合：遇任一免疫命中即短路（阻断），否则允许。
- 调试追踪（建议字段）
  - Timestamp / Target / Source / StateDefId
  - PolicyClass / SelectorClass
  - SelectorParams 摘要（如 Id 列表大小、Tag 根数、MaxPriority）
  - Decision：Blocked/Allowed；ReasonTag
- 工具与可视化
  - 控制台命令：`tcs.policies.trace on|off [N]` 开关与容量；
  - 面板：当前生效策略列表（排序）、最近 N 次评估记录；
  - 导出：将最近评估写入 CSV/JSON 以便问题复现与比对。

——
附注：本文不再描述 RuleSet、Dependencies、Exclusions、FTcsSlotRelationRow 的合并与用法；如需兼容或审计，请用导出工具从 StateTree 的 OnEnter/OnExit 动作生成视图。
\n---\n\n# 策略解析器：项目侧接入与示例（全文）
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
\n---\n\n# 附：调用顺序中的策略相关（摘录）
# TCS 调用顺序图：关键模块（设计示意）

说明：本文汇总当前 TCS 项目若干关键模块的典型调用顺序，作为设计阶段的参考。仅示意，不代表最终实现细节。应你的建议，本稿将顺序图改为“伪代码 + 要点”。

目录
- 0) API 概览（并入 StateManager 后）
- 1) ApplyState 管线（含 CanApplyLight）
- 2) 顶层 StateTree ↔ 槽位 Gate 联动
- 3) AssignStateToStateSlot 与 ActivationMode/合并器
- 4) Cleanse 任务（选择器策略句柄 + 标准净化参数）
- 5) 同槽抢占与挂起（PriorityHangUp + PreemptionPolicy）
- 6) 持续时间计时策略（ETcsDurationTickPolicy）
- 7) 策略解析器注入与调用（项目侧）

——
## 0) API 概览（并入 StateManager 后）

伪代码（接口签名，仅文档）：
```cpp
// StateManager 对外主要接口
class UTcsStateManagerSubsystem {
public:
  bool CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& OutDecision) const; // 仅免疫判定
  bool ApplyState(AActor* Target, const FTcsStateDefinition& Def, AActor* Source, const FTcsEvalContext& Ctx, /*out*/FTcsStateHandle& OutHandle);
  int32 Cleanse(AActor* Target, const FTcsEvalContext& Ctx, const FTcsSelectorHandle& InlineSelector, const FTcsCleanseParams& InlineParams) const;

  // 策略解析器接入（项目侧）
  void AddPolicyResolver(ITcsPolicyResolver* Resolver);
  void ClearPolicyResolvers();
  FTcsCollectPoliciesDelegate OnCollectImmunityPolicies; // 可选委托
  FTcsCollectPoliciesDelegate OnCollectCleansePolicies;  // 可选委托
};
```

## 1) ApplyState 管线（含 CheckImmunity）

伪代码：
```cpp
bool UTcsStateManagerSubsystem::ApplyState(AActor* Target, const FTcsStateDefinition& Def, AActor* Source, const FTcsEvalContext& Ctx, FTcsStateHandle& Out)
{
  // 1) 轻量前置：免疫判定
  FTcsImmunityDecision Dec; if (!CheckImmunity(Def, Ctx, Dec)) { return false; }

  // 2) 交给组件按槽位分配
  UTcsStateComponent* SC = GetStateComponent(Target);
  if (!SC) return false;
  const bool ok = SC->AssignStateToStateSlot(Def);
  if (ok) { Out = MakeHandle(Target, Def); }
  return ok;
}

bool UTcsStateManagerSubsystem::CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& OutDec) const
{
  TArray<FTcsImmunityPolicyHandle> Policies; // 来自 Provider + 解析器 + 委托（见第7节）
  CollectImmunityPolicies(Ctx, Policies);

  FGameplayTag reason;
  for (const FTcsImmunityPolicyHandle& H : Policies) {
    if (!H.Class) continue;
    const auto* CDO = H.Class->GetDefaultObject<UTcsImmunityPolicy>();
    FTcsImmunityDecision D; const bool hit = CDO->Evaluate(Def, Ctx, H.PolicyParams, D);
    if (!hit) continue;
    if (!D.bAllowed) { OutDec = { false, D.ReasonTag }; return false; } // 命中免疫即阻断
    reason = D.ReasonTag.IsValid() ? D.ReasonTag : reason; // 诊断用途
  }
  OutDec = { true, reason };
  return true;
}
```

要点
- CheckImmunity：策略仅返回“命中免疫 → 阻断/允许 + ReasonTag”；无“缩放/抗性”。
- 失败早返回，避免创建实例与后续分配开销。

## 4) Cleanse 任务（选择器策略句柄 + 标准净化参数）

伪代码：
```cpp
int32 UTcsStateManagerSubsystem::Cleanse(AActor* Target, const FTcsEvalContext& Ctx, const FTcsSelectorHandle& InlineSelector, const FTcsCleanseParams& InlineParams) const
{
  TArray<FTcsCleansePolicyHandle> Policies; // 解析器 + 委托提供的净化策略
  CollectCleansePolicies(Ctx, Policies);

  // 将任务内联的 Selector+Params 组装成一个“临时净化策略”并加入队列（可选）
  Policies.Insert(MakeInlineCleansePolicy(InlineSelector, InlineParams), 0);

  int32 total = 0; UTcsStateComponent* SC = GetStateComponent(Target);
  for (const FTcsCleansePolicyHandle& H : Policies) {
    if (!H.Class) continue;
    const auto* CP = H.Class->GetDefaultObject<UTcsCleansePolicy>();
    total += CP->Apply(Ctx, H.PolicyParams); // 策略内部：遍历 SC 的状态，按 Selector 命中后执行移除
  }
  return total;
}
```

要点
- Cleanse 的命中范围由“选择器策略句柄”表达（ByIds/ByTags）。
- 标准净化参数：MaxRemoveCount（-1 表示全部）。
- 项目侧可决定 CleansePolicies 的选择逻辑；也可仅使用“标准净化策略”。

## 7) 策略解析器注入与调用（项目侧）

伪代码：
```cpp
// 启动/配置
void GameModule::StartupModule()
{
  ITcsPolicyResolver* GlobalResolver = NewObject<UTcsPolicyResolver_DataDriven>(...);
  StateManager.AddPolicyResolver(GlobalResolver);
  // 或：注册委托 StateManager.OnCollectImmunityPolicies.BindUObject(...)
}

// 收集策略（StateManager 内）
void UTcsStateManagerSubsystem::CollectImmunityPolicies(const FTcsEvalContext& Ctx, TArray<FTcsImmunityPolicyHandle>& Out) const
{
  // 1) Provider（目标本地策略）
  if (auto* Prov = Cast<ITcsPolicyProvider>(Ctx.Target)) { Prov->GetLocalImmunityPolicies(Out); }
  // 2) 解析器（全局/关卡等）
  for (ITcsPolicyResolver* R : Resolvers) { R->CollectImmunityPolicies(Ctx, Out); }
  // 3) 委托（可选）
  if (OnCollectImmunityPolicies.IsBound()) { OnCollectImmunityPolicies.Execute(Ctx, Out); }
}
```

要点
- 解析器按项目需求实现：可基于 ContextTags/关卡/模式/职业等选择策略集合。
- StateManager 仅负责调用解析器并执行评估/聚合逻辑。

——

附：相关文档
- TCS_状态关系规则系统设计_规则资产与状态管理器.md
- TCS_状态系统整体方案_顶层StateTree_槽位_实例联动与扩展.md
\n---\n\n# 附：被剥离的小节（来自非策略文档）
\n## 来自 TCS_状态系统整体方案：附：策略调用时机（设计约定）
附：策略调用时机（设计约定）
- 轻量前置（CheckImmunity）：在 `ApplyState()` 管线开始处，仅评估免疫；
  - 若命中免疫：直接拒绝应用（不创建实例）；
  - 否则：进入分配流程（不返回“缩放/抗性”）。
- 净化（Cleanse）：由顶层 StateTree 的任务在需要时触发（例如进入某状态节点 OnEnter 调用“Cleanse”任务），传入“选择器策略句柄 + 策略参数”以执行移除。
\n## 来自 TCS_顶层StateTree与槽位映射联动实现方案：策略调用时机（设计）
  - 策略调用时机（设计）：
    - 免疫（CheckImmunity）：发生在 `ApplyState` 管线开始处；若命中免疫则不创建实例；未命中则进入分配（不返回“缩放/抗性”）。
    - 净化（Cleanse）：在顶层 StateTree 的合适节点（如 OnEnter/OnExit）通过“Cleanse 任务”触发，传入“选择器策略句柄 + 策略参数”执行移除。
\n---\n\n# 附：示例中的策略/免疫/净化片段（摘录）
\n## 英雄联盟：策略与净化片段
            OnEnter: ['注册一次性格挡: 命中[State.CC.*,State.Debuff.*]→阻断并清空自身']
            OnEnter: ['规则层对State.CC.(Stun|Root|Silence)返回免疫（阻断）']
## 5. 策略资产（Policies）

```yaml
SlotRelations:
  - SlotTag: StateSlot.CC.*
    CloseGatesOnEnter:   [StateSlot.Action.Cast, StateSlot.Action.Channel, StateSlot.Mobility.Dash, StateSlot.Mobility.Move]
    OpenGatesOnExit:     [StateSlot.Action.Cast, StateSlot.Mobility.Move]
    Notes: '进入任意控制时关闭动作/移动；退出后恢复基础动作/移动'

  - SlotTag: StateSlot.SpellShield
    OneShotBlockSelectors: [State.CC.*, State.Debuff.*]
    Notes: '法术护盾命中一次后清空自身'

  - SlotTag: StateSlot.Action.Channel
    CloseGatesOnEnter:   [StateSlot.Mobility.Dash]
    Interruptors:        [StateSlot.CC.Stun, StateSlot.CC.KnockUp, StateSlot.CC.Suppress]
    ClearSlotsOnInterrupt: [StateSlot.Action.Channel]
    Notes: '引导被强控中断并清槽'
```
- 节点 '召唤师_净化'（OnEnter）
  - Cleanse 任务参数（YAML，仅示意）

```yaml
Task: Cleanse
  Params:
  Selector:
    Class: UTcsSelector_ByTags
    Params:
      IncludeTags: [State.Debuff.*, State.CC.Slow, State.CC.Root]
      MatchLogic: Any
      ExcludeSubtrees: []
      Exceptions: []
      MaxPriority: 120
  MaxRemoveCount: -1
```
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
\n## 卧龙：策略与净化片段
## 5. 策略资产（Policies）

```yaml
SlotRelations:
  - SlotTag: StateSlot.CC.Stagger
    CloseGatesOnEnter:   [StateSlot.Action.*, StateSlot.Channel.*]
    OpenGatesOnExit:     [StateSlot.Action.Attack.Light, StateSlot.Mobility.Walk]
    Notes: '受击硬直期间禁止动作与引导；结束后恢复基础移动与轻攻'

  - SlotTag: StateSlot.CC.Break
    CloseGatesOnEnter:   [StateSlot.Action.*, StateSlot.Mobility.*, StateSlot.Channel.*]
    ClearSlotsOnEnter:   [StateSlot.Action.Combo]
    Notes: '破防关闭所有域并清连击槽'

  - SlotTag: StateSlot.Action.Attack.Heavy
    CloseGatesOnEnter:   [StateSlot.Mobility.Dodge]
    Notes: '重击期间禁用闪避'

  - SlotTag: StateSlot.Channel.SpellCast
    CloseGatesOnEnter:   [StateSlot.Mobility.Dodge, StateSlot.Action.Attack.*]
    Interruptors:        [StateSlot.CC.Stagger, StateSlot.CC.Break]
    ClearSlotsOnInterrupt: [StateSlot.Channel.SpellCast]
    Notes: '引导被硬直/破防中断并清槽'
```

<!-- 旧版规则资产（RuleSet/Dependencies/Exclusions）已废弃，示例段落移除。规则请在 StateTree 编排，免疫/净化通过策略层实现。 -->

- Cleanse（示例，YAML，仅示意）

```yaml
Task: Cleanse
  Params:
  Selector:
    Class: UTcsSelector_ByTags
    Params:
      IncludeTags: [Element.Debuff.*]
      MatchLogic: Any
      ExcludeSubtrees: []
      Exceptions: []
      MaxPriority: 999
  MaxRemoveCount: 2
```
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
\n## 鬼泣5：策略与免疫片段
## 5. 策略资产（Policies）
```YAML
Policies:
  ImmunityPolicies:
    - Class: /Game/TCS/Policies/Pol_BossStaggerImmunity  # 示例：Boss 对硬直类状态免疫（或部分子类）
    - Class: /Game/TCS/Policies/Pol_DevilTriggerImmunity # 示例：恶魔化时对轻度硬直免疫
  CleansePolicies:
    - Class: /Game/TCS/Policies/Pol_None                # 本示例不使用净化，可留空或占位
```

说明：免疫仅返回“允许/阻断”，不返回缩放/抗性；顶层 StateTree 负责 Gate/时序/依赖与互斥。
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
