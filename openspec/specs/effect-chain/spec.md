# effect-chain Specification

## Purpose
定义效果链与链步骤的数据形状、链定义的登记表与解析入口、效果链上下文（黑板：类/实体/运行态/黑板值与生命周期纪律），以及链运行态句柄的反射性。

## Requirements

### Requirement: 链与步骤数据形状

`TcsEffect` MUST 以**数据 struct**（非策略类/非类层级）承载效果链与步骤（D4-3/D4-16；分离宪法 R0 §8：语义 = 词汇（C++）、链 = 句子（数据））：

- `FTcsEffectChain{ FGameplayTag ChainId; TArray<FInstancedStruct> Steps; int32 MaxStepsPerFrame = 64; }`（USTRUCT；**2026-09-22 改造：`ChainId` 类型 `FName` → `FGameplayTag`**）——有序步骤数组 + 单帧步数上限（熔断语义见 `effect-interpreter`）；
- `FTcsEffectStep{ FInstancedStruct StepData; }`（USTRUCT）——"把步骤当字段"的容器形状（链数组直接存 `FInstancedStruct`；本 struct 供内联挂载/传递类场合复用）；
- **步骤类型无公共基类**（D4-16：15 原语按归属分住各模块）：链的步骤数组 MUST NOT 设 `BaseStruct` 编辑器限定——类型合法性由**执行器注册表在执行期**判定（未注册即拒绝，见 `effect-step-dispatch`）；链自带 `MaxStepsPerFrame` 默认 64（单帧步数熔断上限）。

**链 id 的来源（2026-09-22；2026-10-01 换根）**：链是**宿主内容**（策划创作），故 `ChainId` 属**宿主词汇**——由宿主项目 `Config/DefaultGameplayTags.ini` 声明（**根 = `EffectChain`**，形态 `EffectChain.<Id>`；根段注册表与归属规则见 `gameplay-tag-governance` 能力），插件 MUST NOT 声明任何具体链 id。

#### Scenario: 链定义字段完整

- **WHEN** 构造一条 `FTcsEffectChain`（指定 ChainId 与两个步骤、不设上限）
- **THEN** `MaxStepsPerFrame` 取默认值 64，步骤数组保持插入顺序

#### Scenario: 无公共基类的步骤数组

- **WHEN** 在同一链的步骤数组里放入两种不同归属的步骤 struct（如本模块的 `FTcsStepWaitDelay` 与某领域模块的步骤类型）
- **THEN** 二者共存于同一 `TArray<FInstancedStruct>`，链定义本身不因类型族不同而失败（分派发生在执行期）

#### Scenario: 链 id 的根与段数

- **WHEN** 检查一条宿主链 id 的形态
- **THEN** 它形如 `EffectChain.<Id>`（2 段，留 2 段深度余量；换根前 `Tcs.Chain.<Id>` 与链上的其它角色同挤在一个 `Tcs` 根下），且由宿主 ini 声明——插件不声明任何具体链 id

### Requirement: 链定义登记表

`UTcsEffectSubsystem`（世界级子系统门面）MUST 提供链定义登记表，**登记键 = `FTcsEffectChain::ChainId`**（单一真相：id 只写在链定义里，调用方不另传）：

- `RegisterChain(const FTcsEffectChain& Chain)`：登记链定义；
- `UnregisterChain(FGameplayTag ChainId)`：注销链定义；**有活动运行态引用该链时 MUST 拒绝**（否则运行态持有的链定义会被抽走）；
- `FindChain(FGameplayTag ChainId)`：查询（**未登记返回 nullptr，不 ensure**——查询是正常路径，单位/链未登记不算契约违规）；
- 拒绝面（ensure 提示 + 返回 false）：`ChainId` **无效**（`!ChainId.IsValid()`）、同 id 重复登记（D2-1 同款口径：词表/定义重名 = 加载期错误，**不得静默覆写**）；
- 登记表 MUST 以**地址稳定**的持有方式存链定义（`TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>>` 或同效手段；**2026-09-22 改造：键类型改 tag**）——解释器在一次进入执行期间持有链定义引用，后续登记不得使其悬空（引擎事实 2026-09-17：`TMap`/`TSet` 元素住连续缓冲、扩容即搬移）；
- 登记表 MUST 同时做到**对 GC 可见**（2026-09-23 补，与 `damage-flow` 同批修台账 WAIT-8）：链步骤可放**任意宿主自定义 struct**（D4-16 步骤无公共基类、picker 不设限），其中的 `UPROPERTY` 对象引用（委托 `TScriptInterface` / 资产 `TObjectPtr`）须被保活 —— 而**裸 C++ 容器不经 GC 的 `RefLink`**，故子系统 MUST 覆写 `AddReferencedObjects` 并逐链调 `FReferenceCollector::AddPropertyReferencesWithStructARO`，否则那些引用会被静默回收（步骤取到空引用，而非崩溃）。

**地址稳定与 GC 可见是两件正交的事**：前者防 `TMap` 扩容搬移导致解释器持有的 C++ 引用悬空；后者防对象引用被 GC 回收。二者 MUST 同时满足。

