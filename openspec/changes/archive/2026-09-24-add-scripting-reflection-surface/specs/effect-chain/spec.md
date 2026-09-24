## MODIFIED Requirements

### Requirement: 链与步骤数据形状

`TcsEffect` MUST 以**数据 struct**（非策略类/非类层级）承载效果链与步骤（D4-3/D4-16；分离宪法 R0 §8：语义 = 词汇（C++）、链 = 句子（数据））：

- `FTcsEffectChain{ FGameplayTag ChainId; TArray<FInstancedStruct> Steps; int32 MaxStepsPerFrame = 64; }`（USTRUCT；**2026-09-22 改造：`ChainId` 类型 `FName` → `FGameplayTag`**）——有序步骤数组 + 单帧步数上限（熔断语义见 `effect-interpreter`）；
- `FTcsEffectStep{ FInstancedStruct StepData; }`（USTRUCT）——"把步骤当字段"的容器形状（链数组直接存 `FInstancedStruct`；本 struct 供内联挂载/传递类场合复用）；
- **步骤类型无公共基类**（D4-16：15 原语按归属分住各模块）：链的步骤数组 MUST NOT 设 `BaseStruct` 编辑器限定——类型合法性由**执行器注册表在执行期**判定（未注册即拒绝，见 `effect-step-dispatch`）；链自带 `MaxStepsPerFrame` 默认 64（单帧步数熔断上限）。

**链 id 的来源（2026-09-22）**：链是**项目内容**（策划创作），故 `ChainId` 属**项目词汇**——由项目 `Config/DefaultGameplayTags.ini` 声明（`Tcs.Chain.<Id>`），插件 MUST NOT 声明任何具体链 id。

#### Scenario: 链定义字段完整

- **WHEN** 构造一条 `FTcsEffectChain`（指定 ChainId 与两个步骤、不设上限）
- **THEN** `MaxStepsPerFrame` 取默认值 64，步骤数组保持插入顺序

#### Scenario: 无公共基类的步骤数组

- **WHEN** 在同一链的步骤数组里放入两种不同归属的步骤 struct（如本模块的 `FTcsStepWaitDelay` 与某领域模块的步骤类型）
- **THEN** 二者共存于同一 `TArray<FInstancedStruct>`，链定义本身不因类型族不同而失败（分派发生在执行期）

## ADDED Requirements

### Requirement: 链运行态句柄的反射性

`FTcsChainRunHandle` MUST 是**反射可见类型**（`USTRUCT()`）——它是脚本层与宿主消费"起链结果"的唯一身份词：脚本层调 `ExecuteChain` 接住句柄、再把它传回 `IsRunActive` / `ResumeRun` 查询与唤醒（2026-09-24 用户拍板：让项目成员能用 C# 写技能/Buff 逻辑）。

- **字段 MUST 展平**（2026-09-24 实测修正）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`——后者是模板类型、无法作 `UPROPERTY`，会让绑定产物生成**空壳**（`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒ **代际失配、往返失效**（实测：池给 `{Index=0, Generation=1}`，C# 传回 `{0, 0}`，`IsValid` 判 false）；
- `Index` MUST 为 `int32`（UHT 不支持 `uint32` 作属性类型）；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**——MUST 经唯一的转换点（`GetInner`/`SetInner`）与池句柄互转，保证往返无损；
- **MUST NOT** 加 `BlueprintType`：句柄是运行期身份词、非配置数据；加 `BlueprintType` 会让它成为 `BlueprintCallable` 的合法形参，从而**意外扩大蓝图承诺面**（R0 §9 蓝图不承诺）；
- 先例 = `FTcsCombatEntityHandle`（同为展平字段的反射句柄）。

#### Scenario: 句柄可作反射方法的返回与参数

- **WHEN** 检查 `ExecuteChain` / `IsRunActive` / `ResumeRun` 的反射签名
- **THEN** `FTcsChainRunHandle` 作为返回类型与形参类型均合法（UHT 编译通过、宿主脚本层可见该类型）

#### Scenario: 句柄值可跨语言往返

- **WHEN** 脚本层调 `ExecuteChain`（链含挂起步骤）接住返回的句柄，随后原样传回 `IsRunActive`
- **THEN** 判定为 `true`（句柄值完整往返：`Index`/`Generation` 均保持，代际校验通过）

#### Scenario: 句柄字段可被脚本层读出

- **WHEN** 脚本层读取接住句柄的 `Index` / `Generation`
- **THEN** 读到的是池/登记表的真实值（非零值）——绑定产物 MUST 为这两个字段生成真实的读写代码（非空壳）

