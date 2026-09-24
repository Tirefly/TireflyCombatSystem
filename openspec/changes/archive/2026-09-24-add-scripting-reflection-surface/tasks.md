## 1. 运行态句柄反射化（含实测修正：展平）

- [x] 1.1 `Source/TcsEffect/Public/Chain/TcsChainRun.h`：`FTcsChainRunHandle` 加 `USTRUCT()` + `GENERATED_BODY()`
- [x] 1.2 该头 include `TcsChainRun.generated.h`
- [x] 1.3 编译验证：UBT Development 通过
- [x] 1.4 **实测修正（2026-09-24 首轮 PIE 实测后）**：首版按"`USTRUCT()` + 非 `UPROPERTY` 内部字段"实现，实测**句柄往返失效**（C# 接住读不到值、传回写全零 ⇒ 代际失配）——根因 = `TTcsInstanceHandle<T>` 是模板、无法作 `UPROPERTY` ⇒ 绑定产物生成空壳。**改为展平**：`UPROPERTY int32 Index` / `UPROPERTY int32 Generation` + `GetInner`/`SetInner` 唯一转换点（无效值 `-1` 与 `0xFFFFFFFF` 位模式相同，往返无损）
- [x] 1.5 **同根因连带修复**：`FTcsEffectTriggerHandle`（`TcsEffectTriggerInstance.h`）同形态 ⇒ `RegisterTriggerRow`/`UnregisterTriggerRow` 往返同样失效，同批展平
- [x] 1.6 全部 `.Inner` 使用点更新（`TcsEffectSubsystem.cpp` / `TcsTriggerRegistry.cpp` / `TcsTriggerRegistry_Query.cpp` / `TcsStepWaitDelay.cpp`，含 `uint32`↔`int32` 显式转换）

## 2. 门面 API 反射化

- [x] 2.1 `UTcsEffectSubsystem`：`RegisterChain` / `UnregisterChain` / `RegisterTriggerRow` / `UnregisterTriggerRow` / `SetTriggerGateTag` / `IsTriggerGateTagLit` / `GetTriggerRowCount` / `SetTriggerRandomSeed` / `ResumeRun` / `IsRunActive` 补 `UFUNCTION()`（无 specifier，每个上方注释说明理由）
- [x] 2.2 **新增** `IsChainRegistered(FGameplayTag)`——`FindChain` 返回裸 struct 指针，反射层表达不了（UHT 不支持 struct 指针作反射返回，引擎全仓零先例）
- [x] 2.3 **新增** `ExecuteChainForCaster(FGameplayTag, FTcsCombatEntityHandle)`——`ExecuteChain` 形参 `FTcsEffectContext` 是非反射 struct，加 `UFUNCTION` 会让 UHT 报 `Unable to find 'struct'`；本入口按 Caster 装配默认上下文
- [x] 2.4 `UnregisterTriggerRowsBySource` / `SetEntityQuery` / `GetEntityQuery` **保持纯 C++**（形参含非反射 `FTcsSourceHandle` / 依赖未反射化的 `ITcsEntityQuery`——归台账 S-1 B 类与 S-5）
- [x] 2.5 `UTcsCombatEntityComponent::ExecuteChainById` 补 `UFUNCTION()`
- [x] 2.6 **实体注册面反射化（实测补充）**：`UTcsAttributeSubsystem` 的 `RegisterUnit` / `UnregisterUnit` / `AddAttribute` / `SetBaseValue` / `EvaluateCurrent` / `PeekPending` 补 `UFUNCTION()`——首轮实测暴露"脚本层没有注册实体就没有合法 `Caster`"（硬编码句柄触发三处"单位未注册" ensure），这是脚本层起链的**必要前置**
- [x] 2.7 编译验证：UBT Development 通过

## 3. 反射面实证（glue 产物）

- [x] 3.1 读 `TcsEffectSubsystem.generated.cs` 确认方法为 **`public`**（验证 `EFunctionFlags.Public` 推导成立）——**实测成立**
- [x] 3.2 读 `TcsChainRunHandle.generated.cs`：**首轮为空壳**（`ToNative` 函数体空）；展平后确认含真实字段读写代码
- [x] 3.3 读 `TcsCombatEntityComponent.generated.cs` 确认 `ExecuteChainById` 出现

## 4. C# 端到端实证

- [x] 4.1 `Script/LegendAutoChessCS/TcsProbe/TcsChainProbe.cs`：注册实体 → 注册链 → 起链 → 接住句柄 → 查活性 → 读属性
- [x] 4.2 用 `FTcsStepWaitDelay` 构造**挂起链**，使句柄在返回时仍有效
- [x] 4.3 **PIE 实测通过（2026-09-24）**：`句柄 Index=0 Generation=1` / `IsRunActive = true` / 链被到期堆唤醒并跑完 / `Health = 100`；日志零红字（除已修的 `UnregisterChain` ensure）
- [x] 4.4 **实证边界记录**：`FTcsStepSelectTargets` 不可用（`TInstancedStruct` 字段空壳，台账 S-6）；`FTcsStepDamage.DamageBase`（`FTcsParamValue`）为空壳不可配——伤害记录 Base=0.000 即此因

## 5. 附带修复与文档同步

- [x] 5.1 `UnregisterChain` 的"有活动运行态"拒绝面：`ensureMsgf` → `Warning`——**时序竞态不是配置错误**，与同函数"未登记"拒绝面口径统一（TCS 内部既有惯例：悬空句柄/陈旧句柄/世界拆解期均"正常竞态不 ensure"）
- [x] 5.2 提案 `proposal.md`：补"实测发现"节（能导出 ≠ 能往返）
- [x] 5.3 `effect-chain` 规格：句柄反射性需求改为**展平形态 + 往返成立**（含新场景）
- [ ] 5.4 `effect-trigger` 规格：触发行句柄补往返要求
- [ ] 5.5 台账 `deferred-inputs-ledger.md`：S-1 标"已消费"；**新增 S-6**（`TInstancedStruct<T>` 字段导出缺口 + `FInstancedStruct` 换型方案）
- [ ] 5.6 设计文档 `04-module-effects.md`：§7 非目标的"不做 K2/脚本调用层"口径更新
- [ ] 5.7 调研文档 `2026-09-23-csharp-tcs-logic-authoring-research.md`：补实测结论
- [ ] 5.8 提案归档（`openspec archive`）+ 规格库全绿
