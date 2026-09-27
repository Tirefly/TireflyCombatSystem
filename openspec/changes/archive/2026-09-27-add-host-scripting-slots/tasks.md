## 1. 插槽③：伤害流程（最便宜，流程侧 12 执行器已全实现）

- [x] 1.1 新增 `TcsDamage/Public/Flow/TcsDamageFlowContextView.h`：`FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)`）+ `MakeView(const FTcsDamageFlowContext&)` 投影函数（含字段映射与注释说明"为什么是视图而非整体反射化"）
- [x] 1.2 `TcsDamageFlowDelegate.h`：5 方法换签名（`const FTcsDamageFlowContext&` → `const FTcsDamageFlowContextView&`）+ 加 `UFUNCTION(BlueprintNativeEvent)` + 在接口类内声明 `virtual <名>_Implementation(...)`（带中性默认体——**声明存在则 UHT 不生成 stub**，`UhtFunction.cs:681` 的 `ImplFound`）
- [x] 1.3 C++ 调用点改 `Execute_`：`TcsFlowStepsCore.cpp`（`CalculateBaseDamage` / `ModifyShield`）+ `TcsFlowStepsRest.cpp`（`GetBaseHitRate` / `GetBaseCritRate` / `ResolveElement`）——**MUST NOT 虚表直调**（会静默跳过脚本实现）
- [x] 1.4 调用点构造视图：5 处把 `Context` 经 `MakeView` 投影后传入（局部变量，生命周期在本步内）
- [x] 1.5 `TcsDamageSubsystem.h`：`RegisterTemplate` / `UnregisterTemplate` 加 `UFUNCTION()`（无 specifier，注释写明理由）
- [x] 1.6 LAC 侧 `TcsDevDamageFormula.h/.cpp`：覆写签名换视图；取门面由 `Context.Owner` 改自身 `GetWorld()`（视图不含 `Owner`——**BREAKING 的机械修复**）

## 2. 插槽④：门面按句柄访问器（①/② 的前置）

- [x] 2.1 `TcsEffectSubsystem.h/.cpp`：新增 6 个访问器（`GetRunTargets` / `SetRunTargets` / `TryGetRunVariable` / `SetRunVariable` / `GetRunCaster` / `GetRunInstigator`），全部 `UFUNCTION()` 无 specifier
- [x] 2.2 悬空句柄语义：读口返回空值/无效句柄、写口返回 false + Warning（**不 ensure**——口径同 `ResumeRun`，S-7 先例）
- [x] 2.3 `TcsEffectStep.h`：`ETcsStepResult` 升格 `UENUM()`（枚举值不变）——为插槽②的返回类型做准备

## 3. 插槽①：选择器 / 过滤器宿主转发

- [x] 3.1 新增 `TcsTargeting/Public/Host/TcsTargetSelectorHost.h`：`UINTERFACE(MinimalAPI, Blueprintable)` + `ITcsTargetSelectorHost::ResolveTargets`（`UFUNCTION(BlueprintNativeEvent)`）
- [x] 3.2 新增 `TcsTargeting/Public/Host/TcsTargetFilterHost.h`：同款 + `PassTarget`
- [x] 3.3 新增 `TcsTargeting/Public/Targeting/TcsSelHostDelegate.h`：`FTcsSelHostDelegate : FTcsTargetSelectorStrategy`（持 `UPROPERTY TScriptInterface<ITcsTargetSelectorHost> Host`；`Resolve` 纯转发 + 空 Host 守卫）
- [x] 3.4 新增 `TcsTargeting/Public/Targeting/TcsFilterHostDelegate.h`：同款（空 Host = 通过）
- [x] 3.5 头注释写明"为什么必须转发"（虚分派对脚本类型物理不可达——`CppStructOps == nullptr` ⇒ vtable 位为 0）

## 4. 插槽②：步骤执行器 UObject 基类

- [x] 4.1 新增 `TcsEffect/Public/Chain/TcsStepExecutor.h`：`UTcsStepExecutor`（`UCLASS(Abstract, Blueprintable)`）+ `UFUNCTION(BlueprintNativeEvent) ETcsStepResult Execute(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData)`
- [x] 4.2 `TcsChainRun.h`：`FTcsChainRunHandle` 改 `USTRUCT(BlueprintType)`（**MODIFIED 既有规格**——`BlueprintNativeEvent` 形参校验要求）+ `FTcsEffectTriggerHandle` 同批（口径统一）
- [x] 4.3 `TcsEffectSubsystem.h/.cpp`：`UFUNCTION() bool RegisterStepExecutor(UScriptStruct*, UTcsStepExecutor*)`——内部包 `TFunction` 转发进既有注册表；拒绝面 ensure + false
- [x] 4.4 GC 持有：门面加 `UPROPERTY TArray<TObjectPtr<UTcsStepExecutor>> RegisteredStepExecutors`（T-8 教训：裸注册表持不住对象引用）
- [x] 4.5 `TcsDamage`：`UTcsFlowStepExecutor` 同款（`damage-flow` 规格的"宿主脚本可登记流程步骤"）+ 门面 `RegisterStepExecutor` + `UPROPERTY` 持有

