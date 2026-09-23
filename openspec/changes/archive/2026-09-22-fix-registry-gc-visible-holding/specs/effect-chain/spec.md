## MODIFIED Requirements

### Requirement: 链定义登记表

`UTcsEffectSubsystem`（世界级子系统门面）MUST 提供链定义登记表，**登记键 = `FTcsEffectChain::ChainId`**（单一真相：id 只写在链定义里，调用方不另传）：

- `RegisterChain(const FTcsEffectChain& Chain)`：登记链定义；
- `UnregisterChain(FGameplayTag ChainId)`：注销链定义；**有活动运行态引用该链时 MUST 拒绝**（否则运行态持有的链定义会被抽走）；
- `FindChain(FGameplayTag ChainId)`：查询（**未登记返回 nullptr，不 ensure**——查询是正常路径，单位/链未登记不算契约违规）；
- 拒绝面（ensure 提示 + 返回 false）：`ChainId` **无效**（`!ChainId.IsValid()`）、同 id 重复登记（D2-1 同款口径：词表/定义重名 = 加载期错误，**不得静默覆写**）；
- 登记表 MUST 以**地址稳定**的持有方式存链定义（`TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>>` 或同效手段；**2026-09-22 改造：键类型改 tag**）——解释器在一次进入执行期间持有链定义引用，后续登记不得使其悬空（引擎事实 2026-09-17：`TMap`/`TSet` 元素住连续缓冲、扩容即搬移）；
- 登记表 MUST 同时做到**对 GC 可见**（2026-09-23 补，与 `damage-flow` 同批修台账 T-8）：链步骤可放**任意宿主自定义 struct**（D4-16 步骤无公共基类、picker 不设限），其中的 `UPROPERTY` 对象引用（委托 `TScriptInterface` / 资产 `TObjectPtr`）须被保活 —— 而**裸 C++ 容器不经 GC 的 `RefLink`**，故子系统 MUST 覆写 `AddReferencedObjects` 并逐链调 `FReferenceCollector::AddPropertyReferencesWithStructARO`，否则那些引用会被静默回收（步骤取到空引用，而非崩溃）。

**地址稳定与 GC 可见是两件正交的事**：前者防 `TMap` 扩容搬移导致解释器持有的 C++ 引用悬空；后者防对象引用被 GC 回收。二者 MUST 同时满足。

#### Scenario: 登记后按 id 查到

- **WHEN** `RegisterChain` 一条 `ChainId = Tcs.Chain.Chain_Test` 的链后调 `FindChain`（同一 tag）
- **THEN** 返回该链定义，步骤数组与登记内容一致

#### Scenario: 重复登记被拒且不覆写

- **WHEN** 同一 `ChainId` 两次 `RegisterChain`
- **THEN** 第二次 ensure 命中且返回 false，登记表内仍是首次内容

#### Scenario: 链步骤里的对象引用对 GC 可见

- **WHEN** 登记一条含对象引用的链（步骤带 `TScriptInterface` 或 `TObjectPtr` 字段），且该对象**无其他强引用**，随后触发 GC
- **THEN** GC 后该对象仍存活（未被回收）；链步骤取到的引用仍有效
