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