## 5. 编译与 glue 产物验证（核心判据）

- [x] 5.1 UBT Development 编译通过（**UHT 是插槽签名合法性的主判据**）
- [x] 5.2 读 `TcsDamageFlowDelegate.generated.cs`——确认 5 方法生成**真实方法体**（含 `CallGetNativeFunctionFromClassAndName` + 参数偏移），**非空壳**（换型前是零方法空壳，已实测存档）
- [x] 5.3 读 `TcsTargetSelectorHost.generated.cs` / `TcsTargetFilterHost.generated.cs` / `TcsStepExecutor.generated.cs`——确认方法可覆写（`FunctionFlags.BlueprintEvent`）
- [x] 5.4 读 `TcsEffectSubsystem.generated.cs`——确认 6 个访问器为 `public`
- [x] 5.5 读 `TcsDamageSubsystem.generated.cs`——确认 `RegisterTemplate` / `UnregisterTemplate` / `RegisterStepExecutor` 为 `public`

## 6. 端到端 PIE 实证（唯一硬证据）

- [x] 6.1 扩展 `Script/LegendAutoChessCS/TcsProbe/TcsChainProbe.cs`：新增 C# 类实现 `ITcsDamageFlowDelegate::CalculateBaseDamage`（返回固定可辨识值，如 7.0）
- [x] 6.2 脚本层注册含该 delegate 的流程模板（经 `RegisterTemplate`）→ 起链 → 断言伤害值走了 C# 公式（**"自定义伤害流程可用 C# 写"的判据**）
- [x] 6.3 C# 侧读 `GetRunTargets` / 写 `SetRunVariable` 再读回——确认访问器往返成立
- [x] 6.4 记录实测边界（若某插槽仍有配不了的字段，如实记录——"能导出 ≠ 能往返"纪律）

> **PIE 行为证据（2026-09-27）**：依据 `C:/Users/TireflyPC/.codex/attachments/110c674c-fc99-42ef-b780-4d1f9af0f196/已粘贴的文本.txt`。
> - 6.1–6.2：脚本通过 `RegisterTemplate` 登记 `Tcs.Flow.Template.Probe.HostSlot`；C# `CalculateBaseDamage` 日志明确出现，C++ `Execute_CalculateBaseDamage` 路径最终记录 `Final=7`，Health 从 `75` 变为 `68`。
> - 6.3：活动链句柄上 `GetRunCaster = 1`；`SetRunVariable` 写入 `42` 后 `TryGetRunVariable` 读回 `42`；`GetRunTargets` 返回 `1` 个目标。
> - 6.4：同一日志还显示挂起链随后被到期堆唤醒并完成。该证据**只覆盖 UnrealSharp/C# 的伤害流程 + 句柄访问器 PIE 路径**；没有验证 GC 强保活、脚本选择器/过滤器、脚本步骤执行器、悬空句柄安全语义，或 AS/Luau/TS 的跨语言往返。
> - 日志中的三条 Warning 分别是探针清理时模板未登记、活动运行态阻止链注销，以及同样的活动运行态保护；提供的片段未出现 Error、ensure 或 Fatal。

## 7. 文档同步与收束

- [x] 7.1 台账 `deferred-inputs-ledger.md`：S-8 标"已消费"（含落点与实测结论）；S-2 / S-4 / S-5 的定位分别更新为低于 S-8、并入 S-8、或仍为 C++ 专用
- [x] 7.2 设计文档：`04-module-effects.md`（§5b 插槽节的状态由"路线"改"已落地"）、`09-module-damage.md`（§2.4 接口反射化）、`10-module-targeting.md`（§2.1/§2.2 宿主插槽）
- [x] 7.3 调研文档 `2026-09-23-csharp-tcs-logic-authoring-research.md` §12：能力边界表按实测结果更新
- [x] 7.4 `openspec/project.md`：宿主脚本扩展条更新为"插槽已落地"

> **文档收束说明（2026-09-27）**：本轮将“静态实现 / glue 产物 / C# PIE 行为 / 未验证边界”分开记录。`ITcsEntityQuery` 仍是 C++ 专用；未验证项不得因 S-8 插槽形状已落地而自动升级为跨语言或 GC E2E 结论。
- [x] 7.5 提案归档 + 规格库全绿（归档前全量严格验证已通过；归档后继续复核）
