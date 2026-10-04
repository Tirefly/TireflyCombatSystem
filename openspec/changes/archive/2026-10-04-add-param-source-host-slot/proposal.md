# Change: 参数源族宿主插槽（R4.5-b / `R-1`）

## Why

`FTcsParamValueSource` 走 C++ 虚分派（`USTRUCT` 基类 + `virtual double Evaluate(...)`）——脚本定义的 struct **物理不可达**（无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用）。同族的"选择器 / 过滤器"族与"评分器"族都已有转发器（`FTcsSelHostDelegate` / `FTcsFilterHostDelegate` / `FTcsScorerHostDelegate`），**参数源族没有对应物** ⇒ 宿主无法用脚本定义"新求值语义"（如"攻击力 = 基础 + 等级 × 成长 + 装备加成"这类要写逻辑的源）。

本条已登记在册（`LEDGER-reflection` 的 `R-1`，2026-09-24 调研并拍板），2026-10-04 用户裁定**并入 R5 Task 3**——判据是"离第一个真实消费者最近"：本轮四个等级源与状态 Def 参数行就是参数源的第一个真实消费场景，同族同验证面。

## What Changes

- 新增 `ITcsParamSourceHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + 两个 `UFUNCTION(BlueprintNativeEvent)`）：`double Evaluate(const FTcsParamEvaluateContext& Context)` 与 `bool AllowsValueConvention()`——宿主实现它即获得"自定义新求值语义"的权利。
- 新增 `FTcsParamSource_HostDelegate : FTcsParamValueSource`（USTRUCT 策略子类）：持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`，两个虚函数**纯转发** + 空 Host 守卫（返回 0 / true）。
- **载体与调用点零改动**：`FTcsParamValue.Source` 是裸 `FInstancedStruct`，照装转发器；既有 `Evaluate` 调用点走 `GetPtr<FTcsParamValueSource>()` + 判空，转发器作为子类自然命中。
- **本提案动两个新文件 + 一处字段补充**（接口 + 转发器，各带 `.cpp`；另给 `FTcsParamEvaluateContext` 补 `Instigator` 字段——实施期编译暴露它并不在 Task 0 补的两个字段里）。
- **域读口不进 Core 上下文**：调研 R-1 原设想把宿主等级读口直接加成 `FTcsParamEvaluateContext` 的第三字段，**不采用**（见下）。

> **一处对调研结论的偏离（2026-10-05 起草期裁定，理由如下）**：R-1 调研曾设想把宿主等级读口直接加成 `FTcsParamEvaluateContext` 的第三字段。**不采用**——`FTcsParamEvaluateContext` 住 `TcsCore`，而该读口的唯一消费者（四个等级源）住 `TcsState`；把它的字段类型放进 Core 会让 Core 持有一个**只有上层模块使用**的接口类型，且日后每个域都可能想往 Core 上下文里塞自己的读口（Core 变成领域读口的回收站）。改用**同样已立规的扩展机制**：`TcsState` 自己的派生上下文 `FTcsStateEvaluateContext` 持该字段，域侧源按 `GetScriptStruct()->IsChildOf(...)` 判定后取值——先例是同族 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`（PV-1 明文支持多层派生）。**代价**：`ITcsEntityLevelProvider` 住 `TcsState/Public/Host/` 而非 `TcsCore/Public/Parameter/`——与 `TcsAttribute` 的同款选择一致（读口随消费者所属的领域层）。
- **两个虚函数都要转发**（R-1 调研的硬判据）：漏 `AllowsValueConvention` 会让宿主新源在本轮的 `ValueConvention` 白名单校验上拿到错误的默认能力位。

## Impact

- Affected specs：`param-value`（MODIFIED「反射可见求值上下文」补 `Instigator` + ADDED「宿主参数源插槽」）
- Affected code（插件仓）：
  - 新增：`Source/TcsCore/Public/Parameter/TcsParamSourceHost.h`（**接口 + 转发器同住一文件**——同族先例 `TcsParamSource_AttributeScaled.h`：接口、派生上下文与源三者同处一文件；两者是"同一机制的两半"，拆开只会多一跳）
  - 修改：`Source/TcsCore/Public/Parameter/TcsParamValueSource.h`（补 `Instigator` 字段 + 补虚析构——见下）

> **实施期的一处必要补充（2026-10-05）**：`FTcsParamValueSource` 补 `virtual ~FTcsParamValueSource() = default;`。原因 = 等级表两族源（`TArray` / `TMap` 成员 ⇒ **非平凡析构**）触发 MSVC `C4265`（"有虚函数但析构非虚"）——这是**多态基类的正确性要求**而非洁癖（经基类指针删除是未定义行为）；住基类一处即覆盖全族（含宿主自定义源与 R6 的技能源）。
- Affected code（宿主仓）：C# 侧实现一个 `ITcsParamSourceHost` 并配进 `FTcsParamValue.Source` 实测求值；验证流程 MUST 复用既有宿主脚本探针（`verify-tcs-host-scripting-e2e` 那套），**不得重写第三次**（`LEDGER-reflection`《宿主插槽家族的耦合关系》的既有裁定）
- **非目标**：条件求值器 / 载荷读取器注册表的宿主机插槽（`R-2` 后段，排到 R6 开工前）；属性访问写侧插槽（`R-7` / R5.5-g）；不给 `ITcsParamTableReader` 之外的参数域接口加槽位
