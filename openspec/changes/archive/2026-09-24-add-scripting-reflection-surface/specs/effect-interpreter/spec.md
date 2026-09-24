## MODIFIED Requirements

### Requirement: 链执行与池化运行态

`UTcsEffectSubsystem` MUST 提供 `ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context) -> FTcsChainRunHandle`（起链：立即执行至首个挂起点或走完；**2026-09-22 改造：`ChainId` 类型 `FName` → `FGameplayTag`**）：

- 运行态 `FTcsChainRun` **池化**（`TTcsInstancePool`，D0-2 代际句柄防悬空）——控制流状态住数据、**不活在调用栈里**（D4-3 异步语义）；
- `FTcsChainRun` MUST 自持：`ChainId`（**不是链定义指针**——每步入器按 id 重解析；登记表变更不得使运行中链悬空）、`PC`、`Context`（黑板）、宿主门面弱引用与自身句柄（唤醒回入口）、挂起锚；
- 返回句柄**仅在链未走完时有效**：链同步走完即释放运行态（句柄随之失效）；`IsRunActive(Handle)` 是活性查询入口；
- 未登记的链 id MUST 拒绝：Error 日志 + 无效句柄（不 ensure、不崩溃——执行期配置错误，不用 ensure 刷屏）；
- 运行态指针 MUST NOT 跨步骤执行持有：**每次进入步骤执行前按句柄重解析**（引擎事实 2026-09-17：池元素住连续缓冲，新增其他运行态即搬移；步骤执行期间做任何池新增都会让缓存指针失效）。

#### Scenario: 全即时步骤的链同步走完

- **WHEN** 执行一条不含挂起步骤的链
- **THEN** `ExecuteChain` 返回时全部步骤已执行、运行态已释放（`IsRunActive` 为 false）

#### Scenario: 未登记链被拒

- **WHEN** 执行一个未登记的 ChainId（有效 tag 但未登记）
- **THEN** 返回无效句柄 + Error 日志（不崩溃、不 ensure）

#### Scenario: 挂起期间登记新链不影响续走

- **WHEN** 一条链挂起期间又登记了若干其他链定义，随后该链被唤醒
- **THEN** 唤醒重入按 id 解析到自己的定义并正常续走（运行态未持链指针，登记表增长不使其悬空）

## ADDED Requirements

### Requirement: 门面反射面（脚本层可达）

`UTcsEffectSubsystem` 的公开门面方法 MUST 对**宿主脚本层**（UnrealSharp / BP 反射）可达——这是 D4-17"语言无关执行器"预留的落地，也是"宿主用 C# 编写技能/Buff 逻辑"的前提（2026-09-24 用户拍板）。

**口径（2026-09-24 实证定稿）**：

- 方法标记 MUST 为 **`UFUNCTION()` 无 specifier**——**MUST NOT** 加 `BlueprintCallable`：UHT 对 `BlueprintCallable | BlueprintEvent` 校验参数蓝图可表达性（`UhtFunction.cs:859`/`:1043`），而本门面的形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等 **`USTRUCT()` 非 `BlueprintType`** 载体，加 specifier 会被 UHT 拒绝编译；
- **蓝图侧因此不可见，这是有意接受**（R0 §9"蓝图不承诺"）——声明处 MUST 以注释写明，避免被误读为漏写；
- 可见性依赖 C++ 侧 `public` 访问级别（UHT 据此授予 `EFunctionFlags.Public`，UnrealSharp 才生成 `public` 方法）；**实现后 MUST 以生成的 glue 产物验证**（读 `*.generated.cs` 确认修饰符为 `public`），不得仅凭推断；
- 覆盖范围（本轮）：链登记（`RegisterChain`/`UnregisterChain`/`FindChain`）、触发登记（`RegisterTriggerRow`/`UnregisterTriggerRow`/`SetTriggerGateTag`/`IsTriggerGateTagLit`/`GetTriggerRowCount`/`SetTriggerRandomSeed`）、执行（`ExecuteChain`/`ResumeRun`/`IsRunActive`）、能力注入（`SetEntityQuery`/`GetEntityQuery`）；
- **形参含非反射裸 struct 的方法不在本轮**（如 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)`）——需先反射化该 struct，属台账 S-1 的 B 类未覆盖项；
- 反射面**只开"调用"这一扇门**：MUST NOT 被理解为"脚本可提供行为"——行为登记（三张注册表的 `Register`）的反射入口是独立欠账（台账 S-2），其形参 `TFunction` 不可反射。

#### Scenario: 脚本层可调用门面方法

- **WHEN** 检查门面 API 的生成绑定产物（UnrealSharp glue）
- **THEN** 上列方法全部出现且修饰符为 `public`（脚本层可直接调用）

#### Scenario: 门面方法不加蓝图 specifier

- **WHEN** 检查门面方法的 `UFUNCTION` 标记
- **THEN** 全部为无 specifier 形式，且声明处注释写明"形参含非 BlueprintType struct + 蓝图不承诺"的理由

#### Scenario: 运行态句柄可被脚本接住并传回

- **WHEN** 脚本层调 `ExecuteChain` 接住返回的 `FTcsChainRunHandle`，随后把它传回 `IsRunActive`
- **THEN** 编译通过与运行期均成立（句柄是反射可见类型；`FTcsChainRunHandle` MUST 为 `USTRUCT()`——内部 `TTcsInstanceHandle` 字段不加 `UPROPERTY`，照 `FTcsEffectTriggerHandle` 先例）
