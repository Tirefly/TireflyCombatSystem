## 1. 关系行类型与检查器（D1/D2/D5）

- [x] 1.1 `Source/TcsState/Public/State/TcsStateRelation.h`：新增 `FTcsRelationRow`（纯值 struct，五字段 `FGameplayTag AppliesToTag` + 四个 `FGameplayTagContainer`）+ `FTcsStateRelation` 静态函数族声明（`CheckRequired` / `CheckForbidden` / `CollectCancels` / `CollectBlocks`）——形参**全是纯值**（主体词集 + 客体词集 + 行），MUST NOT 出现任一域 Def/运行态类型
- [x] 1.2 `Source/TcsState/Private/State/TcsStateRelation.cpp`：实现——判定只走 `FGameplayTag::MatchesTag`（层级匹配）与 `FGameplayTagContainer::HasAny`，**MUST NOT 引入 `FGameplayTagQuery`**
- [x] 1.3 头注释写清 D2（只走 `CategoryTags` 一条通道、不做并集）与 D5（`Blocks` → `ForbiddenTags` 的方向陷阱）

## 2. 字段改名与新增（D5）

- [x] 2.1 `TcsBuffDef.h`：`Blocks` → `ForbiddenTags`、`Requires` → `RequiredTags`、`Cancels` → `TagsToCancel`；**新增** `TagsToBlock`；四条注释按"受动/施动"两方向重写
- [x] 2.2 `TcsSkillDefData.h`：同上（技能侧同款四字段）
- [x] 2.3 `Priority` **本变更 MUST NOT 触碰**（归 `e4`，A1/A11"零消费者不预建"）

## 3. 跨域访问契约（D3 / N1b）

- [x] 3.1 共享层（`TcsState`）新增类型擦除的**在飞实例查询契约**：形参 = 实体句柄 + 主体词集 + 回调（`TFunctionRef`），返回逐条 `{DefTag, CategoryTags}` 纯值视图
- [x] 3.2 `TcsBuff` 在子系统初始化时**向下注册**该契约的实现（读自己的 per-entity 桶）
- [x] 3.3 双向核：`TcsBuff.Build.cs` 与 `TcsSkill.Build.cs` 的依赖方向不变（**MUST NOT** 引入 `TcsSkill → TcsBuff`）

## 4. 三处挂点（D4）

- [x] 4.1 `TcsStateOps_Relation.cpp`（新）：apply 挂点——`ForbiddenTags` 命中 ⇒ 拒绝（`EBAR_Rejected` + 具名 `Log`，**零事件**）；`RequiredTags` 未满足 ⇒ 同拒
- [x] 4.2 **反向查找**（D4）：apply 时查"是否存在存活实例的 `TagsToBlock` 覆盖我" ⇒ 命中即拒绝。扫描域 = **该实体桶**（非全局）；装置须给扫描边界读数
- [x] 4.3 `TagsToBlock` **即时阻断**：声明方开始时，对**已在跑**的目标逐条移除/标记不可开始
- [x] 4.4 `TagsToCancel`：施加成功路径按它逐条 `RemoveState(handle, EBRC_Cancelled)`——**该枚举首次有内建产生者**
- [x] 4.5 `TcsSkillOps_Relation.cpp`（新）：技能门禁序列插入关系门（同检查器，主体 = `FTcsSkillInstance`）
- [x] 4.6 顺序：关系门判定**排在共存决策之前**（先过关系门再看叠层）

## 5. 改名族（D6）

- [x] 5.1 `FTcsPhaseSpan::bInterruptible` → `bCancellable`；`FTcsSkillDefData::bInterruptibleDefault` → `bCancellableDefault`
- [x] 5.2 `ETcsSkillCooldownTiming::ECT_OnCastInterrupted` → `ECT_OnCastCanceled`
- [x] 5.3 `ECastFinishReason::ECFR_Interrupted` → `ECFR_Canceled`（含 `UMETA` 文案）
- [x] 5.4 `ITcsCastStateQuery::IsInterruptibleNow()` → `CanBeCanceled()`（含实现与门面转发）
- [x] 5.5 **事件 tag `TcsEvent.Cast.Interrupted` MUST NOT 改名**（大概率退役）
- [x] 5.6 **反向核**：按「新名 + 宿主类型」全库复扫（`CanBeCanceled` / `bCancellable` / `ECFR_Canceled` / `ECT_OnCastCanceled`），确认零旧名残留

## 6. 载荷补字段（D8）

- [x] 6.1 `FTcsCastEventPayload` 新增 `ECastFinishReason Reason`（`BlueprintReadOnly`）
- [x] 6.2 四路终结在填载荷时写入 `Reason`（收口在 `FinishSkillInstance` 一处）

## 7. 作者期校验

- [x] 7.1 `TcsBuffDefAsset.cpp`：两条 Error——① `A.TagsToCancel` 与 `B.TagsToCancel` 互相包含 ⇒ 双向强夺；② `A.ForbiddenTags ∋ A` 自指 ⇒ 永不可满足
- [x] 7.2 技能侧同款两条（`TcsSkillDef.cpp`）
- [x] 7.3 **原"六类/八类错误"计数同步**（新增两条后，规格与装置读数须同批回改）

## 8. 资产迁移

- [x] 8.1 2 个资产重存：`DA_SkillDef_E2E_Replaceable` / `DA_SkillDef_E2E_SinglePhase`（`bInterruptible` → `bCancellable`，**同型改名**）
- [x] 8.2 磁盘级复核：旧名 0 命中 / 新名 2 命中（`grep -a`）

## 9. 配置与文档

- [x] 9.1 `Config/DefaultGameplayTags.ini:39` / `:59` 的 `DevComment` 订正（删过期 `Def.StatusTag` 表述）
- [x] 9.2 `STAT-4` 闭合：台账该行结账（**不开根**——词归宿主 ini）
- [x] 9.3 `gameplay-tag-governance` 根表 12 与 5 处数字副本**保持不变**（核对并记明"本批未动"）

## 10. 验收

- [x] 10.1 双配置编译（Editor Win64 Development + Game Win64 Shipping）`Succeeded`，**零 warning**
- [x] 10.2 装置检查：`ForbiddenTags` 命中 ⇒ 拒且**零事件**；`RequiredTags` 未满足 ⇒ 拒  ← 装置 `Tcs.Test.State.Relation` 实测覆盖（见证据档）
- [x] 10.3 装置检查：`TagsToCancel` 命中 ⇒ 目标被移除、**原因读数 = `Cancelled`**  ← 装置 `Tcs.Test.State.Relation` 实测覆盖（见证据档）
- [x] 10.4 装置检查：`TagsToBlock` **两条时机都可判**（目标后到被拒 / 目标已在跑被即时阻断）  ← 装置 `Tcs.Test.State.Relation` 实测覆盖（见证据档）
- [x] 10.5 **阴性对照**：空容器（既有资产现状）⇒ 行为**逐字不变**
- [x] 10.6 **填法即策略**两种配置各取一次读数（`TagsToCancel` 单独 = 后到者赢 / `ForbiddenTags` + `TagsToCancel` = 在位者赢）  ← 装置 `Tcs.Test.State.Relation` 实测覆盖（见证据档）
- [x] 10.7 两条作者期 Error 各被构造一次并报出  ← 装置 `Tcs.Test.State.Relation` 实测覆盖（见证据档）
- [x] 10.8 **PIE 验收（硬门）**：四轮读数、`Run` 区段零非预期红字；**PIE 未跑 ⇒ 本变更 MUST NOT 标记完成**
- [x] 10.9 `openspec validate --all --strict` 通过；提案归档
