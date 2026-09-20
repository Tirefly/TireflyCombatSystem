## ADDED Requirements

### Requirement: 链与步骤数据形状

`TcsEffect` MUST 以**数据 struct**（非策略类/非类层级）承载效果链与步骤（D4-3/D4-16；分离宪法 R0 §8：语义 = 词汇（C++）、链 = 句子（数据））：

- `FTcsEffectChain{ FName ChainId; TArray<FInstancedStruct> Steps; int32 MaxStepsPerFrame = 64; }`（USTRUCT）——有序步骤数组 + 单帧步数上限（熔断语义见 `effect-interpreter`）；
- `FTcsEffectStep{ FInstancedStruct StepData; }`（USTRUCT）——"把步骤当字段"的容器形状（链数组直接存 `FInstancedStruct`；本 struct 供内联挂载/传递类场合复用）；
- **步骤类型无公共基类**（D4-16：15 原语按归属分住各模块）：链的步骤数组 MUST NOT 设 `BaseStruct` 编辑器限定——类型合法性由**执行器注册表在执行期**判定（未注册即拒绝，见 `effect-step-dispatch`）；链自带 `MaxStepsPerFrame` 默认 64（单帧步数熔断上限）。

#### Scenario: 链定义字段完整

- **WHEN** 构造一条 `FTcsEffectChain`（指定 ChainId 与两个步骤、不设上限）
- **THEN** `MaxStepsPerFrame` 取默认值 64，步骤数组保持插入顺序

#### Scenario: 无公共基类的步骤数组

- **WHEN** 在同一链的步骤数组里放入两种不同归属的步骤 struct（如本模块的 `FTcsStepWaitDelay` 与某领域模块的步骤类型）
- **THEN** 二者共存于同一 `TArray<FInstancedStruct>`，链定义本身不因类型族不同而失败（分派发生在执行期）

### Requirement: 链定义登记表

`UTcsEffectSubsystem`（世界级子系统门面）MUST 提供链定义登记表，**登记键 = `FTcsEffectChain::ChainId`**（单一真相：id 只写在链定义里，调用方不另传）：

- `RegisterChain(const FTcsEffectChain& Chain)`：登记链定义；
- `UnregisterChain(FName ChainId)`：注销链定义；**有活动运行态引用该链时 MUST 拒绝**（否则运行态持有的链定义会被抽走）；
- `FindChain(FName ChainId)`：查询（**未登记返回 nullptr，不 ensure**——查询是正常路径，单位/链未登记不算契约违规）；
- 拒绝面（ensure 提示 + 返回 false）：`ChainId` 为空、同 id 重复登记（D2-1 同款口径：词表/定义重名 = 加载期错误，**不得静默覆写**）；
- 登记表 MUST 以**地址稳定**的持有方式存链定义（`TMap<FName, TUniquePtr<FTcsEffectChain>>` 或同效手段）——解释器在一次进入执行期间持有链定义引用，后续登记不得使其悬空（引擎事实 2026-09-17：`TMap`/`TSet` 元素住连续缓冲、扩容即搬移）。

#### Scenario: 登记后按 id 查到

- **WHEN** `RegisterChain` 一条 `ChainId = "Chain_Test"` 的链后调 `FindChain("Chain_Test")`
- **THEN** 返回该链定义，步骤数组与登记内容一致

#### Scenario: 重复登记被拒且不覆写

- **WHEN** 对同一 `ChainId` 再次 `RegisterChain`（内容不同）
- **THEN** 返回 false、原定义保持不变、留 ensure 提示与日志

#### Scenario: 未登记查询不报错

- **WHEN** `FindChain` 一个未登记的 id
- **THEN** 返回 nullptr（无 ensure、无 Error 日志）

#### Scenario: 活动运行态的链不可注销

- **WHEN** 一条链仍有挂起中的运行态时 `UnregisterChain` 该 id
- **THEN** 返回 false、定义保留（运行态不因定义被抽走而悬空）

#### Scenario: 登记其他链不使已取引用悬空

- **WHEN** 取到某链定义引用后，再登记若干新链定义
- **THEN** 原引用仍指向同一条链定义（内容未损）
