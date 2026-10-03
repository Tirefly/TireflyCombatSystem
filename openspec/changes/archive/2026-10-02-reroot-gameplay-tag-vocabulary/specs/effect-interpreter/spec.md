## MODIFIED Requirements

### Requirement: 门面反射面（脚本层可达）

`UTcsEffectSubsystem` 的公开门面方法 MUST 对**宿主脚本层**（UnrealSharp / BP 反射）可达——这是 D4-17"语言无关执行器"预留的落地，也是"宿主用 C# 编写技能/Buff 逻辑"的前提（2026-09-24 用户拍板）。

**口径（2026-09-24 实证定稿）**：

- 方法标记 MUST 为 **`UFUNCTION()` 无 specifier**——**MUST NOT** 加 `BlueprintCallable`：UHT 对 `BlueprintCallable | BlueprintEvent` 校验参数蓝图可表达性（`UhtFunction.cs:859`/`:1043`），而本门面的形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等 **`USTRUCT()` 非 `BlueprintType`** 载体，加 specifier 会被 UHT 拒绝编译；
- **蓝图侧因此不可见，这是有意接受**（R0 §9"蓝图不承诺"）——声明处 MUST 以注释写明，避免被误读为漏写；
- 可见性依赖 C++ 侧 `public` 访问级别（UHT 据此授予 `EFunctionFlags.Public`，UnrealSharp 才生成 `public` 方法）；**实现后 MUST 以生成的 glue 产物验证**（读 `*.generated.cs` 确认修饰符为 `public`），不得仅凭推断；
- 覆盖范围（本轮）：链登记（`RegisterChain`/`UnregisterChain`/`FindChain`）、触发登记（`RegisterTriggerRow`/`UnregisterTriggerRow`/`SetTriggerGateTag`/`IsTriggerGateTagLit`/`GetTriggerRowCount`/`SetTriggerRandomSeed`）、执行（`ExecuteChain`/`ResumeRun`/`IsRunActive`）、能力注入（`SetEntityQuery`/`GetEntityQuery`）；
- **形参含非反射裸 struct 的方法不在本轮**（如 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)`）——需先反射化该 struct，属台账 SCRIPT-1 的 B 类未覆盖项；
- 反射面**只开"调用"这一扇门**：MUST NOT 被理解为"脚本可提供行为"——行为登记（三张注册表的 `Register`）的反射入口是独立欠账（台账 SCRIPT-2），其形参 `TFunction` 不可反射。**（2026-09-24 补：该欠账的替代路径已由台账 SCRIPT-8 的"步骤执行器插槽"给出——UObject 基类不动 `TFunction` 签名即可让脚本登记执行器，见 `effect-step-dispatch`）**；
- **按句柄的上下文访问器（2026-09-24 新增，台账 SCRIPT-8）**：门面 MUST 提供下列方法，**全部 `UFUNCTION()` 无 specifier**——它们是"传句柄、不传上下文"手法的落地，使宿主脚本能在不反射化 `FTcsEffectContext` 的前提下读写运行态：

  | 方法 | 语义 |
  |---|---|
  | `TArray<FTcsCombatEntityHandle> GetRunTargets(FTcsChainRunHandle)` | 读运行态目标集（悬空句柄返回空数组——不 ensure，时序竞态） |
  | `bool SetRunTargets(FTcsChainRunHandle, const TArray<FTcsCombatEntityHandle>&)` | 写运行态目标集（悬空句柄返回 false + Warning） |
  | `bool TryGetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double& OutValue)` | 读链内变量（miss 返回 false，出参内容未定义） |
  | `bool SetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double Value)` | 写链内变量 |
  | `FTcsCombatEntityHandle GetRunCaster(FTcsChainRunHandle)` | 读施法者句柄（悬空句柄返回无效句柄） |
  | `FTcsCombatEntityHandle GetRunInstigator(FTcsChainRunHandle)` | 读发起者句柄（同上） |

  **MUST NOT 提供 `FTcsEffectContext` 整体读写口**——该 struct 是非反射纯 C++ 类型（`TcsEffectContext.h:24`），且含 `FInstancedStruct EventPayload`（反射）与 `TMap` 变量表；整体暴露即等于要求上下文反射化（台账 SCRIPT-3），而访问器路线的全部收益正是**避开**它。
  - **链内变量键的根（2026-10-01 换根）**：上表 `TryGetRunVariable` / `SetRunVariable` 的 `FGameplayTag Key` MUST 落在 **`EffectChainRunVar`** 根下——**消费角色 = 链内变量读写（即本表这两条 API）**，声明方 = 宿主 `Config/DefaultGameplayTags.ini`，形态 `EffectChainRunVar.<键>`（2 段）；根段注册表与归属规则见 `gameplay-tag-governance` 能力。该键空间与链 id 根 `EffectChain`（登记表按 id 查找）、参数表根 `TcsStateParam`（参数表读取）**分属三个消费角色**，MUST NOT 合并进同一个根用 facet 段区分；插件 MUST NOT 声明任何具体变量键。存储侧 = `FTcsEffectContext::Variables`（定义住 `effect-chain`，该需求只留指针、不重复规则）。

#### Scenario: 脚本层可调用门面方法

- **WHEN** 检查门面 API 的生成绑定产物（UnrealSharp glue）
- **THEN** 上列方法全部出现且修饰符为 `public`（脚本层可直接调用）

#### Scenario: 门面方法不加蓝图 specifier

- **WHEN** 检查门面方法的 `UFUNCTION` 标记
- **THEN** 全部为无 specifier 形式，且声明处注释写明"形参含非 BlueprintType struct + 蓝图不承诺"的理由

#### Scenario: 运行态句柄可被脚本接住并传回

- **WHEN** 脚本层调 `ExecuteChain` 接住返回的 `FTcsChainRunHandle`，随后把它传回 `IsRunActive`
- **THEN** 编译通过与运行期均成立（句柄是反射可见类型；`FTcsChainRunHandle` MUST 为 `USTRUCT(BlueprintType)` 且字段展平——见 `effect-chain`）

#### Scenario: 脚本层经句柄访问器读写运行态

- **WHEN** 脚本层在挂起链的运行态句柄上调用 `GetRunTargets` / `SetRunVariable` / `GetRunCaster`
- **THEN** 读到本次执行的真实值、写入的值对后续步骤可见（脚本无需持有 `FTcsEffectContext`）

#### Scenario: 访问器对悬空句柄安全

- **WHEN** 以已释放的句柄调用任一访问器
- **THEN** 读口返回空值/无效句柄、写口返回 false + Warning（**不 ensure**——代际竞态是正常路径，口径同 `ResumeRun`）

#### Scenario: 链内变量键住 `EffectChainRunVar` 根

- **WHEN** 检查 `SetRunVariable` / `TryGetRunVariable` 的 `Key` 形参与宿主 ini 里对应词的形态
- **THEN** 形参是 `FGameplayTag`、宿主词形如 `EffectChainRunVar.<键>`（2 段）；该根由本表这两条 API 独占消费，MUST NOT 与 `EffectChain`（链 id）或 `TcsStateParam`（参数表键）共根
