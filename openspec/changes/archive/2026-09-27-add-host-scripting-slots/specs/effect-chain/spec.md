## MODIFIED Requirements

### Requirement: 链运行态句柄的反射性

`FTcsChainRunHandle` MUST 是**反射可见类型**（`USTRUCT()`）——它是脚本层与宿主消费"起链结果"的唯一身份词：脚本层调 `ExecuteChain` 接住句柄、再把它传回 `IsRunActive` / `ResumeRun` 查询与唤醒（2026-09-24 用户拍板：让项目成员能用 C# 写技能/Buff 逻辑）。

- **字段 MUST 展平**（2026-09-24 实测修正）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`——后者是模板类型、无法作 `UPROPERTY`，会让绑定产物生成**空壳**（`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒ **代际失配、往返失效**（实测：池给 `{Index=0, Generation=1}`，C# 传回 `{0, 0}`，`IsValid` 判 false）；
- `Index` MUST 为 `int32`（UHT 不支持 `uint32` 作属性类型）；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**——MUST 经唯一的转换点（`GetInner`/`SetInner`）与池句柄互转，保证往返无损；
- **MUST 为 `BlueprintType`（2026-09-24 放宽，台账 S-8 连带）**：原"MUST NOT 加 `BlueprintType`"的顾虑是"成为 `BlueprintCallable` 的合法形参 ⇒ 意外扩大蓝图承诺面"；插槽路线（`effect-step-dispatch` 的"步骤执行器插槽"）需要句柄出现在 **`BlueprintNativeEvent` 签名**里，而 UHT 对 `BlueprintEvent` 强制全部形参蓝图可表达（`UhtFunction.cs:859`/`:1043-1053`）⇒ 非 `BlueprintType` 则插槽**编译不过**。放宽的**承诺面代价为零**：消费句柄的门面方法仍为 `UFUNCTION()` 无 specifier（蓝图不可见），蓝图能"看见类型"却**无任何可调用的门面**；插槽接口确实蓝图可实现，但那是 R0 §9 已接受的"恰好蓝图也能用"（台账 S-8），**不是新增承诺**；
- 先例 = `FTcsCombatEntityHandle`（同为展平字段的**反射 + `BlueprintType`** 句柄）——两条句柄分道扬镳无技术依据。

#### Scenario: 句柄可作反射方法的返回与参数

- **WHEN** 检查 `ExecuteChain` / `IsRunActive` / `ResumeRun` 的反射签名
- **THEN** `FTcsChainRunHandle` 作为返回类型与形参类型均合法（UHT 编译通过、宿主脚本层可见该类型）

#### Scenario: 句柄值可跨语言往返

- **WHEN** 脚本层调 `ExecuteChain`（链含挂起步骤）接住返回的句柄，随后原样传回 `IsRunActive`
- **THEN** 判定为 `true`（句柄值完整往返：`Index`/`Generation` 均保持，代际校验通过）

#### Scenario: 句柄字段可被脚本层读出

- **WHEN** 脚本层读取接住句柄的 `Index` / `Generation`
- **THEN** 读到的是池/登记表的真实值（非零值）——绑定产物 MUST 为这两个字段生成真实的读写代码（非空壳）

#### Scenario: 句柄可作插槽接口的形参

- **WHEN** 检查 `UTcsStepExecutor::Execute` 的反射签名（含 `FTcsChainRunHandle Run` 形参）
- **THEN** UHT 编译通过（句柄为 `BlueprintType`，满足 `BlueprintNativeEvent` 的形参校验）