#### Scenario: 登记后按 id 查到

- **WHEN** `RegisterChain` 一条 `ChainId = EffectChain.ChainTest` 的链后调 `FindChain`（同一 tag）
- **THEN** 返回该链定义，步骤数组与登记内容一致

#### Scenario: 重复登记被拒且不覆写

- **WHEN** 同一 `ChainId` 两次 `RegisterChain`
- **THEN** 第二次 ensure 命中且返回 false，登记表内仍是首次内容

#### Scenario: 链步骤里的对象引用对 GC 可见

- **WHEN** 登记一条含对象引用的链（步骤带 `TScriptInterface` 或 `TObjectPtr` 字段），且该对象**无其他强引用**，随后触发 GC
- **THEN** GC 后该对象仍存活（未被回收）；链步骤取到的引用仍有效

### Requirement: 效果链上下文（黑板）

`TcsEffect` MUST 以运行态结构 `FTcsEffectContext` 承载链执行期间的数据流（04 §2.1 黑板即上下文）：

- **参与者一律为实体身份句柄**（`FTcsCombatEntityHandle`）——`Caster`（施法者）/ `Instigator`（发起者）为单句柄，`Targets` 为句柄数组；**MUST NOT 使用 `AActor*` / `TWeakObjectPtr<AActor>`**（D3-1 Actor 无关性；06 §33"Mass 适配核心零改动"的前提）；
- `EventPayload`（`FInstancedStruct`——触发事件载荷，无事件触发时为默认构造）；
- `Variables`（`TMap<FGameplayTag, double>`——链内变量，SetVar/Branch 类步骤的载体；**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）——**键的根 = `EffectChainRunVar`**；该根的定义（消费角色 / 声明方 / 形态）与命名契约住 `effect-interpreter` 的门面 API 需求（消费者 `SetRunVariable` / `TryGetRunVariable` 的定义处）与 `gameplay-tag-governance` 的根段注册表，本需求不重复规则；
- **属性捕获（CapturedAttrs）与宿主能力引用不住这里**：前者归 TcsDamage 流程上下文（09 §2.1），后者经门面注入点取得（`GetEntityQuery` 等）——黑板只持本次执行的数据；
- 生命期：随链运行态（`FTcsChainRun`）自持，**MUST NOT 跨帧持有**（运行态释放即失效）；
- **MUST NOT 作配置数据载体**（纯运行态）：MUST NOT 加 `BlueprintType`、MUST NOT 出现在任何 Def 资产的可编辑字段里、MUST NOT 加 `EditAnywhere` 类 specifier（措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`）；
- **本约束不涉及类型反射可见性**（`USTRUCT()` 宏的有无）：类型是否反射可见与"能否作配置数据"是两件独立的事——前者由本需求不约束，若未来需让宿主脚本**直接持有/构造**本 struct（而非经门面按句柄访问器读写），须另行评估（台账 SCRIPT-3）。

#### Scenario: 参与者是句柄而非 Actor

- **WHEN** 检查 `FTcsEffectContext` 的参与者字段类型
- **THEN** 全部为 `FTcsCombatEntityHandle`，无任何 Actor 指针形态

### Requirement: 链运行态句柄的反射性

`FTcsChainRunHandle` MUST 是**反射可见类型**（`USTRUCT()`）——它是脚本层与宿主消费"起链结果"的唯一身份词：脚本层调 `ExecuteChain` 接住句柄、再把它传回 `IsRunActive` / `ResumeRun` 查询与唤醒（2026-09-24 用户拍板：让项目成员能用 C# 写技能/Buff 逻辑）。

- **字段 MUST 展平**（2026-09-24 实测修正）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`——后者是模板类型、无法作 `UPROPERTY`，会让绑定产物生成**空壳**（`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒ **代际失配、往返失效**（实测：池给 `{Index=0, Generation=1}`，C# 传回 `{0, 0}`，`IsValid` 判 false）；
- `Index` MUST 为 `int32`（UHT 不支持 `uint32` 作属性类型）；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**——MUST 经唯一的转换点（`GetInner`/`SetInner`）与池句柄互转，保证往返无损；
- **MUST 为 `BlueprintType`（2026-09-24 放宽，台账 SCRIPT-8 连带）**：原"MUST NOT 加 `BlueprintType`"的顾虑是"成为 `BlueprintCallable` 的合法形参 ⇒ 意外扩大蓝图承诺面"；插槽路线（`effect-step-dispatch` 的"步骤执行器插槽"）需要句柄出现在 **`BlueprintNativeEvent` 签名**里，而 UHT 对 `BlueprintEvent` 强制全部形参蓝图可表达（`UhtFunction.cs:859`/`:1043-1053`）⇒ 非 `BlueprintType` 则插槽**编译不过**。放宽的**承诺面代价为零**：消费句柄的门面方法仍为 `UFUNCTION()` 无 specifier（蓝图不可见），蓝图能"看见类型"却**无任何可调用的门面**；插槽接口确实蓝图可实现，但那是 R0 §9 已接受的"恰好蓝图也能用"（台账 SCRIPT-8），**不是新增承诺**；
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
