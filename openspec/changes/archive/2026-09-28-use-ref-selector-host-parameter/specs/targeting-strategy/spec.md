## MODIFIED Requirements

### Requirement: 宿主脚本选择器插槽

`TcsTargeting` MUST 提供宿主选择器插槽——**让宿主用任意 UE 脚本语言（C#/AS/Luau/TS/蓝图）实现目标选择，零 C++ 改动**：

- 新增 `ITcsTargetSelectorHost`（`UINTERFACE`，住 `Public/Host/TcsTargetSelectorHost.h`），MUST 提供：
  - `UFUNCTION(BlueprintNativeEvent) void ResolveTargets(FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator, UPARAM(ref) TArray<FTcsCombatEntityHandle>& OutTargets)`——**填充语义同 `FTcsTargetSelectorStrategy::Resolve`**（只填充不清空；调用方负责清空；反射绑定向脚本呈现为 `ref` 输入输出容器）；
- 新增转发策略 `FTcsSelHostDelegate : FTcsTargetSelectorStrategy`（USTRUCT，可被编辑器 picker 选中）：持 `UPROPERTY TScriptInterface<ITcsTargetSelectorHost> Host`，`Resolve` 覆写为**纯转发**（含空实现守卫：`Host` 未配时留 Warning 并产出空集，MUST NOT 崩溃）；
- **转发是必需的，不是选择**：`FTcsTargetSelectorStrategy` 走 **C++ 虚分派**，而脚本定义的 struct **没有 C++ 类型**（`CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即野函数指针）——**引擎层面无解**。故 MUST 经"USTRUCT 转发器 → UObject 反射接口"两层；
- 框架默认选择器（`FTcsSelSelf`）与 C++ 策略子类 MUST 保持原样（**双轨并存**：内置走虚分派快路径，插槽只给宿主扩展）；
- 接口 MUST 加 `Blueprintable`（脚本层要能实现它）；
- 形参 MUST 全反射（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，`UhtFunction.cs:859`/`:1043-1053`）——故传**句柄**而非上下文 struct。

#### Scenario: 脚本层实现选择器并生效

- **WHEN** 宿主脚本层实现 `ResolveTargets` 返回自定义目标集，配进 `FTcsStepSelectTargets.Selector`（经 `FTcsSelHostDelegate` 转发器），链跑到该步
- **THEN** `Context.Targets` 等于脚本实现产出的目标集

#### Scenario: 未配 Host 的转发器安全降级

- **WHEN** `FTcsSelHostDelegate.Host` 为空（未配脚本实现）而该转发器被配进链
- **THEN** 不崩溃、产出空集 + Warning 日志（不静默通过、不 ensure——配置残缺是内容问题）

#### Scenario: 内置策略不受影响

- **WHEN** 链使用 `FTcsSelSelf`（框架默认）
- **THEN** 行为与插槽引入前完全一致（虚分派路径未被改动）

#### Scenario: C# 引用容器回写

- **WHEN** 宿主 C# 对象实现 `ResolveTargets(ref IList<FTcsCombatEntityHandle>)`，向传入列表依次追加施法者和第二目标，并由 TCS 原生转发器调用
- **THEN** UnrealSharp 生成的接口形参为 `ref`、链的目标集顺序与脚本追加顺序一致，且无需 UnrealSharp `out` 容器参数的生成器修补
