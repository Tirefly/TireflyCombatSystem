## MODIFIED Requirements

### Requirement: 通用数据步骤

`TcsDamage` MUST 提供两个**数据化**步骤（把"宿主挂点"变成可配置数据；09 §2.2 的两处）：

- `FTcsFlowModify{ FGameplayTag TargetKey; ETcsAttributeOp Op; FTcsParamValue Operand; TArray<FInstancedStruct> Conditions; }`——数据化黑板写入（"破甲阶段" = 一个数据步骤；**2026-09-22 改造：`TargetKey` 类型 `FName` → `FGameplayTag`**）；Operand 为 `FTcsParamValue`（黑板键引用保留为流程域自身的 Operand 选项，随其来源策略轮落地）。**MUST NOT 携带消耗策略**——**理由于 2026-09-30 改写**：原理由为"`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调（纯 C++、无 `USTRUCT` 宏、不可作 `UPROPERTY`）"，该技术事实已由本批的「消耗策略为纯数据可反射结构」需求消除（`OnConsumed` 改事件语义）；**禁令本身不变，理由改为职责划分**——数据步骤只做**纯数值写入**，消耗型提交属"修改器通道"语义（`ModifyFlow` 链原语与 C++ 步骤的职责；见 `damage-primitive` 的「ModifyFlow 链原语」需求）；
- `FTcsFlowDelegate{ FGameplayTag TargetKey; TScriptInterface<UTcsDamageFlowDelegate> Delegate; ... }`——数据化委托调用（轻量公式挂法；**2026-09-22 改造：`TargetKey` 类型改 tag**）。

#### Scenario: 数据步骤可配可跑

- **WHEN** 模板里放一个 `FTcsFlowModify`（TargetKey = 某 tag、Op = `TAO_Add`、Operand = Literal 5）
- **THEN** 该步执行后黑板对应键多出 5 的贡献（无需任何 C++ 步骤）
